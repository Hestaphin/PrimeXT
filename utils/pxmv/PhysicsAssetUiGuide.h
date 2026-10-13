#pragma once
#include <string>
#include <vector>
class mxWindow;
class mxWidget;
class mxChoice;
class mxLabel;
class PhysicsAssetHelpDialog;

// UI-only services: never change a .phys asset or simulation state.
namespace physui {
// Geometry is recorded before scaling. 100/125/150 percent are absolute,
// never multiplied into already-scaled coordinates.
int CurrentTextScale();
void SetTextScale(int percent);
int PercentFromChoice(int index);
int ChoiceFromPercent(int percent);
void InstallScrollableScaling(mxWindow *window, int designWidth, int designHeight);
void RefreshScaling(mxWindow *window);
// Calls onFocus when a field receives keyboard/mouse focus, on Qt and Win32.
void BindFocus(mxWidget *field, void *owner, void (*onFocus)(void *));
// Undo the focus callback before its owner is destroyed. Safe for null fields.
void UnbindFocus(mxWidget *field);
#ifdef PHYSUI_PORTABLE_TEST
void TestFocus(mxWidget *field);
#endif
}

class PhysicsAssetContextGuide {
public:
    PhysicsAssetContextGuide(mxWindow *parent, int y, int width, const char *section,
                             int selectAction, int detailsAction);
    ~PhysicsAssetContextGuide();
    void Select(const char *entryName);
    void SelectionChanged();
    void ShowDetails(PhysicsAssetHelpDialog *&dialog);
    void Bind(mxWidget *field, const char *entryName);
    mxChoice *Choice() const {return m_choice;}
    const std::vector<mxWidget*> &Controls() const {return m_controls;}
private:
    struct Bound {PhysicsAssetContextGuide *self; std::string name; mxWidget *field;};
    static void FocusField(void *bound);
    std::string m_section;
    std::vector<std::string> m_names;
    std::vector<mxWidget*> m_controls;
    std::vector<Bound*> m_bindings; // per-control binding lifetime follows dialog
    mxChoice *m_choice=nullptr;
    mxLabel *m_note=nullptr;
    mxWindow *m_parent=nullptr;
};
