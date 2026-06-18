#!/usr/bin/env sh
set -eu

sudo apt update
sudo apt install -y \
  make \
  g++ \
  mingw-w64 \
  gcc-mingw-w64-i686 \
  g++-mingw-w64-i686 \
  binutils-mingw-w64-i686

command -v make
command -v g++
command -v i686-w64-mingw32-gcc
command -v i686-w64-mingw32-g++
command -v i686-w64-mingw32-windres
