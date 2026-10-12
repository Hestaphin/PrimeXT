#include "PhysicsSettingsDialog.h"
#include "PhysicsAssetHelpDialog.h"
#include "PhysicsAssetUiGuide.h"
#include "physics_asset_settings.h"
#include <mx.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <sstream>
namespace
{
void TuningNumber(mxLineEdit *f,float v) {std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(9)<<v;f->setLabel(s.str().c_str());}
bool TuningFloat(mxLineEdit *f,float &v)
{
    const char *p=f->getLabel();const std::string s=p?p:"";char *end=nullptr;errno=0;const float value=std::strtof(s.c_str(),&end);
    if(s.empty()||end!=s.c_str()+s.size()||errno==ERANGE||!std::isfinite(value)) return false;
    v=value;return true;
}
std::string TuningLower(std::string s) {for(char &c:s) if(c>='A'&&c<='Z') c=char(c-'A'+'a');return s;}
}
PhysicsSettingsDialog::PhysicsSettingsDialog(mxWindow *parent,const physicsasset::AssetDef &asset,bool templateRules,std::function<bool(const physicsasset::AssetDef &)> apply)
    :mxWindow(parent,45,45,640,830,templateRules?"Template Physics Settings":"Physics Asset Settings",mxWindow::Dialog),m_draft(asset),m_templateRules(templateRules),m_apply(std::move(apply))
{
    new mxLabel(this,10,14,160,18,"Edit settings for");m_scope=new mxChoice(this,178,10,440,24,Scope);
    m_scope->add("Asset / template profile");m_scope->add(templateRules?"Role body override":"Body override");m_scope->add(templateRules?"Role joint override":"Joint override");
    new mxLabel(this,10,48,160,18,"Body / joint");m_target=new mxChoice(this,178,44,440,24,Target);
    m_inherit=new mxCheckBox(this,10,78,610,24,"Use profile settings for this body/joint",Inherit);
    const char *labels[]={"Linear damping (/s)","Angular damping (/s)","Position solver passes","Velocity solver passes","Linear speed (units/s)","Angular speed (rad/s)","Depenetration (units/s)","Sleep energy threshold","Angular resistance","Maximum torque","Projection distance","Projection angle (deg)","Contact margin (units)","Rest margin (units)"};
    for(int i=0;i<14;++i) {int x=10+(i/7)*300,y=114+(i%7)*34;new mxLabel(this,x,y+3,160,18,labels[i]);m_values[i]=new mxLineEdit(this,x+168,y,130,22);}
    new mxLabel(this,10,367,160,18,"Joint projection");m_projection=new mxChoice(this,178,363,140,24);m_projection->add("Off");m_projection->add("On");
    m_ccd=new mxCheckBox(this,335,363,280,24,"Continuous collision detection");
    new mxLabel(this,10,401,160,18,"Self collision");m_self=new mxChoice(this,178,397,440,24);m_self->add("Off (keep configured pairs)");m_self->add("Selective");m_self->add("Custom");
    m_first=new mxChoice(this,10,432,300,24);m_second=new mxChoice(this,320,432,300,24);
    new mxButton(this,10,466,90,24,"Allow pair",AllowPair);new mxButton(this,108,466,90,24,"Deny pair",DenyPair);new mxButton(this,206,466,90,24,"Default",DefaultPair);
    m_pairs=new mxListBox(this,10,500,610,116);
    new mxButton(this,10,631,150,24,"Apply settings",Apply);new mxButton(this,170,631,100,24,"Cancel",Close);new mxButton(this,280,631,115,24,"Help / ?",Help);
    m_status=new mxLabel(this,10,668,610,38);
    m_contextGuide=new PhysicsAssetContextGuide(this,713,640,"Tuning",ContextTopic,ContextDetails);
    const char *helpNames[]={"Linear damping","Angular damping","Position solver passes","Velocity solver passes",
        "Max linear velocity","Max angular velocity","Max depenetration velocity","Sleep energy threshold","Angular resistance",
        "Maximum torque","Projection distance","Projection angle","Contact margin","Rest margin"};
    for(int i=0;i<14;++i)m_contextGuide->Bind(m_values[i],helpNames[i]);
    m_contextGuide->Bind(m_projection,"Joint projection");m_contextGuide->Bind(m_ccd,"Continuous collision detection");
    m_contextGuide->Bind(m_self,"Self collision");
    new mxLabel(this,10,781,120,20,"Editor text size");
    m_textSize=new mxChoice(this,140,778,180,26,TextSize);
    m_textSize->add("Normal 100%");m_textSize->add("Large 125%");m_textSize->add("Extra large 150%");
    m_textSize->select(physui::ChoiceFromPercent(physui::CurrentTextScale()));
    physui::InstallScrollableScaling(this,640,830);
    Open(asset,std::move(m_apply));
}
PhysicsSettingsDialog::~PhysicsSettingsDialog(){delete m_contextGuide;}
void PhysicsSettingsDialog::Open(const physicsasset::AssetDef &asset,std::function<bool(const physicsasset::AssetDef &)> apply)
{
    m_draft=asset;m_apply=std::move(apply);m_scopeIndex=0;m_scope->select(0);m_first->removeAll();m_second->removeAll();
    for(const auto &b:asset.bodies) {m_first->add(b.name.c_str());m_second->add(b.name.c_str());}
    m_first->select(asset.bodies.empty()?-1:0);m_second->select(asset.bodies.size()>1?1:(asset.bodies.empty()?-1:0));
    m_status->setLabel(m_templateRules?"Pair rules override generated defaults; Generate Preview resolves them.":"Unlisted pairs are denied. Off disables all configured contacts.");
    ShowScope();RefreshPairs();setVisible(true);
}
void PhysicsSettingsDialog::Cancel() {setVisible(false);}
void PhysicsSettingsDialog::ShowScope()
{
    m_target->removeAll();if(m_scopeIndex==1) for(const auto &b:m_draft.bodies) m_target->add(b.name.c_str());
    else if(m_scopeIndex==2) for(const auto &j:m_draft.joints) m_target->add(j.name.c_str());else m_target->add("Profile defaults");
    m_targetIndex=0;m_target->select(0);ShowTarget();
}
void PhysicsSettingsDialog::ShowTarget()
{
    auto s=m_draft.simulation;physicsasset::JointTuning j;j.angularResistance=s.angularResistance;j.maxTorque=s.maxTorque;j.projection=s.projection;j.projectionLinearTolerance=s.projectionLinearTolerance;j.projectionAngularTolerance=s.projectionAngularTolerance;
    bool inherit=false,hinge=false;
    if(m_scopeIndex==1&&m_targetIndex>=0&&m_targetIndex<int(m_draft.bodies.size()))
    {
        const auto &b=m_draft.bodies[m_targetIndex];s=physicsasset::ResolveBodySettings(m_draft,b);const auto &t=b.tuning;
        inherit=b.linearDamping<0&&b.angularDamping<0&&t.positionIterations<0&&t.velocityIterations<0&&t.maxLinearVelocity<0&&t.maxAngularVelocity<0&&t.maxDepenetrationVelocity<0&&t.sleepThreshold<0&&t.ccd<0&&t.contactOffset<0&&t.restOffset<0;
    }
    if(m_scopeIndex==2&&m_targetIndex>=0&&m_targetIndex<int(m_draft.joints.size()))
    {
        const auto &joint=m_draft.joints[m_targetIndex];j=physicsasset::ResolveJointSettings(m_draft,joint);const auto &t=joint.tuning;hinge=joint.kind==physicsasset::JointKind::Hinge;
        inherit=t.angularResistance<0&&t.maxTorque<0&&t.projection==physicsasset::ProjectionMode::Inherit&&t.projectionLinearTolerance<0&&t.projectionAngularTolerance<0;
    }
    m_inherit->setChecked(inherit);m_inherit->setEnabled(m_scopeIndex!=0);
    const float values[]={s.linearDamping,s.angularDamping,float(s.positionIterations),float(s.velocityIterations),s.maxLinearVelocity,s.maxAngularVelocity,s.maxDepenetrationVelocity,s.sleepThreshold,j.angularResistance,j.maxTorque,j.projectionLinearTolerance,j.projectionAngularTolerance,s.contactOffset,s.restOffset};
    for(int i=0;i<14;++i) {TuningNumber(m_values[i],values[i]);m_values[i]->setEnabled(!inherit&&(m_scopeIndex==0||(m_scopeIndex==1&&(i<8||i>=12))||(m_scopeIndex==2&&i>=8&&i<12))&&!(hinge&&(i==8||i==9)));}
    m_projection->select(j.projection==physicsasset::ProjectionMode::On?1:0);m_projection->setEnabled(!inherit&&m_scopeIndex!=1);
    m_ccd->setChecked(s.ccd);m_ccd->setEnabled(!inherit&&m_scopeIndex!=2);m_self->select(int(m_draft.selfCollision));
}
bool PhysicsSettingsDialog::ReadFields()
{
    auto value=m_draft;const bool inherit=m_scopeIndex!=0&&m_inherit->isChecked();float f[14]{};
    for(int i=0;i<14;++i) if(!inherit&&(m_scopeIndex==0||(m_scopeIndex==1&&(i<8||i>=12))||(m_scopeIndex==2&&i>=8&&i<12)))
        if(!TuningFloat(m_values[i],f[i])) {m_status->setLabel("Settings require finite numbers.");return false;}
    if((m_scopeIndex==0||m_scopeIndex==1)&&!inherit&&(f[2]!=std::floor(f[2])||f[3]!=std::floor(f[3])||f[2]<1||f[2]>255||f[3]<0||f[3]>255)) {m_status->setLabel("Solver passes must be integers: position 1–255, velocity 0–255.");return false;}
    const int selected=m_projection->getSelectedIndex();if(!inherit&&m_scopeIndex!=1&&(selected<0||selected>1)) {m_status->setLabel("Select a projection mode.");return false;}
    const auto projection=selected==1?physicsasset::ProjectionMode::On:physicsasset::ProjectionMode::Off;
    if(m_scopeIndex==0)
    {
        auto &s=value.simulation;s.linearDamping=f[0];s.angularDamping=f[1];s.positionIterations=int(f[2]);s.velocityIterations=int(f[3]);s.maxLinearVelocity=f[4];s.maxAngularVelocity=f[5];s.maxDepenetrationVelocity=f[6];s.sleepThreshold=f[7];s.ccd=m_ccd->isChecked();s.angularResistance=f[8];s.maxTorque=f[9];s.projection=projection;s.projectionLinearTolerance=f[10];s.projectionAngularTolerance=f[11];s.contactOffset=f[12];s.restOffset=f[13];
    }
    else if(m_scopeIndex==1)
    {
        if(m_targetIndex<0||m_targetIndex>=int(value.bodies.size())) {m_status->setLabel("Select a body.");return false;}
        auto &b=value.bodies[m_targetIndex];if(inherit) {b.linearDamping=b.angularDamping=-1;b.tuning=physicsasset::BodyTuning{};}
        else
        {
            const auto old=physicsasset::ResolveBodySettings(value,b);const auto t=b.tuning;
            const bool all=b.linearDamping<0&&b.angularDamping<0&&t.positionIterations<0&&t.velocityIterations<0&&t.maxLinearVelocity<0&&t.maxAngularVelocity<0&&t.maxDepenetrationVelocity<0&&t.sleepThreshold<0&&t.ccd<0&&t.contactOffset<0&&t.restOffset<0;
            const auto o=[&](float raw,float effective,float entered){return !all&&raw<0&&effective==entered?-1.0f:entered;};
            b.linearDamping=o(b.linearDamping,old.linearDamping,f[0]);b.angularDamping=o(b.angularDamping,old.angularDamping,f[1]);
            b.tuning.positionIterations=!all&&t.positionIterations<0&&old.positionIterations==int(f[2])?-1:int(f[2]);b.tuning.velocityIterations=!all&&t.velocityIterations<0&&old.velocityIterations==int(f[3])?-1:int(f[3]);
            b.tuning.maxLinearVelocity=o(t.maxLinearVelocity,old.maxLinearVelocity,f[4]);b.tuning.maxAngularVelocity=o(t.maxAngularVelocity,old.maxAngularVelocity,f[5]);b.tuning.maxDepenetrationVelocity=o(t.maxDepenetrationVelocity,old.maxDepenetrationVelocity,f[6]);b.tuning.sleepThreshold=o(t.sleepThreshold,old.sleepThreshold,f[7]);
            b.tuning.ccd=!all&&t.ccd<0&&old.ccd==m_ccd->isChecked()?-1:(m_ccd->isChecked()?1:0);b.tuning.contactOffset=o(t.contactOffset,old.contactOffset,f[12]);b.tuning.restOffset=o(t.restOffset,old.restOffset,f[13]);
        }
    }
    else
    {
        if(m_targetIndex<0||m_targetIndex>=int(value.joints.size())) {m_status->setLabel("Select a joint.");return false;}
        auto &j=value.joints[m_targetIndex];if(inherit) j.tuning=physicsasset::JointTuning{};
        else
        {
            const auto old=physicsasset::ResolveJointSettings(value,j);const auto t=j.tuning;const bool all=t.angularResistance<0&&t.maxTorque<0&&t.projection==physicsasset::ProjectionMode::Inherit&&t.projectionLinearTolerance<0&&t.projectionAngularTolerance<0;
            const auto o=[&](float raw,float effective,float entered){return !all&&raw<0&&effective==entered?-1.0f:entered;};
            j.tuning.angularResistance=j.kind==physicsasset::JointKind::Hinge?t.angularResistance:o(t.angularResistance,old.angularResistance,f[8]);j.tuning.maxTorque=j.kind==physicsasset::JointKind::Hinge?t.maxTorque:o(t.maxTorque,old.maxTorque,f[9]);
            j.tuning.projection=!all&&t.projection==physicsasset::ProjectionMode::Inherit&&old.projection==projection?physicsasset::ProjectionMode::Inherit:projection;j.tuning.projectionLinearTolerance=o(t.projectionLinearTolerance,old.projectionLinearTolerance,f[10]);j.tuning.projectionAngularTolerance=o(t.projectionAngularTolerance,old.projectionAngularTolerance,f[11]);
        }
    }
    int self=m_self->getSelectedIndex();if(self<0||self>2) {m_status->setLabel("Select a self-collision mode.");return false;}value.selfCollision=physicsasset::SelfCollisionMode(self);
    std::string error;if(!physicsasset::Validate(value,&error)) {m_status->setLabel(error.c_str());return false;}m_draft=std::move(value);return true;
}
void PhysicsSettingsDialog::Pair(bool enabled,bool remove)
{
    if(!ReadFields()) return;
    int a=m_first->getSelectedIndex(),b=m_second->getSelectedIndex();if(a<0||b<0||a>=int(m_draft.bodies.size())||b>=int(m_draft.bodies.size())||a==b) {m_status->setLabel("Select two different bodies or roles.");return;}
    const auto first=m_draft.bodies[a].name,second=m_draft.bodies[b].name;auto &pairs=m_draft.collisionPairs;
    pairs.erase(std::remove_if(pairs.begin(),pairs.end(),[&](const auto &p){return (TuningLower(p.first)==TuningLower(first)&&TuningLower(p.second)==TuningLower(second))||(TuningLower(p.first)==TuningLower(second)&&TuningLower(p.second)==TuningLower(first));}),pairs.end());
    if(!remove) pairs.push_back({first,second,enabled});
    RefreshPairs();m_status->setLabel(enabled&&!remove?"Allowed pair: inspect overlaps and simulate before accepting contacts.":"Updated pair rule. Apply commits these settings.");
}
void PhysicsSettingsDialog::RefreshPairs() {m_pairs->removeAll();for(const auto &p:m_draft.collisionPairs) m_pairs->add((p.first+" / "+p.second+": "+(p.enabled?"allow":"deny")).c_str());}
void PhysicsSettingsDialog::ApplySettings() {if(!ReadFields()) return;if(m_apply&&m_apply(m_draft)) setVisible(false);else m_status->setLabel("The asset changed. Reopen this dialog before applying settings.");}
int PhysicsSettingsDialog::handleEvent(mxEvent *e)
{
    if(!e) return 1;
    if(e->event==mxEvent::Close) {Cancel();return 1;}
    if(e->event!=mxEvent::Action) return 1;
    switch(e->action)
    {
    case Scope:{int requested=m_scope->getSelectedIndex();if((requested==1&&m_draft.bodies.empty())||(requested==2&&m_draft.joints.empty())) {m_scope->select(m_scopeIndex);m_status->setLabel("This asset has no bodies/joints for that scope.");break;}if(ReadFields()&&requested>=0&&requested<=2) {m_scopeIndex=requested;ShowScope();}else m_scope->select(m_scopeIndex);break;}
    case Target:{int requested=m_target->getSelectedIndex(),count=m_scopeIndex==1?int(m_draft.bodies.size()):(m_scopeIndex==2?int(m_draft.joints.size()):1);if(requested>=0&&requested<count&&ReadFields()) {m_targetIndex=requested;ShowTarget();}else m_target->select(m_targetIndex);break;}
    case Inherit:
        for(int i=0;i<14;++i) m_values[i]->setEnabled(!m_inherit->isChecked()&&((m_scopeIndex==1&&(i<8||i>=12))||(m_scopeIndex==2&&i>=8&&i<12)));
        m_ccd->setEnabled(!m_inherit->isChecked()&&m_scopeIndex==1);m_projection->setEnabled(!m_inherit->isChecked()&&m_scopeIndex==2);
        if(m_scopeIndex==2&&m_targetIndex>=0&&m_targetIndex<int(m_draft.joints.size())&&m_draft.joints[m_targetIndex].kind==physicsasset::JointKind::Hinge) {m_values[8]->setEnabled(false);m_values[9]->setEnabled(false);}break;
    case ContextTopic:m_contextGuide->SelectionChanged();break;
    case ContextDetails:m_contextGuide->ShowDetails(m_helpDialog);break;
    case TextSize:
        physui::SetTextScale(physui::PercentFromChoice(m_textSize->getSelectedIndex()));
        physui::RefreshScaling(this);break;
    case Help:if(!m_helpDialog)m_helpDialog=new PhysicsAssetHelpDialog(this);m_helpDialog->Open("Tuning");break;
    case AllowPair:Pair(true,false);break;case DenyPair:Pair(false,false);break;case DefaultPair:Pair(false,true);break;case Apply:ApplySettings();break;case Close:Cancel();break;
    }
    return 1;
}
