"""Package and smoke-test a public openUCC Windows x64 distribution."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LAUNCHERS = ('Compile.cmd', 'Compile.ps1')
DIRECTORIES = ('bin', 'Classes', 'Dependencies', 'References', 'Packages',
               'Output', 'Work', 'Logs', 'Examples')


def validate_identity(version, revision):
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*)?', version):
        raise ValueError('Version must be X.Y.Z with an optional prerelease suffix')
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        raise ValueError('Revision must be a full lowercase Git commit SHA')


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def check_exe(data):
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError('Compiler is not a Windows PE executable')
    offset = struct.unpack_from('<I', data, 60)[0]
    if offset + 6 > len(data) or data[offset:offset + 4] != b'PE\0\0':
        raise ValueError('Invalid Windows PE header')
    if struct.unpack_from('<H', data, offset + 4)[0] != 0x8664:
        raise ValueError('Compiler must target Windows x64')


def check_sensitive_data(name, data):
    """Reject common credential and personal-path patterns in shipped files."""
    patterns = (
        ('email address', rb'[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}'),
        ('user path', rb'(?:[A-Z]:[\\/](?:USERS|DOCUMENTS AND SETTINGS)[\\/]|'
                      rb'/(?:HOME|USERS)/|/MNT/[A-Z]/USERS/)'),
        ('credential', rb'(?:GH[POUSR]_[A-Z0-9_]{20,}|GITHUB_PAT_[A-Z0-9_]{20,}|'
                       rb'AKIA[A-Z0-9]{16}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)'),
    )
    # Windows binaries can contain both narrow and UTF-16LE strings.
    strings = [data]
    strings.extend(match.group().decode('utf-16-le').encode('ascii')
                   for match in re.finditer(rb'(?:[\x20-\x7e]\x00){6,}', data))
    for description, pattern in patterns:
        if any(re.search(pattern, value, re.IGNORECASE) for value in strings):
            raise ValueError(f'Possible {description} in release file: {name}')


def contents(version, revision, exe):
    """Use an explicit allowlist; never traverse local or user-populated trees."""
    validate_identity(version, revision)
    binary = exe.read_bytes()
    check_exe(binary)
    files = {
        'bin/openucc.exe': binary,
        'LICENSE': (ROOT / 'LICENSE').read_bytes(),
        'README_RU.txt': (ROOT / 'tools/portable/PUBLIC_README_RU.txt').read_bytes(),
        'Examples/Example.uc': (ROOT / 'tools/portable/Example.uc').read_bytes(),
    }
    for name in LAUNCHERS:
        files[name] = (ROOT / 'tools/portable/public' / name).read_bytes()
    files['packages.json'] = b'{}\n'
    for name, data in files.items():
        check_sensitive_data(name, data)
    info = {
        'format': 1, 'application': 'openUCC', 'version': version,
        'revision': revision, 'target': 'Windows x64',
        'files': {name: sha256(data) for name, data in sorted(files.items())},
    }
    files['BUILDINFO.json'] = (json.dumps(info, indent=2) + '\n').encode('utf-8')
    return files


def directories():
    return set(DIRECTORIES)


def archive_name(version):
    return f'OpenUCC-{version}-windows-x64'


def make_archive(path, version, revision, exe):
    files = contents(version, revision, exe)
    prefix = archive_name(version)
    with zipfile.ZipFile(path, 'x', compression=zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(directories()):
            entry = zipfile.ZipInfo(f'{prefix}/{name}/', (1980, 1, 1, 0, 0, 0))
            entry.external_attr = (0o40755 << 16) | 0x10
            archive.writestr(entry, b'')
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(f'{prefix}/{name}', (1980, 1, 1, 0, 0, 0))
            entry.external_attr = 0o100644 << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data)
    return files


def verify_archive(path, version, expected):
    prefix = archive_name(version) + '/'
    names = {prefix + name for name in expected}
    dirs = {prefix + name + '/' for name in directories()}
    with zipfile.ZipFile(path) as archive:
        members = archive.namelist()
        if len(members) != len(set(members)) or set(members) != names | dirs:
            raise ValueError('Unexpected, missing or duplicate ZIP members')
        for name, data in expected.items():
            if archive.read(prefix + name) != data:
                raise ValueError('ZIP contents differ: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', required=True)
    parser.add_argument('--revision', required=True, help='Full SHA of the built source revision')
    parser.add_argument('--exe', type=Path, required=True, help='Built Windows x64 openucc.exe')
    parser.add_argument('--fixture-tool', type=Path, required=True, help='Built synthetic test fixture generator')
    parser.add_argument('--out', type=Path, required=True, help='New output directory; never overwrites an existing path')
    args = parser.parse_args()
    validate_identity(args.version, args.revision)
    if sys.platform != 'win32':
        parser.error('Packaged launcher verification requires Windows')
    args.out.mkdir(parents=True, exist_ok=False)
    name = archive_name(args.version)
    archive = args.out / (name + '.zip')
    expected = make_archive(archive, args.version, args.revision, args.exe)
    verify_archive(archive, args.version, expected)
    with tempfile.TemporaryDirectory(prefix='portable smoke ', dir=args.out) as temp:
        unpack = Path(temp) / 'Проверка с пробелами'
        with zipfile.ZipFile(archive) as source:
            source.extractall(unpack)
        bundle = unpack / name
        for relative, data in expected.items():
            if (bundle / relative).read_bytes() != data:
                raise ValueError('Extracted file differs: ' + relative)
        if not all((bundle / directory).is_dir() for directory in directories()):
            raise ValueError('Extracted archive is missing a portable directory')
        subprocess.run([sys.executable, '-B', str(ROOT / 'tests/portable_smoke.py'),
                        '--bundle', str(bundle), '--fixture-tool',
                        str(args.fixture_tool.resolve())], check=True)
    publish = args.out / 'publish'
    publish.mkdir()
    destination = publish / archive.name
    shutil.move(str(archive), destination)
    (publish / 'SHA256SUMS.txt').write_text(
        f'{sha256(destination.read_bytes())}  {destination.name}\n', encoding='ascii')
    (publish / 'RELEASE_NOTES.md').write_text(
        f'# openUCC {args.version} — Windows x64\n\n'
        f'Ревизия исходников: `{args.revision}`.\n\n'
        'Распакуйте архив и следуйте README_RU.txt. Пользователь предоставляет '
        'свои классы, целевой Core.u и необходимые зависимости. '
        'Игровые пакеты, SDK и чужие исходники не включены.\n\n'
        'После распаковки проверен запуск на синтетических данных и сохранение '
        'предыдущего результата при ошибке. Python и SDK при запуске не нужны. '
        'Успешная сборка не доказывает поведение в игре.\n',
        encoding='utf-8')
    print('PASS: verified portable ZIP:', destination)


if __name__ == '__main__':
    main()
