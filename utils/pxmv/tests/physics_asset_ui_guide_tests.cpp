// Build with -DPHYSUI_PORTABLE_TEST against the real context-guide implementation.
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#define private public
#include "../PhysicsAssetUiGuide.h"
#undef private
#include "../PhysicsAssetHelpDialog.h"
#include <mx/mxWindow.h>
#include <mx/mxWidget.h>
#include <mx/mxChoice.h>
#include <mx/mxLabel.h>
#include <mx/mxButton.h>

struct mxWidget_i {std::string text;void *handle;};
struct mxWindow_i {};
static std::map<const mxChoice*,std::vector<std::string>> options;
static std::map<const mxChoice*,int> selection;
static std::string openedSection,openedName;
mxWidget::mxWidget(mxWindow *,int,int,int,int,const char *label):d_this(new mxWidget_i{label?label:"",this}){}
mxWidget::~mxWidget(){delete d_this;}
void *mxWidget::getHandle() const {return d_this->handle;}
void mxWidget::setHandle(void *p){d_this->handle=p;}
void mxWidget::setLabel(const char *p){d_this->text=p?p:"";}
const char *mxWidget::getLabel() const {return d_this->text.c_str();}
void mxWidget::setVisible(bool){}
void mxWidget::setEnabled(bool){}
mxWindow::mxWindow(mxWindow *p,int x,int y,int w,int h,const char *title,int):mxWidget(p,x,y,w,h,title),d_this(new mxWindow_i){}
mxWindow::~mxWindow(){delete d_this;}
void mxWindow::addWidget(mxWidget*){}
int mxWindow::handleEvent(mxEvent*){return 1;}
void mxWindow::redraw(){}
mxLabel::mxLabel(mxWindow *p,int x,int y,int w,int h,const char *t):mxWidget(p,x,y,w,h,t){}
mxLabel::~mxLabel()=default;
mxChoice::mxChoice(mxWindow *p,int x,int y,int w,int h,int):mxWidget(p,x,y,w,h){}
mxChoice::~mxChoice()=default;
void mxChoice::add(const char *s){options[this].push_back(s?s:"");}
void mxChoice::select(int i){selection[this]=i;}
int mxChoice::getSelectedIndex()const{auto it=selection.find(this);return it==selection.end()?-1:it->second;}
void mxChoice::removeAll(){options[this].clear();selection[this]=-1;}
mxButton::mxButton(mxWindow *p,int x,int y,int w,int h,const char *s,int):mxWidget(p,x,y,w,h,s){}
mxButton::~mxButton()=default;
PhysicsAssetHelpDialog::PhysicsAssetHelpDialog(mxWindow *p):mxWindow(p,0,0,1,1,"Help",mxWindow::Dialog){}
void PhysicsAssetHelpDialog::Open(const std::string &section,const std::string &name){openedSection=section;openedName=name;}
int PhysicsAssetHelpDialog::handleEvent(mxEvent*){return 1;}
int main()
{
    mxWindow root(nullptr,0,0,900,850,"test");
    PhysicsAssetContextGuide body(&root,730,730,"Bodies",101,102);
    assert(options[body.Choice()].size()>=15);
    mxWidget mass(&root,0,0,5,5,"mass field");
    body.Bind(&mass,"Mass");
    physui::TestFocus(&mass);
    assert(options[body.Choice()][body.Choice()->getSelectedIndex()]=="Mass");
    assert(std::string(body.m_note->getLabel()).find("inertia")!=std::string::npos);
    PhysicsAssetHelpDialog *help=nullptr;
    body.ShowDetails(help);
    assert(openedSection=="Bodies"&&openedName=="Mass");
    delete help;
    PhysicsAssetContextGuide generator(&root,730,850,"Generator",103,104);
    mxWidget chain(&root,0,0,4,4,"chain");
    generator.Bind(&chain,"Chain edges");
    physui::TestFocus(&chain);
    assert(options[generator.Choice()][generator.Choice()->getSelectedIndex()]=="Chain edges");
    const int old=generator.Choice()->getSelectedIndex();
    generator.Bind(&chain,"Not a real setting");
    physui::TestFocus(&chain);assert(generator.Choice()->getSelectedIndex()==old);
    physui::UnbindFocus(&chain);
    generator.Select("Minimum detail");
    const int afterUnbind=generator.Choice()->getSelectedIndex();
    physui::TestFocus(&chain);
    assert(generator.Choice()->getSelectedIndex()==afterUnbind);
    generator.Bind(&chain,"Chain edges");
    physui::TestFocus(&chain);
    assert(options[generator.Choice()][generator.Choice()->getSelectedIndex()]=="Chain edges");
    physui::SetTextScale(150);
    assert(physui::CurrentTextScale()==150&&physui::ChoiceFromPercent(150)==2);
    physui::SetTextScale(100);
    assert(physui::CurrentTextScale()==100&&physui::PercentFromChoice(0)==100);
    physui::SetTextScale(999);assert(physui::CurrentTextScale()==100);
    std::cout<<"PASS actual context guide focus, unbind/rebind lifetime, parameter selection, detail targeting, invalid bindings, text presets\n";
}
