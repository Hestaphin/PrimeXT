#!/usr/bin/env python3
"""K.11.4.1.7 close-window source regression (native Win32 UI needs Windows for runtime verification)."""
from pathlib import Path
s = (Path(__file__).resolve().parents[1] / "pxwe_win32.cpp").read_text(encoding="utf-8")
assert "bool ConfirmDirty(bool closing = false)" in s
assert "case WM_CLOSE: if (app->ConfirmDirty(true)) DestroyWindow(window);" in s
assert "if (!closing) throw;" in s
assert "The current property cannot be applied." in s
assert "Close without saving your unsaved changes?" in s
assert "project.Save(); // Keeps strict validation on export." in s
assert "case CMD_SAVE: CommitEditor(); project.Save();" in s
assert s.count("MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING") == 2
print("PASS K.11.4.1.7 native close flow: invalid drafts can be discarded; explicit export still validates")
