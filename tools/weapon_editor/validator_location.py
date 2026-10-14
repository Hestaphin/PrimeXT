"""Shared, Tk-free executable discovery for the standalone editor on Windows/Linux."""
from pathlib import Path


def find_validator(build: Path) -> Path:
    candidates = [build / 'primext-weapon-validate',
                  build / 'primext-weapon-validate.exe',
                  build / 'Release' / 'primext-weapon-validate.exe',
                  build / 'Debug' / 'primext-weapon-validate.exe',
                  build / 'RelWithDebInfo' / 'primext-weapon-validate.exe']
    return next((candidate for candidate in candidates if candidate.is_file()), candidates[0])
