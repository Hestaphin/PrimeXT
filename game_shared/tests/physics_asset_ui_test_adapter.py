"""GUI service adapters shared by old and current physics authoring harnesses.

Production generator, settings, fitting and transaction functions remain intact.
Contextual help/focus/scroll services are tested separately in utils/pxmv/tests.
"""
MOCK = 'namespace physui {\nstatic int mockScale=125;\nint CurrentTextScale(){return mockScale;}\nvoid SetTextScale(int value){mockScale=value;}\nint PercentFromChoice(int index){return index==0?100:index==2?150:125;}\nint ChoiceFromPercent(int p){return p==100?0:p==150?2:1;}\nvoid InstallScrollableScaling(mxWindow*,int,int){}\nvoid RefreshScaling(mxWindow*){}\nvoid BindFocus(mxWidget*,void*,void(*)(void*)){}\n}\nPhysicsAssetContextGuide::PhysicsAssetContextGuide(mxWindow *p,int y,int w,const char *section,int action,int)\n :m_section(section),m_parent(p) {\n    m_controls.push_back(new mxLabel(p,10,y+3,145,20,"Parameter guidance"));\n    m_choice=new mxChoice(p,155,y,w-320,24,action);\n    m_controls.push_back(new mxButton(p,w-155,y,145,25,"Full explanation",0));\n    m_note=new mxLabel(p,10,y+32,w-20,28,"");\n    m_controls.push_back(m_choice);m_controls.push_back(m_note);\n }\nPhysicsAssetContextGuide::~PhysicsAssetContextGuide(){}\nvoid PhysicsAssetContextGuide::Select(const char*){}\nvoid PhysicsAssetContextGuide::SelectionChanged(){}\nvoid PhysicsAssetContextGuide::ShowDetails(PhysicsAssetHelpDialog *&){}\nvoid PhysicsAssetContextGuide::Bind(mxWidget*,const char*){}\nvoid PhysicsAssetContextGuide::FocusField(void*){}\nPhysicsAssetHelpDialog::PhysicsAssetHelpDialog(mxWindow *p):mxWindow(p,0,0,100,100,"help",mxWindow::Dialog){}\nvoid PhysicsAssetHelpDialog::Open(const std::string &,const std::string &){}\nint PhysicsAssetHelpDialog::handleEvent(mxEvent*){return 1;}\n'

def adapt(source):
    headers = '#include "PhysicsAssetHelpDialog.h"\n#include "PhysicsAssetUiGuide.h"\n'
    source = headers + source
    if 'PhysicsAssetContextGuide::PhysicsAssetContextGuide(' not in source:
        source += '\n' + MOCK
    if 'void PhysicsAssetEditor::SelectGenerationJoint(' not in source:
        source += '\nvoid PhysicsAssetEditor::SelectGenerationJoint(const std::string &name){m_generationSelectedJoint=name;}\n'
    return source
