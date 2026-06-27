#!/usr/bin/env sh
set -eu

# 先清理，避免上次构建残留的目标文件被原生 make test 复用而链接失败
make clean
make check-mingw64
make test
make mingw64-test-build
make mingw64
