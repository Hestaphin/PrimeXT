// Test actual help-window UI events against minimal mock mxToolKit services.
#include <map>
#include <vector>
#include <string>
#define private public
#include "../PhysicsAssetHelpDialog.h"
#undef private
#include "../PhysicsAssetHelpData.h"
#include <mx.h>
#include <mx/mxButton.h>
#include <mx/mxLabel.h>
#include <mx/mxListBox.h>
#include <mx/mxChoice.h>
#include "../PhysicsAssetUiGuide.h"
#include <cassert>
#include <iostream>

class mxWidget_i { public: mxWindow *parent=nullptr;std::string label;bool visible=false; };
class mxWindow_i {};
namespace physui {
static int mockTextScale=125;
int CurrentTextScale(){return mockTextScale;}
void SetTextScale(int p){mockTextScale=p;}
int ChoiceFromPercent(int p){return p==100?0:p==150?2:1;}
int PercentFromChoice(int i){return i==0?100:i==2?150:125;}
void InstallScrollableScaling(mxWindow*,int,int){}
void RefreshScaling(mxWindow*){}
void BindFocus(mxWidget*,void*,void(*)(void*)){}
}
static std::map<const mxListBox*,std::vector<std::string>> items;
static std::map<const mxListBox*,int> selection;
mxWidget::mxWidget(mxWindow *p,int,int,int,int,const char *label):d_this(new mxWidget_i)
{ d_this->parent=p;d_this->label=label?label:""; }
mxWidget::~mxWidget() {delete d_this;}
void mxWidget::setHandle(void*){}
void *mxWidget::getHandle() const {return nullptr;}
void mxWidget::setParent(mxWindow *p) {d_this->parent=p;}
mxWindow *mxWidget::getParent() const {return d_this->parent;}
void mxWidget::setType(int) {}
void mxWidget::setVisible(bool v) {d_this->visible=v;}
bool mxWidget::isVisible() const {return d_this->visible;}
void mxWidget::setLabel(const char *v) {d_this->label=v?v:"";}
const char *mxWidget::getLabel() const {return d_this->label.c_str();}
mxWindow::mxWindow(mxWindow *p,int x,int y,int w,int h,const char *s,int):mxWidget(p,x,y,w,h,s),d_this(new mxWindow_i){}
mxWindow::~mxWindow(){delete d_this;}
void mxWindow::addWidget(mxWidget*){}
int mxWindow::handleEvent(mxEvent*){return 0;}
void mxWindow::redraw(){}
mxButton::mxButton(mxWindow *p,int x,int y,int w,int h,const char *s,int):mxWidget(p,x,y,w,h,s){}
mxButton::~mxButton()=default;
mxLabel::mxLabel(mxWindow *p,int x,int y,int w,int h,const char *s):mxWidget(p,x,y,w,h,s){}
mxLabel::~mxLabel()=default;
mxListBox::mxListBox(mxWindow *p,int x,int y,int w,int h,int,int):mxWidget(p,x,y,w,h){}
mxListBox::~mxListBox()=default;
mxChoice::mxChoice(mxWindow *p,int x,int y,int w,int h,int):mxWidget(p,x,y,w,h){}
mxChoice::~mxChoice()=default;
void mxChoice::add(const char *s){items[reinterpret_cast<const mxListBox*>(this)].emplace_back(s?s:"");}
void mxChoice::select(int i){selection[reinterpret_cast<const mxListBox*>(this)]=i;}
int mxChoice::getSelectedIndex()const{return selection[reinterpret_cast<const mxListBox*>(this)];}
void mxChoice::removeAll(){items[reinterpret_cast<const mxListBox*>(this)].clear();}
void mxListBox::add(const char *s){items[this].emplace_back(s?s:"");}
void mxListBox::removeAll(){items[this].clear();selection[this]=-1;}
void mxListBox::select(int v){selection[this]=v;}
int mxListBox::getSelectedIndex()const{return selection[this];}
int mxListBox::getItemCount()const{return static_cast<int>(items[this].size());}
int main()
{
  mxWindow root(nullptr,0,0,100,100,"root");
  PhysicsAssetHelpDialog dialog(&root);
  assert(!dialog.isVisible());
  dialog.Open("Generator");
  assert(dialog.isVisible());
  assert(dialog.m_sectionNames[dialog.m_sections->getSelectedIndex()]=="Generator");
  assert(dialog.m_parameters->getItemCount()>20);
  int target=-1;
  for(int i=0;i<dialog.m_parameters->getItemCount();++i)
    if(items[dialog.m_parameters][i]=="Chain edges")target=i;
  assert(target>=0);
  dialog.m_parameters->select(target);
  mxEvent click{};click.event=mxEvent::Action;click.action=8902;
  dialog.handleEvent(&click);
  assert(std::string(dialog.m_units->getLabel()).find("0..16")!=std::string::npos);
  assert(dialog.m_detail->getItemCount()>5);
  dialog.Open("Start here","First ragdoll walkthrough");
  assert(std::string(dialog.m_guideStatus->getLabel()).find("1 / 8")!=std::string::npos);
  mxEvent next{};next.event=mxEvent::Action;next.action=8906;dialog.handleEvent(&next);
  assert(dialog.m_guideStage==1);
  assert(std::string(dialog.m_guideStatus->getLabel()).find("2 / 8")!=std::string::npos);
  next.action=8905;dialog.handleEvent(&next);assert(dialog.m_guideStage==0);
  dialog.Open("Bodies","Mass");assert(std::string(dialog.m_brief->getLabel()).find("inertia")!=std::string::npos);
  dialog.m_scale->select(2);next.action=8904;dialog.handleEvent(&next);
  assert(physui::CurrentTextScale()==150);
  dialog.Open("Tuning");
  assert(dialog.m_sectionNames[dialog.m_sections->getSelectedIndex()]=="Tuning");
  click.action=8903;dialog.handleEvent(&click);assert(!dialog.isVisible());
  dialog.Open("nonexistent");assert(dialog.m_sectionNames[dialog.m_sections->getSelectedIndex()]=="Start here");
  mxEvent close{};close.event=mxEvent::Close;dialog.handleEvent(&close);assert(!dialog.isVisible());
  std::cout<<"PASS actual Help dialog category navigation, field guidance, revisit, Close button and window close\n";
}
