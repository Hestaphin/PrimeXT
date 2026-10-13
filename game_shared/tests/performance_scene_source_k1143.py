#!/usr/bin/env python3
"""Protect runtime/editor contact-cache parity and simulation scratch plumbing."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
runtime=(root/'server/physx/physx_impl.cpp').read_text()
editor=(root/'utils/pxmv/PhysicsAssetEditor.cpp').read_text()
for text in (runtime,editor):
    assert 'sceneDesc.flags |= PxSceneFlag::eENABLE_CCD;' in text
    assert 'sceneDesc.flags = PxSceneFlag::eENABLE_CCD;' not in text
assert 'm_simulationScratch ? static_cast<PxU32>(performance::kPhysXScratchBytes) : 0' in runtime
assert 'm_pScene->fetchResults(true)' in runtime
print('K.11.4.3 runtime/editor retain PCM+CCD defaults; synchronous scratch lifecycle wired PASS (source check)')
