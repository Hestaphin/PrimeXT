#!/usr/bin/env python3
"""Run actual pxmv mode/start/stop methods with a recording scene-build service.

The service can succeed or fail without a PhysX SDK; no solver is exercised.
The inherited GUI/engine services and actual capture/release code are retained.
"""
from pathlib import Path
import argparse, sys
import ragdoll_template_dialog_integration as baseline

CASES = r'''
    using PreviewMode=PhysicsAssetEditor::SimulationMode;
    const int previousPanelApplies=panelApplies;
    const auto savedAsset=physicsasset::Serialize(editor.m_asset);
    const bool savedDirty=editor.m_dirty;
    recordingPreviewBuildSuccess=true;
    editor.SetSimulationMode(PreviewMode::PartialRagdoll);
    editor.StartSimulation();
    Check(editor.IsSimulating()&&!editor.PausesAnimationPlayback()&&!model.m_toolAnimationFrozen,"successful Partial start permits animation and leaves capture released");
    const int buildsAtStart=recordingPreviewBuildCalls;
    editor.SetSimulationMode(PreviewMode::FullRagdoll);
    Check(editor.IsSimulating()&&editor.PausesAnimationPlayback()&&model.m_toolAnimationFrozen,"running mode switch enables full pose ownership");
    editor.SetSimulationMode(PreviewMode::PartialRagdoll);
    Check(recordingPreviewBuildCalls==buildsAtStart&&editor.IsSimulating(),"switching mode does not rebuild or restart the physical scene");
    editor.StopSimulation();
    editor.SetSimulationMode(PreviewMode::FullRagdoll);
    editor.StartSimulation();
    Check(editor.IsSimulating()&&editor.PausesAnimationPlayback()&&model.m_toolAnimationFrozen,"successful Full start captures all animation bones");
    editor.StopSimulation();
    Check(!editor.PausesAnimationPlayback()&&!model.m_toolAnimationFrozen,"Stop releases full pose ownership");
    recordingPreviewBuildSuccess=false;
    editor.StartSimulation();
    Check(!editor.IsSimulating()&&!model.m_toolAnimationFrozen,"failed scene build cannot freeze animation or claim simulation");
    Check(physicsasset::Serialize(editor.m_asset)==savedAsset&&editor.m_dirty==savedDirty,"preview mode and scene services never modify authored physics data");
    panelApplies=previousPanelApplies;
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan',action='store_true')
    args=parser.parse_args()
    original_function=baseline.function
    original_read=Path.read_text
    def extract(source, signature):
        if signature=='bool PhysicsAssetEditor::RebuildPreview':
            return '''static bool recordingPreviewBuildSuccess=false;
static int recordingPreviewBuildCalls=0;
bool PhysicsAssetEditor::RebuildPreview(){++recordingPreviewBuildCalls;return recordingPreviewBuildSuccess;}'''
        return original_function(source, signature)
    def read(path,*a,**kw):
        source=original_read(path,*a,**kw)
        if path.name=='template_dialog.cpp.in':
            marker='    const std::string builtin=ragtemplate::Serialize(ragtemplate::Builtins()[0]);'
            assert marker in source
            source=source.replace(marker,CASES+'\n'+marker)
            source=source.replace('F1 actual dialog/editor methods:','K.11.4 recording scene/start/stop and inherited dialog methods:')
        return source
    argv=sys.argv
    baseline.function=extract
    Path.read_text=read
    sys.argv=[__file__,*(['--ubsan'] if args.ubsan else [])]
    try:
        return baseline.main()
    finally:
        sys.argv=argv
        Path.read_text=original_read
        baseline.function=original_function

if __name__=='__main__':raise SystemExit(main())
