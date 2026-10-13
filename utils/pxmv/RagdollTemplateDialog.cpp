#include "RagdollTemplateDialog.h"
#include "PhysicsSettingsDialog.h"
#include "PhysicsAssetHelpDialog.h"
#include "PhysicsAssetUiGuide.h"
#include "PhysicsAssetEditor.h"
#include "GlWindow.h"
#include <mx.h>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace
{
std::string Field(mxLineEdit *edit) { const char *p=edit->getLabel();return p?p:""; }
void Number(mxLineEdit *edit,float value) {edit->setLabel(PhysicsUiNumber(value).c_str());}
bool Float(mxLineEdit *edit,float &value)
{
    const std::string s=Field(edit);char *end=nullptr;errno=0;const float n=std::strtof(s.c_str(),&end);
    if(s.empty()||end!=s.c_str()+s.size()||errno==ERANGE||!std::isfinite(n)) return false;
    value=n;return true;
}
bool Integer(mxLineEdit *edit,int &value)
{
    const std::string s=Field(edit);char *end=nullptr;errno=0;const long n=std::strtol(s.c_str(),&end,10);
    if(s.empty()||end!=s.c_str()+s.size()||errno==ERANGE||n<(std::numeric_limits<int>::min)()||n>(std::numeric_limits<int>::max)()) return false;
    value=static_cast<int>(n);return true;
}
std::string Trim(std::string s)
{
    const auto first=s.find_first_not_of(" \t");if(first==std::string::npos) return {};
    return s.substr(first,s.find_last_not_of(" \t")-first+1);
}
std::string LowerId(std::string s)
{
    for(char &c:s) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');
    return s;
}
}

