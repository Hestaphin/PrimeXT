// Standalone native Windows editor. Only OS controls/GDI and the portable game
// parser/integrator are linked; end users need neither source nor Python.
// Copyright (C) 2026 PrimeXT contributors. GPL-3.0-or-later.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <mmsystem.h>
#include "pxwe_project.h"
#include "pxwe_properties.h"
#include "pxwe_preview.h"
#include "cartridge_damage.h"
#include "pxwe_resource.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
HINSTANCE gInstance = nullptr;
constexpr wchar_t kWindowClass[] = L"PrimeXTWeaponEditor";
constexpr wchar_t kPreviewClass[] = L"PrimeXTWeaponPreview";
enum Control {
    C_TREE = 300, C_TAB, C_PROPERTIES, C_VALUE, C_APPLY, C_HINT,
    C_COMPAT, C_ALLOW, C_DISALLOW, C_DEFAULT, C_FLIGHT,
    C_MODEL_KIND, C_MODEL_LOAD, C_MODEL_OPEN, C_SEQUENCES, C_BOUNDS, C_MODEL_INFO, C_STATUS, C_BROWSE,
    C_VARIANTS, C_VARIANT_ADD, C_VARIANT_EDIT, C_VARIANT_COPY, C_VARIANT_REMOVE, C_VARIANT_PREVIEW,
    C_PLAY, C_RESET, C_SOUND, C_SKIN, C_BODY, C_COEFFICIENT, C_OVERRIDES,
    C_CURVES, C_METRIC, C_SPRITE_KIND, C_SPRITE_LOAD, C_SPRITE_PREVIEW, C_SPRITE_INFO
};
std::wstring Wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n) throw std::runtime_error("Cannot decode text as UTF-8.");
    std::wstring result(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), n);
    return result;
}
std::string Utf8(const std::wstring& s) {
    if (s.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (!n) throw std::runtime_error("Cannot encode text as UTF-8.");
    std::string result(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), n, nullptr, nullptr);
    return result;
}
std::wstring Text(HWND window) {
    const int n = GetWindowTextLengthW(window);
    std::wstring value(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(window, value.data(), n + 1); value.resize(n); return value;
}
pxwe::fs::path ExecutablePath() {
    std::wstring buffer(512, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!n) throw std::runtime_error("Cannot locate pxwe.exe.");
        if (n < buffer.size()) { buffer.resize(n); return pxwe::fs::path(buffer); }
        if (buffer.size() >= 32768) throw std::runtime_error("Executable path is too long.");
        buffer.resize(buffer.size() * 2);
    }
}
UINT WindowDpi(HWND window) {
    using GetDpi = UINT (WINAPI*)(HWND);
    const auto getDpi = reinterpret_cast<GetDpi>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (getDpi) return getDpi(window);
    HDC dc = GetDC(window); UINT dpi = dc ? static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX)) : 96;
    if (dc) ReleaseDC(window, dc); return dpi ? dpi : 96;
}
struct Input { std::wstring title, prompt, value; };
INT_PTR CALLBACK InputProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* input = reinterpret_cast<Input*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (msg == WM_INITDIALOG) {
        input = reinterpret_cast<Input*>(lParam); SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        SetWindowTextW(dialog, input->title.c_str());
        SetDlgItemTextW(dialog, IDC_PROMPT, input->prompt.c_str());
        SetDlgItemTextW(dialog, IDC_INPUT, input->value.c_str());
        SendDlgItemMessageW(dialog, IDC_INPUT, EM_SETLIMITTEXT, 255, 0);
        SendDlgItemMessageW(dialog, IDC_INPUT, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(dialog, IDC_INPUT)); return FALSE;
    }
    if (msg == WM_COMMAND) {
        if (LOWORD(wParam) == IDOK && input) {
            input->value = Text(GetDlgItem(dialog, IDC_INPUT)); EndDialog(dialog, IDOK); return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) { EndDialog(dialog, IDCANCEL); return TRUE; }
    }
    return FALSE;
}
bool Ask(HWND owner, Input& input) {
    const auto result = DialogBoxParamW(gInstance, MAKEINTRESOURCEW(IDD_INPUT), owner, InputProc, reinterpret_cast<LPARAM>(&input));
    if (result == -1) throw std::runtime_error("Cannot open the input dialog.");
    return result == IDOK;
}
bool PickResource(HWND owner, const pxwe::fs::path& root, const std::string& role, std::string& path) {
    const auto type = role == "hud_sprite" ? pxwe::ResourceType::Sprite : role == "kill_sprite" ? pxwe::ResourceType::KillSprite :
        pxwe::DescribeProperty(pxwe::Kind::Weapon, role).resource;
    const wchar_t* filter = type == pxwe::ResourceType::Model ? L"Studio models (*.mdl)\0*.mdl\0\0" :
        type == pxwe::ResourceType::Sound ? L"Wave sounds (*.wav)\0*.wav\0\0" :
        type == pxwe::ResourceType::KillSprite ? L"Sprites (*.spr)\0*.spr\0\0" : L"Sprites and HUD lists\0*.spr;*.txt\0\0";
    const auto directory = pxwe::BrowseDirectory(root, type).wstring(); std::wstring file(32768, L'\0');
    OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = owner; dialog.lpstrFilter = filter;
    dialog.lpstrFile = file.data(); dialog.nMaxFile = static_cast<DWORD>(file.size()); dialog.lpstrInitialDir = directory.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog)) { if (CommDlgExtendedError()) throw std::runtime_error("Cannot open resource browser."); return false; }
    path = pxwe::ResourcePath(root, pxwe::fs::path(file.c_str()), type); return true;
}
struct VariantInput {
    pxwe::fs::path root; std::wstring title; std::string id, role, path, when;
    std::vector<std::string> roles, conditions;
};
INT_PTR CALLBACK VariantProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* input = reinterpret_cast<VariantInput*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (msg == WM_INITDIALOG) {
        input = reinterpret_cast<VariantInput*>(lParam); SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        SetWindowTextW(dialog, input->title.c_str());
        SetDlgItemTextW(dialog, IDC_VARIANT_ID, Wide(input->id).c_str()); SetDlgItemTextW(dialog, IDC_VARIANT_PATH, Wide(input->path).c_str());
        for (int id : {IDC_VARIANT_ID, IDC_VARIANT_PATH}) SendDlgItemMessageW(dialog, id, EM_SETLIMITTEXT, id == IDC_VARIANT_ID ? 63 : 240, 0);
        for (size_t i = 0; i < input->roles.size(); ++i) {
            const auto label = Wide(input->roles[i]); SendDlgItemMessageW(dialog, IDC_VARIANT_ROLE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            if (input->roles[i] == input->role) SendDlgItemMessageW(dialog, IDC_VARIANT_ROLE, CB_SETCURSEL, i, 0);
        }
        for (const auto& choice : input->conditions) { const auto label = Wide(choice); SendDlgItemMessageW(dialog, IDC_VARIANT_WHEN, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())); }
        SendDlgItemMessageW(dialog, IDC_VARIANT_WHEN, CB_LIMITTEXT, 80, 0);
        SetDlgItemTextW(dialog, IDC_VARIANT_WHEN, Wide(input->when).c_str()); return TRUE;
    }
    if (msg == WM_COMMAND && input) {
        try {
            const int command = LOWORD(wParam);
            if (command == IDC_VARIANT_BROWSE) {
                const auto role = Utf8(Text(GetDlgItem(dialog, IDC_VARIANT_ROLE))); auto path = Utf8(Text(GetDlgItem(dialog, IDC_VARIANT_PATH)));
                if (PickResource(dialog, input->root, role, path)) SetDlgItemTextW(dialog, IDC_VARIANT_PATH, Wide(path).c_str());
                return TRUE;
            }
            if (command == IDOK) {
                input->id = Utf8(Text(GetDlgItem(dialog, IDC_VARIANT_ID))); input->role = Utf8(Text(GetDlgItem(dialog, IDC_VARIANT_ROLE)));
                input->path = Utf8(Text(GetDlgItem(dialog, IDC_VARIANT_PATH))); input->when = Utf8(Text(GetDlgItem(dialog, IDC_VARIANT_WHEN)));
                EndDialog(dialog, IDOK); return TRUE;
            }
            if (command == IDCANCEL) { EndDialog(dialog, IDCANCEL); return TRUE; }
        } catch (const std::exception& e) { MessageBoxW(dialog, Wide(e.what()).c_str(), L"Conditional asset", MB_OK | MB_ICONERROR); return TRUE; }
    }
    return FALSE;
}
const char* const kOverrideRoles[] = {"view_model", "player_model", "world_model", "fire_sound", "reload_sound", "sprite", "hud_sprite", "kill_sprite"};
struct OverrideInput { pxwe::fs::path root; std::wstring title; pxwe::Fields fields; };
INT_PTR CALLBACK OverrideProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* input = reinterpret_cast<OverrideInput*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (msg == WM_INITDIALOG) {
        input = reinterpret_cast<OverrideInput*>(lParam); SetWindowLongPtrW(dialog, DWLP_USER, lParam); SetWindowTextW(dialog, input->title.c_str());
        for (int i = 0; i < 8; ++i) {
            SetDlgItemTextW(dialog, IDC_OVERRIDE_FIRST+i, Wide(input->fields[size_t(i)].second).c_str());
            SendDlgItemMessageW(dialog, IDC_OVERRIDE_FIRST+i, EM_SETLIMITTEXT, 240, 0);
        }
        return TRUE;
    }
    if (msg == WM_COMMAND && input) {
        try {
            const int command = LOWORD(wParam);
            if (command >= IDC_OVERRIDE_BROWSE_FIRST && command < IDC_OVERRIDE_BROWSE_FIRST+8) {
                const int i = command - IDC_OVERRIDE_BROWSE_FIRST; auto path = Utf8(Text(GetDlgItem(dialog, IDC_OVERRIDE_FIRST+i)));
                if (PickResource(dialog, input->root, kOverrideRoles[i], path)) SetDlgItemTextW(dialog, IDC_OVERRIDE_FIRST+i, Wide(path).c_str());
                return TRUE;
            }
            if (command == IDOK) { for (int i = 0; i < 8; ++i) input->fields[size_t(i)].second = Utf8(Text(GetDlgItem(dialog, IDC_OVERRIDE_FIRST+i))); EndDialog(dialog, IDOK); return TRUE; }
            if (command == IDCANCEL) { EndDialog(dialog, IDCANCEL); return TRUE; }
        } catch (const std::exception& e) { MessageBoxW(dialog, Wide(e.what()).c_str(), L"Ammo overrides", MB_OK | MB_ICONERROR); return TRUE; }
    }
    return FALSE;
}
INT_PTR CALLBACK ReportProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INITDIALOG) {
        SendDlgItemMessageW(dialog, IDC_REPORT, EM_SETLIMITTEXT, 262144, 0);
        SetDlgItemTextW(dialog, IDC_REPORT, reinterpret_cast<const wchar_t*>(lParam)); return TRUE;
    }
    if (msg == WM_COMMAND && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)) {
        EndDialog(dialog, IDOK); return TRUE;
    }
    return FALSE;
}
void Report(HWND owner, const std::string& text) {
    std::wstring crlf;
    for (wchar_t c : Wide(text)) { if (c == L'\n') crlf += L'\r'; crlf += c; }
    if (DialogBoxParamW(gInstance, MAKEINTRESOURCEW(IDD_REPORT), owner, ReportProc, reinterpret_cast<LPARAM>(crlf.c_str())) == -1)
        MessageBoxW(owner, crlf.c_str(), L"pxwe", MB_OK);
}
void Column(HWND list, int index, const wchar_t* name, int width) {
    LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM; c.pszText = const_cast<wchar_t*>(name);
    c.cx = width; c.iSubItem = index; ListView_InsertColumn(list, index, &c);
}
void Cell(HWND list, int row, int column, const std::wstring& text) {
    if (column == 0) {
        LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = row; item.pszText = const_cast<wchar_t*>(text.c_str());
        ListView_InsertItem(list, &item);
    } else {
        LVITEMW item{}; item.iItem = row; item.iSubItem = column; item.pszText = const_cast<wchar_t*>(text.c_str());
        SendMessageW(list, LVM_SETITEMTEXTW, row, reinterpret_cast<LPARAM>(&item));
    }
}
std::wstring Number(double value, int precision = 2) {
    std::wostringstream out; out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(precision) << value; return out.str();
}
class Application;
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK PreviewProc(HWND, UINT, WPARAM, LPARAM);

