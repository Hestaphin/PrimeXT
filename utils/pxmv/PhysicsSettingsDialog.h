#pragma once
#include <mx/mxWindow.h>
#include "physics_asset_format.h"
#include <functional>
class PhysicsAssetContextGuide;class mxChoice;class mxLineEdit;class mxCheckBox;class mxListBox;class mxLabel;class PhysicsAssetHelpDialog;
class PhysicsSettingsDialog:public mxWindow
{
public:
    PhysicsSettingsDialog(mxWindow *,const physicsasset::AssetDef &,bool,std::function<bool(const physicsasset::AssetDef &)>);
    ~PhysicsSettingsDialog() override;
    int handleEvent(mxEvent *) override;
    void Open(const physicsasset::AssetDef &,std::function<bool(const physicsasset::AssetDef &)>);
    void Cancel();
private:
    enum {Scope=8701,Target,Inherit,AllowPair,DenyPair,DefaultPair,Apply,Close,Help,ContextTopic,ContextDetails,TextSize};
    void ShowScope();void ShowTarget();bool ReadFields();void Pair(bool,bool);void RefreshPairs();void ApplySettings();
    PhysicsAssetHelpDialog *m_helpDialog=nullptr;
    PhysicsAssetContextGuide *m_contextGuide=nullptr;mxChoice *m_textSize=nullptr;
    physicsasset::AssetDef m_draft;bool m_templateRules;std::function<bool(const physicsasset::AssetDef &)> m_apply;
    int m_scopeIndex=0,m_targetIndex=0;
    mxChoice *m_scope=nullptr,*m_target=nullptr,*m_projection=nullptr,*m_self=nullptr,*m_first=nullptr,*m_second=nullptr;
    mxCheckBox *m_inherit=nullptr,*m_ccd=nullptr;mxLineEdit *m_values[14]{};mxListBox *m_pairs=nullptr;mxLabel *m_status=nullptr;
};
