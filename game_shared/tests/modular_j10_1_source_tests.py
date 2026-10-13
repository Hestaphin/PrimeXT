from pathlib import Path
root=Path(__file__).resolve().parents[2]
manager=(root/'server/modular_ballistics.cpp').read_text()
cache=(root/'game_shared/ballistic_path_cache.cpp').read_text()
assert 'shot.lastSimulatedTime=gpGlobals->time' in manager
assert 'ShotTimeBudget(now,shot.lastSimulatedTime)' in manager
assert 'AdvanceTransit(shot.state,shot.transit' in manager
assert 'if (shot.transit.active)' in manager
assert 'd.transitActive=1' in manager
assert 'm_savedRecordVersion=kShotDiskVersion' in manager
assert 'm_savedRecordVersion!=kShotDiskVersion' in manager
assert 'modular::PlausibleBallisticEnergy(s,p,initialEnergy)' in manager
assert 'clipped.meters=range;' in cache
assert 'std::lower_bound' in cache
print('PASS J.10.1: manager/cache integration checks')
