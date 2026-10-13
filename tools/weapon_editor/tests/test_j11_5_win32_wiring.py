#!/usr/bin/env python3
"""Source-only J.11.5 GUI wiring regression. Does not emulate Windows GUI."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
s = (root / "pxwe_win32.cpp").read_text(encoding="utf-8")
resources = (root / "pxwe_resource.h").read_text(encoding="utf-8")
for tag in ("CMD_RENAME_ID", "CMD_USER_GUIDE", "CMD_COPY_HELP"):
    assert tag in resources and tag in s, f"missing UI command: {tag}"
for text in ("void RenameSelectedAsset()", "project.RenameId(selectionKind, original, requested)",
             "if (categorySelected) CategoryCommand(CMD_CATEGORY_RENAME)",
             "else RenameSelectedAsset()", "DetailedPropertyHelp(", "OpenClipboard(window)",
             "PrimeXT_Weapon_Editor_Beginner_Guide.md", "ShellExecuteW(",
             "PrimeXT Weapon Editor K.11"):
    assert text in s, f"missing UI wiring: {text}"
assert (root / "PrimeXT_Weapon_Editor_Beginner_Guide.md").exists()
manual = (root / "PrimeXT_Weapon_Editor_Beginner_Guide.md").read_text(encoding="utf-8")
for term in ("Attack Logic", "Damage Logic", "Default Fire Rate", "zero", "Rename ID",
             "External references WARNING", "First Person", "Preview Scale", "Undo"):
    assert term.lower() in manual.lower(), f"missing help topic: {term}"
print("PASS J.11.5 native editor wiring and beginner manual source guards")
