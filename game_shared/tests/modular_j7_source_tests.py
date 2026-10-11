"""J.7 merged wiring checks; not a substitute for compiling/running Xash."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
def read(name):
    return (ROOT / name).read_text()

def main():
    physics = read("server/physx/physx_impl.cpp")
    bullet = read("server/modular_ballistics.cpp")
    weapons = read("server/weapons.cpp")
    client = read("client/weapon_predicting_context.cpp")
    registry = read("game_shared/modular_weapons.cpp")
    editor = read("tools/weapon_editor/CMakeLists.txt")
    server_build = read("server/CMakeLists.txt")
    client_build = read("client/CMakeLists.txt")
    # Preserve I.10.10 primitive/hinged child sweep owner handling.
    assert "SelectMovementSweepOwners" in physics
    assert "physics_asset_trace_owner.h" in physics
    # J.6 projectiles still use the collision resolver and material/decal path.
    assert "FTRACE_MATERIAL_TRACE | FTRACE_BULLET_PHYSICS" in bullet
    assert "ResolveRagdollBulletCollision" in bullet
    assert "ResolveBulletImpactEffects" in bullet
    assert "ApplyPhysicsBulletImpulse" in bullet
    assert "gpGlobals->trace_flags = previousTraceFlags" in bullet
    assert "ResolveRagdollBulletCollision" in weapons
    # Client/server share weapon definitions, without embedding the editor in pxmv.
    assert "modular::SharedRegistry()" in client
    assert "Registry::" in registry
    assert "modular_ballistics.cpp" in server_build
    assert "modular_npc.cpp" in server_build
    assert 'game_shared/modular_weapons.cpp' in server_build
    assert 'game_shared/modular_weapons.cpp' in client_build
    assert 'pxmv' in editor and 'no pxmv' in editor
    print("PASS J.7 source integration: physics, projectiles, shared registry, standalone editor")

if __name__ == "__main__":
    main()
