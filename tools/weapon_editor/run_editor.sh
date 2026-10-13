#!/bin/sh
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ ! -x "$DIR/build/pxwe" ]; then
  cmake -S "$DIR" -B "$DIR/build"
  cmake --build "$DIR/build" --parallel 2 --target pxwe
fi
exec "$DIR/build/pxwe" "$@"
