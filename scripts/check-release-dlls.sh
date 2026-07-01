#!/usr/bin/env sh
set -eu

check_command()
{
  command -v "$1" >/dev/null 2>&1 || {
    echo "missing required command: $1" >&2
    exit 1
  }
}

check_dependency()
{
  case "$1" in
    KERNEL32.dll|msvcrt.dll)
      return 0
      ;;
    *)
      echo "$2 imports unexpected DLL: $1" >&2
      exit 1
      ;;
  esac
}

check_dll()
{
  dll="$1"
  file_token="$2"
  objdump_prefix="$3"

  if [ ! -f "$dll" ]; then
    echo "missing release DLL: $dll" >&2
    exit 1
  fi

  desc="$(file "$dll")"
  case "$desc" in
    *"$file_token"*)
      ;;
    *)
      echo "$dll has unexpected file type: $desc" >&2
      exit 1
      ;;
  esac

  deps="$("${objdump_prefix}objdump" -p "$dll" | sed -n 's/^[	 ]*DLL Name: //p')"
  if [ -z "$deps" ]; then
    echo "$dll has no parsed DLL imports" >&2
    exit 1
  fi

  for dep in $deps; do
    check_dependency "$dep" "$dll"
  done
  echo "$deps" | grep -Fx KERNEL32.dll >/dev/null || {
    echo "$dll is missing KERNEL32.dll import" >&2
    exit 1
  }
  echo "$deps" | grep -Fx msvcrt.dll >/dev/null || {
    echo "$dll is missing msvcrt.dll import" >&2
    exit 1
  }

  timestamp="$("${objdump_prefix}objdump" -x "$dll" | sed -n 's/^Time\/Date stamp[	 ]*//p' | head -1)"
  if [ "$timestamp" != "0" ]; then
    echo "$dll has non-reproducible PE timestamp: $timestamp" >&2
    exit 1
  fi

  echo "[OK] $dll"
  echo "     $desc"
  echo "     imports: $(echo "$deps" | tr '\n' ' ')"
  echo "     timestamp: $timestamp"
}

check_command file
check_command sed
check_command grep
check_command i686-w64-mingw32-objdump
check_command x86_64-w64-mingw32-objdump

check_dll build/CZSC.dll "PE32 executable" "i686-w64-mingw32-"
check_dll build/CZSC64.dll "PE32+ executable" "x86_64-w64-mingw32-"
