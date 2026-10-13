#pragma once
#include <mx/mxWindow.h>
#include "ragdoll_template.h"
#include "PhysicsAssetPages.h"
class PhysicsAssetEditor;
class PhysicsSettingsDialog;
class PhysicsAssetHelpDialog;
class PhysicsAssetContextGuide;
class mxChoice;
class mxLineEdit;
class mxListBox;
class mxCheckBox;
class mxLabel;
class mxButton;
class RagdollTemplateDialog : public mxWindow
{
public:
    explicit RagdollTemplateDialog(PhysicsAssetEditor &);
    ~RagdollTemplateDialog() override;
    int handleEvent(mxEvent *) override;
    void Open();
    void Cancel();
private:
    friend class PhysicsAssetEditor;
    enum { SelectTemplate=8401, Import, SaveAs, Duplicate, Reset, SelectRole, AddRole, RemoveRole, ApplyRole, Generate, Simulate, Apply, Close, PickBone, PhysicsSettings, Help, ContextTopic, ContextDetails, TextSize, BeginnerGuide, SelectPreviewJoint, ChangePage, AdvancedMode, ApplyAndSave, PreviewMode };
    void BuildUI();
    void EditPhysicsSettings();
    void ShowTemplate();
    void ShowRole(int);
    bool ReadFields();
    void ImportTemplate();
    void ExportTemplate();
    bool GeneratePreview();
    bool ApplyPreview(bool save=false);
    void ShowPage();
    void SyncSimulationMode();
    static int CustomCopyIndex() { return static_cast<int>(ragtemplate::Builtins().size()); }
    std::string PreviewKey() const;
    void Status(const std::string &);
    PhysicsSettingsDialog *m_settingsDialog=nullptr;
    PhysicsAssetHelpDialog *m_helpDialog=nullptr;
    PhysicsAssetContextGuide *m_contextGuide=nullptr;
    mxChoice *m_textSize=nullptr;
    mxChoice *m_page=nullptr;
    mxCheckBox *m_advanced=nullptr;
    mxButton *m_applySave=nullptr;
    PhysicsAssetPages m_pages;
    PhysicsAssetEditor &m_editor;
    ragtemplate::Template m_draft, m_custom;
    std::vector<ragtemplate::Bone> m_bones;
    bool m_builtin = true, m_hasCustom = false;
    int m_selectedRole = -1, m_selectedTemplate = 0;
    std::string m_previewKey;
    mxChoice *m_templates=nullptr, *m_detail=nullptr, *m_mode=nullptr, *m_shape=nullptr, *m_roleDetail=nullptr, *m_bonePicker=nullptr;
    mxChoice *m_frameMode=nullptr;
    mxChoice *m_motion[3]{};
    mxLineEdit *m_id=nullptr, *m_name=nullptr, *m_description=nullptr, *m_anatomy=nullptr;
    mxLineEdit *m_budget=nullptr, *m_linear=nullptr, *m_angular=nullptr, *m_roleId=nullptr, *m_parent=nullptr, *m_aliases=nullptr, *m_override=nullptr;
    mxLineEdit *m_templateAnchor[3]{},*m_templateAngles[3]{};
    mxLineEdit *m_region=nullptr,*m_exclusions=nullptr,*m_chainExclusions=nullptr,*m_sign=nullptr,*m_radiusRatio=nullptr,*m_trim=nullptr,*m_massRatio=nullptr;
    mxLineEdit *m_weight=nullptr, *m_hit=nullptr, *m_limits[4]{};
    mxListBox *m_roles=nullptr, *m_report=nullptr;
    mxChoice *m_previewJoints=nullptr;
    mxChoice *m_simulationModeChoice=nullptr;
    mxCheckBox *m_required=nullptr,*m_axisReviewed=nullptr,*m_risksReviewed=nullptr,*m_chainBranches=nullptr;
    mxLineEdit *m_chainDepth=nullptr;
    mxLabel *m_status=nullptr;
    mxLabel *m_constraintBasicNote=nullptr;
    mxButton *m_apply=nullptr;
};
