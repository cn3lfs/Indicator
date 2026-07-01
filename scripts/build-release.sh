#!/usr/bin/env sh
set -eu

sh scripts/build-mingw32.sh
sh scripts/build-mingw64.sh
make release-check
