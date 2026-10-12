#pragma once

#include <mx/mxWindow.h>
#include "physics_asset_format.h"
#include "ragdoll_diagnostics.h"
#include "ragdoll_template.h"

#include <memory>
#include <string>
#include <vector>

class StudioModel;
class ViewerSettings;
class mxListBox;
class mxChoice;
class mxLineEdit;
class mxLabel;
class mxButton;
class RagdollTemplateDialog;
class PhysicsSettingsDialog;
class PhysicsAssetHelpDialog;
class PhysicsAssetContextGuide;

class PhysicsAssetEditor : public mxWindow
{
public:
    enum
    {
        IDC_PHYS_BODY_LIST = 8101,
        IDC_PHYS_BODY_ADD,
        IDC_PHYS_BODY_REMOVE,
        IDC_PHYS_BODY_APPLY,
        IDC_PHYS_JOINT_LIST,
        IDC_PHYS_JOINT_ADD,
        IDC_PHYS_JOINT_REMOVE,
        IDC_PHYS_JOINT_APPLY,
        IDC_PHYS_AUTOGENERATE,
        IDC_PHYS_SAVE,
        IDC_PHYS_RELOAD,
        IDC_PHYS_SIMULATE,
        IDC_PHYS_RESET,
        IDC_PHYS_IMPORT_CP,
        IDC_PHYS_CLEAR_COLLISION,
        IDC_PHYS_BONE,
        IDC_PHYS_SHAPE,
        IDC_PHYS_JOINT_PARENT,
        IDC_PHYS_JOINT_CHILD,
        IDC_PHYS_MAKE_PART,
        IDC_PHYS_MAKE_HINGE,
        IDC_PHYS_SCALE_ASSET,
        IDC_PHYS_DIAGNOSTICS,
        IDC_PHYS_SAVE_DIAGNOSTICS,
        IDC_PHYS_UNDO_GENERATION,
        IDC_PHYS_SETTINGS,
        IDC_PHYS_JOINT_GUIDES,
        IDC_PHYS_HELP,
        IDC_PHYS_CONTEXT_TOPIC,
        IDC_PHYS_CONTEXT_DETAILS,
        IDC_PHYS_JOINT_CONTEXT_TOPIC,
        IDC_PHYS_JOINT_CONTEXT_DETAILS,
        IDC_PHYS_BEGINNER_GUIDE,
        IDC_PHYS_TEXT_SIZE,
    };

    PhysicsAssetEditor(mxWindow *parent, StudioModel &model, ViewerSettings &settings);
    ~PhysicsAssetEditor() override;

    int handleEvent(mxEvent *event) override;
    void OnModelLoaded(const char *modelPath);
    void Update(float dt);
    void DrawOverlay();
    void StopSimulation();
    // Candidate-only joint identity, independent of the existing asset indices.
    void SelectGenerationJoint(const std::string &jointName);

    bool HasAsset() const { return !m_asset.bodies.empty() || !m_asset.collisions.empty(); }
    bool IsSimulating() const { return m_simulating; }
    const std::string &AssetPath() const { return m_assetPath; }

private:
    friend class RagdollTemplateDialog;
    PhysicsSettingsDialog *m_settingsDialog=nullptr;
    PhysicsAssetHelpDialog *m_helpDialog=nullptr;
    PhysicsAssetContextGuide *m_contextGuide=nullptr;
    PhysicsAssetContextGuide *m_jointGuide=nullptr;
    mxChoice *m_textSize=nullptr;
    struct PreviewImpl;

