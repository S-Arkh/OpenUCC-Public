"""The release archive is an explicit allowlist, not a copy of local inputs."""
from pathlib import Path
import json
import struct
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import package_portable as package


class PortablePackaging(unittest.TestCase):
    def test_identity_validation(self):
        package.validate_identity('1.2.3-rc.1', 'a' * 40)
        for version in ('v1.2.3', '../1.2.3', '1.2', '1.2.3+test', '1.2.3;echo'):
            with self.subTest(version=version), self.assertRaises(ValueError):
                package.validate_identity(version, 'a' * 40)
        with self.assertRaises(ValueError):
            package.validate_identity('1.2.3', 'HEAD')

    def test_release_sensitive_data_check(self):
        samples = (
            b'person' + b'@' + b'example.' + b'org',
            b'C:' + b'\\Users\\' + b'sample\\file',
            b'/home/' + b'sample/file',
            b'gh' + b'p_' + b'a' * 36,
            b'-----BEGIN ' + b'PRIVATE KEY-----',
        )
        for sample in samples:
            for data in (sample, sample.decode('ascii').encode('utf-16-le')):
                with self.subTest(sample=sample, data=data), self.assertRaises(ValueError):
                    package.check_sensitive_data('file', data)
        package.check_sensitive_data('file', b'Examples/Example.uc')

    def test_allowlist_neutral_layout_and_pe(self):
        (ROOT / 'local').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'local') as temp:
            folder = Path(temp)
            exe = folder / 'openucc.exe'
            binary = bytearray(128)
            binary[:2] = b'MZ'
            struct.pack_into('<I', binary, 60, 64)
            binary[64:68] = b'PE\0\0'
            struct.pack_into('<H', binary, 68, 0x8664)
            exe.write_bytes(binary)
            archive = folder / 'portable.zip'
            files = package.make_archive(archive, '1.2.3', 'b' * 40, exe)
            package.verify_archive(archive, '1.2.3', files)
            info = json.loads(files['BUILDINFO.json'])
            self.assertEqual((info['version'], info['revision']), ('1.2.3', 'b' * 40))
            self.assertEqual(info['files'], {name: package.sha256(data)
                             for name, data in files.items() if name != 'BUILDINFO.json'})
            with zipfile.ZipFile(archive) as z:
                members = z.namelist()
                forbidden = ('.u', '.utx', '.usx', '.ukx', '.uax', '.unr', '.dll', '.xdat')
                self.assertFalse(any(name.lower().endswith(forbidden) for name in members))
                prefix = 'OpenUCC-1.2.3-windows-x64/'
                for directory in package.DIRECTORIES:
                    self.assertIn(prefix + directory + '/', members)
                self.assertFalse(any(name.startswith(prefix + 'Profiles/') for name in members))
                self.assertEqual(z.read(prefix + 'packages.json'), b'{}\n')
                self.assertEqual([name for name in members if name.lower().endswith(('.uc', '.uobj'))],
                                 [prefix + 'Examples/Example.uc'])
                self.assertEqual({name[len(prefix):] for name in members
                                  if name.lower().endswith(('.cmd', '.ps1'))},
                                 {'Compile.cmd', 'Compile.ps1'})
            with self.assertRaises(FileExistsError):
                package.make_archive(archive, '1.2.3', 'b' * 40, exe)
            with zipfile.ZipFile(archive, 'a') as z:
                z.writestr('OpenUCC-1.2.3-windows-x64/Dependencies/Unexpected.u', b'test')
            with self.assertRaisesRegex(ValueError, 'Unexpected'):
                package.verify_archive(archive, '1.2.3', files)
            struct.pack_into('<H', binary, 68, 0x14c)
            with self.assertRaisesRegex(ValueError, 'x64'):
                package.check_exe(binary)
            with self.assertRaises(ValueError):
                package.check_exe(b'MZ')


if __name__ == '__main__':
    unittest.main()
