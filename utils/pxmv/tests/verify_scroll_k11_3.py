#!/usr/bin/env python3
"""Compile actual Win32 scaler functions with a recording window API (no native GUI)."""
from pathlib import Path
import os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'game_shared/tests'))
from ragdoll_f0_integration import function
source=(ROOT/'utils/pxmv/PhysicsAssetUiGuide.cpp').read_text()
chunks=[function(source,s) for s in ['struct NativeItem','struct NativeScaler','static int ApplyDpi','static void ResizeNative','static LRESULT CALLBACK ScalerProc']]
# The generic brace extractor omits the struct terminator.
chunks=[c+';' if c.startswith('struct ') else c for c in chunks]
unit=Path(__file__).with_name('physics_asset_scroll_k11_3.cpp.in').read_text().replace('@PRODUCTION@','\n'.join(chunks))
with tempfile.TemporaryDirectory(prefix='k113-scroll-') as tmp:
 p=Path(tmp);(p/'scroll.cpp').write_text(unit)
 command=[os.environ.get('CXX','c++'),'-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all',str(p/'scroll.cpp'),'-o',str(p/'scroll')]
 r=subprocess.run(command,capture_output=True,text=True);print(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(r.returncode)
 r=subprocess.run([str(p/'scroll')],capture_output=True,text=True);print(r.stdout+r.stderr);raise SystemExit(r.returncode)