    void BuildUI();
    void EditPhysicsSettings();
    void PopulateBones();
    void RefreshBodyList(int select = -1);
    void RefreshJointList(int select = -1);
    void RefreshBodySelectors();
    void ShowBody(int index);
    void ShowJoint(int index);
    bool ApplyBodyFields();
    bool ApplyJointFields();
    void MakeInteractivePart();
    void MakeHinge();
    void AddBody();
    void RemoveBody();
    void AddJoint();
    void RemoveJoint();
    void AutoGenerate();
    void UndoGeneration();
    bool CollectTemplateBones(std::vector<ragtemplate::Bone> &, std::string &, const std::vector<std::string> &excludedSections = {});
    void ClearGenerationPreview();
    const physicsasset::AssetDef &ActivePreviewAsset() const { return m_generationPreview ? *m_generationPreview : m_asset; }
    void ImportLegacyCollisionProxy();
    void ClearCollisionGeometry();
    void ScaleAuthoredGeometry();
    bool SaveAsset();
    void ReloadAsset();
    bool Validate(std::string &error) const;
    int FindBone(const char *name) const;
    int FindBoneBody(int boneIndex) const;
    int FindNearestParentBody(int boneIndex) const;
    void StartSimulation();
    bool RebuildPreview();
    void SyncSimulationPoseToModel();
    void ToggleDiagnostics();
    void BeginDiagnosticCapture();
    void RecordPreviewDiagnostics();
    void SaveDiagnostics();
    void UpdateStatus(const char *message);
    void MarkDirty();

    StudioModel &m_model;
    ViewerSettings &m_settings;
    physicsasset::AssetDef m_asset;
    std::string m_modelPath;
    std::string m_assetPath;
    bool m_dirty = false;
    bool m_simulating = false;
    RagdollTemplateDialog *m_templateDialog = nullptr; // owned by mxWindow child list
    ragtemplate::Transaction m_generation;
    std::unique_ptr<physicsasset::AssetDef> m_generationPreview;
    std::string m_generationSelectedJoint;
    int m_selectedBody = -1;
    int m_selectedJoint = -1;

    mxListBox *m_bodyList = nullptr;
    mxListBox *m_jointList = nullptr;
    mxChoice *m_boneChoice = nullptr;
    mxChoice *m_shapeChoice = nullptr;
    mxChoice *m_jointParent = nullptr;
    mxChoice *m_jointChild = nullptr;
    mxChoice *m_jointMotion[3]{};
    mxLineEdit *m_bodyName = nullptr;
    mxLineEdit *m_offset[3]{};
    mxLineEdit *m_angles[3]{};
    mxLineEdit *m_dimensions[3]{};
    mxLineEdit *m_mass = nullptr;
    mxLineEdit *m_linearDamping = nullptr;
    mxLineEdit *m_angularDamping = nullptr;
    mxLineEdit *m_hitGroup = nullptr;
    mxLineEdit *m_partName = nullptr;
    mxChoice *m_propDriveChoice = nullptr;
    mxLineEdit *m_driveStrength = nullptr;
    mxLineEdit *m_driveDamping = nullptr;
    mxLineEdit *m_driveMaxForce = nullptr;
    mxLabel *m_dimensionLabels[3]{};
    mxLineEdit *m_geometryScale = nullptr;
    mxLineEdit *m_jointName = nullptr;
    mxLineEdit *m_swing1 = nullptr;
    mxLineEdit *m_swing2 = nullptr;
    mxLineEdit *m_twistLow = nullptr;
    mxLineEdit *m_twistHigh = nullptr;
    mxChoice *m_jointKindChoice = nullptr;
    mxLineEdit *m_hingeMin = nullptr;
    mxLineEdit *m_hingeMax = nullptr;
    mxLineEdit *m_frameAnchor[3]{};
    mxLineEdit *m_frameAngles[3]{};
    mxLineEdit *m_hingeAnchor[3]{};
    mxLineEdit *m_hingeAxis[3]{};
    mxLineEdit *m_releaseDamage = nullptr;
    mxLineEdit *m_breakDamage = nullptr;
    mxLineEdit *m_breakImpulse = nullptr;
    mxLabel *m_status = nullptr;
    mxButton *m_simulateButton = nullptr;
    mxButton *m_diagnosticsButton = nullptr;
    bool m_diagnosticsEnabled = false;
    bool m_showJointGuides = true;
    mxButton *m_jointGuidesButton = nullptr;
    bool m_diagnosticsReportDue = false;
    std::string m_diagnosticsMetadata;
    std::vector<std::string> m_diagnosticsRows;
    unsigned m_diagnosticsDroppedRows = 0;
    ragdolldiag::Snapshot m_lastDiagnostics;

    std::unique_ptr<PreviewImpl> m_preview;
};
