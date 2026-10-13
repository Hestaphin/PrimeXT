#!/usr/bin/env python3
"""Execute actual toolkit methods with explicit Qt/Win32 service adapters."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
from ragdoll_f0_integration import function

ROOT=Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ubsan',action='store_true');args=parser.parse_args()
    toolkit=ROOT/'external/mxtk'
    label=function((toolkit/'src/qt/mxWidget.cpp').read_text(),'const char*\nmxWidget::getLabel')
    close=function((toolkit/'src/qt/mxWindow_p.h').read_text(),'virtual void closeEvent')
    windows=(toolkit/'src/win32/mx.cpp').read_text();start=windows.index('\tcase WM_CLOSE:');end=windows.index('\n/*',start)
    source=(ROOT/'game_shared/tests/fixtures/j10_6_10_1/toolkit_methods.cpp.in').read_text()
    source=source.replace('@LABEL@',label).replace('@QT_CLOSE@',close).replace('@WIN_CLOSE@',windows[start:end])
    with tempfile.TemporaryDirectory(prefix='f1_toolkit_') as tmp:
        tmp=Path(tmp);unit=tmp/'toolkit.cpp';unit.write_text(source);binary=tmp/'toolkit'
        flags=['-std=c++17','-O0','-g','-fno-exceptions','-Wall','-Wextra','-Werror','-Wpedantic','-I'+str(toolkit/'include')]
        if args.ubsan: flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
        for command in [[os.environ.get('CXX','c++'),*flags,str(unit),'-o',str(binary)],[str(binary)]]:
            run=subprocess.run(command,text=True,capture_output=True,cwd=ROOT)
            if run.stdout or run.stderr: print((run.stdout+run.stderr)[-6000:])
            if run.returncode: return run.returncode
    return 0

if __name__=='__main__': raise SystemExit(main())
