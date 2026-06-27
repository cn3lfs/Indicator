#!/usr/bin/env sh
set -eu

# 先清理，避免上次 mingw32 残留的 MinGW 目标文件被原生 make test 复用而链接失败
make clean
make check-mingw32
make test
make mingw32-test-build
make mingw32
