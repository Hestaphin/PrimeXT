#!/usr/bin/env python3
"""Standalone launcher. Requires Python 3.10+ with Tcl/Tk installed."""
import sys
from pathlib import Path
from app import main
if __name__ == '__main__':
    main(Path(sys.argv[1]) if len(sys.argv)>1 else None)
