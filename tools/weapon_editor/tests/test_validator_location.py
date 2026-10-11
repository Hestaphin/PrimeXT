import sys
import tempfile
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validator_location import find_validator

class ValidatorDiscoveryTests(unittest.TestCase):
    def test_windows_build_root_exe(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = Path(tmp)
            executable = build/'primext-weapon-validate.exe'
            executable.touch()
            self.assertEqual(executable, find_validator(build))
    def test_windows_release_exe(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = Path(tmp)
            (build/'Release').mkdir()
            executable = build/'Release'/'primext-weapon-validate.exe'
            executable.touch()
            self.assertEqual(executable, find_validator(build))
    def test_linux_executable(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = Path(tmp)
            executable = build/'primext-weapon-validate'
            executable.touch()
            self.assertEqual(executable, find_validator(build))

if __name__ == '__main__':
    unittest.main()
