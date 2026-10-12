from pathlib import Path
root = Path(__file__).resolve().parents[2]
cpp = (root / 'utils/pxmv/PhysicsAssetEditor.cpp').read_text()
fmt = (root / 'game_shared/physics_asset_format.cpp').read_text()
sidecar = (root / 'game_shared/physics_asset_sidecar_removal.h').read_text()
save = cpp.split('bool PhysicsAssetEditor::SaveAsset()', 1)[1].split('void PhysicsAssetEditor::ReloadAsset()', 1)[0]
clear = cpp.split('void PhysicsAssetEditor::ClearCollisionGeometry()', 1)[1].split('bool PhysicsAssetEditor::Validate(', 1)[0]
assert 'm_asset.collisions.clear()' in clear
assert 'Save will ask to DELETE' in clear
assert 'm_asset.bodies.empty() && m_asset.joints.empty() && m_asset.collisions.empty()' in save
assert 'MX_MB_YESNO' in save and 'MX_MB_WARNING' in save
assert 'RemoveEmptyAssetSidecar(m_assetPath, m_asset, removed, &error)' in save
assert save.index('RemoveEmptyAssetSidecar(') < save.index('if(!Validate(error))')
assert 'if(!physicsasset::SaveFile(' in save and 'if(!Validate(error))' in save
assert 'if(removed)' in save and 'm_dirty = false;' in save
assert 'return false;' in save.split('"Empty .phys removal cancelled', 1)[1].split('StopSimulation()', 1)[0]
assert 'std::filesystem::is_regular_file(status)' in sidecar
assert 'std::filesystem::remove(path, ec)' in sidecar
assert 'asset.joints.empty()' in sidecar and 'asset.collisions.empty()' in sidecar
assert 'physics asset has neither collision geometry nor articulated bodies' in fmt
print('I.10.8 editor confirmation/removal and nonempty serialization wiring PASS')
