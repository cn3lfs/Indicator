#!/usr/bin/env python3
import difflib
import pathlib
import re
import subprocess
import sys
import tempfile

CANDIDATE_LINE = re.compile(r"^  [0-9]{4}-[0-9]{2}-[0-9]{2}  ")


def validate_candidate_context(text: str):
  errors = []
  for n_line, line in enumerate(text.splitlines(), start=1):
    if CANDIDATE_LINE.search(line) is None:
      continue
    for snippet in (" A[价", " C[价", " 比[价", " dvg[", " flags["):
      if snippet not in line:
        errors.append(f"line {n_line}: candidate missing {snippet.strip()}")
  return errors


def main() -> int:
  if len(sys.argv) != 3:
    print("usage: check_sse_result.py <dump_executable> <golden_file>", file=sys.stderr)
    return 2

  dump_exe = pathlib.Path(sys.argv[1])
  golden_file = pathlib.Path(sys.argv[2])
  if not dump_exe.exists():
    print(f"SSE dump executable not found: {dump_exe}", file=sys.stderr)
    return 2
  if not golden_file.exists():
    print(f"SSE golden file not found: {golden_file}", file=sys.stderr)
    return 2

  with tempfile.TemporaryDirectory(prefix="czsc-sse-") as tmp_dir:
    actual_file = pathlib.Path(tmp_dir) / "czsc_sse_result.txt"
    subprocess.run([str(dump_exe), str(actual_file)], check=True)

    expected = golden_file.read_text(encoding="utf-8").splitlines(keepends=True)
    actual = actual_file.read_text(encoding="utf-8").splitlines(keepends=True)
    context_errors = validate_candidate_context("".join(actual))
    if context_errors:
      print(f"{actual_file} is missing candidate strength context.", file=sys.stderr)
      for item in context_errors:
        print(item, file=sys.stderr)
      return 1

    if actual == expected:
      return 0

    print(f"{golden_file} is stale; run `make sse-result` and review the diff.", file=sys.stderr)
    for line in difflib.unified_diff(
        expected,
        actual,
        fromfile=str(golden_file),
        tofile="generated",
        n=3):
      sys.stderr.write(line)
    return 1


if __name__ == "__main__":
  raise SystemExit(main())
