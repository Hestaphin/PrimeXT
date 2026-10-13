#!/usr/bin/env python3
"""J.10.6.5 production pose regressions plus inherited portable acceptance.

Actual PhysX SDK links and engine/live-map acceptance remain separate.
"""
from pathlib import Path
import argparse
import subprocess
import sys

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan',action='store_true')
    args=parser.parse_args()
    tests=Path(__file__).resolve().parent
    flags=['--ubsan'] if args.ubsan else []
    for name in ['pose_j10_6_5_integration.py','verify_j10_6_3.py']:
        result=subprocess.run([sys.executable,str(tests/name),*flags],cwd=tests.parents[1])
        if result.returncode:return result.returncode
    print('J.10.6.5 production pose regressions and inherited acceptance passed',flush=True)
    return 0

if __name__=='__main__':raise SystemExit(main())
