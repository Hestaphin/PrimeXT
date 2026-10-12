#!/usr/bin/env python3
"""Portable source guards for the J.10.6.9.9 blank Win32 page regression.

This does not emulate Windows painting; run the manual GUI smoke test too.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
s = (ROOT / 'pxwe_win32.cpp').read_text(encoding='utf-8')
assert 'TabCtrl_SetCurSel(tabs, 0);' in s, 'First tab must be selected on initialization'
assert 'int ActiveTab() const' in s
assert 'current >= 0 && current < 5 ? current : 0' in s
assert re.search(r'Move\(tabs, x, margin, width, tabHeight\);', s), 'Native tab HWND should cover strip only'
assert 'Move(tabs, x, margin, width, client.bottom' not in s, 'Native tab must not cover its sibling content'
assert 'TabCtrl_AdjustRect(tabs, FALSE, &page);' not in s, 'Do not place pages inside the native tab HWND'
assert 'const int px = x + Scale(8), py = margin + tabHeight + Scale(8);' in s
assert 'const int pw = width - Scale(16), ph = paneHeight - tabHeight - Scale(16);' in s
assert 'SetWindowPos(tabs, HWND_BOTTOM' in s, 'Keep native tab behind page controls'
assert 'void ShowPage()' in s
assert 'const int tab = ActiveTab();' in s
for expression in ['tab == 0 || tab >= 3', 'tab == 1', 'tab == 2', 'tab == 3', 'tab == 4']:
    assert expression in s, f'Missing visible page condition: {expression}'
assert 'header->code == TCN_SELCHANGE' in s
assert 'RefreshProperties(); RefreshCompatibility(); RefreshFlight(); RefreshVariants(); Layout();' in s
assert 'K.11' in s
print('PASS: initial tab selection, separate strip/page bounds, z-order, and all five tab transitions')