class Application {
public:
    pxwe::Project project;
    pxwe::fs::path executable;
    HWND window = nullptr, browse = nullptr;
    HWND tree = nullptr, tabs = nullptr, properties = nullptr, value = nullptr, apply = nullptr, hint = nullptr;
    HWND compatibility = nullptr, allow = nullptr, disallow = nullptr, setDefault = nullptr, flight = nullptr;
    HWND modelKind = nullptr, modelLoad = nullptr, modelOpen = nullptr, sequences = nullptr, bounds = nullptr, modelInfo = nullptr;
    HWND variants = nullptr, variantAdd = nullptr, variantEdit = nullptr, variantCopy = nullptr, variantRemove = nullptr, variantPreview = nullptr;
    HWND play = nullptr, reset = nullptr, sound = nullptr, skin = nullptr, body = nullptr, coefficient = nullptr, overrides = nullptr;
    HWND curvesList = nullptr, metric = nullptr, spriteKind = nullptr, spriteLoad = nullptr, spriteWindow = nullptr, spriteInfo = nullptr;
    HWND status = nullptr;
    HFONT font = nullptr;
    UINT dpi = 96;
    bool updating = false;
    pxwe::Kind selectionKind = pxwe::Kind::Weapon;
    std::string selectionId, editingKey, editingValue;
    std::vector<std::pair<pxwe::Kind, std::string>> treeAssets;
    std::vector<pxwe::Category> treeCategories;
    std::vector<HTREEITEM> treeCategoryItems;
    bool categorySelected = false;
    bool treeRefreshPending = false;
    std::string selectionCategory;
    std::vector<std::string> propertyKeys, compatibilityIds;
    std::vector<pxwe::PropertyRow> propertyRows;
    std::set<std::string> collapsedSections;
    std::vector<modular::BallisticState> trajectory;
    std::vector<pxwe::AmmoCurve> curves;
    std::set<std::string> hiddenCurves;
    std::vector<std::string> variantIds, modelPaths, spritePaths;
    pxwe::StudioPreview studio;
    pxwe::SpritePreview sprite;
    pxwe::Bytes soundData;
    bool playing = false, hasSprite = false;
    int sequenceIndex = 0;
    double animationSeconds = 0, spriteSeconds = 0;
    ULONGLONG lastTick = 0;
    std::wstring flightText = L"Select an asset to preview its default cartridge.";
    pxwe::ModelInfo model;
    bool hasModel = false, dragging = false;
    pxwe::fs::path modelPath;
    std::wstring modelText = L"Load a view, player, or world model to inspect bounds and animation sequences.";
    double yaw = .65, tilt = .35;
    POINT dragPoint{};
    ~Application() { PlaySoundW(nullptr, nullptr, 0); if (font) DeleteObject(font); }
    int Scale(int n) const { return MulDiv(n, static_cast<int>(dpi), 96); }
    HWND ControlWindow(const wchar_t* cls, const wchar_t* text, DWORD style, int id) {
        // The tab and page controls are overlapping siblings. Clip every
        // sibling so a repaint of the tab cannot erase a property's text.
        HWND child = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style, 0, 0, 1, 1,
            window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), gInstance, this);
        if (!child) throw std::runtime_error("Cannot create an editor control.");
        return child;
    }
    void Create(HWND hwnd) {
        window = hwnd; dpi = WindowDpi(window);
        HMENU menu = CreateMenu(), file = CreatePopupMenu(), asset = CreatePopupMenu(), edit = CreatePopupMenu(), tools = CreatePopupMenu();
        AppendMenuW(file, MF_STRING, CMD_OPEN, L"&Open game folder...\tCtrl+O");
        AppendMenuW(file, MF_STRING, CMD_SAVE, L"&Save / Export\tCtrl+S");
        AppendMenuW(file, MF_SEPARATOR, 0, nullptr); AppendMenuW(file, MF_STRING, CMD_EXIT, L"E&xit");
        AppendMenuW(asset, MF_STRING, CMD_NEW_WEAPON, L"New &weapon...\tCtrl+N");
        AppendMenuW(asset, MF_STRING, CMD_NEW_CARTRIDGE, L"New &cartridge...\tCtrl+Shift+N");
        AppendMenuW(asset, MF_STRING, CMD_DUPLICATE, L"&Duplicate...\tCtrl+D");
        AppendMenuW(asset, MF_STRING, CMD_DELETE, L"&Delete\tCtrl+Del");
        AppendMenuW(asset, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(asset, MF_STRING, CMD_CATEGORY_NEW, L"New &category...");
        AppendMenuW(asset, MF_STRING, CMD_MOVE, L"&Move to category...");
        AppendMenuW(asset, MF_STRING, CMD_CATEGORY_RENAME, L"Rename category...");
        AppendMenuW(edit, MF_STRING, CMD_UNDO, L"&Undo\tCtrl+Z");
        AppendMenuW(edit, MF_STRING, CMD_REDO, L"&Redo\tCtrl+Y");
        AppendMenuW(tools, MF_STRING, CMD_VALIDATE, L"&Validate\tF5");
        AppendMenuW(tools, MF_STRING, CMD_HELP, L"&Help / About");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(asset), L"&Asset");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), L"&Edit");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), L"&Tools"); SetMenu(window, menu);
        tree = ControlWindow(WC_TREEVIEWW, L"", TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS | WS_TABSTOP | WS_BORDER, C_TREE);
        tabs = ControlWindow(WC_TABCONTROLW, L"", WS_TABSTOP, C_TAB);
        const wchar_t* names[] = {L"Properties", L"Cartridges", L"Ballistics", L"Model / Animations", L"Sprites"};
        for (int i = 0; i < 5; ++i) {
            TCITEMW item{}; item.mask = TCIF_TEXT; item.pszText = const_cast<wchar_t*>(names[i]); TabCtrl_InsertItem(tabs, i, &item);
        }
        const DWORD listStyle = LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP;
        properties = ControlWindow(WC_LISTVIEWW, L"", listStyle, C_PROPERTIES);
        ListView_SetExtendedListViewStyle(properties, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        Column(properties, 0, L"Property", Scale(215)); Column(properties, 1, L"Value (blank = unset)", Scale(390));
        value = ControlWindow(L"COMBOBOX", L"", CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP, C_VALUE);
        SendMessageW(value, CB_LIMITTEXT, 240, 0);
        apply = ControlWindow(L"BUTTON", L"Apply", BS_PUSHBUTTON | WS_TABSTOP, C_APPLY);
        browse = ControlWindow(L"BUTTON", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, C_BROWSE);
        hint = ControlWindow(L"EDIT", L"Select a property for help. Double-click a section to collapse it or a value to edit it.",
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, C_HINT);
        compatibility = ControlWindow(WC_LISTVIEWW, L"", listStyle, C_COMPAT);
        ListView_SetExtendedListViewStyle(compatibility, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        Column(compatibility, 0, L"Cartridge", Scale(185)); Column(compatibility, 1, L"Chamber", Scale(110));
        Column(compatibility, 2, L"Allowed / Default", Scale(130)); Column(compatibility, 3, L"Velocity (m/s)", Scale(130));
        Column(compatibility, 4, L"Muzzle damage / projectile", Scale(175));
        Column(compatibility, 5, L"Firing override", Scale(175)); Column(compatibility, 6, L"Reload override", Scale(175));
        allow = ControlWindow(L"BUTTON", L"Allow / Edit velocity", BS_PUSHBUTTON | WS_TABSTOP, C_ALLOW);
        disallow = ControlWindow(L"BUTTON", L"Disallow", BS_PUSHBUTTON | WS_TABSTOP, C_DISALLOW);
        setDefault = ControlWindow(L"BUTTON", L"Set default", BS_PUSHBUTTON | WS_TABSTOP, C_DEFAULT);
        coefficient = ControlWindow(L"BUTTON", L"Edit coefficient...", BS_PUSHBUTTON | WS_TABSTOP, C_COEFFICIENT);
        overrides = ControlWindow(L"BUTTON", L"Ammo assets / sounds...", BS_PUSHBUTTON | WS_TABSTOP, C_OVERRIDES);
        flight = ControlWindow(kPreviewClass, L"", WS_BORDER, C_FLIGHT);
        curvesList = ControlWindow(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP, C_CURVES);
        ListView_SetExtendedListViewStyle(curvesList, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
        Column(curvesList, 0, L"Visible cartridges", Scale(190)); Column(curvesList, 1, L"Color", Scale(70));
        metric = ControlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, C_METRIC);
        for (const wchar_t* label : {L"Trajectory (height / distance)", L"Kinetic energy (J / distance)", L"Damage per projectile / distance"}) SendMessageW(metric, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
        SendMessageW(metric, CB_SETCURSEL, 0, 0);
        modelKind = ControlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, C_MODEL_KIND);
        for (const wchar_t* label : {L"View model", L"Player model", L"World model"})
            SendMessageW(modelKind, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
        SendMessageW(modelKind, CB_SETCURSEL, 0, 0);
        modelLoad = ControlWindow(L"BUTTON", L"Load model", BS_PUSHBUTTON | WS_TABSTOP, C_MODEL_LOAD);
        modelOpen = ControlWindow(L"BUTTON", L"Open in pxmv", BS_PUSHBUTTON | WS_TABSTOP, C_MODEL_OPEN);
        modelInfo = ControlWindow(L"STATIC", modelText.c_str(), 0, C_MODEL_INFO);
        sequences = ControlWindow(WC_LISTVIEWW, L"", listStyle, C_SEQUENCES);
        ListView_SetExtendedListViewStyle(sequences, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        Column(sequences, 0, L"Index", Scale(60)); Column(sequences, 1, L"Sequence", Scale(190));
        Column(sequences, 2, L"FPS", Scale(70)); Column(sequences, 3, L"Frames", Scale(70));
        bounds = ControlWindow(kPreviewClass, L"", WS_BORDER, C_BOUNDS);
        play = ControlWindow(L"BUTTON", L"Play / Pause", BS_PUSHBUTTON | WS_TABSTOP, C_PLAY);
        reset = ControlWindow(L"BUTTON", L"Restart", BS_PUSHBUTTON | WS_TABSTOP, C_RESET);
        sound = ControlWindow(L"BUTTON", L"Play sound", BS_PUSHBUTTON | WS_TABSTOP, C_SOUND);
        skin = ControlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, C_SKIN);
        body = ControlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, C_BODY);
        variants = ControlWindow(WC_LISTVIEWW, L"", listStyle, C_VARIANTS);
        ListView_SetExtendedListViewStyle(variants, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        Column(variants, 0, L"Conditional entry", Scale(130)); Column(variants, 1, L"Role", Scale(110));
        Column(variants, 2, L"Condition", Scale(120)); Column(variants, 3, L"Path", Scale(220));
        variantAdd = ControlWindow(L"BUTTON", L"Add...", BS_PUSHBUTTON | WS_TABSTOP, C_VARIANT_ADD);
        variantEdit = ControlWindow(L"BUTTON", L"Edit...", BS_PUSHBUTTON | WS_TABSTOP, C_VARIANT_EDIT);
        variantCopy = ControlWindow(L"BUTTON", L"Duplicate...", BS_PUSHBUTTON | WS_TABSTOP, C_VARIANT_COPY);
        variantRemove = ControlWindow(L"BUTTON", L"Remove", BS_PUSHBUTTON | WS_TABSTOP, C_VARIANT_REMOVE);
        variantPreview = ControlWindow(L"BUTTON", L"Preview entry", BS_PUSHBUTTON | WS_TABSTOP, C_VARIANT_PREVIEW);
        spriteKind = ControlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, C_SPRITE_KIND);
        spriteLoad = ControlWindow(L"BUTTON", L"Load sprite", BS_PUSHBUTTON | WS_TABSTOP, C_SPRITE_LOAD);
        spriteWindow = ControlWindow(kPreviewClass, L"", WS_BORDER, C_SPRITE_PREVIEW);
        spriteInfo = ControlWindow(L"STATIC", L"Select a default, conditional or ammo sprite to preview.", 0, C_SPRITE_INFO);
        status = ControlWindow(L"STATIC", L"", SS_LEFT | SS_PATHELLIPSIS, C_STATUS);
        SetFont(); RefreshTree(); Layout(); lastTick = GetTickCount64();
        if (!SetTimer(window, 1, 33, nullptr)) throw std::runtime_error("Cannot start preview timer.");
    }
    void SetFont() {
        HFONT replacement = CreateFontW(-MulDiv(9, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (!replacement) return;
        for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(replacement), TRUE);
        if (font) DeleteObject(font); font = replacement;
    }
    void Move(HWND child, int x, int y, int w, int h) {
        MoveWindow(child, x, y, std::max(w, 1), std::max(h, 1), TRUE);
    }
    void Layout() {
        RECT client{}; GetClientRect(window, &client);
        const int margin = Scale(10), left = Scale(220), bottom = Scale(30);
        Move(tree, margin, margin, left, client.bottom - bottom - margin * 2);
        const int x = left + margin * 2, width = client.right - x - margin;
        Move(tabs, x, margin, width, client.bottom - bottom - margin * 2);
        RECT page{0, 0, width, client.bottom - bottom - margin * 2};
        TabCtrl_AdjustRect(tabs, FALSE, &page);
        const int px = x + page.left + Scale(8), py = margin + page.top + Scale(8);
        const int pw = page.right - page.left - Scale(16), ph = page.bottom - page.top - Scale(16);
        const int row = Scale(28), gap = Scale(8);
        const int tab = TabCtrl_GetCurSel(tabs), editorWidth = tab >= 3 ? pw / 2 : pw;
        const int propertyBottom = py + ph - (tab >= 3 ? Scale(204) : 0);
        Move(properties, px, py, editorWidth, propertyBottom - py - Scale(148));
        Move(hint, px, propertyBottom - Scale(140), editorWidth, Scale(104));
        Move(value, px, propertyBottom - row, editorWidth - Scale(152), Scale(260));
        Move(browse, px + editorWidth - Scale(144), propertyBottom - row, Scale(68), row);
        Move(apply, px + editorWidth - Scale(68), propertyBottom - row, Scale(68), row);
        const int propertyColumn = std::min(Scale(215), editorWidth / 2);
        ListView_SetColumnWidth(properties, 0, propertyColumn);
        ListView_SetColumnWidth(properties, 1, std::max(Scale(100), editorWidth - propertyColumn - Scale(24)));
        Move(compatibility, px, py, pw, ph - row * 2 - gap * 2);
        const int ammoTop = py + ph - row * 2 - gap;
        const int third = (pw - gap * 2) / 3;
        Move(allow, px, ammoTop, third, row);
        Move(disallow, px + third + gap, ammoTop, third, row);
        Move(setDefault, px + (third + gap) * 2, ammoTop, pw - (third + gap) * 2, row);
        Move(coefficient, px, py + ph - row, pw / 2 - gap, row);
        Move(overrides, px + pw / 2, py + ph - row, pw - pw / 2, row);
        const int legend = std::min(Scale(230), pw / 3);
        ListView_SetColumnWidth(curvesList, 0, legend - Scale(86)); ListView_SetColumnWidth(curvesList, 1, Scale(70));
        Move(metric, px, py, pw, Scale(220));
        Move(curvesList, px, py + row + gap, legend, ph - row - gap);
        Move(flight, px + legend + gap, py + row + gap, pw - legend - gap, ph - row - gap);
        const int right = px + pw / 2 + gap, rightWidth = pw - pw / 2 - gap;
        Move(modelKind, right, py, rightWidth - Scale(190), Scale(300));
        Move(modelLoad, right + rightWidth - Scale(182), py, Scale(87), row);
        Move(modelOpen, right + rightWidth - Scale(87), py, Scale(87), row);
        Move(modelInfo, right, py + row + gap, rightWidth, Scale(52));
        Move(bounds, right, py + Scale(92), rightWidth, ph - Scale(286));
        Move(sequences, right, py + ph - Scale(186), rightWidth, Scale(110));
        Move(play, right, py + ph - Scale(68), Scale(96), row);
        Move(reset, right + Scale(104), py + ph - Scale(68), Scale(75), row);
        Move(sound, right + Scale(187), py + ph - Scale(68), rightWidth - Scale(187), row);
        Move(skin, right, py + ph - row, rightWidth / 2 - gap, Scale(180));
        Move(body, right + rightWidth / 2, py + ph - row, rightWidth / 2, Scale(180));
        Move(variants, px, propertyBottom + gap, editorWidth, Scale(128));
        const int small = std::max(Scale(55), (editorWidth - gap * 3) / 4);
        for (const auto& b : std::vector<std::pair<HWND, int>>{{variantAdd,0},{variantEdit,1},{variantCopy,2},{variantRemove,3}})
            Move(b.first, px + b.second * (small + gap), py + ph - Scale(60), small, row);
        Move(variantPreview, px, py + ph - row, editorWidth, row);
        Move(spriteKind, right, py, rightWidth - Scale(100), Scale(300));
        Move(spriteLoad, right + rightWidth - Scale(92), py, Scale(92), row);
        Move(spriteWindow, right, py + row + gap, rightWidth, ph - Scale(104));
        Move(spriteInfo, right, py + ph - Scale(68), rightWidth, Scale(68));
        Move(status, margin, client.bottom - bottom + Scale(3), client.right - margin * 2, Scale(23));
        ShowPage();
    }
    void ShowPage() {
        const int tab = TabCtrl_GetCurSel(tabs);
        for (HWND c : {properties, value, apply, browse, hint}) ShowWindow(c, tab == 0 || tab >= 3 ? SW_SHOW : SW_HIDE);
        for (HWND c : {compatibility, allow, disallow, setDefault, coefficient, overrides}) ShowWindow(c, tab == 1 ? SW_SHOW : SW_HIDE);
        for (HWND c : {flight, curvesList, metric}) ShowWindow(c, tab == 2 ? SW_SHOW : SW_HIDE);
        for (HWND c : {modelKind, modelLoad, modelOpen, modelInfo, sequences, bounds, play, reset, sound, skin, body}) ShowWindow(c, tab == 3 ? SW_SHOW : SW_HIDE);
        for (HWND c : {variants, variantAdd, variantEdit, variantCopy, variantRemove, variantPreview}) ShowWindow(c, tab >= 3 ? SW_SHOW : SW_HIDE);
        for (HWND c : {spriteKind, spriteLoad, spriteWindow, spriteInfo}) ShowWindow(c, tab == 4 ? SW_SHOW : SW_HIDE);
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
    void Title() {
        const auto title = std::wstring(L"pxwe — PrimeXT Weapon Editor J.10.6.9.7") + (project.Dirty() ? L" *" : L"");
        SetWindowTextW(window, title.c_str());
        const auto label = project.Root().empty() ? L"Open a game/mod folder to begin." :
            project.Root().wstring() + (project.Dirty() ? L"   — Unsaved changes" : L"");
        SetWindowTextW(status, label.c_str());
    }
    void RefreshTree() {
        treeRefreshPending=false;
        const bool first = treeCategoryItems.empty();
        std::set<std::pair<pxwe::Kind,std::string>> expanded;
        for (size_t i=0;i<treeCategoryItems.size();++i)
            if (TreeView_GetItemState(tree,treeCategoryItems[i],TVIS_EXPANDED)&TVIS_EXPANDED)
                expanded.emplace(treeCategories[i].kind,treeCategories[i].path);
        updating = true; SendMessageW(tree,WM_SETREDRAW,FALSE,0);
        treeAssets.clear(); treeCategories.clear(); treeCategoryItems.clear(); TreeView_DeleteAllItems(tree);
        HTREEITEM selected = nullptr, firstAsset = nullptr;
        for (pxwe::Kind kind : {pxwe::Kind::Weapon, pxwe::Kind::Cartridge}) {
            std::map<std::string,HTREEITEM> folders;
            auto folder = [&](const std::string& path, HTREEITEM parent, const std::wstring& label) {
                treeCategories.push_back({kind,path});
                TVINSERTSTRUCTW entry{}; entry.hParent=parent; entry.hInsertAfter=TVI_LAST;
                entry.item.mask=TVIF_TEXT|TVIF_PARAM; entry.item.pszText=const_cast<wchar_t*>(label.c_str());
                entry.item.lParam=-2-static_cast<LPARAM>(treeCategories.size()-1);
                const auto item=TreeView_InsertItem(tree,&entry); treeCategoryItems.push_back(item); folders[path]=item;
                if (categorySelected && kind==selectionKind && path==selectionCategory) selected=item;
            };
            folder("",TVI_ROOT,kind==pxwe::Kind::Weapon?L"Weapons":L"Cartridges");
            for (const auto& path:project.Categories(kind)) {
                const auto parent=pxwe::CategoryParent(path);
                folder(path,folders.at(parent),Wide(path.substr(parent.empty()?0:parent.size()+1)));
            }
            const auto& assets = kind == pxwe::Kind::Weapon ? project.Data().weapons : project.Data().cartridges;
            for (const auto& a : assets) {
                treeAssets.emplace_back(kind, a.Id()); auto name = Wide(a.Id());
                const auto parent=folders.find(a.Get("category"));
                TVINSERTSTRUCTW child{}; child.hParent = parent==folders.end()?folders.at(""):parent->second; child.hInsertAfter = TVI_LAST;
                child.item.mask = TVIF_TEXT | TVIF_PARAM;
                child.item.pszText = name.data(); child.item.lParam = static_cast<LPARAM>(treeAssets.size() - 1);
                HTREEITEM added = TreeView_InsertItem(tree, &child);
                if (!firstAsset) firstAsset=added;
                if (!categorySelected && kind == selectionKind && a.Id() == selectionId) selected = added;
            }
            // Expand only after descendants exist. Native controls cannot
            // expand a newly inserted empty item before its children arrive.
            for (const auto& entry:folders)
                if (first || expanded.count({kind,entry.first})) TreeView_Expand(tree,entry.second,TVE_EXPAND);
            if (categorySelected && kind==selectionKind && !selected) {
                while (!selectionCategory.empty() && !folders.count(selectionCategory)) selectionCategory=pxwe::CategoryParent(selectionCategory);
                selected=folders.at(selectionCategory);
            }
        }
        if (!selected && firstAsset) {
            categorySelected=false;
            selectionKind = treeAssets.front().first; selectionId = treeAssets.front().second;
            selected=firstAsset;
        }
        if (!selected) { categorySelected=true; selectionKind=pxwe::Kind::Weapon; selectionCategory.clear(); selected=treeCategoryItems.front(); }
        if (categorySelected) selectionId.clear();
        TreeView_SelectItem(tree, selected); TreeView_EnsureVisible(tree,selected);
        SendMessageW(tree,WM_SETREDRAW,TRUE,0); RedrawWindow(tree,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE);
        updating = false; RefreshSelection(); Title();
    }
    std::string CurrentCategory() const {
        if (categorySelected) return selectionCategory;
        const auto* a=project.Find(selectionKind,selectionId);
        return a?a->Get("category"):"";
    }
    void TreeContext(POINT point, bool keyboard) {
        CommitEditor();
        HTREEITEM selected=TreeView_GetSelection(tree);
        if (keyboard) {
            if (!selected) return;
            RECT rect{}; TreeView_GetItemRect(tree,selected,&rect,TRUE); point={rect.left,rect.bottom}; ClientToScreen(tree,&point);
        } else {
            TVHITTESTINFO hit{}; hit.pt=point; ScreenToClient(tree,&hit.pt); selected=TreeView_HitTest(tree,&hit);
            if (!selected || !(hit.flags&TVHT_ONITEM)) return;
        }
        TVITEMW item{}; item.mask=TVIF_PARAM; item.hItem=selected;
        if (!TreeView_GetItem(tree,&item)) return;
        if (selected!=TreeView_GetSelection(tree)) {
            updating=true; TreeView_SelectItem(tree,selected); updating=false;
            NMTREEVIEWW change{}; change.hdr.idFrom=C_TREE; change.hdr.code=TVN_SELCHANGEDW; change.itemNew=item; Notify(&change.hdr);
        }
        HMENU popup=CreatePopupMenu(); if (!popup) throw std::runtime_error("Cannot open category menu.");
        AppendMenuW(popup,MF_STRING,selectionKind==pxwe::Kind::Weapon?CMD_NEW_WEAPON:CMD_NEW_CARTRIDGE,L"New asset here...");
        AppendMenuW(popup,MF_STRING,CMD_CATEGORY_NEW,L"New subcategory...");
        const bool asset=!categorySelected && project.Find(selectionKind,selectionId);
        if (asset || !selectionCategory.empty()) {
            AppendMenuW(popup,MF_SEPARATOR,0,nullptr);
            AppendMenuW(popup,MF_STRING,CMD_MOVE,L"Move to category...");
            AppendMenuW(popup,MF_STRING,asset?CMD_DUPLICATE:CMD_CATEGORY_COPY,L"Duplicate...");
            if (asset) AppendMenuW(popup,MF_STRING,CMD_DELETE,L"Delete asset...");
            else {
                AppendMenuW(popup,MF_STRING,CMD_CATEGORY_RENAME,L"Rename category...");
                AppendMenuW(popup,MF_STRING,CMD_CATEGORY_REMOVE,L"Remove category (keep assets)...");
                AppendMenuW(popup,MF_STRING,CMD_CATEGORY_DELETE_ASSETS,L"Delete category and its assets...");
            }
        }
        const int command=static_cast<int>(TrackPopupMenu(popup,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window,nullptr));
        DestroyMenu(popup); if (command) Command(command);
    }
    void CategoryCommand(int command) {
        CommitEditor();
        if (project.Root().empty()) throw std::runtime_error("Open a game folder first.");
        const auto source=CurrentCategory();
        if ((command==CMD_CATEGORY_RENAME || command==CMD_CATEGORY_COPY) && !categorySelected)
            throw std::runtime_error("Select a category to rename or duplicate.");
        if (command==CMD_CATEGORY_REMOVE || command==CMD_CATEGORY_DELETE_ASSETS) {
            if (!categorySelected || source.empty()) throw std::runtime_error("Select a category below Weapons or Cartridges.");
            const bool erase=command==CMD_CATEGORY_DELETE_ASSETS;
            const auto question=Wide(erase?"Delete category '"+source+"' and ALL assets in its subtree?":"Remove category '"+source+"' and move its assets to its parent?");
            if (MessageBoxW(window,question.c_str(),L"pxwe",MB_YESNO|MB_ICONQUESTION)!=IDYES) return;
            project.RemoveCategory(selectionKind,source,erase); categorySelected=true; selectionId.clear();
            selectionCategory=pxwe::CategoryParent(source); RefreshTree(); return;
        }
        if ((command==CMD_CATEGORY_RENAME || command==CMD_CATEGORY_COPY || (command==CMD_MOVE && categorySelected)) && source.empty())
            throw std::runtime_error("Select a category below Weapons or Cartridges.");
        Input input{L"Category",L"Full category path: ASCII IDs separated by / (up to 16 levels / 255 characters).",Wide(source)};
        if (command==CMD_CATEGORY_NEW) input.value=Wide(source.empty()?"new_category":source+"/new_category");
        if (command==CMD_CATEGORY_COPY) input.value=Wide(source+"_copy");
        if (command==CMD_MOVE) { input.prompt=L"Destination parent/category path. Blank means Weapons or Cartridges root."; if (categorySelected) input.value=Wide(pxwe::CategoryParent(source)); }
        for (;;) {
            if (!Ask(window,input)) return;
            const auto target=Utf8(input.value);
            try {
                if (command==CMD_CATEGORY_NEW) { project.CreateCategory(selectionKind,target); categorySelected=true; selectionId.clear(); selectionCategory=target; }
                else if (command==CMD_MOVE && !categorySelected) project.SetCategory(selectionKind,selectionId,target);
                else {
                    auto destination=target;
                    if (command==CMD_MOVE) {
                        if (!pxwe::ValidCategory(target,true)) throw std::runtime_error("Invalid destination parent category.");
                        const auto leaf=source.substr(source.rfind('/')==std::string::npos?0:source.rfind('/')+1);
                        destination=target.empty()?leaf:target+"/"+leaf;
                    }
                    if (command==CMD_CATEGORY_COPY) {
                        Input prefix{L"Duplicate category",L"New asset IDs will be prefix_<original ID>. References keep their existing cartridge IDs.",L"copy"};
                        if (!Ask(window,prefix)) return;
                        project.DuplicateCategory(selectionKind,source,destination,Utf8(prefix.value));
                    } else project.RelocateCategory(selectionKind,source,destination);
                    categorySelected=true; selectionId.clear(); selectionCategory=destination;
                }
                RefreshTree(); return;
            } catch (const std::exception& e) { MessageBoxW(window,Wide(e.what()).c_str(),L"Category change rejected",MB_OK|MB_ICONERROR); }
        }
    }
    void RefreshProperties() {
        updating = true; editingKey.clear(); editingValue.clear(); propertyKeys.clear();
        propertyRows.clear();
        SendMessageW(properties, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(properties); SetWindowTextW(value, L""); EnableWindow(value, FALSE); EnableWindow(apply, FALSE);
        EnableWindow(browse, FALSE);
        SetWindowTextW(hint, L"Select a property for help. Double-click a section to collapse it or a value to edit it. Blank removes an optional field; F5 validates required values.");
        const auto* a = project.Find(selectionKind, selectionId);
        if (a) {
            const int tab = TabCtrl_GetCurSel(tabs);
            const auto page = tab == 3 ? pxwe::PropertyPage::Models : tab == 4 ? pxwe::PropertyPage::Sprites : pxwe::PropertyPage::General;
            propertyRows = pxwe::PageRows(*a, collapsedSections, page);
            for (size_t i = 0; i < propertyRows.size(); ++i) {
                const auto& r = propertyRows[i]; propertyKeys.push_back(r.key);
                const auto label = r.Heading() ? (collapsedSections.count(r.section) ? "[+] " : "[-] ") + r.section : r.key;
                Cell(properties, static_cast<int>(i), 0, Wide(label));
                Cell(properties, static_cast<int>(i), 1, r.Heading() ? L"Double-click to expand / collapse" : Wide(a->Get(r.key)));
            }
        }
        SendMessageW(properties, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(properties, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE);
        updating = false;
    }
    void RefreshSelection() {
        RefreshProperties(); updating = true;
        const auto* a = project.Find(selectionKind, selectionId);
        hasModel = false; hasSprite = false; playing = false; animationSeconds = spriteSeconds = 0;
        modelPath.clear(); ListView_DeleteAllItems(sequences);
        modelText = L"Load a textured model. Select a sequence to play it; drag the preview to orbit.";
        SetWindowTextW(modelInfo, modelText.c_str()); EnableWindow(modelOpen, FALSE);
        EnableWindow(modelLoad, a && a->kind == pxwe::Kind::Weapon);
        for (HWND c : {play, reset, skin, body}) EnableWindow(c, FALSE);
        SetWindowTextW(spriteInfo, L"Load a default, conditional or ammo sprite. Animated frames play automatically.");
        updating = false; RefreshCompatibility(); RefreshFlight(); RefreshResources(); RefreshVariants();
        if (categorySelected) SetWindowTextW(hint,L"Right-click this category to create assets/subcategories, move, rename, duplicate or remove it. F2 renames a category. Asset IDs and file paths stay stable; Ctrl+Z / Ctrl+Y undo / redo.");
        InvalidateRect(bounds, nullptr, FALSE); InvalidateRect(spriteWindow, nullptr, FALSE);
    }
    void EditProperty(int row) {
        CommitEditor();
        if (row < 0 || static_cast<size_t>(row) >= propertyRows.size()) return;
        const auto* a = project.Find(selectionKind, selectionId); if (!a) return;
        if (propertyRows[row].Heading()) {
            editingKey.clear(); editingValue.clear(); SetWindowTextW(value, L"");
            for (HWND c : {value, apply, browse}) EnableWindow(c, FALSE);
            SetWindowTextW(hint, L"Double-click this section or press Enter to expand / collapse. Collapsing hides properties and preserves their values.");
            return;
        }
        editingKey = propertyRows[row].key; editingValue = a->Get(editingKey);
        const auto info = pxwe::DescribeProperty(a->kind, editingKey, a);
        SendMessageW(value, CB_RESETCONTENT, 0, 0);
        std::vector<std::wstring> choices;
        for (const auto& choice : info.choices) choices.push_back(Wide(choice));
        if (editingKey == "default_cartridge" || editingKey == "ammo_name")
            for (const auto& c : project.Data().cartridges) choices.push_back(Wide(c.Id()));
        if (hasModel && editingKey.size() >= 10 && editingKey.substr(editingKey.size()-10) == "_animation")
            for (size_t i = 0; i < model.sequences.size(); ++i) choices.push_back(std::to_wstring(i));
        for (const auto& choice : choices) SendMessageW(value, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.c_str()));
        SetWindowTextW(value, Wide(editingValue).c_str());
        const bool editable = !info.readOnly;
        EnableWindow(value, editable); EnableWindow(apply, editable);
        EnableWindow(browse, editable && info.resource != pxwe::ResourceType::None);
        const auto label = Wide(editingKey + ": " + info.description);
        SetWindowTextW(hint, label.c_str());
    }
    void ActivateProperty(int row) {
        if (row < 0 || static_cast<size_t>(row) >= propertyRows.size()) return;
        if (propertyRows[row].Heading()) {
            CommitEditor(); const auto section = propertyRows[row].section;
            if (!collapsedSections.erase(section)) collapsedSections.insert(section);
            RefreshProperties();
            for (size_t i = 0; i < propertyRows.size(); ++i) if (propertyRows[i].Heading() && propertyRows[i].section == section) {
                ListView_SetItemState(properties, static_cast<int>(i), LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                ListView_EnsureVisible(properties, static_cast<int>(i), FALSE); break;
            }
        } else {
            EditProperty(row);
            if (editingKey != "id") { SetFocus(value); SendMessageW(value, CB_SETEDITSEL, 0, MAKELPARAM(0, -1)); }
        }
    }
    void BrowseProperty() {
        const auto* a = project.Find(selectionKind, selectionId);
        if (!a || editingKey.empty()) throw std::runtime_error("Select a resource property first.");
        const auto info = pxwe::DescribeProperty(a->kind, editingKey, a);
        if (info.readOnly || info.resource == pxwe::ResourceType::None) throw std::runtime_error("This property does not contain a resource path.");
        const wchar_t* filter = info.resource == pxwe::ResourceType::Model ? L"Studio models (*.mdl)\0*.mdl\0\0" :
            info.resource == pxwe::ResourceType::Sound ? L"Wave sounds (*.wav)\0*.wav\0\0" :
            info.resource == pxwe::ResourceType::Sprite ? L"Sprites and HUD lists (*.spr;*.txt)\0*.spr;*.txt\0\0" :
            info.resource == pxwe::ResourceType::KillSprite ? L"Kill sprites (*.spr)\0*.spr\0\0" :
            L"Client event scripts (*.sc)\0*.sc\0\0";
        std::wstring file(32768, L'\0');
        auto directory = pxwe::BrowseDirectory(project.Root(), info.resource).wstring();
        const auto current = Utf8(Text(value));
        if (!current.empty()) try {
            const auto candidate = project.Root() / (info.resource == pxwe::ResourceType::Sound ? "sound" : "") / pxwe::fs::u8path(current);
            // A malformed draft path must not become the dialog's start folder.
            pxwe::ResourcePath(project.Root(), candidate, info.resource);
            directory = candidate.parent_path().wstring();
        } catch (const std::exception&) {}
        OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = window;
        dialog.lpstrFilter = filter; dialog.lpstrFile = file.data(); dialog.nMaxFile = static_cast<DWORD>(file.size());
        dialog.lpstrInitialDir = directory.c_str(); dialog.lpstrTitle = L"Choose a resource inside the game folder";
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
        if (!GetOpenFileNameW(&dialog)) {
            if (CommDlgExtendedError()) throw std::runtime_error("Cannot open the resource browser.");
            return; // Cancel preserves the pending draft and project history.
        }
        const auto rel = pxwe::ResourcePath(project.Root(), pxwe::fs::path(file.c_str()), info.resource);
        SetWindowTextW(value, Wide(rel).c_str()); CommitEditor();
    }
    void CommitEditor() {
        if (updating || editingKey.empty() || editingKey == "id") return;
        const auto current = Utf8(Text(value));
        if (current == editingValue) return;
        const bool categoryEdit=editingKey=="category";
        if (selectionKind == pxwe::Kind::Cartridge && editingKey == "damage_coefficient") project.SetCoefficient(selectionId, current);
        else if (editingKey == "sprite") project.SetSprite(selectionKind, selectionId, current);
        else project.SetField(selectionKind, selectionId, editingKey, current);
        const auto* a = project.Find(selectionKind, selectionId);
        editingValue = a ? a->Get(editingKey) : "";
        const auto it = std::find(propertyKeys.begin(), propertyKeys.end(), editingKey);
        if (it != propertyKeys.end()) Cell(properties, static_cast<int>(it - propertyKeys.begin()), 1, Wide(editingValue));
        Title(); RefreshCompatibility(); RefreshFlight(); RefreshResources(); RefreshVariants();
        if (categoryEdit) {
            // Never delete tree items inside TVN_SELCHANGING (or property rows
            // inside a list notification). Finish that notification first.
            if (!treeRefreshPending) {
                if (!PostMessageW(window,WM_COMMAND,CMD_REFRESH_TREE,0)) throw std::runtime_error("Cannot refresh category tree.");
                treeRefreshPending=true;
            }
        }
        else if (selectionKind == pxwe::Kind::Cartridge && (editingKey == "damage_coefficient" || editingKey == "sprite")) RefreshProperties();
    }
    void RefreshCompatibility() {
        const int selected = ListView_GetNextItem(compatibility, -1, LVNI_SELECTED);
        const auto previous = selected >= 0 && static_cast<size_t>(selected) < compatibilityIds.size() ? compatibilityIds[size_t(selected)] : "";
        updating = true; ListView_DeleteAllItems(compatibility); compatibilityIds.clear();
        const auto validation = project.Validate();
        const auto* w = project.Find(selectionKind, selectionId);
        const bool isWeapon = w && w->kind == pxwe::Kind::Weapon;
        for (const auto& c : project.Data().cartridges) {
            const int row = static_cast<int>(compatibilityIds.size()); compatibilityIds.push_back(c.Id());
            Cell(compatibility, row, 0, Wide(c.Id())); Cell(compatibility, row, 1, Wide(c.Get("chamber")));
            std::string velocity, permitted = "No";
            if (isWeapon) for (const auto& a : w->accepted) if (a.first == c.Id()) {
                permitted = w->Get("default_cartridge") == c.Id() ? "Default" : "Yes"; velocity = a.second;
            }
            Cell(compatibility, row, 2, Wide(permitted)); Cell(compatibility, row, 3, Wide(velocity));
            const auto* parsed = validation.registry.FindCartridge(c.Id());
            const auto damage = validation.Ok() && parsed && !velocity.empty() ? Number(modular::MuzzleDamage(*parsed, std::stod(velocity)), 4) : L"—";
            Cell(compatibility, row, 4, damage);
            Cell(compatibility, row, 5, isWeapon ? Wide(w->Get("ammo." + c.Id() + ".fire_sound")) : L"");
            Cell(compatibility, row, 6, isWeapon ? Wide(w->Get("ammo." + c.Id() + ".reload_sound")) : L"");
        }
        for (HWND c : {allow, disallow, setDefault}) EnableWindow(c, isWeapon);
        EnableWindow(coefficient, !project.Data().cartridges.empty()); EnableWindow(overrides, isWeapon);
        const auto restore = std::find(compatibilityIds.begin(), compatibilityIds.end(), previous);
        if (restore != compatibilityIds.end()) ListView_SetItemState(compatibility, static_cast<int>(restore-compatibilityIds.begin()), LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        updating = false;
    }
    void CompatibilityCommand(int command) {
        CommitEditor();
        const int row = ListView_GetNextItem(compatibility, -1, LVNI_SELECTED);
        if (row < 0 || static_cast<size_t>(row) >= compatibilityIds.size()) throw std::runtime_error("Select a cartridge first.");
        const auto cid = compatibilityIds[row]; const auto* w = project.Find(pxwe::Kind::Weapon, selectionId);
        if (!w || selectionKind != pxwe::Kind::Weapon) throw std::runtime_error("Select a weapon first.");
        if (command == C_ALLOW) {
            std::string v = project.Find(pxwe::Kind::Cartridge, cid)->Get("reference_velocity_mps");
            for (const auto& a : w->accepted) if (a.first == cid) v = a.second;
            Input input{L"Cartridge velocity", Wide(cid) + L": muzzle velocity in metres/second (0.01..10000)", Wide(v)};
            if (!Ask(window, input)) return;
            project.Allow(selectionId, cid, Utf8(input.value));
        } else if (command == C_DISALLOW) project.Disallow(selectionId, cid);
        else project.SetDefault(selectionId, cid);
        RefreshSelection(); Title();
        ListView_SetItemState(compatibility, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    void RefreshFlight() {
        trajectory.clear(); curves.clear();
        const auto validation = project.Validate();
        if (!validation.Ok()) flightText = L"Validate and correct the definitions before previewing ballistic flight.";
        else if (selectionId.empty()) flightText = L"Select a weapon or cartridge.";
        else {
            curves = pxwe::ComparisonCurves(validation.registry, selectionKind, selectionId);
            flightText = L"Horizontal launch for 2 seconds; weapon-specific muzzle speeds.\n"
                L"Check cartridges to compare. Game gravity / drag and damage per projectile; no collision or penetration.";
        }
        updating = true; ListView_DeleteAllItems(curvesList);
        for (size_t i = 0; i < curves.size(); ++i) {
            const auto color = CurveColor(i); std::wostringstream label;
            label << L"#" << std::hex << std::setfill(L'0') << std::setw(2) << unsigned(GetRValue(color)) << std::setw(2) << unsigned(GetGValue(color)) << std::setw(2) << unsigned(GetBValue(color));
            Cell(curvesList, static_cast<int>(i), 0, Wide(curves[i].id)); Cell(curvesList, static_cast<int>(i), 1, label.str());
            ListView_SetCheckState(curvesList, static_cast<int>(i), hiddenCurves.count(curves[i].id) == 0);
        }
        updating = false;
        InvalidateRect(flight, nullptr, FALSE);
    }
    void RefreshResources() {
        const bool old = updating; updating = true;
        const int modelSelection = static_cast<int>(SendMessageW(modelKind, CB_GETCURSEL, 0, 0));
        const int spriteSelection = static_cast<int>(SendMessageW(spriteKind, CB_GETCURSEL, 0, 0));
        const auto previousModel = modelSelection >= 0 && static_cast<size_t>(modelSelection) < modelPaths.size() ? modelPaths[size_t(modelSelection)] : "";
        const auto previousSprite = spriteSelection >= 0 && static_cast<size_t>(spriteSelection) < spritePaths.size() ? spritePaths[size_t(spriteSelection)] : "";
        modelPaths.clear(); spritePaths.clear(); SendMessageW(modelKind, CB_RESETCONTENT, 0, 0); SendMessageW(spriteKind, CB_RESETCONTENT, 0, 0);
        const auto* a = project.Find(selectionKind, selectionId);
        if (a) {
            auto add = [&](const std::string& label, const std::string& role, const std::string& path) {
                if (path.empty()) return;
                const bool mdl = role == "view_model" || role == "player_model" || role == "world_model";
                const bool spr = role == "sprite" || role == "hud_sprite" || role == "kill_sprite";
                if (!mdl && !spr) return;
                auto& paths = mdl ? modelPaths : spritePaths; paths.push_back(path);
                const auto text = Wide(label + ": " + path); SendMessageW(mdl ? modelKind : spriteKind, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
            };
            for (const char* role : {"view_model", "player_model", "world_model", "sprite"}) add(role, role, a->Get(role));
            for (const auto& f : a->fields) {
                if (f.first.compare(0, 6, "asset.") == 0 && f.first.size() > 5 && f.first.substr(f.first.size()-5) == ".path") {
                    const auto base = f.first.substr(0, f.first.size()-5); add(base, a->Get(base + ".role"), f.second);
                } else if (f.first.compare(0, 5, "ammo.") == 0) {
                    const auto dot = f.first.find('.', 5); if (dot != std::string::npos) add(f.first, f.first.substr(dot+1), f.second);
                }
            }
        }
        auto restore = [](const std::vector<std::string>& paths, const std::string& previous, int index) {
            if (paths.empty()) return -1;
            if (index >= 0 && static_cast<size_t>(index) < paths.size() && paths[size_t(index)] == previous) return index;
            const auto found = std::find(paths.begin(), paths.end(), previous);
            return found != paths.end() ? static_cast<int>(found-paths.begin()) : std::clamp(index, 0, static_cast<int>(paths.size())-1);
        };
        SendMessageW(modelKind, CB_SETCURSEL, restore(modelPaths, previousModel, modelSelection), 0);
        SendMessageW(spriteKind, CB_SETCURSEL, restore(spritePaths, previousSprite, spriteSelection), 0);
        EnableWindow(modelLoad, !modelPaths.empty()); EnableWindow(spriteLoad, !spritePaths.empty());
        updating = old;
    }
    void RefreshVariants() {
        const int selected = ListView_GetNextItem(variants, -1, LVNI_SELECTED);
        const auto previous = selected >= 0 && static_cast<size_t>(selected) < variantIds.size() ? variantIds[size_t(selected)] : "";
        const bool old = updating; updating = true; ListView_DeleteAllItems(variants); variantIds.clear();
        const auto* a = project.Find(selectionKind, selectionId); const int tab = TabCtrl_GetCurSel(tabs);
        if (a && a->kind == pxwe::Kind::Weapon) for (const auto& f : a->fields) {
            if (f.first.compare(0, 6, "asset.") != 0 || f.first.substr(f.first.size()-5) != ".role") continue;
            const auto id = f.first.substr(6, f.first.size()-11), prefix = "asset." + id + ".";
            const bool spr = f.second == "sprite" || f.second == "hud_sprite" || f.second == "kill_sprite";
            if (spr != (tab == 4)) continue;
            const int row = static_cast<int>(variantIds.size()); variantIds.push_back(id);
            Cell(variants, row, 0, Wide(id)); Cell(variants, row, 1, Wide(f.second));
            Cell(variants, row, 2, Wide(a->Get(prefix + "when"))); Cell(variants, row, 3, Wide(a->Get(prefix + "path")));
        }
        for (HWND c : {variantAdd, variantEdit, variantCopy, variantRemove, variantPreview}) EnableWindow(c, a && a->kind == pxwe::Kind::Weapon);
        const auto restore = std::find(variantIds.begin(), variantIds.end(), previous);
        if (restore != variantIds.end()) ListView_SetItemState(variants, static_cast<int>(restore-variantIds.begin()), LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        updating = old;
    }
    std::string SelectedVariant() const {
        const int row = ListView_GetNextItem(variants, -1, LVNI_SELECTED);
        if (row < 0 || static_cast<size_t>(row) >= variantIds.size()) throw std::runtime_error("Select a conditional entry first.");
        return variantIds[size_t(row)];
    }
    void VariantCommand(int command) {
        CommitEditor(); const auto* a = project.Find(pxwe::Kind::Weapon, selectionId);
        if (!a || selectionKind != pxwe::Kind::Weapon) throw std::runtime_error("Select a weapon first.");
        const auto original = command == C_VARIANT_ADD ? "" : SelectedVariant(); const auto prefix = "asset." + original + ".";
        if (command == C_VARIANT_REMOVE) { project.RemoveVariant(selectionId, original); RefreshProperties(); RefreshVariants(); RefreshResources(); Title(); return; }
        if (command == C_VARIANT_PREVIEW) {
            const auto role = a->Get(prefix + "role"), path = a->Get(prefix + "path");
            if (role.find("sound") != std::string::npos) PlayWave(path);
            else if (role.find("model") != std::string::npos) LoadModelPath(path);
            else LoadSpritePath(path);
            return;
        }
        VariantInput input; input.root = project.Root();
        input.title = command == C_VARIANT_COPY ? L"Duplicate conditional entry" : command == C_VARIANT_ADD ? L"Add conditional entry" : L"Edit conditional entry";
        input.roles = TabCtrl_GetCurSel(tabs) == 4 ? std::vector<std::string>{"sprite", "hud_sprite", "kill_sprite"} :
            std::vector<std::string>{"view_model", "player_model", "world_model", "fire_sound", "reload_sound", "pickup_sound", "primary_sound", "secondary_sound"};
        input.id = command == C_VARIANT_COPY ? original + "_copy" : original;
        input.role = original.empty() ? input.roles.front() : a->Get(prefix + "role");
        input.path = original.empty() ? a->Get(input.role) : a->Get(prefix + "path");
        input.when = original.empty() ? "random" : a->Get(prefix + "when");
        if (command == C_VARIANT_COPY && (input.when.empty() || input.when == "default")) input.when = "random";
        input.conditions = {"default", "random", "mode:auto", "mode:semi", "mode:burst", "mode:single"};
        for (const auto& c : a->accepted) input.conditions.push_back("ammo:" + c.first);
        for (;;) {
            const auto result = DialogBoxParamW(gInstance, MAKEINTRESOURCEW(IDD_VARIANT), window, VariantProc, reinterpret_cast<LPARAM>(&input));
            if (result == -1) throw std::runtime_error("Cannot open conditional entry dialog.");
            if (result != IDOK) return;
            try { project.PutVariant(selectionId, input.id, input.role, input.path, input.when, command == C_VARIANT_EDIT ? original : ""); break; }
            catch (const std::exception& e) { MessageBoxW(window, Wide(e.what()).c_str(), L"Correct conditional entry", MB_OK | MB_ICONERROR); }
        }
        RefreshProperties(); RefreshVariants(); RefreshResources(); Title();
    }
    void EditCoefficient() {
        CommitEditor(); std::string id = selectionId;
        if (selectionKind == pxwe::Kind::Weapon) {
            const int row = ListView_GetNextItem(compatibility, -1, LVNI_SELECTED);
            if (row < 0 || static_cast<size_t>(row) >= compatibilityIds.size()) throw std::runtime_error("Select a cartridge first.");
            id = compatibilityIds[size_t(row)];
        }
        Input input{L"Cartridge damage coefficient", L"Damage per projectile = k × energy (J) / area (mm²).\nLegacy conversion preserves reference-speed damage; all using weapons update.", Wide(project.SuggestedCoefficient(id))};
        if (!Ask(window, input)) return;
        project.SetCoefficient(id, Utf8(input.value));
        RefreshProperties(); RefreshCompatibility(); RefreshFlight(); Title();
    }
    void EditOverrides() {
        CommitEditor(); const int row = ListView_GetNextItem(compatibility, -1, LVNI_SELECTED);
        if (selectionKind != pxwe::Kind::Weapon || row < 0 || static_cast<size_t>(row) >= compatibilityIds.size()) throw std::runtime_error("Select a weapon and an allowed cartridge first.");
        const auto id = compatibilityIds[size_t(row)]; const auto* a = project.Find(pxwe::Kind::Weapon, selectionId);
        OverrideInput input{project.Root(), L"Ammo overrides: " + Wide(id), {}};
        for (const char* role : kOverrideRoles) input.fields.emplace_back(role, a->Get("ammo." + id + "." + role));
        for (;;) {
            const auto result = DialogBoxParamW(gInstance, MAKEINTRESOURCEW(IDD_OVERRIDES), window, OverrideProc, reinterpret_cast<LPARAM>(&input));
            if (result == -1) throw std::runtime_error("Cannot open ammo overrides dialog.");
            if (result != IDOK) return;
            try { project.SetAmmoOverrides(selectionId, id, input.fields); break; }
            catch (const std::exception& e) { MessageBoxW(window, Wide(e.what()).c_str(), L"Correct ammo overrides", MB_OK | MB_ICONERROR); }
        }
        RefreshProperties(); RefreshCompatibility(); RefreshResources(); Title();
    }
    void PlayWave(const std::string& path) {
        PlaySoundW(nullptr, nullptr, 0); soundData = pxwe::ReadResource(project.Root(), "sound/" + path);
        if (!PlaySoundW(reinterpret_cast<LPCWSTR>(soundData.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT)) throw std::runtime_error("Cannot play the selected WAV sound.");
    }
    void PlaySelectedSound() {
        CommitEditor(); const auto* a = project.Find(selectionKind, selectionId);
        if (a && !editingKey.empty() && pxwe::DescribeProperty(a->kind, editingKey, a).resource == pxwe::ResourceType::Sound) { PlayWave(a->Get(editingKey)); return; }
        const auto id = SelectedVariant();
        if (!a || a->Get("asset." + id + ".role").find("sound") == std::string::npos) throw std::runtime_error("Select a sound property or sound entry first.");
        PlayWave(a->Get("asset." + id + ".path"));
    }
    void LoadSpritePath(const std::string& path) {
        sprite.Load(project.Root(), path); hasSprite = true; spriteSeconds = 0;
        const auto& frame = sprite.Frames().front();
        const auto label = Wide(path) + L"\n" + std::to_wstring(frame.image.width) + L" × " + std::to_wstring(frame.image.height) + L"; " + std::to_wstring(sprite.Frames().size()) + L" frames. Checkerboard shows transparency.";
        SetWindowTextW(spriteInfo, label.c_str()); InvalidateRect(spriteWindow, nullptr, FALSE);
    }
    void LoadSprite() {
        CommitEditor(); const int selected = static_cast<int>(SendMessageW(spriteKind, CB_GETCURSEL, 0, 0));
        if (selected < 0 || static_cast<size_t>(selected) >= spritePaths.size()) throw std::runtime_error("Set a sprite path first.");
        LoadSpritePath(spritePaths[size_t(selected)]);
    }
    void Tick() {
        const auto now = GetTickCount64(); const double dt = std::min(.1, double(now-lastTick)/1000.); lastTick = now;
        const int tab = TabCtrl_GetCurSel(tabs);
        if (hasModel && playing && tab == 3) { animationSeconds += dt; InvalidateRect(bounds, nullptr, FALSE); }
        if (hasSprite && tab == 4 && sprite.Frames().size() > 1) { spriteSeconds += dt; InvalidateRect(spriteWindow, nullptr, FALSE); }
    }
    bool ConfirmDirty() {
        CommitEditor();
        if (!project.Dirty()) return true;
        const int choice = MessageBoxW(window, L"Save changes before continuing?", L"pxwe", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (choice == IDCANCEL) return false;
        if (choice == IDYES) { project.Save(); Title(); }
        return true;
    }
    void OpenFolder() {
        if (!ConfirmDirty()) return;
        BROWSEINFOW browse{}; browse.hwndOwner = window;
        browse.lpszTitle = L"Choose the game/mod folder (for example, primext)";
        browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&browse); if (!item) return;
        wchar_t path[MAX_PATH]{};
        const bool ok = SHGetPathFromIDListW(item, path) != FALSE; CoTaskMemFree(item);
        if (!ok) throw std::runtime_error("Cannot read the selected folder path.");
        project.Load(pxwe::fs::path(path)); selectionId.clear(); selectionCategory.clear(); categorySelected=false; RefreshTree();
    }
    void NewAsset(pxwe::Kind kind, bool duplicate) {
        CommitEditor();
        if (project.Root().empty()) throw std::runtime_error("Open a game folder first.");
        if (duplicate && !project.Find(selectionKind, selectionId)) throw std::runtime_error("Select an asset to duplicate.");
        Input input{duplicate ? L"Duplicate asset" : (kind == pxwe::Kind::Weapon ? L"New weapon" : L"New cartridge"),
            L"Asset ID: ASCII letters, digits, _ or - (1..63 characters)", duplicate ? Wide(selectionId + "_copy") : L""};
        for (;;) {
            if (!Ask(window, input)) return;
            const auto id = Utf8(input.value);
            try {
                project.CheckNewId(kind, id);
            } catch (const std::exception& e) {
                MessageBoxW(window, Wide(e.what()).c_str(), L"Choose a different asset ID", MB_OK | MB_ICONERROR);
                continue;
            }
            const auto category=kind==selectionKind?CurrentCategory():"";
            project.Add(kind, id, duplicate ? selectionId : "",category);
            selectionKind = kind; selectionId = id; categorySelected=false; RefreshTree(); return;
        }
    }
    void Command(int command) {
        switch (command) {
        case CMD_OPEN: OpenFolder(); break;
        case CMD_SAVE: CommitEditor(); project.Save(); Title(); MessageBoxW(window, L"Validated definitions exported.", L"pxwe", MB_OK); break;
        case CMD_EXIT: SendMessageW(window, WM_CLOSE, 0, 0); break;
        case CMD_NEW_WEAPON: NewAsset(pxwe::Kind::Weapon, false); break;
        case CMD_NEW_CARTRIDGE: NewAsset(pxwe::Kind::Cartridge, false); break;
        case CMD_DUPLICATE: if (categorySelected) CategoryCommand(CMD_CATEGORY_COPY); else NewAsset(selectionKind, true); break;
        case CMD_DELETE:
            if (categorySelected) { CategoryCommand(CMD_CATEGORY_REMOVE); break; }
            CommitEditor();
            if (!project.Find(selectionKind, selectionId)) throw std::runtime_error("Select an asset to delete.");
            if (MessageBoxW(window, (L"Delete " + Wide(selectionId) + L"?").c_str(), L"pxwe", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                project.Remove(selectionKind, selectionId); selectionId.clear(); RefreshTree();
            }
            break;
        case CMD_CATEGORY_NEW: case CMD_CATEGORY_RENAME: case CMD_MOVE: case CMD_CATEGORY_COPY:
        case CMD_CATEGORY_REMOVE: case CMD_CATEGORY_DELETE_ASSETS: CategoryCommand(command); break;
        case CMD_UNDO: CommitEditor(); project.Undo(); RefreshTree(); break;
        case CMD_REDO: CommitEditor(); project.Redo(); RefreshTree(); break;
        case CMD_REFRESH_TREE: if (treeRefreshPending) { treeRefreshPending=false; CommitEditor(); RefreshTree(); } break;
        case CMD_VALIDATE: CommitEditor(); Report(window, project.Validate().Message()); break;
        case CMD_HELP:
            MessageBoxW(window,
                L"pxwe — PrimeXT Weapon Editor J.10.6.9.7\n\n"
                L"File > Open game folder selects a mod. Asset creates weapons and cartridges.\n"
                L"Right-click the tree for nested categories and move/duplicate/delete actions. F2 renames categories.\n"
                L"Select a property for detailed help. Double-click a value to edit below.\n"
                L"Double-click section headings to collapse / expand. Browse chooses resource files.\n"
                L"Apply, Enter or selection changes commit a value; blank removes an optional field.\n"
                L"New model paths use models/v_<ID>.mdl, p_<ID>.mdl and w_<ID>.mdl.\n"
                L"Cartridges sets allowed ammunition, velocity, read-only muzzle damage, coefficient and ammo asset/sound overrides.\n"
                L"F5 validates with the game's C++ parser. Ctrl+S exports scripts and manifests.\n"
                L"Restart/load the game to read changed definitions.\n\n"
                L"Model / Animations edits model, sound and animation controls and conditional entries.\n"
                L"Load model previews textures; select a sequence to play. Drag to orbit.\n"
                L"Sprites previews .spr frames / HUD .txt crops. Ballistics compares checked ammo in colored trajectory, energy or damage curves.\n"
                L"Runtime IDs are 32..62. The base game also uses some of the 32 ammunition slots.\n"
                L"Keep copies of projects before editing IDs used by saved games.\n\n"
                L"Native C++17 / Win32. GPL-3.0-or-later.",
                L"About pxwe", MB_OK); break;
        case IDOK:
            if (GetFocus() == properties) ActivateProperty(ListView_GetNextItem(properties, -1, LVNI_SELECTED));
            else CommitEditor();
            break;
        case C_APPLY: CommitEditor(); break;
        case C_BROWSE: BrowseProperty(); break;
        case C_ALLOW: case C_DISALLOW: case C_DEFAULT: CompatibilityCommand(command); break;
        case C_COEFFICIENT: EditCoefficient(); break;
        case C_OVERRIDES: EditOverrides(); break;
        case C_VARIANT_ADD: case C_VARIANT_EDIT: case C_VARIANT_COPY: case C_VARIANT_REMOVE: case C_VARIANT_PREVIEW: VariantCommand(command); break;
        case C_PLAY: if (hasModel) playing = !playing; break;
        case C_RESET: animationSeconds = 0; InvalidateRect(bounds, nullptr, FALSE); break;
        case C_SOUND: PlaySelectedSound(); break;
        case C_SKIN: case C_BODY: InvalidateRect(bounds, nullptr, FALSE); break;
        case C_METRIC: InvalidateRect(flight, nullptr, FALSE); break;
        case C_SPRITE_LOAD: LoadSprite(); break;
        case C_MODEL_LOAD: LoadModel(); break;
        case C_MODEL_OPEN: OpenModelViewer(); break;
        }
    }
    void LoadModel() {
        CommitEditor();
        const int selected = static_cast<int>(SendMessageW(modelKind, CB_GETCURSEL, 0, 0));
        if (selected < 0 || static_cast<size_t>(selected) >= modelPaths.size()) throw std::runtime_error("Set a model path first.");
        LoadModelPath(modelPaths[size_t(selected)]);
    }
    void LoadModelPath(const std::string& rel) {
        studio.Load(project.Root(), rel); model = studio.Info(); modelPath = project.Root() / pxwe::fs::u8path(rel);
        hasModel = true; sequenceIndex = 0; animationSeconds = 0; playing = true;
        updating = true;
        ListView_DeleteAllItems(sequences);
        for (size_t i = 0; i < model.sequences.size(); ++i) {
            const auto& seq = model.sequences[i]; Cell(sequences, static_cast<int>(i), 0, std::to_wstring(i));
            Cell(sequences, static_cast<int>(i), 1, Wide(seq.name)); Cell(sequences, static_cast<int>(i), 2, Number(seq.fps));
            Cell(sequences, static_cast<int>(i), 3, std::to_wstring(seq.frames));
        }
        ListView_SetItemState(sequences, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        for (const auto& choice : std::vector<std::pair<HWND, int>>{{skin, studio.SkinCount()}, {body, studio.BodyCount()}}) {
            SendMessageW(choice.first, CB_RESETCONTENT, 0, 0);
            for (int i = 0; i < choice.second; ++i) { const auto text = (choice.first == skin ? L"Skin " : L"Body ") + std::to_wstring(i); SendMessageW(choice.first, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str())); }
            SendMessageW(choice.first, CB_SETCURSEL, 0, 0); EnableWindow(choice.first, TRUE);
        }
        updating = false;
        modelText = Wide(rel) + L"\nSelect a sequence to play. Drag to orbit. Skin/body selects texture family and body groups.";
        SetWindowTextW(modelInfo, modelText.c_str()); EnableWindow(modelOpen, pxwe::fs::is_regular_file(modelPath));
        EnableWindow(play, TRUE); EnableWindow(reset, TRUE);
        InvalidateRect(bounds, nullptr, FALSE);
    }
    void OpenModelViewer() {
        if (!hasModel) throw std::runtime_error("Load a model first.");
        pxwe::fs::path viewer;
        for (const auto& candidate : {executable.parent_path() / "pxmv.exe", project.Root() / "devkit/pxmv.exe", project.Root() / "pxmv.exe"})
            if (pxwe::fs::is_regular_file(candidate)) { viewer = candidate; break; }
        if (viewer.empty()) throw std::runtime_error("pxmv.exe is not installed. Copy it to the game's devkit folder to enable full model viewing.");
        const auto argument = L"\"" + modelPath.wstring() + L"\"";
        SHELLEXECUTEINFOW launch{}; launch.cbSize = sizeof(launch); launch.fMask = SEE_MASK_NOASYNC;
        launch.hwnd = window; launch.lpVerb = L"open"; launch.lpFile = viewer.c_str(); launch.lpParameters = argument.c_str();
        const auto dir = project.Root().wstring(); launch.lpDirectory = dir.c_str(); launch.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&launch)) throw std::runtime_error("Cannot start the installed pxmv.exe.");
    }
    LRESULT Notify(NMHDR* header) {
        if (updating) return 0;
        if (header->idFrom == C_TREE && header->code == TVN_SELCHANGINGW) { CommitEditor(); return 0; }
        if (header->idFrom == C_TREE && header->code == TVN_SELCHANGEDW) {
            const auto* change = reinterpret_cast<NMTREEVIEWW*>(header); const auto index = change->itemNew.lParam;
            if (index >= 0 && static_cast<size_t>(index) < treeAssets.size()) {
                categorySelected=false; selectionKind = treeAssets[index].first; selectionId = treeAssets[index].second; RefreshSelection();
            } else if (index<=-2 && static_cast<size_t>(-2-index)<treeCategories.size()) {
                const auto& folder=treeCategories[static_cast<size_t>(-2-index)];
                categorySelected=true; selectionKind=folder.kind; selectionCategory=folder.path; selectionId.clear(); RefreshSelection();
            } else {
                categorySelected=false; selectionId.clear(); RefreshSelection();
            }
        } else if (header->idFrom==C_TREE && header->code==TVN_KEYDOWN) {
            if (reinterpret_cast<NMTVKEYDOWN*>(header)->wVKey==VK_F2) CategoryCommand(CMD_CATEGORY_RENAME);
        } else if (header->idFrom == C_PROPERTIES && header->code == LVN_ITEMCHANGING) {
            const auto* change = reinterpret_cast<NMLISTVIEW*>(header);
            if ((change->uChanged & LVIF_STATE) && (change->uOldState & LVIS_SELECTED) && !(change->uNewState & LVIS_SELECTED)) CommitEditor();
        } else if (header->idFrom == C_PROPERTIES && header->code == NM_DBLCLK) {
            ActivateProperty(reinterpret_cast<NMITEMACTIVATE*>(header)->iItem);
        } else if (header->idFrom == C_PROPERTIES && header->code == LVN_KEYDOWN) {
            if (reinterpret_cast<NMLVKEYDOWN*>(header)->wVKey == VK_RETURN)
                ActivateProperty(ListView_GetNextItem(properties, -1, LVNI_SELECTED));
        } else if (header->idFrom == C_PROPERTIES && header->code == LVN_ITEMCHANGED) {
            const auto* change = reinterpret_cast<NMLISTVIEW*>(header);
            if ((change->uChanged & LVIF_STATE) && (change->uNewState & LVIS_SELECTED) && !(change->uOldState & LVIS_SELECTED))
                EditProperty(change->iItem);
        } else if (header->idFrom == C_TAB && header->code == TCN_SELCHANGING) { CommitEditor(); return 0; }
        else if (header->idFrom == C_TAB && header->code == TCN_SELCHANGE) {
            RefreshProperties(); RefreshCompatibility(); RefreshFlight(); RefreshVariants(); Layout();
        } else if (header->idFrom == C_COMPAT && header->code == NM_DBLCLK) CompatibilityCommand(C_ALLOW);
        else if (header->idFrom == C_VARIANTS && header->code == NM_DBLCLK) VariantCommand(C_VARIANT_EDIT);
        else if (header->idFrom == C_SEQUENCES && header->code == LVN_ITEMCHANGED) {
            const auto* change = reinterpret_cast<NMLISTVIEW*>(header);
            if ((change->uChanged & LVIF_STATE) && (change->uNewState & LVIS_SELECTED) && hasModel && change->iItem >= 0 && static_cast<size_t>(change->iItem) < model.sequences.size()) {
                sequenceIndex = change->iItem; animationSeconds = 0; playing = true; InvalidateRect(bounds, nullptr, FALSE);
            }
        } else if (header->idFrom == C_CURVES && header->code == LVN_ITEMCHANGED) {
            const auto* change = reinterpret_cast<NMLISTVIEW*>(header);
            if ((change->uChanged & LVIF_STATE) && change->iItem >= 0 && static_cast<size_t>(change->iItem) < curves.size()) {
                const auto& id = curves[size_t(change->iItem)].id;
                if (ListView_GetCheckState(curvesList, change->iItem)) hiddenCurves.erase(id); else hiddenCurves.insert(id);
                InvalidateRect(flight, nullptr, FALSE);
            }
        } else if (header->idFrom == C_CURVES && header->code == NM_CUSTOMDRAW) return CurveCustomDraw(header);
        return 0;
    }
    void PaintPreview(HWND child) {
        PAINTSTRUCT paint{}; HDC output = BeginPaint(child, &paint); RECT r{}; GetClientRect(child, &r);
        if (r.right <= 0 || r.bottom <= 0) { EndPaint(child, &paint); return; }
        HDC dc = CreateCompatibleDC(output); HBITMAP bitmap = CreateCompatibleBitmap(output, r.right, r.bottom);
        if (!dc || !bitmap) {
            if (bitmap) DeleteObject(bitmap); if (dc) DeleteDC(dc); EndPaint(child, &paint); return;
        }
        auto oldBitmap = SelectObject(dc, bitmap); auto oldFont = SelectObject(dc, font);
        HBRUSH background = CreateSolidBrush(RGB(245, 247, 250)); FillRect(dc, &r, background); DeleteObject(background);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(35, 45, 60));
        RECT text{Scale(14), Scale(12), r.right - Scale(14), Scale(105)};
        try {
        if (child == flight) {
            DrawTextW(dc, flightText.c_str(), -1, &text, DT_WORDBREAK | DT_NOPREFIX);
            if (!curves.empty()) DrawFlight(dc, r);
        } else if (child == bounds && hasModel) {
            const int skinIndex = std::max(0, static_cast<int>(SendMessageW(skin, CB_GETCURSEL, 0, 0)));
            const int bodyIndex = std::max(0, static_cast<int>(SendMessageW(body, CB_GETCURSEL, 0, 0)));
            DrawImage(dc, r, studio.Render(std::min(1024L, r.right), std::min(1024L, r.bottom), sequenceIndex, animationSeconds, yaw, tilt, skinIndex, bodyIndex));
        } else if (child == spriteWindow && hasSprite) DrawImage(dc, r, sprite.Render(std::min(1024L, r.right), std::min(1024L, r.bottom), spriteSeconds));
        else DrawTextW(dc, child == spriteWindow ? L"Load a sprite or HUD list to preview its frames." : L"Load a textured model.\nDrag with the left mouse button to orbit.", -1, &text, DT_WORDBREAK);
        } catch (const std::exception& e) {
            playing = false;
            const auto error = Wide(e.what());
            DrawTextW(dc, error.c_str(), -1, &text, DT_WORDBREAK | DT_NOPREFIX);
            SetWindowTextW(child == spriteWindow ? spriteInfo : modelInfo, error.c_str());
        }
        BitBlt(output, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldFont); SelectObject(dc, oldBitmap); DeleteObject(bitmap); DeleteDC(dc); EndPaint(child, &paint);
    }
    void DrawImage(HDC dc, const RECT& r, const pxwe::Image& image) {
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = image.width; info.bmiHeader.biHeight = -image.height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        StretchDIBits(dc, 0, 0, r.right, r.bottom, 0, 0, image.width, image.height, image.pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
    }
    COLORREF CurveColor(size_t index) const {
        const COLORREF first[] = {RGB(25,105,200),RGB(225,110,15),RGB(30,145,70),RGB(125,65,190),RGB(205,45,55),RGB(0,145,160),RGB(140,95,35),RGB(190,60,140)};
        if (index < 8) return first[index];
        const double hue = std::fmod(index * .618033988749895, 1.) * 6.;
        const int sector = static_cast<int>(hue); const double f = hue-sector;
        const int v=185, low=45, up=static_cast<int>(low+(v-low)*f), down=static_cast<int>(v-(v-low)*f);
        switch (sector) { case 0: return RGB(v,up,low); case 1: return RGB(down,v,low); case 2: return RGB(low,v,up); case 3: return RGB(low,down,v); case 4: return RGB(up,low,v); default: return RGB(v,low,down); }
    }
    LRESULT CurveCustomDraw(NMHDR* header) {
        auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(header);
        if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) return CDRF_NOTIFYSUBITEMDRAW;
        if (draw->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM) && draw->iSubItem == 1) draw->clrText = CurveColor(static_cast<size_t>(draw->nmcd.dwItemSpec));
        return CDRF_DODEFAULT;
    }
    void DrawFlight(HDC dc, const RECT& r) {
        const int x = Scale(65), y = Scale(130), width = r.right-x-Scale(25), height = r.bottom-y-Scale(64);
        if (width < 20 || height < 20) return;
        const int selectedMetric = static_cast<int>(SendMessageW(metric, CB_GETCURSEL, 0, 0));
        auto valueOf = [&](const pxwe::CurveSample& p) { return selectedMetric == 1 ? p.energy : selectedMetric == 2 ? p.damage : p.height; };
        double xmax=.001, ymin=0, ymax=selectedMetric == 0 ? 0 : .001; size_t visible=0;
        for (const auto& curve : curves) if (!hiddenCurves.count(curve.id)) {
            ++visible; for (const auto& sample : curve.samples) { xmax=std::max(xmax,sample.distance); ymin=std::min(ymin,valueOf(sample)); ymax=std::max(ymax,valueOf(sample)); }
        }
        if (!visible) { RECT caption{x,y,r.right,r.bottom}; DrawTextW(dc,L"Check a cartridge to show its curve.",-1,&caption,DT_WORDBREAK); return; }
        if (ymax-ymin < .001) ymax=ymin+.001;
        HPEN grid = CreatePen(PS_SOLID,1,RGB(205,212,222)); auto oldPen=SelectObject(dc,grid);
        for (int i=0;i<=4;++i) {
            const int gx=x+width*i/4,gy=y+height*i/4;
            MoveToEx(dc,gx,y,nullptr); LineTo(dc,gx,y+height); MoveToEx(dc,x,gy,nullptr); LineTo(dc,x+width,gy);
            const auto distance=Number(xmax*i/4,0), amount=Number(ymax-(ymax-ymin)*i/4,selectedMetric==0?1:2);
            TextOutW(dc,gx-Scale(8),y+height+Scale(5),distance.c_str(),static_cast<int>(distance.size()));
            TextOutW(dc,Scale(3),gy-Scale(8),amount.c_str(),static_cast<int>(amount.size()));
        }
        for (size_t i=0;i<curves.size();++i) if (!hiddenCurves.count(curves[i].id)) {
            HPEN pen=CreatePen(PS_SOLID,Scale(2),CurveColor(i)); SelectObject(dc,pen); std::vector<POINT> points;
            for (const auto& p : curves[i].samples) points.push_back({x+static_cast<LONG>(width*p.distance/xmax), y+static_cast<LONG>(height*(ymax-valueOf(p))/(ymax-ymin))});
            if (points.size()>1) Polyline(dc,points.data(),static_cast<int>(points.size()));
            SelectObject(dc,grid); DeleteObject(pen);
        }
        const wchar_t* label = selectedMetric == 1 ? L"Distance (m) / Kinetic energy (J)" : selectedMetric == 2 ? L"Distance (m) / Damage per projectile" : L"Distance (m) / Height (m)";
        RECT caption{x,y+height+Scale(28),r.right-Scale(8),r.bottom}; DrawTextW(dc,label,-1,&caption,DT_WORDBREAK);
        SelectObject(dc,oldPen); DeleteObject(grid);
    }
    double GetPreviewMass() const {
        const auto validation = project.Validate(); const modular::Cartridge* c = nullptr;
        if (selectionKind == pxwe::Kind::Cartridge) c = validation.registry.FindCartridge(selectionId);
        else if (const auto* w = validation.registry.FindWeapon(selectionId)) c = validation.registry.FindCartridge(w->defaultCartridgeId);
        return c ? c->massGrams / 1000.0 : 0;
    }
    void DrawBounds(HDC dc, const RECT& r) {
        POINT points[8]{};
        double extent = 1;
        for (int i = 0; i < 3; ++i) extent = std::max(extent, double(model.maxs[i]) - model.mins[i]);
        const double scale = std::min(r.right, r.bottom - Scale(60)) * .48 / extent;
        for (int i = 0; i < 8; ++i) {
            double p[3]{};
            for (int axis = 0; axis < 3; ++axis)
                p[axis] = ((i & (1 << axis)) ? double(model.maxs[axis]) : double(model.mins[axis])) -
                    (double(model.maxs[axis]) + model.mins[axis]) * .5;
            const double rx = p[0] * std::cos(yaw) - p[1] * std::sin(yaw);
            const double depth = p[0] * std::sin(yaw) + p[1] * std::cos(yaw);
            const double ry = p[2] * std::cos(tilt) - depth * std::sin(tilt);
            points[i] = {r.right / 2 + static_cast<LONG>(rx * scale), r.bottom / 2 - static_cast<LONG>(ry * scale)};
        }
        HPEN pen = CreatePen(PS_SOLID, Scale(2), RGB(30, 115, 190)); auto oldPen = SelectObject(dc, pen);
        for (int i = 0; i < 8; ++i) for (int axis = 0; axis < 3; ++axis) if (!(i & (1 << axis))) {
            MoveToEx(dc, points[i].x, points[i].y, nullptr); LineTo(dc, points[i | (1 << axis)].x, points[i | (1 << axis)].y);
        }
        const auto label = L"Studio bounds: " + Number(double(model.maxs[0]) - model.mins[0], 1) + L" × " +
            Number(double(model.maxs[1]) - model.mins[1], 1) + L" × " + Number(double(model.maxs[2]) - model.mins[2], 1);
        RECT caption{Scale(10), Scale(10), r.right - Scale(10), Scale(55)}; DrawTextW(dc, label.c_str(), -1, &caption, DT_WORDBREAK);
        SelectObject(dc, oldPen); DeleteObject(pen);
    }
};

LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        app = reinterpret_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, msg, wParam, lParam);
    try {
        switch (msg) {
        case WM_CREATE: app->Create(window); return 0;
        case WM_TIMER: if (wParam == 1) app->Tick(); return 0;
        case WM_SIZE: if (app->tree && wParam != SIZE_MINIMIZED) app->Layout(); return 0;
        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
            limits->ptMinTrackSize = {app->Scale(1100), app->Scale(720)};
            MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
            if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
                limits->ptMinTrackSize.x = std::min(limits->ptMinTrackSize.x, monitor.rcWork.right - monitor.rcWork.left);
                limits->ptMinTrackSize.y = std::min(limits->ptMinTrackSize.y, monitor.rcWork.bottom - monitor.rcWork.top);
            }
            return 0;
        }
        case WM_DPICHANGED: {
            app->dpi = HIWORD(wParam); app->SetFont();
            const auto* rect = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            app->Layout(); return 0;
        }
        case WM_COMMAND: app->Command(LOWORD(wParam)); return 0;
        case WM_NOTIFY: return app->Notify(reinterpret_cast<NMHDR*>(lParam));
        case WM_CONTEXTMENU:
            if (reinterpret_cast<HWND>(wParam)==app->tree) {
                POINT point{static_cast<short>(LOWORD(lParam)),static_cast<short>(HIWORD(lParam))};
                app->TreeContext(point,point.x==-1 && point.y==-1); return 0;
            }
            break;
        case WM_CLOSE: if (app->ConfirmDirty()) DestroyWindow(window); return 0;
        case WM_DESTROY: KillTimer(window, 1); PlaySoundW(nullptr, nullptr, 0); PostQuitMessage(0); return 0;
        }
    } catch (const std::exception& e) {
        MessageBoxW(window, Wide(e.what()).c_str(), L"pxwe", MB_OK | MB_ICONERROR);
        if (msg == WM_CREATE) return -1;
        // Selection/tab-changing notifications return nonzero to keep the old
        // edit active when it couldn't be committed.
        if (msg == WM_NOTIFY) return TRUE;
        return 0;
    }
    return DefWindowProcW(window, msg, wParam, lParam);
}
LRESULT CALLBACK PreviewProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        app = reinterpret_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, msg, wParam, lParam);
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
        try { app->PaintPreview(window); } catch (...) { ValidateRect(window, nullptr); }
        return 0;
    }
    if (window == app->bounds) {
        if (msg == WM_LBUTTONDOWN) {
            app->dragging = true; app->dragPoint = {static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
            SetCapture(window); return 0;
        }
        if (msg == WM_MOUSEMOVE && app->dragging && (wParam & MK_LBUTTON)) {
            POINT p{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
            app->yaw += (p.x - app->dragPoint.x) * .01;
            app->tilt = std::clamp(app->tilt + (p.y - app->dragPoint.y) * .01, -1.5, 1.5);
            app->dragPoint = p; InvalidateRect(window, nullptr, FALSE); return 0;
        }
        if (msg == WM_LBUTTONUP || msg == WM_CAPTURECHANGED) { app->dragging = false; if (GetCapture() == window) ReleaseCapture(); return 0; }
    }
    return DefWindowProcW(window, msg, wParam, lParam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    gInstance = instance;
    // DPI awareness is set by the embedded manifest before any HWND is created.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result = 1;
    try {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES};
        if (!InitCommonControlsEx(&controls)) throw std::runtime_error("Cannot initialise Windows common controls.");
        Application app; app.executable = ExecutablePath();
        int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv) throw std::runtime_error("Cannot read command-line arguments.");
        pxwe::fs::path explicitRoot; bool validateOnly = false;
        if (argc == 2) explicitRoot = pxwe::fs::path(argv[1]);
        else if (argc == 3 && std::wstring(argv[1]) == L"--validate") {
            validateOnly = true; explicitRoot = pxwe::fs::path(argv[2]);
        } else if (argc > 1) { LocalFree(argv); throw std::runtime_error("Usage: pxwe.exe [game-folder]\npxwe.exe --validate game-folder"); }
        LocalFree(argv);
        std::string loadError;
        try { app.project.Load(pxwe::DiscoverRoot(app.executable, explicitRoot)); }
        catch (const std::exception& e) { loadError = e.what(); }
        if (validateOnly) {
            if (!loadError.empty()) throw std::runtime_error(loadError);
            const auto validation = app.project.Validate(); Report(nullptr, validation.Message()); result = validation.Ok() ? 0 : 1;
        } else {
            WNDCLASSEXW frame{}; frame.cbSize = sizeof(frame); frame.lpfnWndProc = WindowProc; frame.hInstance = instance;
            frame.hCursor = LoadCursorW(nullptr, IDC_ARROW); frame.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
            frame.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); frame.lpszClassName = kWindowClass;
            if (!RegisterClassExW(&frame)) throw std::runtime_error("Cannot register the editor window.");
            WNDCLASSEXW preview = frame; preview.lpfnWndProc = PreviewProc; preview.lpszClassName = kPreviewClass;
            preview.hbrBackground = nullptr; preview.hIcon = nullptr;
            if (!RegisterClassExW(&preview)) throw std::runtime_error("Cannot register the preview window.");
            HWND window = CreateWindowExW(0, kWindowClass, L"pxwe", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                CW_USEDEFAULT, CW_USEDEFAULT, 1180, 780, nullptr, nullptr, instance, &app);
            if (!window) throw std::runtime_error("Cannot create the editor window.");
            MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
            if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
                const int width = std::min<int>(app.Scale(1180), monitor.rcWork.right - monitor.rcWork.left);
                const int height = std::min<int>(app.Scale(780), monitor.rcWork.bottom - monitor.rcWork.top);
                SetWindowPos(window, nullptr, monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2,
                    monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2,
                    width, height, SWP_NOZORDER | SWP_NOACTIVATE);
            }
            ShowWindow(window, show); UpdateWindow(window);
            if (!loadError.empty()) MessageBoxW(window, (Wide(loadError) + L"\nUse File > Open game folder to choose a valid project.").c_str(), L"pxwe", MB_OK | MB_ICONERROR);
            HACCEL accelerators = LoadAcceleratorsW(instance, MAKEINTRESOURCEW(IDR_ACCEL));
            MSG message{}; BOOL next;
            while ((next = GetMessageW(&message, nullptr, 0, 0)) > 0) {
                if (!TranslateAcceleratorW(window, accelerators, &message) && !IsDialogMessageW(window, &message)) {
                    TranslateMessage(&message); DispatchMessageW(&message);
                }
            }
            result = next == -1 ? 1 : static_cast<int>(message.wParam);
        }
    } catch (const std::exception& e) { MessageBoxW(nullptr, Wide(e.what()).c_str(), L"pxwe", MB_OK | MB_ICONERROR); }
    if (SUCCEEDED(com)) CoUninitialize();
    return result;
}
