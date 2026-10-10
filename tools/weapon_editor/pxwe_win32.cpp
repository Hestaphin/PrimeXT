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
#include <shlobj.h>
#include <shellapi.h>
#include "pxwe_project.h"
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
    C_MODEL_KIND, C_MODEL_LOAD, C_MODEL_OPEN, C_SEQUENCES, C_BOUNDS, C_MODEL_INFO, C_STATUS
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
        SendDlgItemMessageW(dialog, IDC_INPUT, EM_SETLIMITTEXT, 240, 0);
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
    HWND window = nullptr;
    HWND tree = nullptr, tabs = nullptr, properties = nullptr, value = nullptr, apply = nullptr, hint = nullptr;
    HWND compatibility = nullptr, allow = nullptr, disallow = nullptr, setDefault = nullptr, flight = nullptr;
    HWND modelKind = nullptr, modelLoad = nullptr, modelOpen = nullptr, sequences = nullptr, bounds = nullptr, modelInfo = nullptr;
    HWND status = nullptr;
    HFONT font = nullptr;
    UINT dpi = 96;
    bool updating = false;
    pxwe::Kind selectionKind = pxwe::Kind::Weapon;
    std::string selectionId, editingKey, editingValue;
    std::vector<std::pair<pxwe::Kind, std::string>> treeAssets;
    std::vector<std::string> propertyKeys, compatibilityIds;
    std::vector<modular::BallisticState> trajectory;
    std::wstring flightText = L"Select an asset to preview its default cartridge.";
    pxwe::ModelInfo model;
    bool hasModel = false, dragging = false;
    pxwe::fs::path modelPath;
    std::wstring modelText = L"Load a view, player, or world model to inspect bounds and animation sequences.";
    double yaw = .65, tilt = .35;
    POINT dragPoint{};
    ~Application() { if (font) DeleteObject(font); }
    int Scale(int n) const { return MulDiv(n, static_cast<int>(dpi), 96); }
    HWND ControlWindow(const wchar_t* cls, const wchar_t* text, DWORD style, int id) {
        HWND child = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1,
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
        const wchar_t* names[] = {L"Properties", L"Cartridges", L"Ballistics", L"Model / Animations"};
        for (int i = 0; i < 4; ++i) {
            TCITEMW item{}; item.mask = TCIF_TEXT; item.pszText = const_cast<wchar_t*>(names[i]); TabCtrl_InsertItem(tabs, i, &item);
        }
        const DWORD listStyle = LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP;
        properties = ControlWindow(WC_LISTVIEWW, L"", listStyle, C_PROPERTIES);
        ListView_SetExtendedListViewStyle(properties, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        Column(properties, 0, L"Property", Scale(215)); Column(properties, 1, L"Value (blank = unset)", Scale(390));
        value = ControlWindow(L"COMBOBOX", L"", CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP, C_VALUE);
        SendMessageW(value, CB_LIMITTEXT, 240, 0);
        apply = ControlWindow(L"BUTTON", L"Apply", BS_PUSHBUTTON | WS_TABSTOP, C_APPLY);
        hint = ControlWindow(L"STATIC", L"Select a property. Changes are applied on selection, Save, or Apply.", 0, C_HINT);
        compatibility = ControlWindow(WC_LISTVIEWW, L"", listStyle, C_COMPAT);
        ListView_SetExtendedListViewStyle(compatibility, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        Column(compatibility, 0, L"Cartridge", Scale(185)); Column(compatibility, 1, L"Chamber", Scale(110));
        Column(compatibility, 2, L"Allowed / Default", Scale(130)); Column(compatibility, 3, L"Velocity (m/s)", Scale(130));
        allow = ControlWindow(L"BUTTON", L"Allow / Edit velocity", BS_PUSHBUTTON | WS_TABSTOP, C_ALLOW);
        disallow = ControlWindow(L"BUTTON", L"Disallow", BS_PUSHBUTTON | WS_TABSTOP, C_DISALLOW);
        setDefault = ControlWindow(L"BUTTON", L"Set default", BS_PUSHBUTTON | WS_TABSTOP, C_DEFAULT);
        flight = ControlWindow(kPreviewClass, L"", WS_BORDER, C_FLIGHT);
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
        status = ControlWindow(L"STATIC", L"", SS_LEFT | SS_PATHELLIPSIS, C_STATUS);
        SetFont(); RefreshTree(); Layout();
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
        Move(properties, px, py, pw, ph - Scale(85));
        Move(hint, px, py + ph - Scale(76), pw, Scale(36));
        Move(value, px, py + ph - row, pw - Scale(95), Scale(260));
        Move(apply, px + pw - Scale(87), py + ph - row, Scale(87), row);
        ListView_SetColumnWidth(properties, 0, std::min(Scale(215), pw / 2));
        ListView_SetColumnWidth(properties, 1, std::max(Scale(120), pw - std::min(Scale(215), pw / 2) - Scale(24)));
        Move(compatibility, px, py, pw, ph - row - gap);
        Move(allow, px, py + ph - row, Scale(168), row);
        Move(disallow, px + Scale(176), py + ph - row, Scale(90), row);
        Move(setDefault, px + Scale(274), py + ph - row, Scale(108), row);
        Move(flight, px, py, pw, ph);
        Move(modelKind, px, py, Scale(150), Scale(200));
        Move(modelLoad, px + Scale(158), py, Scale(100), row);
        Move(modelOpen, px + Scale(266), py, Scale(120), row);
        Move(modelInfo, px, py + row + gap, pw, Scale(58));
        const int top = row + gap + Scale(66), modelWidth = pw / 2;
        Move(sequences, px, py + top, modelWidth, ph - top);
        Move(bounds, px + modelWidth + gap, py + top, pw - modelWidth - gap, ph - top);
        Move(status, margin, client.bottom - bottom + Scale(3), client.right - margin * 2, Scale(23));
        ShowPage();
    }
    void ShowPage() {
        const int tab = TabCtrl_GetCurSel(tabs);
        for (HWND c : {properties, value, apply, hint}) ShowWindow(c, tab == 0 ? SW_SHOW : SW_HIDE);
        for (HWND c : {compatibility, allow, disallow, setDefault}) ShowWindow(c, tab == 1 ? SW_SHOW : SW_HIDE);
        ShowWindow(flight, tab == 2 ? SW_SHOW : SW_HIDE);
        for (HWND c : {modelKind, modelLoad, modelOpen, modelInfo, sequences, bounds}) ShowWindow(c, tab == 3 ? SW_SHOW : SW_HIDE);
    }
    void Title() {
        const auto title = std::wstring(L"pxwe — PrimeXT Weapon Editor J.10.6.9") + (project.Dirty() ? L" *" : L"");
        SetWindowTextW(window, title.c_str());
        const auto label = project.Root().empty() ? L"Open a game/mod folder to begin." :
            project.Root().wstring() + (project.Dirty() ? L"   — Unsaved changes" : L"");
        SetWindowTextW(status, label.c_str());
    }
    void RefreshTree() {
        updating = true; treeAssets.clear(); TreeView_DeleteAllItems(tree);
        HTREEITEM selected = nullptr;
        for (pxwe::Kind kind : {pxwe::Kind::Weapon, pxwe::Kind::Cartridge}) {
            TVINSERTSTRUCTW group{}; group.hParent = TVI_ROOT; group.hInsertAfter = TVI_LAST;
            group.item.mask = TVIF_TEXT | TVIF_PARAM; group.item.lParam = -1;
            group.item.pszText = const_cast<wchar_t*>(kind == pxwe::Kind::Weapon ? L"Weapons" : L"Cartridges");
            HTREEITEM parent = TreeView_InsertItem(tree, &group);
            const auto& assets = kind == pxwe::Kind::Weapon ? project.Data().weapons : project.Data().cartridges;
            for (const auto& a : assets) {
                treeAssets.emplace_back(kind, a.Id()); auto name = Wide(a.Id());
                TVINSERTSTRUCTW child{}; child.hParent = parent; child.hInsertAfter = TVI_LAST;
                child.item.mask = TVIF_TEXT | TVIF_PARAM;
                child.item.pszText = name.data(); child.item.lParam = static_cast<LPARAM>(treeAssets.size() - 1);
                HTREEITEM added = TreeView_InsertItem(tree, &child);
                if (kind == selectionKind && a.Id() == selectionId) selected = added;
            }
            TreeView_Expand(tree, parent, TVE_EXPAND);
        }
        if (!selected && !treeAssets.empty()) {
            selectionKind = treeAssets.front().first; selectionId = treeAssets.front().second;
            HTREEITEM group = TreeView_GetRoot(tree);
            selected = TreeView_GetChild(tree, group);
            if (!selected) selected = TreeView_GetChild(tree, TreeView_GetNextSibling(tree, group));
        }
        if (treeAssets.empty()) selectionId.clear();
        if (selected) TreeView_SelectItem(tree, selected);
        updating = false; RefreshSelection(); Title();
    }
    void RefreshSelection() {
        updating = true; editingKey.clear(); editingValue.clear(); propertyKeys.clear();
        ListView_DeleteAllItems(properties); SetWindowTextW(value, L""); EnableWindow(value, FALSE); EnableWindow(apply, FALSE);
        const auto* a = project.Find(selectionKind, selectionId);
        if (a) {
            propertyKeys = pxwe::FieldNames(a->kind);
            for (const auto& f : a->fields)
                if (std::find(propertyKeys.begin(), propertyKeys.end(), f.first) == propertyKeys.end()) propertyKeys.push_back(f.first);
            for (size_t i = 0; i < propertyKeys.size(); ++i) {
                Cell(properties, static_cast<int>(i), 0, Wide(propertyKeys[i]));
                Cell(properties, static_cast<int>(i), 1, Wide(a->Get(propertyKeys[i])));
            }
        }
        hasModel = false; modelPath.clear(); ListView_DeleteAllItems(sequences);
        modelText = L"Load a model to inspect bounds and sequences. Drag the bounds preview to orbit.";
        SetWindowTextW(modelInfo, modelText.c_str()); EnableWindow(modelOpen, FALSE);
        EnableWindow(modelLoad, a && a->kind == pxwe::Kind::Weapon);
        updating = false; RefreshCompatibility(); RefreshFlight(); InvalidateRect(bounds, nullptr, FALSE);
    }
    void EditProperty(int row) {
        CommitEditor();
        if (row < 0 || static_cast<size_t>(row) >= propertyKeys.size()) return;
        const auto* a = project.Find(selectionKind, selectionId); if (!a) return;
        editingKey = propertyKeys[row]; editingValue = a->Get(editingKey);
        SendMessageW(value, CB_RESETCONTENT, 0, 0);
        std::vector<std::wstring> choices;
        if (editingKey == "primary_behavior" || editingKey == "secondary_behavior")
            choices = {L"ballistic", L"hitscan", L"ammo_cycle", L"none"};
        else if (editingKey == "primary_mode" || editingKey == "secondary_mode")
            choices = {L"auto", L"semi", L"burst", L"single"};
        else if (editingKey == "muzzle_flash" || editingKey == "fire_underwater" || editingKey == "auto_reload")
            choices = {L"true", L"false"};
        else if (editingKey == "npc_usable") choices = {L"1", L"0"};
        else if (editingKey == "version") choices = {L"2", L"1"};
        else if (editingKey == "default_cartridge" || editingKey == "ammo_name")
            for (const auto& c : project.Data().cartridges) choices.push_back(Wide(c.Id()));
        for (const auto& choice : choices) SendMessageW(value, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.c_str()));
        SetWindowTextW(value, Wide(editingValue).c_str());
        const bool editable = editingKey != "id";
        EnableWindow(value, editable); EnableWindow(apply, editable);
        const auto label = Wide(editingKey) + (editable ? L": edit the value below. Clear it to remove the property." :
            L": read-only. Duplicate creates an asset with a new ID.");
        SetWindowTextW(hint, label.c_str());
    }
    void CommitEditor() {
        if (updating || editingKey.empty() || editingKey == "id") return;
        const auto current = Utf8(Text(value));
        if (current == editingValue) return;
        project.SetField(selectionKind, selectionId, editingKey, current);
        const auto* a = project.Find(selectionKind, selectionId);
        editingValue = a ? a->Get(editingKey) : "";
        const auto it = std::find(propertyKeys.begin(), propertyKeys.end(), editingKey);
        if (it != propertyKeys.end()) Cell(properties, static_cast<int>(it - propertyKeys.begin()), 1, Wide(editingValue));
        Title();
    }
    void RefreshCompatibility() {
        updating = true; ListView_DeleteAllItems(compatibility); compatibilityIds.clear();
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
        }
        for (HWND c : {allow, disallow, setDefault}) EnableWindow(c, isWeapon);
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
        trajectory.clear();
        const auto validation = project.Validate();
        if (!validation.Ok()) flightText = L"Validate and correct the definitions before previewing ballistic flight.";
        else if (selectionId.empty()) flightText = L"Select a weapon or cartridge.";
        else {
            const modular::Cartridge* c = nullptr; double velocity = 0;
            if (selectionKind == pxwe::Kind::Cartridge) {
                c = validation.registry.FindCartridge(selectionId); if (c) velocity = c->referenceVelocityMps;
            } else if (const auto* w = validation.registry.FindWeapon(selectionId)) {
                c = validation.registry.FindCartridge(w->defaultCartridgeId);
                for (const auto& a : w->compatible) if (a.cartridgeId == w->defaultCartridgeId) velocity = a.muzzleVelocityMps;
            }
            if (c) {
                trajectory = pxwe::Flight(*c, velocity);
                flightText = Wide(c->id) + L" at " + Number(velocity) + L" m/s. Horizontal launch, 2 seconds.\n"
                    L"Game integrator; gravity and drag at default air density. No collision or penetration.";
            } else flightText = L"Choose a valid default cartridge and muzzle velocity.";
        }
        InvalidateRect(flight, nullptr, FALSE);
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
        project.Load(pxwe::fs::path(path)); selectionId.clear(); RefreshTree();
    }
    void NewAsset(pxwe::Kind kind, bool duplicate) {
        CommitEditor();
        if (project.Root().empty()) throw std::runtime_error("Open a game folder first.");
        if (duplicate && !project.Find(selectionKind, selectionId)) throw std::runtime_error("Select an asset to duplicate.");
        Input input{duplicate ? L"Duplicate asset" : (kind == pxwe::Kind::Weapon ? L"New weapon" : L"New cartridge"),
            L"Asset ID: ASCII letters, digits, _ or - (1..63 characters)", duplicate ? Wide(selectionId + "_copy") : L""};
        if (!Ask(window, input)) return;
        const auto id = Utf8(input.value);
        project.Add(kind, id, duplicate ? selectionId : "");
        selectionKind = kind; selectionId = id; RefreshTree();
    }
    void Command(int command) {
        switch (command) {
        case CMD_OPEN: OpenFolder(); break;
        case CMD_SAVE: CommitEditor(); project.Save(); Title(); MessageBoxW(window, L"Validated definitions exported.", L"pxwe", MB_OK); break;
        case CMD_EXIT: SendMessageW(window, WM_CLOSE, 0, 0); break;
        case CMD_NEW_WEAPON: NewAsset(pxwe::Kind::Weapon, false); break;
        case CMD_NEW_CARTRIDGE: NewAsset(pxwe::Kind::Cartridge, false); break;
        case CMD_DUPLICATE: NewAsset(selectionKind, true); break;
        case CMD_DELETE:
            CommitEditor();
            if (!project.Find(selectionKind, selectionId)) throw std::runtime_error("Select an asset to delete.");
            if (MessageBoxW(window, (L"Delete " + Wide(selectionId) + L"?").c_str(), L"pxwe", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                project.Remove(selectionKind, selectionId); selectionId.clear(); RefreshTree();
            }
            break;
        case CMD_UNDO: CommitEditor(); project.Undo(); RefreshTree(); break;
        case CMD_REDO: CommitEditor(); project.Redo(); RefreshTree(); break;
        case CMD_VALIDATE: CommitEditor(); Report(window, project.Validate().Message()); break;
        case CMD_HELP:
            MessageBoxW(window,
                L"pxwe — PrimeXT Weapon Editor J.10.6.9\n\n"
                L"File > Open game folder selects a mod. Asset creates weapons and cartridges.\n"
                L"Select a property, edit its value below, then Apply or select another property.\n"
                L"Cartridges sets allowed ammunition, per-weapon velocity and the default cartridge.\n"
                L"F5 validates with the game's C++ parser. Ctrl+S exports scripts and manifests.\n"
                L"Restart/load the game to read changed definitions.\n\n"
                L"Model preview shows bounds and sequence metadata. Open in pxmv uses a packaged pxmv.exe.\n"
                L"Runtime IDs are 32..62. The base game also uses some of the 32 ammunition slots.\n"
                L"Keep copies of projects before editing IDs used by saved games.\n\n"
                L"Native C++17 / Win32. GPL-3.0-or-later.",
                L"About pxwe", MB_OK); break;
        case IDOK: case C_APPLY: CommitEditor(); break;
        case C_ALLOW: case C_DISALLOW: case C_DEFAULT: CompatibilityCommand(command); break;
        case C_MODEL_LOAD: LoadModel(); break;
        case C_MODEL_OPEN: OpenModelViewer(); break;
        }
    }
    void LoadModel() {
        CommitEditor(); const auto* w = project.Find(selectionKind, selectionId);
        if (!w || w->kind != pxwe::Kind::Weapon) throw std::runtime_error("Select a weapon first.");
        const char* keys[] = {"view_model", "player_model", "world_model"};
        const int selected = static_cast<int>(SendMessageW(modelKind, CB_GETCURSEL, 0, 0));
        if (selected < 0 || selected > 2) return;
        const auto rel = w->Get(keys[selected]);
        // The parser checks paths on export. Apply the same relative-path
        // restrictions before reading a preview from a still-invalid project.
        if (rel.empty() || rel.size() > 240 || rel[0] == '/' || rel.find("..") != std::string::npos ||
            rel.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_./-") != std::string::npos)
            throw std::runtime_error("Enter a valid relative model path first.");
        const auto path = project.Root() / pxwe::fs::u8path(rel);
        auto info = pxwe::ReadModel(path);
        model = std::move(info); modelPath = path; hasModel = true;
        ListView_DeleteAllItems(sequences);
        for (size_t i = 0; i < model.sequences.size(); ++i) {
            const auto& seq = model.sequences[i]; Cell(sequences, static_cast<int>(i), 0, std::to_wstring(i));
            Cell(sequences, static_cast<int>(i), 1, Wide(seq.name)); Cell(sequences, static_cast<int>(i), 2, Number(seq.fps));
            Cell(sequences, static_cast<int>(i), 3, std::to_wstring(seq.frames));
        }
        modelText = Wide(rel) + L"\n" + std::to_wstring(model.sequences.size()) +
            L" sequences. Bounds preview; drag to orbit. Use pxmv for textured mesh/animation.";
        SetWindowTextW(modelInfo, modelText.c_str()); EnableWindow(modelOpen, TRUE);
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
                selectionKind = treeAssets[index].first; selectionId = treeAssets[index].second; RefreshSelection();
            } else {
                selectionId.clear(); RefreshSelection();
            }
        } else if (header->idFrom == C_PROPERTIES && header->code == LVN_ITEMCHANGED) {
            const auto* change = reinterpret_cast<NMLISTVIEW*>(header);
            if ((change->uChanged & LVIF_STATE) && (change->uNewState & LVIS_SELECTED) && !(change->uOldState & LVIS_SELECTED))
                EditProperty(change->iItem);
        } else if (header->idFrom == C_TAB && header->code == TCN_SELCHANGING) { CommitEditor(); return 0; }
        else if (header->idFrom == C_TAB && header->code == TCN_SELCHANGE) {
            RefreshCompatibility(); RefreshFlight(); ShowPage();
        } else if (header->idFrom == C_COMPAT && header->code == NM_DBLCLK) CompatibilityCommand(C_ALLOW);
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
        if (child == flight) {
            DrawTextW(dc, flightText.c_str(), -1, &text, DT_WORDBREAK | DT_NOPREFIX);
            if (!trajectory.empty()) DrawFlight(dc, r);
        } else if (hasModel) DrawBounds(dc, r);
        else DrawTextW(dc, L"Load a model to view its bounds.\nDrag with the left mouse button to orbit.", -1, &text, DT_WORDBREAK);
        BitBlt(output, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldFont); SelectObject(dc, oldBitmap); DeleteObject(bitmap); DeleteDC(dc); EndPaint(child, &paint);
    }
    void DrawFlight(HDC dc, const RECT& r) {
        const int x = Scale(60), y = Scale(130), width = r.right - x - Scale(28), height = r.bottom - y - Scale(68);
        if (width < 20 || height < 20) return;
        const auto& end = trajectory.back(); const double xmax = std::max(end.position.x, .001);
        const double drop = std::max(-end.position.z, .01);
        HPEN grid = CreatePen(PS_SOLID, 1, RGB(205, 212, 222)); auto oldPen = SelectObject(dc, grid);
        for (int i = 0; i <= 4; ++i) {
            const int gx = x + width * i / 4, gy = y + height * i / 4;
            MoveToEx(dc, gx, y, nullptr); LineTo(dc, gx, y + height);
            MoveToEx(dc, x, gy, nullptr); LineTo(dc, x + width, gy);
            auto distance = Number(xmax * i / 4, 0); TextOutW(dc, gx - Scale(8), y + height + Scale(5), distance.c_str(), static_cast<int>(distance.size()));
            auto fall = Number(-drop * i / 4, 1); TextOutW(dc, Scale(5), gy - Scale(8), fall.c_str(), static_cast<int>(fall.size()));
        }
        HPEN curve = CreatePen(PS_SOLID, Scale(2), RGB(25, 105, 200)); SelectObject(dc, curve);
        std::vector<POINT> points;
        for (const auto& state : trajectory) points.push_back({x + static_cast<LONG>(width * state.position.x / xmax),
            y + static_cast<LONG>(height * -state.position.z / drop)});
        Polyline(dc, points.data(), static_cast<int>(points.size()));
        const auto label = L"Distance (m) / Height (m). After 2 s: " + Number(end.position.x) + L" m, " +
            Number(end.position.z) + L" m; energy " + Number(modular::BallisticEnergy(end,
                GetPreviewMass())) + L" J.";
        RECT caption{x, y + height + Scale(28), r.right - Scale(8), r.bottom}; DrawTextW(dc, label.c_str(), -1, &caption, DT_WORDBREAK);
        SelectObject(dc, oldPen); DeleteObject(curve); DeleteObject(grid);
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
        case WM_SIZE: if (app->tree && wParam != SIZE_MINIMIZED) app->Layout(); return 0;
        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
            limits->ptMinTrackSize = {app->Scale(820), app->Scale(480)};
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
        case WM_CLOSE: if (app->ConfirmDirty()) DestroyWindow(window); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
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
