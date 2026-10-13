#include "PhysicsAssetHelpDialog.h"
#include "PhysicsAssetHelpData.h"
#include "PhysicsAssetUiGuide.h"
#include <mx/mxChoice.h>
#include <mx.h>
#include <mx/mxButton.h>
#include <mx/mxListBox.h>
#include <mx/mxLabel.h>
#include <algorithm>

PhysicsAssetHelpDialog::PhysicsAssetHelpDialog(mxWindow *parent)
    : mxWindow(parent, 75, 75, 900, 698, "Physics Asset Editor - Help and Instructions", mxWindow::Dialog)
{
    new mxLabel(this, 12, 12, 206, 22, "1. Choose a subject");
    new mxLabel(this, 230, 12, 300, 22, "2. Choose a field or question");
    m_sections = new mxListBox(this, 12, 40, 205, 548, SelectSection);
    m_parameters = new mxListBox(this, 230, 40, 278, 548, SelectParameter);
    new mxLabel(this, 525, 12, 350, 22, "3. Read its meaning and advice");
    m_brief = new mxLabel(this, 525, 42, 358, 56, "Select a parameter on the left.");
    m_units = new mxLabel(this, 525, 103, 358, 26, "");
    m_detail = new mxListBox(this, 525, 139, 358, 449);
    new mxLabel(this, 12, 602, 730, 22, "Guidance only: Always inspect generated bodies and perform an in-game PhysX test.");
    new mxButton(this, 780, 615, 103, 27, "Close Help", CloseHelp);
    new mxButton(this,317,647,100,28,"< Previous",GuidePrevious);
    m_guideStatus=new mxLabel(this,428,650,200,21,"Walkthrough: 1 / 8");
    new mxButton(this,641,647,109,28,"Next step >",GuideNext);
    new mxLabel(this,12,651,110,21,"Text size");
    m_scale=new mxChoice(this,121,648,175,26,TextScale);
    m_scale->add("Normal 100%");m_scale->add("Large 125%");m_scale->add("Extra large 150%");
    m_scale->select(physui::ChoiceFromPercent(physui::CurrentTextScale()));
    physui::InstallScrollableScaling(this,900,698);
    m_sectionNames = physhelp::Sections();
    for(const auto &name : m_sectionNames) m_sections->add(name.c_str());
    setVisible(false);
}
void PhysicsAssetHelpDialog::Open(const std::string &section,const std::string &parameter)
{
    auto it=std::find(m_sectionNames.begin(),m_sectionNames.end(),section);
    int selected=it==m_sectionNames.end()?0:static_cast<int>(it-m_sectionNames.begin());
    ShowSection(selected);
    if(!parameter.empty()) {
        for(std::size_t i=0;i<m_matching.size();++i)
            if(parameter==physhelp::Catalog()[m_matching[i]].name){ShowParameter(static_cast<int>(i));break;}
    }
    m_scale->select(physui::ChoiceFromPercent(physui::CurrentTextScale()));
    m_guideStage=0;
    if(parameter=="First ragdoll walkthrough")ShowGuideStage();
    setVisible(true);
}
void PhysicsAssetHelpDialog::ShowSection(int selected)
{
    if(selected<0 || selected>=static_cast<int>(m_sectionNames.size())) return;
    m_sections->select(selected);
    m_matching.clear();m_parameters->removeAll();
    const auto &entries=physhelp::Catalog();
    for(std::size_t i=0;i<entries.size();++i)
        if(m_sectionNames[selected]==entries[i].section)
        {m_matching.push_back(i);m_parameters->add(entries[i].name);}
    ShowParameter(0);
}
void PhysicsAssetHelpDialog::ShowParameter(int selected)
{
    if(selected<0||selected>=static_cast<int>(m_matching.size())) return;
    m_parameters->select(selected);
    const auto &entry=physhelp::Catalog()[m_matching[selected]];
    // mxLabel has no portable multiline word wrap. Show short prose in the
    // label and all extended, wrapped paragraphs in the scrollable list.
    m_brief->setLabel(entry.brief);
    const std::string units=std::string("Units / values: ")+entry.units;
    m_units->setLabel(units.c_str());
    m_detail->removeAll();
    if(m_guideStatus)m_guideStatus->setLabel(entry.name==std::string("First ragdoll walkthrough")?"Walkthrough: 1 / 8":"Select walkthrough to use steps");
    auto add=[this](const std::string &text)
    {
        for(const auto &line:physhelp::Wrap(text,51)) m_detail->add(line.c_str());
    };
    add(std::string("WHAT IT DOES: ")+entry.brief);
    m_detail->add(" ");
    add(entry.details);
    m_detail->add(" ");
    add(std::string("EXAMPLE / SUGGESTED USE: ")+entry.example);
    if(std::string(entry.name)=="First ragdoll walkthrough")ShowGuideStage();
}
void PhysicsAssetHelpDialog::ShowGuideStage()
{
    static const char *steps[]={
      "1 / 8: Load the character .mdl in pxmv and open Physics Asset Editor.",
      "2 / 8: Open Templates, choose a preset and body detail. Use Edit copy or Import .ragtemplate to edit a recipe.",
      "3 / 8: Generate Preview. Check every body, bone mapping, joint and warning before Apply.",
      "4 / 8: Inspect joint frame axes and collision shape placement in the model viewport.",
      "5 / 8: Choose Full to freeze unboxed bones or Partial to keep them animating. Simulate Preview and inspect the drop.",
      "6 / 8: Correct mapping and weights on their pages. Enable Advanced fields for limits; regenerate and review changes.",
      "7 / 8: Apply + save .phys transfers the reviewed candidate to the asset editor and writes the model sidecar.",
      "8 / 8: Run an in-game PhysX drop and hit test. Editor simulation is not a full game test."
    };
    if(!m_guideStatus)return;
    m_guideStatus->setLabel(("Walkthrough: "+std::to_string(m_guideStage+1)+" / 8").c_str());
    // Add the currently requested step to the scrollable help detail panel.
    m_detail->removeAll();
    for(const auto &line:physhelp::Wrap(steps[m_guideStage],51))m_detail->add(line.c_str());
    m_detail->add(" ");
    if(const auto *entry=physhelp::Find("Start here","First ragdoll walkthrough")) {
        for(const auto &line:physhelp::Wrap(entry->details,51))m_detail->add(line.c_str());
    }
}
int PhysicsAssetHelpDialog::handleEvent(mxEvent *event)
{
    if(!event) return 1;
    if(event->event==mxEvent::Close){setVisible(false);return 1;}
    if(event->event!=mxEvent::Action) return 1;
    switch(event->action)
    {
    case SelectSection: ShowSection(m_sections->getSelectedIndex()); break;
    case SelectParameter: ShowParameter(m_parameters->getSelectedIndex()); break;
    case CloseHelp: setVisible(false); break;
    case GuidePrevious:
        if(m_guideStage>0)--m_guideStage;
        ShowSection(0);
        for(int i=0;i<m_parameters->getItemCount();++i)
            if(m_matching[i]<physhelp::Catalog().size() &&
               std::string(physhelp::Catalog()[m_matching[i]].name)=="First ragdoll walkthrough") {ShowParameter(i);break;}
        ShowGuideStage();break;
    case GuideNext:
        if(m_guideStage<7)++m_guideStage;
        ShowSection(0);
        for(int i=0;i<m_parameters->getItemCount();++i)
            if(m_matching[i]<physhelp::Catalog().size() &&
               std::string(physhelp::Catalog()[m_matching[i]].name)=="First ragdoll walkthrough") {ShowParameter(i);break;}
        ShowGuideStage();break;
    case TextScale:
        physui::SetTextScale(physui::PercentFromChoice(m_scale->getSelectedIndex()));
        physui::RefreshScaling(this);
        break;
    }
    return 1;
}