RagdollTemplateDialog::RagdollTemplateDialog(PhysicsAssetEditor &editor)
    :mxWindow(&editor,55,55,860,690,"Ragdoll Template Generator K.11.4",mxWindow::Dialog),m_editor(editor),m_draft(ragtemplate::Builtins()[0])
{
    BuildUI();setVisible(false);
}
RagdollTemplateDialog::~RagdollTemplateDialog(){delete m_contextGuide;}
void RagdollTemplateDialog::BuildUI()
{
    new mxLabel(this,10,12,76,24,"Template");m_templates=new mxChoice(this,90,8,328,28,SelectTemplate);
    for(const auto &t:ragtemplate::Builtins())m_templates->add(t.name.c_str());
    m_templates->add("Imported / editable copy");m_templates->select(0);
    new mxButton(this,432,8,136,28,"Edit copy",Duplicate);
    new mxButton(this,582,8,152,28,"Save template as...",SaveAs);
    new mxButton(this,748,8,102,28,"Help / ?",Help);
    new mxLabel(this,10,52,72,24,"Section");m_page=new mxChoice(this,90,48,240,28,ChangePage);
    for(const char *name:{"Template settings","Bone mapping","Constraints / chains","Review + apply","Help"})m_page->add(name);
    m_page->select(0);
    m_advanced=new mxCheckBox(this,344,48,166,28,"Advanced fields",AdvancedMode);
    m_textSize=new mxChoice(this,524,48,172,28,TextSize);
    for(const char *name:{"Normal 100%","Large 125%","Extra large 150%"})m_textSize->add(name);
    m_textSize->select(physui::ChoiceFromPercent(physui::CurrentTextScale()));
    new mxButton(this,710,48,140,28,"Cancel",Close);
    const unsigned settings=1u, mapping=2u, constraints=4u, review=8u, help=16u;
    auto label=[this](unsigned page,int y,const char *text,bool advanced=false) {
        m_pages.Add(new mxLabel(this,10,y+4,166,24,text),page,advanced);
    };
    auto field=[this](unsigned page,int y,bool advanced=false) {
        return m_pages.Add(new mxLineEdit(this,184,y,650,28),page,advanced);
    };
    m_pages.Add(new mxButton(this,10,96,230,30,"Import .ragtemplate...",Import),settings);
    m_pages.Add(new mxButton(this,252,96,230,30,"Reset to preset",Reset),settings);
    m_pages.Add(new mxLabel(this,10,138,824,44,"Built-in presets are read-only. Edit copy unlocks a preset; Import loads an existing template for editing."),settings);
    label(settings,190,"Template ID");m_id=field(settings,190);
    label(settings,226,"Display name");m_name=field(settings,226);
    label(settings,262,"Description");m_description=field(settings,262);
    label(settings,298,"Anatomy");m_anatomy=field(settings,298);
    label(settings,334,"Total mass");m_budget=field(settings,334);
    label(settings,370,"Linear damping",true);m_linear=field(settings,370,true);
    label(settings,406,"Angular damping",true);m_angular=field(settings,406,true);
    label(settings,442,"Body detail");m_detail=m_pages.Add(new mxChoice(this,184,442,254,28),settings);
    for(const char *name:{"Simple","Standard","Detailed"})m_detail->add(name);m_detail->select(1);
    m_pages.Add(new mxButton(this,462,442,372,28,"Physics profile / self collision...",PhysicsSettings),settings);
    label(settings,478,"Apply policy");m_mode=m_pages.Add(new mxChoice(this,184,478,650,28),settings);
    m_mode->add("Replace ragdoll; keep props and collision");m_mode->add("Merge; keep authored bodies and joints");m_mode->select(0);
    const char *fitLabels[]={"Radius / length","End trim","Mass ratio warning"};mxLineEdit **fitFields[]={&m_radiusRatio,&m_trim,&m_massRatio};
    for(int i=0;i<3;++i){m_pages.Add(new mxLabel(this,10+i*280,520,266,24,fitLabels[i]),settings,true);*fitFields[i]=m_pages.Add(new mxLineEdit(this,10+i*280,546,266,28),settings,true);}

    m_roles=m_pages.Add(new mxListBox(this,10,96,226,338,SelectRole),mapping|constraints);
    m_pages.Add(new mxButton(this,10,446,106,30,"Add role",AddRole),mapping);
    m_pages.Add(new mxButton(this,126,446,110,30,"Remove role",RemoveRole),mapping);
    auto roleLabel=[this](unsigned page,int y,const char *text,bool advanced=false){m_pages.Add(new mxLabel(this,258,y+4,158,24,text),page,advanced);};
    auto roleField=[this](unsigned page,int y,bool advanced=false){return m_pages.Add(new mxLineEdit(this,426,y,408,28),page,advanced);};
    roleLabel(mapping,96,"Role ID");m_roleId=roleField(mapping,96);
    roleLabel(mapping,132,"Parent role");m_parent=roleField(mapping,132);
    roleLabel(mapping,168,"Bone mapping");m_bonePicker=m_pages.Add(new mxChoice(this,426,168,408,28,PickBone),mapping);
    roleLabel(mapping,204,"Bone override",true);m_override=roleField(mapping,204,true);
    roleLabel(mapping,240,"Aliases (;)",true);m_aliases=roleField(mapping,240,true);
    roleLabel(mapping,276,"Mass weight");m_weight=roleField(mapping,276);
    roleLabel(mapping,312,"Hitgroup");m_hit=roleField(mapping,312);
    m_required=m_pages.Add(new mxCheckBox(this,426,348,408,28,"Required role"),mapping);
    roleLabel(mapping,384,"Shape");m_shape=m_pages.Add(new mxChoice(this,426,384,408,28),mapping);
    for(const char *name:{"Box","Sphere","Capsule"})m_shape->add(name);
    roleLabel(mapping,420,"Minimum detail");m_roleDetail=m_pages.Add(new mxChoice(this,426,420,408,28),mapping);
    for(const char *name:{"Simple","Standard","Detailed"})m_roleDetail->add(name);
    m_pages.Add(new mxButton(this,426,466,260,30,"Apply role edits",ApplyRole),mapping);

    for(int i=0;i<3;++i){m_motion[i]=m_pages.Add(new mxChoice(this,426+i*138,96,130,28),constraints,true);for(const char *name:{"Locked","Limited","Free"})m_motion[i]->add(name);}
    roleLabel(constraints,96,"Twist / Y / Z",true);
    const char *limitLabels[]={"Swing Y","Swing Z","Twist low","Twist high"};
    for(int i=0;i<4;++i){m_pages.Add(new mxLabel(this,426+i*103,136,98,24,limitLabels[i]),constraints,true);m_limits[i]=m_pages.Add(new mxLineEdit(this,426+i*103,162,98,28),constraints,true);}
    roleLabel(constraints,208,"Frame mode",true);m_frameMode=m_pages.Add(new mxChoice(this,426,208,408,28),constraints,true);
    for(const char *name:{"Bone axes","Segment twist","Flexion X","Explicit"})m_frameMode->add(name);
    roleLabel(constraints,244,"Flexion sign",true);m_sign=roleField(constraints,244,true);
    roleLabel(constraints,280,"Anchor XYZ",true);roleLabel(constraints,316,"Pitch / yaw / roll",true);
    for(int i=0;i<3;++i){m_templateAnchor[i]=m_pages.Add(new mxLineEdit(this,426+i*138,280,130,28),constraints,true);m_templateAngles[i]=m_pages.Add(new mxLineEdit(this,426+i*138,316,130,28),constraints,true);}
    roleLabel(constraints,352,"Chain edges",true);m_chainDepth=m_pages.Add(new mxLineEdit(this,426,352,94,28),constraints,true);m_chainBranches=m_pages.Add(new mxCheckBox(this,536,352,298,28,"Expand branches"),constraints,true);
    roleLabel(constraints,388,"Mass region",true);m_region=roleField(constraints,388,true);
    roleLabel(constraints,424,"Exclude sections (;)",true);m_exclusions=roleField(constraints,424,true);
    roleLabel(constraints,460,"Exclude bones (;)",true);m_chainExclusions=roleField(constraints,460,true);
    m_constraintBasicNote=new mxLabel(this,258,104,576,140,"Template joint limits are retained. Enable Advanced fields to edit angles, joint frames, chain expansion and exclusions. Select the role on the left before editing.");

    m_report=m_pages.Add(new mxListBox(this,10,96,824,262),review);
    m_pages.Add(new mxLabel(this,10,372,166,24,"Preview joint"),review);
    m_previewJoints=m_pages.Add(new mxChoice(this,184,368,650,28,SelectPreviewJoint),review);m_previewJoints->setEnabled(false);
    m_axisReviewed=m_pages.Add(new mxCheckBox(this,10,416,824,30,"Reviewed inferred axes, flexion directions and expanded chains"),review);
    m_risksReviewed=m_pages.Add(new mxCheckBox(this,10,458,824,30,"Reviewed mass-ratio and overlap warnings"),review);
    m_pages.Add(new mxLabel(this,10,504,166,24,"Simulation mode"),review);
    m_simulationModeChoice=m_pages.Add(new mxChoice(this,184,500,650,28,PreviewMode),review);
    m_simulationModeChoice->add("Full ragdoll: freeze unboxed bones");
    m_simulationModeChoice->add("Partial ragdoll: animate unboxed bones");
    SyncSimulationMode();
    m_pages.Add(new mxLabel(this,10,540,824,38,"Full freezes unboxed bones; Partial lets them animate. Apply transfers the preview; Apply + save .phys also writes it."),review);
    m_contextGuide=new PhysicsAssetContextGuide(this,96,850,"Generator",ContextTopic,ContextDetails);
    for(auto *control:m_contextGuide->Controls())m_pages.Add(control,help);
    m_contextGuide->Bind(m_templates,"Template preset");m_contextGuide->Bind(m_budget,"Mass budget");m_contextGuide->Bind(m_weight,"Mass weight");m_contextGuide->Bind(m_hit,"Role hitgroup");
    m_contextGuide->Bind(m_chainDepth,"Chain edges");m_contextGuide->Bind(m_chainBranches,"Expand branches");m_contextGuide->Bind(m_frameMode,"Frame mode");m_contextGuide->Bind(m_radiusRatio,"Radius / length");
    m_pages.Add(new mxButton(this,10,210,300,32,"Beginner walkthrough",BeginnerGuide),help);
    m_pages.Add(new mxLabel(this,10,270,824,160,"Edit an existing template: Import .ragtemplate, change its settings or bone mapping, then Save template as. Edit a preset: choose it and click Edit copy. A .ragtemplate is a reusable recipe; a .phys is the generated model asset. Generate, inspect Review + apply, acknowledge required checks, then Apply + save .phys."),help);
    new mxButton(this,10,590,170,32,"Generate preview",Generate);
    new mxButton(this,192,590,170,32,"Simulate preview",Simulate);
    m_apply=new mxButton(this,374,590,180,32,"Apply to editor",Apply);m_apply->setEnabled(false);
    m_applySave=new mxButton(this,566,590,284,32,"Apply + save .phys",ApplyAndSave);m_applySave->setEnabled(false);
    m_status=new mxLabel(this,10,634,840,48,"Choose a template, Generate, then review and Apply + save .phys.");
    physui::InstallScrollableScaling(this,860,690);ShowPage();
}

