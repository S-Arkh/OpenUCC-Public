"""Exercise the extracted Windows launchers using only authored synthetic data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def launcher(bundle, arguments='', success=True, message=None, without_filehash=False):
    script = 'Compile.cmd'
    logs = bundle / 'Logs'
    before = set(logs.glob('*/result.json'))
    env = os.environ.copy()
    env['OPENUCC_NO_PAUSE'] = '1'
    if without_filehash:
        # Reproduce a Windows PowerShell host where the other required commands
        # are available but the optional Get-FileHash function is not.
        powershell = Path(os.environ['SystemRoot']) / 'System32/WindowsPowerShell/v1.0/powershell.exe'
        script_path = str(bundle / 'Compile.ps1').replace("'", "''")
        code = f"""
            $required = 'ConvertFrom-Json','Sort-Object','Get-ChildItem','New-Item',
                        'Get-Content','Test-Path','Resolve-Path','Copy-Item','Start-Process'
            Get-Command $required -ErrorAction Stop | Out-Null
            Remove-Item Function:\\Get-FileHash -ErrorAction SilentlyContinue
            $PSModuleAutoLoadingPreference = 'None'
            if (Get-Command Get-FileHash -ErrorAction SilentlyContinue) {{
                throw 'Get-FileHash must be unavailable for this regression test'
            }}
            & '{script_path}' {arguments}
        """
        command = [str(powershell), '-NoProfile', '-NonInteractive',
                   '-ExecutionPolicy', 'Bypass', '-Command', code]
    else:
        # /s /c removes the outer pair, preserving the quoted batch-file path.
        command = f'cmd.exe /d /s /c ""{bundle / script}" {arguments}"'
    run = subprocess.run(command, cwd=bundle.parent, env=env, capture_output=True,
                         text=True, errors='replace', timeout=120)
    records = set(logs.glob('*/result.json')) - before
    require(len(records) == 1, f'{script}: expected one result.json: {run.stdout} {run.stderr}')
    record_path = records.pop()
    record = json.loads(record_path.read_text(encoding='utf-8-sig'))
    require((run.returncode == 0) == success, f'{script}: exit {run.returncode}: {run.stdout} {run.stderr}')
    require(record['status'] == ('PASS' if success else 'FAIL'), str(record))
    if message:
        require(message in record.get('error', ''), str(record))
    if message == 'Compiler failed':
        require((record_path.parent / 'stderr.log').read_bytes(), 'Compiler diagnostics are missing')
    if success:
        output = bundle / 'Output' / (record['package'] + '.u')
        require(output.is_file() and digest(output) == record['sha256'].lower(), str(record))
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--fixture-tool', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Requires Windows')
    bundle = args.bundle.resolve()
    classes = bundle / 'Classes'
    dependencies = bundle / 'Dependencies'
    require(not (bundle / 'Profiles').exists(), 'Unexpected regional profile layout')
    launcher(bundle, success=False, message='No *.uc')
    example = classes / 'Example.uc'
    shutil.copyfile(bundle / 'Examples/Example.uc', example)
    launcher(bundle, success=False, message='Missing target Core.u')
    with tempfile.TemporaryDirectory(prefix='synthetic core ') as temp:
        core = Path(temp) / 'Core.u'
        subprocess.run([str(args.fixture_tool.resolve()), str(core)], check=True, timeout=30)
        core_data = core.read_bytes()
    (dependencies / 'Core.u').write_bytes(core_data)
    launcher(bundle)
    default_output = bundle / 'Output/Interface.u'
    require(default_output.is_file(), 'Default Interface output missing')
    inspect = subprocess.run([str(bundle / 'bin/openucc.exe'), 'inspect', str(default_output)],
                             check=True, capture_output=True, text=True, timeout=30)
    result = json.loads(inspect.stdout)
    require((result['version'], result['licensee']) == (133, 40),
            'Output did not follow the supplied synthetic Core')
    launcher(bundle, '-Package NoHashCmd', without_filehash=True)
    no_hash_output = bundle / 'Output/NoHashCmd.u'
    no_hash_previous = digest(no_hash_output)
    example.write_text('class Example extends Object; invalid syntax\n', encoding='utf-8')
    launcher(bundle, '-Package NoHashCmd', success=False, message='Compiler failed',
             without_filehash=True)
    require(digest(no_hash_output) == no_hash_previous,
            'Failure without Get-FileHash changed previous output')
    example.write_text('class Example extends Object; var int Value; '
                       'defaultproperties { Value=7 }\n', encoding='utf-8')
    launcher(bundle, '-Package NoHashCmd', without_filehash=True)
    require(digest(no_hash_output) != no_hash_previous,
            'Successful rebuild without Get-FileHash did not replace output')
    shutil.copyfile(bundle / 'Examples/Example.uc', example)

    # The target is determined by the supplied Core, not a regional directory.
    # Both minimal Core packages are synthetic, not client compatibility tests.
    older_core = bytearray(core_data)
    struct.pack_into('<HH', older_core, 4, 123, 37)
    (dependencies / 'Core.u').write_bytes(older_core)
    launcher(bundle, '-Package OtherTarget')
    other = bundle / 'Output/OtherTarget.u'
    inspect = subprocess.run([str(bundle / 'bin/openucc.exe'), 'inspect', str(other)],
                             check=True, capture_output=True, text=True, timeout=30)
    result = json.loads(inspect.stdout)
    require((result['version'], result['licensee']) == (123, 37),
            'Output did not follow the supplied synthetic Core')
    (dependencies / 'Core.u').write_bytes(core_data)

    example.unlink()
    source = classes / 'BaseClass.uc'
    source.write_text('class BaseClass extends Object;\n', encoding='utf-8')
    launcher(bundle, '-Package BaseLib')
    base = bundle / 'Output/BaseLib.u'
    shutil.copyfile(base, dependencies / 'BaseLib.u')
    source.unlink()
    source = classes / 'MyClass.uc'
    good = 'class MyClass extends BaseLib.BaseClass;\nvar int Value;\ndefaultproperties { Value=42 }\n'
    source.write_text(good, encoding='utf-8')
    launcher(bundle, '-Package MyPackage')
    output = bundle / 'Output/MyPackage.u'
    inspect = subprocess.run([str(bundle / 'bin/openucc.exe'), 'inspect', str(output)],
                             check=True, capture_output=True, text=True, timeout=30)
    require('MyClass' in json.loads(inspect.stdout)['classes'], 'Compiled class missing')
    previous = digest(output)
    source.write_text('class MyClass extends BaseLib.BaseClass; invalid syntax\n', encoding='utf-8')
    launcher(bundle, '-Package MyPackage', success=False, message='Compiler failed')
    require(digest(output) == previous, 'Compiler failure changed previous output')
    source.write_text(good.replace('Value=42', 'Value=7'), encoding='utf-8')
    launcher(bundle, '-Package MyPackage')
    require(digest(output) != previous, 'Successful rebuild did not replace previous output')

    external = bundle.parent / 'Custom source с пробелами'
    external.mkdir()
    (external / 'External.uc').write_text('class External extends Object;\n', encoding='utf-8')
    launcher(bundle, f'-Package External -SourceDir "{external}"')

    # A package directory wins over root Classes even without a manifest entry.
    package_sources = bundle / 'Packages/MyPackage/Classes'
    package_sources.mkdir(parents=True)
    (package_sources / 'MyClass.uc').write_text(good, encoding='utf-8')
    source.write_text('this root file must not be compiled', encoding='utf-8')
    launcher(bundle, '-Package MyPackage')

    # A manifest pins only the requested dependencies.
    manifest = bundle / 'packages.json'
    (dependencies / 'Unused.u').write_bytes(b'not a package')
    previous = digest(output)
    launcher(bundle, '-Package MyPackage', success=False, message='Compiler failed')
    require(digest(output) == previous, 'Invalid automatic dependency changed previous output')
    manifest.write_text(json.dumps({'MyPackage': {'mode': 'compile', 'dependencies': ['BaseLib']}}),
                        encoding='utf-8')
    launcher(bundle, '-Package MyPackage')
    previous = digest(output)
    manifest.write_text(json.dumps({'MyPackage': {'mode': 'compile', 'dependencies': ['Missing']}}),
                        encoding='utf-8')
    launcher(bundle, '-Package MyPackage', success=False, message='Missing dependency')
    require(digest(output) == previous, 'Missing dependency changed previous output')
    manifest.write_text(json.dumps({'MyPackage': {'mode': 'compile',
                                                  'dependencies': ['BaseLib', 'BaseLib']}}),
                        encoding='utf-8')
    launcher(bundle, '-Package MyPackage', success=False, message='Duplicate dependency')
    require(digest(output) == previous, 'Duplicate dependency changed previous output')
    only_core = bundle / 'Packages/OnlyCore/Classes'
    only_core.mkdir(parents=True)
    (only_core / 'OnlyCore.uc').write_text('class OnlyCore extends Object;\n', encoding='utf-8')
    manifest.write_text(json.dumps({'OnlyCore': {'mode': 'compile', 'dependencies': []}}),
                        encoding='utf-8')
    launcher(bundle, '-Package OnlyCore')
    manifest.write_text('{}', encoding='utf-8')
    (classes / 'Object.uobj').write_text('object Class=Missing Name="Test" Outer="None" {}',
                                         encoding='utf-8')
    launcher(bundle, '-Package Objects -Objects',
             success=False, message='Object package requires its reference')
    print('PASS: neutral launcher, synthetic targets, dependencies and failure preservation')


if __name__ == '__main__':
    main()
