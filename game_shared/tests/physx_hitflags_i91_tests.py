#!/usr/bin/env python3
"""I.9.1 source regression: PhysX 4.1 does not expose eDISTANCE.

This is a source-only check, NOT a substitute for compiling the PhysX server.
Run: python3 game_shared/tests/physx_hitflags_i91_tests.py
"""

from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
locations = {
    "server/physx/physics_asset.cpp": 1,
    "server/physx/physx_impl.cpp": 2,
}
for filename, expected_count in locations.items():
    source = (root / filename).read_text(encoding="utf-8")
    assert "PxHitFlag::eDISTANCE" not in source, filename
    requests = re.findall(r"const\s+PxHitFlags\s+flags\s*=\s*([^;]+);", source)
    matches = [
        expr for expr in requests
        if "PxHitFlag::ePOSITION" in expr and "PxHitFlag::eNORMAL" in expr
    ]
    assert len(matches) >= expected_count, (filename, matches)
    assert len(matches) == len(requests), (filename, requests)
    assert all("PxHitFlag::eDISTANCE" not in expr for expr in matches)
    print(f"PASS {filename}: {len(matches)} valid position/normal raycast flag sets")
print("Milestone I.9.1 hit-flag source regression PASS (SDK build not checked)")