void RagdollTemplateDialog::ShowPage()
{
    const int page=m_page->getSelectedIndex();m_pages.Show(page,m_advanced->isChecked());
    m_constraintBasicNote->setVisible(page==2&&!m_advanced->isChecked());
}

void RagdollTemplateDialog::SyncSimulationMode()
{
    if(m_simulationModeChoice)m_simulationModeChoice->select(
        m_editor.GetSimulationMode()==PhysicsAssetEditor::SimulationMode::PartialRagdoll?1:0);
}

void RagdollTemplateDialog::Open()
{
    if(isVisible()) return;
    SyncSimulationMode();
    m_editor.ClearGenerationPreview();std::string error;
    if(!m_editor.CollectTemplateBones(m_bones,error,m_draft.excludedSections)) { m_editor.UpdateStatus(error.c_str());return; }
    m_editor.m_generation.Begin(m_editor.m_asset);
    m_bonePicker->removeAll();m_bonePicker->add("(automatic aliases)");
    for(const auto &bone:m_bones) m_bonePicker->add(bone.name.c_str());
    ShowTemplate();setVisible(true);Status("Select a template and detail, then Generate Preview. Cancel leaves the authored asset unchanged.");
}
void RagdollTemplateDialog::Cancel()
{
    if(m_settingsDialog) m_settingsDialog->Cancel();
    m_editor.ClearGenerationPreview();m_editor.m_generation.Cancel();m_previewKey.clear();m_apply->setEnabled(false);if(m_applySave)m_applySave->setEnabled(false);m_previewJoints->removeAll();m_previewJoints->setEnabled(false);setVisible(false);
}
void RagdollTemplateDialog::Status(const std::string &message) { m_status->setLabel(message.c_str()); }
void RagdollTemplateDialog::ShowTemplate()
{
    m_editor.ClearGenerationPreview();m_previewKey.clear();m_apply->setEnabled(false);if(m_applySave)m_applySave->setEnabled(false);m_report->removeAll();m_axisReviewed->setChecked(false);m_risksReviewed->setChecked(false);m_previewJoints->removeAll();m_previewJoints->setEnabled(false);
    m_id->setLabel(m_draft.id.c_str());m_name->setLabel(m_draft.name.c_str());m_description->setLabel(m_draft.description.c_str());m_anatomy->setLabel(m_draft.anatomy.c_str());
    Number(m_radiusRatio,m_draft.radiusRatio);Number(m_trim,m_draft.endTrim);Number(m_massRatio,m_draft.massRatioWarning);
    std::string excluded;for(const auto &section:m_draft.excludedSections) {if(!excluded.empty()) excluded+=";";excluded+=section;}m_exclusions->setLabel(excluded.c_str());
    std::string chainExcluded;for(const auto &bone:m_draft.chainExcludedBones) {if(!chainExcluded.empty()) chainExcluded+=";";chainExcluded+=bone;}m_chainExclusions->setLabel(chainExcluded.c_str());
    Number(m_budget,m_draft.massBudget);Number(m_linear,m_draft.simulation.linearDamping);Number(m_angular,m_draft.simulation.angularDamping);
    m_roles->removeAll();for(const auto &role:m_draft.roles) m_roles->add(role.id.c_str());
    m_roles->select(0);ShowRole(0);
    for(mxWidget *widget:std::vector<mxWidget*>{m_id,m_name,m_description,m_anatomy,m_budget,m_linear,m_angular,m_roleId,m_parent,m_aliases,m_override,m_bonePicker,m_weight,m_hit,m_required,m_shape,m_roleDetail,m_motion[0],m_motion[1],m_motion[2],m_limits[0],m_limits[1],m_limits[2],m_limits[3],m_frameMode,m_region,m_sign,m_exclusions,m_chainExclusions,m_radiusRatio,m_trim,m_massRatio,m_templateAnchor[0],m_templateAnchor[1],m_templateAnchor[2],m_templateAngles[0],m_templateAngles[1],m_templateAngles[2],m_chainDepth,m_chainBranches}) widget->setEnabled(!m_builtin);
}
void RagdollTemplateDialog::ShowRole(int index)
{
    if(index<0||index>=static_cast<int>(m_draft.roles.size())) return;
    m_selectedRole=index;const auto &r=m_draft.roles[index];m_chainDepth->setLabel(std::to_string(r.chainDepth).c_str());m_chainBranches->setChecked(r.chainBranches);m_region->setLabel(r.massRegion.c_str());m_frameMode->select(static_cast<int>(r.joint.frameMode));m_sign->setLabel(std::to_string(r.joint.flexionSign).c_str());m_roleId->setLabel(r.id.c_str());m_parent->setLabel(r.parent.c_str());
    std::string aliases;for(const auto &a:r.aliases) { if(!aliases.empty()) aliases+=";";aliases+=a; }
    m_aliases->setLabel(aliases.c_str());m_override->setLabel(r.boneOverride.c_str());m_required->setChecked(r.required);
    m_shape->select(static_cast<int>(r.shape));m_roleDetail->select(r.minimumDetail);Number(m_weight,r.massWeight);m_hit->setLabel(std::to_string(r.hitGroup).c_str());
    m_motion[0]->select(static_cast<int>(r.joint.twistMotion));m_motion[1]->select(static_cast<int>(r.joint.swing1Motion));m_motion[2]->select(static_cast<int>(r.joint.swing2Motion));
    Number(m_limits[0],r.joint.swing1);Number(m_limits[1],r.joint.swing2);Number(m_limits[2],r.joint.twistLow);Number(m_limits[3],r.joint.twistHigh);
    Number(m_templateAnchor[0],r.joint.frameAnchor.x);Number(m_templateAnchor[1],r.joint.frameAnchor.y);Number(m_templateAnchor[2],r.joint.frameAnchor.z);
    Number(m_templateAngles[0],r.joint.frameAngles.x);Number(m_templateAngles[1],r.joint.frameAngles.y);Number(m_templateAngles[2],r.joint.frameAngles.z);
    m_bonePicker->select(0);
    for(size_t i=0;i<m_bones.size();++i) if(m_bones[i].name==r.boneOverride) m_bonePicker->select(static_cast<int>(i)+1);
}
bool RagdollTemplateDialog::ReadFields()
{
    if(m_builtin) return true;
    auto value=m_draft;value.id=Field(m_id);value.name=Field(m_name);value.description=Field(m_description);value.anatomy=Field(m_anatomy);
    value.excludedSections.clear();std::istringstream sections(Field(m_exclusions));std::string section;while(std::getline(sections,section,';')) if(!Trim(section).empty()) value.excludedSections.push_back(Trim(section));
    value.chainExcludedBones.clear();std::istringstream chainSections(Field(m_chainExclusions));
    while(std::getline(chainSections,section,';')) if(!Trim(section).empty()) value.chainExcludedBones.push_back(Trim(section));
    if(!Float(m_radiusRatio,value.radiusRatio)||!Float(m_trim,value.endTrim)||!Float(m_massRatio,value.massRatioWarning)) {Status("Invalid fitting/ratio numbers.");return false;}
    if(!Float(m_budget,value.massBudget)||!Float(m_linear,value.simulation.linearDamping)||!Float(m_angular,value.simulation.angularDamping)) { Status("Mass and damping require finite numbers.");return false; }
    if(m_selectedRole>=0&&m_selectedRole<static_cast<int>(value.roles.size()))
    {
        auto &r=value.roles[m_selectedRole];const std::string oldId=r.id;r.id=Field(m_roleId);r.parent=Field(m_parent);r.boneOverride=Field(m_override);
        r.massRegion=Field(m_region);r.joint.frameMode=static_cast<ragtemplate::FrameMode>(m_frameMode->getSelectedIndex());
        if(!Float(m_templateAnchor[0],r.joint.frameAnchor.x)||!Float(m_templateAnchor[1],r.joint.frameAnchor.y)||!Float(m_templateAnchor[2],r.joint.frameAnchor.z)||
           !Float(m_templateAngles[0],r.joint.frameAngles.x)||!Float(m_templateAngles[1],r.joint.frameAngles.y)||!Float(m_templateAngles[2],r.joint.frameAngles.z)) {Status("Frame anchor/angles require finite numbers.");return false;}
        if(!Integer(m_sign,r.joint.flexionSign)) {Status("Flexion sign must be +1 or -1.");return false;}
        if(!Integer(m_chainDepth,r.chainDepth)) {Status("Chain edges require an integer from 0 to 16.");return false;}
        r.chainBranches=m_chainBranches->isChecked();
        r.required=m_required->isChecked();r.minimumDetail=m_roleDetail->getSelectedIndex();r.shape=static_cast<physicsasset::ShapeType>(m_shape->getSelectedIndex());
        r.aliases.clear();std::istringstream aliases(Field(m_aliases));std::string alias;
        while(std::getline(aliases,alias,';')) if(!Trim(alias).empty()) r.aliases.push_back(Trim(alias));
        if(!Float(m_weight,r.massWeight)||!Integer(m_hit,r.hitGroup)||!Float(m_limits[0],r.joint.swing1)||!Float(m_limits[1],r.joint.swing2)||!Float(m_limits[2],r.joint.twistLow)||!Float(m_limits[3],r.joint.twistHigh)) { Status("Role weights, hitgroup and limits require valid numbers.");return false; }
        r.joint.twistMotion=static_cast<physicsasset::AngularMotion>(m_motion[0]->getSelectedIndex());r.joint.swing1Motion=static_cast<physicsasset::AngularMotion>(m_motion[1]->getSelectedIndex());r.joint.swing2Motion=static_cast<physicsasset::AngularMotion>(m_motion[2]->getSelectedIndex());
        if(r.id!=oldId)
        {
            for(auto &child:value.roles) if(LowerId(child.parent)==LowerId(oldId)) child.parent=r.id;
            for(auto &pair:value.pairRules) {if(LowerId(pair.first)==LowerId(oldId)) pair.first=r.id;if(LowerId(pair.second)==LowerId(oldId)) pair.second=r.id;}
        }
    }
    std::string error;if(!ragtemplate::Validate(value,&error)) { Status(error);return false; }
    m_draft=std::move(value);m_custom=m_draft;m_hasCustom=true;return true;
}
void RagdollTemplateDialog::EditPhysicsSettings()
{
    if(m_builtin) {Status("Duplicate a built-in to edit its physics profile.");return;}
    if(!ReadFields()) return;
    if(m_settingsDialog&&m_settingsDialog->isVisible()) return;
    const std::string before=ragtemplate::Serialize(m_draft);
    physicsasset::AssetDef settings;settings.simulation=m_draft.simulation;settings.selfCollision=m_draft.selfCollision;settings.collisionPairs=m_draft.pairRules;
    for(const auto &role:m_draft.roles)
    {
        physicsasset::BodyDef b;b.name=b.bone=role.id;b.linearDamping=role.linearDamping;b.angularDamping=role.angularDamping;b.tuning=role.bodyTuning;settings.bodies.push_back(b);
        if(!role.parent.empty()) {physicsasset::JointDef j;j.name=role.id+"_joint";j.parent=role.parent;j.child=role.id;j.tuning=role.joint.tuning;settings.joints.push_back(j);}
    }
    auto apply=[this,before](const physicsasset::AssetDef &value)->bool
    {
        if(before!=ragtemplate::Serialize(m_draft)) {Status("Template changed. Reopen physics settings.");return false;}
        auto draft=m_draft;draft.simulation=value.simulation;draft.selfCollision=value.selfCollision;draft.pairRules=value.collisionPairs;
        for(auto &role:draft.roles)
        {
            const int body=physicsasset::FindBody(value,role.id.c_str());if(body<0) return false;
            role.linearDamping=value.bodies[body].linearDamping;role.angularDamping=value.bodies[body].angularDamping;role.bodyTuning=value.bodies[body].tuning;
            for(const auto &joint:value.joints) if(LowerId(joint.child)==LowerId(role.id)) role.joint.tuning=joint.tuning;
        }
        std::string error;if(!ragtemplate::Validate(draft,&error)) {Status(error);return false;}
        m_draft=m_custom=std::move(draft);m_hasCustom=true;Number(m_linear,m_draft.simulation.linearDamping);Number(m_angular,m_draft.simulation.angularDamping);
        m_editor.ClearGenerationPreview();m_previewKey.clear();m_apply->setEnabled(false);if(m_applySave)m_applySave->setEnabled(false);m_axisReviewed->setChecked(false);m_risksReviewed->setChecked(false);m_previewJoints->removeAll();m_previewJoints->setEnabled(false);
        Status("Updated physics profile and pairs. Generate a fresh preview before Apply.");return true;
    };
    if(m_settingsDialog) m_settingsDialog->Open(settings,std::move(apply));else m_settingsDialog=new PhysicsSettingsDialog(this,settings,true,std::move(apply));
}

