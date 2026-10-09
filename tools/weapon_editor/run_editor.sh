#!/bin/sh
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ ! -x "$DIR/build/primext-weapon-validate" ]; then
  cmake -S "$DIR" -B "$DIR/build"
  cmake --build "$DIR/build" --parallel 2
fi
exec python3 "$DIR/launch.py" "$@"
