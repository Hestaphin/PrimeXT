#pragma once
#include <mx/mxWindow.h>
#include <string>
#include <vector>
class mxListBox;
class mxLabel;
class PhysicsAssetHelpDialog : public mxWindow
{
public:
    explicit PhysicsAssetHelpDialog(mxWindow *parent);
    void Open(const std::string &section = "Start here", const std::string &parameter = "");
    int handleEvent(mxEvent *event) override;
private:
    enum { SelectSection = 8901, SelectParameter, CloseHelp, TextScale, GuidePrevious, GuideNext };
    void ShowSection(int selected);
    void ShowParameter(int selected);
    void ShowGuideStage();
    class mxChoice *m_scale=nullptr;
    mxLabel *m_guideStatus=nullptr;
    int m_guideStage=0;
    mxListBox *m_sections = nullptr, *m_parameters = nullptr, *m_detail = nullptr;
    mxLabel *m_brief = nullptr, *m_units = nullptr;
    std::vector<std::string> m_sectionNames;
    std::vector<std::size_t> m_matching;
};
