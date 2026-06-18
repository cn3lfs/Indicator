#!/usr/bin/env sh
set -eu

make check-mingw32
make test
make mingw32-test-build
make mingw32
