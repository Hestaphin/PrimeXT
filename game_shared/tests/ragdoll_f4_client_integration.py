#!/usr/bin/env python3
"""Test F4 through actual packet, CPU prediction/draw, animation, IK and math code."""
from pathlib import Path
import argparse
import pose_j10_6_2_integration as pose
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ubsan',action='store_true');a=p.parse_args()
 pose.main(Path(__file__).resolve().parent/'fixtures/j10_6_10_4/client_pose.cpp.in',a.ubsan)
 return 0
if __name__=='__main__':raise SystemExit(main())
