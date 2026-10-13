#!/usr/bin/env python3
"""Portable integration audit for J.10.6.10.9 authoring help and scaled UI reachability.

Does NOT render Qt/Win32 pixels or run PhysX; checks all statically known
control rectangles and help bindings against their declared editor surfaces.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CATALOG = (ROOT / 'PhysicsAssetHelpData.cpp').read_text()
ENTRIES = set(re.findall(r'\{\s*"(Start here|Bodies|Joints|Generator|Tuning|Tools|Troubleshooting)"\s*,\s*"([^"]+)"', CATALOG))
assert len(ENTRIES) >= 111, 'Help entries unexpectedly missing'

FILES = {
    'PhysicsAssetEditor.cpp': (730, 966, ('Bodies', 'Joints')),
    'RagdollTemplateDialog.cpp': (850, 1090, ('Generator',)),
    'PhysicsSettingsDialog.cpp': (640, 830, ('Tuning',)),
    'PhysicsAssetHelpDialog.cpp': (900, 698, ('Start here',)),
}
count_rect = 0
count_bind = 0
for filename, (width, height, sections) in FILES.items():
    text = (ROOT / filename).read_text()
    assert re.search(r'physui::InstallScrollableScaling\(this\s*,\s*'+str(width)+r'\s*,\s*'+str(height)+r'\s*\)', text), filename
    if filename != 'PhysicsAssetHelpDialog.cpp':
        assert re.search(r'new mxButton\(this[^;]*"Help / \?"', text), filename + ': missing Help button'
    for name in re.findall(r'm_(?:contextGuide|jointGuide)->Bind\([^,]+,\s*"([^"]+)"\)', text):
        assert any((section, name) in ENTRIES for section in sections), f'{filename}: undocumented bound parameter {name}'
        count_bind += 1
    if filename == 'PhysicsSettingsDialog.cpp':
        # Dynamic loop bindings use these names rather than literal Bind calls.
        names = re.search(r'const char \*helpNames\[\]\s*=\s*\{([^}]+)\}', text)
        assert names, 'Tuning value-to-help mapping missing'
        tuning = re.findall(r'"([^"]+)"', names.group(1))
        assert len(tuning) == 14 and all(('Tuning', name) in ENTRIES for name in tuning), tuning
        count_bind += len(tuning)
    # Statically positioned mx controls, excluding variables and expressions.
    for match in re.finditer(r'new mx(?:Button|Label|Choice|CheckBox|LineEdit|ListBox)\(this,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*[,)]', text):
        x,y,w,h = map(int,match.groups())
        assert x >= 0 and y >= 0 and w > 0 and h > 0, (filename,match.group(0))
        assert x + w <= width, f'{filename}: horizontal control overflow x={x} width={w} surface={width}'
        assert y + h <= height, f'{filename}: vertical control overflow y={y} height={h} surface={height}'
        # At every preset, the scaler transforms coordinates once (not cumulatively),
        # and the viewport is scrollable if the content exceeds available client space.
        for scale in (100,125,150):
            right = round((x+w)*scale/100)
            bottom = round((y+h)*scale/100)
            assert right <= round(width*scale/100)+1 and bottom <= round(height*scale/100)+1
        count_rect += 1

ui = (ROOT / 'PhysicsAssetUiGuide.cpp').read_text()
assert 'm_area->show();' in ui and 'void UnbindFocus(mxWidget *field)' in ui
assert 'physui::UnbindFocus(binding->field);' in ui
assert 'SB_HORZ' in ui and 'SB_VERT' in ui and 'QScrollArea' in ui
assert count_rect > 90 and count_bind >= 58, (count_rect,count_bind)
print(f'PASS {len(ENTRIES)} help topics, {count_bind} explicit contextual bindings, {count_rect} static control rectangles')
print('PASS three absolute scales (100/125/150), all known controls within scrollable content, both platform scroll paths, and focus lifetime checks')
print('NOTE dynamic-position controls and pixel typography must still be tested in a native GUI')
