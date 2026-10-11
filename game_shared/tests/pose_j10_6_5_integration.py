#!/usr/bin/env python3
"""Run actual packet/CPU/draw methods with the J.10.6.5 timing regressions."""
from pathlib import Path
import argparse
import os
from pose_j10_6_2_integration import main as run_pose

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan',action='store_true')
    parser.add_argument('--case',choices=['delayed_clear','draw_clock','packet_clock','invalid_snapshot','implicit_capture','reentry','shared_cache','fallback_capture'])
    args=parser.parse_args()
    if args.case:os.environ['J1065_POSE_CASE']=args.case
    run_pose(Path(__file__).parent/'fixtures/j10_6_5/pose_cases.cpp.in',args.ubsan)

if __name__=='__main__':main()