void RagdollTemplateDialog::ImportTemplate()
{
    const char *chosen=mxGetOpenFileName(this,m_editor.m_modelPath.c_str(),"Ragdoll Templates (*.ragtemplate)\0*.ragtemplate\0\0");
    if(!chosen) return;
    const std::string path=chosen;ragtemplate::Template imported;std::string error;
    if(!ragtemplate::LoadFile(path.c_str(),imported,&error)) { Status(error);return; }
    m_draft=m_custom=std::move(imported);m_hasCustom=true;m_builtin=false;m_selectedTemplate=CustomCopyIndex();m_templates->select(CustomCopyIndex());ShowTemplate();Status("Imported template as an editable custom copy. Generate Preview checks its mapping.");
}
void RagdollTemplateDialog::ExportTemplate()
{
    if(!ReadFields()) return;
    const size_t slash=m_editor.m_modelPath.find_last_of("/\\");
    const std::string suggested=(slash==std::string::npos?"":m_editor.m_modelPath.substr(0,slash+1))+m_draft.id+".ragtemplate";
    const char *chosen=mxGetSaveFileName(this,suggested.c_str(),"Ragdoll Templates (*.ragtemplate)\0*.ragtemplate\0\0");if(!chosen) return;
    std::string path=chosen;std::string lower=path;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(lower.size()<12||lower.substr(lower.size()-12)!=".ragtemplate") path+=".ragtemplate";
    const bool overwrite=std::ifstream(path,std::ios::binary).good();
    if(overwrite&&mxMessageBox(this,("Replace template file?\n"+path).c_str(),"Save template",MX_MB_YESNO|MX_MB_QUESTION)!=0) return;
    std::string error;if(!ragtemplate::SaveFile(path.c_str(),m_draft,overwrite,&error)) { Status(error);return; }
    Status("Saved reusable template: "+path);
}
std::string RagdollTemplateDialog::PreviewKey() const
{
    return ragtemplate::Serialize(m_draft)+"\ndetail="+std::to_string(m_detail->getSelectedIndex())+";mode="+std::to_string(m_mode->getSelectedIndex());
}
bool RagdollTemplateDialog::GeneratePreview()
{
    if(!ReadFields()) return false;
    const int mode=m_mode->getSelectedIndex();if(mode<0||mode>1) { Status("Select Replace or Merge.");return false; }
    ragtemplate::Candidate candidate;std::string error;
    if(!m_editor.CollectTemplateBones(m_bones,error,m_draft.excludedSections)) {Status(error);return false;}
    if(!ragtemplate::Generate(m_draft,m_bones,m_detail->getSelectedIndex(),m_editor.m_asset,mode==0?ragtemplate::ComposeMode::ReplaceRagdoll:ragtemplate::ComposeMode::Merge,candidate,&error)) { Status(error);m_apply->setEnabled(false);if(m_applySave)m_applySave->setEnabled(false);return false; }
    m_editor.m_generation.Begin(m_editor.m_asset);
    if(!m_editor.m_generation.SetCandidate(candidate,&error)) { Status(error);return false; }
    m_editor.ClearGenerationPreview();m_editor.m_generationPreview.reset(new physicsasset::AssetDef(candidate.asset));m_previewKey=PreviewKey();m_apply->setEnabled(true);if(m_applySave)m_applySave->setEnabled(true);m_axisReviewed->setChecked(false);m_risksReviewed->setChecked(false);m_report->removeAll();
    m_previewJoints->removeAll();
    for(const auto &joint:candidate.asset.joints) m_previewJoints->add((joint.name+" : "+joint.parent+" -> "+joint.child).c_str());
    m_previewJoints->setEnabled(!candidate.asset.joints.empty());
    if(!candidate.asset.joints.empty()) {m_previewJoints->select(0);m_editor.SelectGenerationJoint(candidate.asset.joints[0].name);}
    m_report->add(("Axis reviews="+std::to_string(candidate.axisReviews)+"; chain reviews="+std::to_string(candidate.chainReviews)+"; mass-ratio warnings="+std::to_string(candidate.massRatioWarnings)+"; overlaps="+std::to_string(candidate.overlapWarnings)).c_str());
    m_report->add(("Bodies: +"+std::to_string(candidate.addedBodies)+" -"+std::to_string(candidate.removedBodies)+" reused="+std::to_string(candidate.reusedBodies)+"; joints: +"+std::to_string(candidate.addedJoints)+" -"+std::to_string(candidate.removedJoints)+"; collision sections="+std::to_string(candidate.preservedCollisions)+"; protected bodies="+std::to_string(candidate.protectedBodies)).c_str());
    for(const auto &map:candidate.mapping) m_report->add((map.role+" -> "+(map.bone.empty()?"(none)":map.bone)+" ["+map.source+"; confidence="+std::to_string(map.confidence)+"]").c_str());
    for(const auto &body:m_editor.m_asset.bodies) if(physicsasset::FindBody(candidate.asset,body.name.c_str())<0) m_report->add(("REMOVE body: "+body.name).c_str());
    for(const auto &body:candidate.asset.bodies)
    {
        const int old = physicsasset::FindBody(m_editor.m_asset,body.name.c_str());
        std::string action = "ADD body: ";
        if(old >= 0)
        {
            physicsasset::AssetDef a,b; a.bodies={m_editor.m_asset.bodies[old]}; b.bodies={body};
            action=physicsasset::Serialize(a)==physicsasset::Serialize(b)?"REUSE body: ":"UPDATE body: ";
        }
        m_report->add((action+body.name+" mass="+std::to_string(body.mass)).c_str());
    }
    for(const auto &joint:candidate.asset.joints)
        m_report->add(("Joint: "+joint.parent+" -> "+joint.child+" translation=locked; T/Y/Z="+physicsasset::AngularMotionName(joint.twistMotion)+"/"+physicsasset::AngularMotionName(joint.swing1Motion)+"/"+physicsasset::AngularMotionName(joint.swing2Motion)).c_str());
    for(const auto &note:candidate.observations) m_report->add(note.c_str());
    if(g_GlWindow) g_GlWindow->redraw();
    if(m_page){m_page->select(3);ShowPage();}
    Status("Candidate shown in the model overlay. Review mappings, diff and shapes; Simulate Preview is optional before Apply.");return true;
}
bool RagdollTemplateDialog::ApplyPreview(bool save)
{
    setVisible(true);if(m_page){m_page->select(3);ShowPage();}
    if(!ReadFields()) return false;
    if(m_previewKey.empty()||m_previewKey!=PreviewKey()||!m_editor.m_generation.HasCandidate()) { Status("Settings changed or no candidate: Generate Preview and review it before Apply.");return false; }
    const auto &candidate=m_editor.m_generation.Preview();
    if((candidate.axisReviews||candidate.chainReviews)&&!m_axisReviewed->isChecked()) {Status("Review inferred joint frames and expanded chain mappings, then check the review box before Apply.");return false;}
    if((candidate.massRatioWarnings||candidate.overlapWarnings)&&!m_risksReviewed->isChecked()) {Status("Inspect mass-ratio and overlap warnings in the report, then explicitly acknowledge their remaining risks before Apply.");return false;}
    const std::string prompt="Apply this reviewed candidate to the editor?\nMass-ratio warnings: "+std::to_string(candidate.massRatioWarnings)+"; overlaps: "+std::to_string(candidate.overlapWarnings)+".\nBodies removed: "+std::to_string(candidate.removedBodies)+"; added: "+std::to_string(candidate.addedBodies)+"; reused: "+std::to_string(candidate.reusedBodies)+".\nCollision and interactive sections are preserved.\nUnapplied body/joint panel fields are replaced. Undo Gen restores the previous asset.\nSave .phys writes the result to disk.";
    if(mxMessageBox(this,prompt.c_str(),"Apply ragdoll generation",MX_MB_YESNO|MX_MB_QUESTION)!=0) return false;
    std::string error;if(!m_editor.m_generation.Apply(m_editor.m_asset,&error)) { Status(error);return false; }
    m_editor.ClearGenerationPreview();m_editor.MarkDirty();m_editor.RefreshBodyList(0);m_editor.RefreshJointList(m_editor.m_asset.joints.empty()?-1:0);setVisible(false);
    if(m_editor.m_page){m_editor.m_page->select(0);m_editor.ShowPage();}
    if(save&&!m_editor.SaveAsset()){setVisible(true);Status("Candidate transferred to the editor, but saving failed. Correct the asset fields or save path, then Save .phys.");return false;}
    m_editor.UpdateStatus(save?"Template applied and .phys saved.":"Template applied to the asset editor. Save .phys writes it to disk.");
    return true;
}
int RagdollTemplateDialog::handleEvent(mxEvent *event)
{
    if(!event) return 1;
    if(event->event==mxEvent::Close) { Cancel();return 1; }
    if(event->event!=mxEvent::Action) return 1;
    switch(event->action)
    {
    case BeginnerGuide:
        if(!m_helpDialog)m_helpDialog=new PhysicsAssetHelpDialog(this);
        m_helpDialog->Open("Start here","First ragdoll walkthrough");break;
    case ContextTopic:m_contextGuide->SelectionChanged();break;
    case ContextDetails:m_contextGuide->ShowDetails(m_helpDialog);break;
    case SelectPreviewJoint:
        if(m_editor.m_generationPreview)
        {
            const int index=m_previewJoints->getSelectedIndex();
            const auto &joints=m_editor.m_generationPreview->joints;
            m_editor.SelectGenerationJoint(index>=0 && index<int(joints.size())?joints[index].name:std::string{});
        }
        break;
    case TextSize:
        physui::SetTextScale(physui::PercentFromChoice(m_textSize->getSelectedIndex()));
        physui::RefreshScaling(this);break;
    case Help:
        if(!m_helpDialog) m_helpDialog=new PhysicsAssetHelpDialog(this);
        m_helpDialog->Open("Generator");
        break;
    case SelectTemplate:
    {
        const int selected=m_templates->getSelectedIndex();
        if(!ReadFields()) { m_templates->select(m_selectedTemplate);break; }
        if(selected==CustomCopyIndex()&&!m_hasCustom) { m_templates->select(m_selectedTemplate);Status("Import or Duplicate a template first.");break; }
        if(selected<0||selected>CustomCopyIndex()) break;
        m_selectedTemplate=selected;m_builtin=selected<CustomCopyIndex();m_draft=m_builtin?ragtemplate::Builtins()[selected]:m_custom;ShowTemplate();break;
    }
    case PhysicsSettings: EditPhysicsSettings();break;
    case Import: ImportTemplate();break;
    case SaveAs: ExportTemplate();break;
    case Duplicate:
        if(!ReadFields()) break;
        m_draft.id="custom_copy";m_draft.name="Custom copy of "+m_draft.name.substr(0,80);m_custom=m_draft;m_hasCustom=true;m_builtin=false;m_selectedTemplate=CustomCopyIndex();m_templates->select(CustomCopyIndex());ShowTemplate();Status("Editable copy created. Set a unique ID/name and Save As to reuse it.");break;
    case Reset:
        if(mxMessageBox(this,"Discard current template edits and reset the selected built-in (Custom for an imported copy)?","Reset template",MX_MB_YESNO|MX_MB_QUESTION)!=0) break;
        if(!m_builtin) { m_selectedTemplate=2;m_builtin=true;m_templates->select(2); }
        m_draft=ragtemplate::Builtins()[m_selectedTemplate];ShowTemplate();break;
    case SelectRole:
    {
        const int requested=m_roles->getSelectedIndex();if(!ReadFields()) { m_roles->select(m_selectedRole);break; }
        ShowRole(requested);break;
    }
    case PickBone:
    {
        if(m_builtin) break;
        const int index=m_bonePicker->getSelectedIndex();
        if(index==0) m_override->setLabel("");else if(index>0&&index<=static_cast<int>(m_bones.size())) m_override->setLabel(m_bones[index-1].name.c_str());break;
    }
    case ApplyRole:
        if(m_builtin) { Status("Duplicate the built-in before editing roles.");break; }
        if(ReadFields()) { const int role=m_selectedRole;ShowTemplate();m_roles->select(role);ShowRole(role);Status("Custom role edits applied. Generate Preview checks the new mapping."); }break;
    case AddRole:
        if(m_builtin) { Status("Duplicate a built-in to add roles.");break; }
        if(ReadFields())
        {
            ragtemplate::Role role;unsigned suffix=static_cast<unsigned>(m_draft.roles.size());
            do { role.id="role_"+std::to_string(suffix++); }
            while(std::any_of(m_draft.roles.begin(),m_draft.roles.end(),[&](const ragtemplate::Role &old){return LowerId(old.id)==LowerId(role.id);}));
            role.parent=m_draft.roles.front().id;role.massRegion=m_draft.roles.front().massRegion;role.aliases={role.id};role.required=false;
            auto next=m_draft;next.roles.push_back(role);std::string error;
            if(!ragtemplate::Validate(next,&error)) { Status(error);break; }
            m_draft=m_custom=next;ShowTemplate();m_roles->select(static_cast<int>(next.roles.size())-1);ShowRole(static_cast<int>(next.roles.size())-1);
        }break;
    case RemoveRole:
        if(m_builtin) { Status("Duplicate a built-in to remove roles.");break; }
        if(ReadFields()&&m_selectedRole>=0)
        {
            auto next=m_draft;const auto removed=next.roles[m_selectedRole].id;next.roles.erase(next.roles.begin()+m_selectedRole);
            next.pairRules.erase(std::remove_if(next.pairRules.begin(),next.pairRules.end(),[&](const auto &p){return LowerId(p.first)==LowerId(removed)||LowerId(p.second)==LowerId(removed);}),next.pairRules.end());std::string error;
            if(!ragtemplate::Validate(next,&error)) { Status("Cannot remove role: "+error+". Update its children first.");break; }
            m_draft=m_custom=next;ShowTemplate();
        }break;
    case Generate: GeneratePreview();break;
    case Simulate: if((ReadFields()&&!m_previewKey.empty()&&m_previewKey==PreviewKey())||GeneratePreview()) { m_editor.StartSimulation();Status(m_editor.IsSimulating()?"Candidate simulation running. F0 diagnostics can record it; Apply commits editor data, Cancel restores authoring view.":"Candidate is reviewable; live simulation needs a PhysX-enabled pxmv build."); }break;
    case Apply: ApplyPreview();break;
    case ApplyAndSave:ApplyPreview(true);break;
    case ChangePage:case AdvancedMode:ShowPage();break;
    case PreviewMode:m_editor.SetSimulationMode(m_simulationModeChoice->getSelectedIndex()==1
        ?PhysicsAssetEditor::SimulationMode::PartialRagdoll:PhysicsAssetEditor::SimulationMode::FullRagdoll);break;
    case Close: Cancel();break;
    }
    return 1;
}
