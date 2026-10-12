#!/usr/bin/env python3
"""J.11.4 Win32 source wiring smoke test, NOT a Windows GUI run."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'pxwe_win32.cpp').read_text(encoding='utf-8')
r=(root/'pxwe.rc').read_text(encoding='utf-8')
for token in (
    'C_CAMERA_PRESET', 'C_CAMERA_FIT', 'C_CAMERA_RESET',
    'C_CAMERA_FOV', 'C_CAMERA_SCALE', 'C_ANIMATION_SPEED',
    'pxwe::CameraPreset::FirstPerson', 'camera.SetPreset(',
    'camera.Rotate(', 'camera.Pan(', 'camera.ZoomBy(',
    'studio.Render(std::min(1024L, r.right)',
    'sequenceIndex, animationSeconds, camera, skinIndex, bodyIndex)',
    'Inherit', 'Custom', 'Effective', 'Edit cartridge settings...',
    'Attack Logic', 'Damage Logic', '1141', '1142', '1143', '1144',
    'EditOverrides(); else CompatibilityCommand(C_ALLOW);',
    'void SyncCameraControls()', 'PlaySoundW(nullptr, nullptr, 0); input->soundBytes.clear();'
):
    assert token in s, f'missing J.11.4 editor connection: {token}'
for text in (
    'IDD_OVERRIDES DIALOGEX 0, 0, 430, 348',
    'Inherit fire sound', 'Play effective WAV',
    'EDITTEXT 1108', 'EDITTEXT 1109', '1140'
):
    assert text in r, f'missing J.11.4 dialog control: {text}'
assert s.count('SetWindowPos(tabs, HWND_BOTTOM') == 1
print('PASS J.11.4 Win32 editor controls, ammo inheritance, sound and camera connections (source only)')
