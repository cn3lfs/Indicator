#!/usr/bin/env python3
import difflib
import pathlib
import re
import subprocess
import sys
import tempfile

CANDIDATE_LINE = re.compile(r"^  [0-9]{4}-[0-9]{2}-[0-9]{2}  ")
BREAKOUT_FIELD = re.compile(r"突破(?P<breakout>-?[0-9]+)")
ABC_FIELD = re.compile(r" ABC(?P<abc>-?[0-9]+) ")
SMALL_TURN_FIELD = re.compile(r" 小转大(?P<small>-?[0-9]+) ")
DEBUG_IDS = re.compile(
  r"调试CEN(?P<center>[0-9]+) BKO(?P<bko>[0-9]+) "
  r"BLP(?P<blp>[0-9]+) BRP(?P<brp>[0-9]+) "
  r"ABK(?P<abk>[0-9]+) ABL(?P<abl>[0-9]+) ABR(?P<abr>[0-9]+) "
  r"STL(?P<stl>[0-9]+) STR(?P<str>[0-9]+) "
  r"STF(?P<stf>[0-9]+) "
  r"SFP(?P<sfp>[0-9]+) SMP(?P<smp>[0-9]+) "
  r"APS(?P<aps>[0-9]+) APE(?P<ape>[0-9]+) "
  r"CPS(?P<cps>[0-9]+) CPE(?P<cpe>[0-9]+) "
  r"PID(?P<pid>[0-9]+) TID(?P<trend>[0-9]+)"
)
BKO_CONTEXT = re.compile(
  r"bko\[离P(?P<leave>[0-9]+)/(?P<leave_date>[0-9]{4}-[0-9]{2}-[0-9]{2}) "
  r"回P(?P<retest>[0-9]+)/(?P<retest_date>[0-9]{4}-[0-9]{2}-[0-9]{2}) "
  r"首(?P<first>[01]) 回中(?P<back>[01]) 三(?P<third>[01])\]"
)
SECTION_HEADER = re.compile(r"^========== (?P<title>[^=]+)\((?P<count>[0-9]+)(?:,[^)]*)?\) ==========$")
POINT_LINE = re.compile(
  r"^(?P<prefix>[BL])(?P<id>[0-9]{3})\s+"
  r"(?P<date>[0-9]{4}-[0-9]{2}-[0-9]{2})\s+"
  r"(?P<kind>顶|底)\s+"
  r"(?P<price>-?[0-9]+(?:\.[0-9]+)?)$"
)


def parse_point_sections(text: str):
  sections = {}
  current = None
  for line in text.splitlines():
    header = SECTION_HEADER.match(line)
    if header is not None:
      title = header.group("title")
      if title in ("线段端点", "笔端点"):
        current = title
        sections[current] = {
          "declared": int(header.group("count")),
          "points": [],
        }
      else:
        current = None
      continue
    if current is None:
      continue
    point = POINT_LINE.match(line)
    if point is None:
      if line.strip():
        current = None
      continue
    sections[current]["points"].append((
      point.group("date"),
      point.group("kind"),
      point.group("price"),
    ))
  return sections


def validate_point_structure(text: str):
  errors = []
  sections = parse_point_sections(text)
  for title in ("线段端点", "笔端点"):
    if title not in sections:
      errors.append(f"missing point section: {title}")
      continue
    declared = sections[title]["declared"]
    actual = len(sections[title]["points"])
    if declared != actual:
      errors.append(f"{title}: declared {declared} points but parsed {actual}")
  if errors:
    return errors

  segment_points = sections["线段端点"]["points"]
  stroke_points = sections["笔端点"]["points"]
  if len(segment_points) >= len(stroke_points):
    errors.append(
      f"line segments should reduce point count: segments {len(segment_points)} >= strokes {len(stroke_points)}"
    )
  stroke_set = set(stroke_points)
  for point in segment_points:
    if point not in stroke_set:
      errors.append(f"line segment point not found in stroke points: {point[0]} {point[1]} {point[2]}")
  return errors


def validate_candidate_context(text: str):
  errors = []
  for n_line, line in enumerate(text.splitlines(), start=1):
    if CANDIDATE_LINE.search(line) is None:
      continue
    for snippet in (" A[价", " C[价", " 比[价", " dvg[", " bko[", " flags["):
      if snippet not in line:
        errors.append(f"line {n_line}: candidate missing {snippet.strip()}")
    line_date = line[2:12]
    debug_ids = DEBUG_IDS.search(line)
    if debug_ids is None:
      errors.append(f"line {n_line}: candidate missing debug ids")
      continue
    abc_field = ABC_FIELD.search(line)
    if abc_field is None:
      errors.append(f"line {n_line}: candidate missing ABC field")
      continue
    small_turn_field = SMALL_TURN_FIELD.search(line)
    if small_turn_field is None:
      errors.append(f"line {n_line}: candidate missing small turn field")
      continue
    n_abc = int(abc_field.group("abc"))
    n_small_turn = int(small_turn_field.group("small"))
    n_abk = int(debug_ids.group("abk"))
    n_abl = int(debug_ids.group("abl"))
    n_abr = int(debug_ids.group("abr"))
    n_stl = int(debug_ids.group("stl"))
    n_str = int(debug_ids.group("str"))
    n_stf = int(debug_ids.group("stf"))
    n_sfp = int(debug_ids.group("sfp"))
    n_smp = int(debug_ids.group("smp"))
    n_aps = int(debug_ids.group("aps"))
    n_ape = int(debug_ids.group("ape"))
    n_cps = int(debug_ids.group("cps"))
    n_cpe = int(debug_ids.group("cpe"))
    n_pid = int(debug_ids.group("pid"))
    is_second_signal = ("  二买  " in line) or ("  二卖  " in line)
    is_third_signal = ("  三买  " in line) or ("  三卖  " in line)
    if n_abc == 0 and n_abk != 0:
      errors.append(f"line {n_line}: ABC0 conflicts with ABK{n_abk}")
    if n_abc != 0 and n_abk <= 0:
      errors.append(f"line {n_line}: ABC{n_abc} requires positive ABK")
    if n_abk == 0 and (n_abl != 0 or n_abr != 0):
      errors.append(f"line {n_line}: ABK0 conflicts with ABL{n_abl}/ABR{n_abr}")
    if n_abk > 0 and (n_abl <= 0 or n_abr <= 0 or n_abl >= n_abr):
      errors.append(f"line {n_line}: ABK{n_abk} requires ordered positive ABL/ABR")
    if n_small_turn == 0 and (n_stl != 0 or n_str != 0 or n_stf != 0):
      errors.append(f"line {n_line}: small turn 0 conflicts with STL{n_stl}/STR{n_str}/STF{n_stf}")
    if n_small_turn != 0 and (n_stf <= 0 or n_stl <= 0 or n_str <= 0 or n_stf >= n_stl or n_stl >= n_str):
      errors.append(f"line {n_line}: small turn {n_small_turn} requires ordered positive STF/STL/STR")
    if not is_second_signal and (n_sfp != 0 or n_smp != 0):
      errors.append(f"line {n_line}: non-second signal conflicts with SFP{n_sfp}/SMP{n_smp}")
    if is_second_signal and (n_sfp <= 0 or n_smp <= n_sfp or n_pid <= n_smp):
      errors.append(f"line {n_line}: second signal requires ordered SFP/SMP/PID")
    if any(item != 0 for item in (n_aps, n_ape, n_cps, n_cpe)):
      if not (0 < n_aps < n_ape < n_cps < n_cpe):
        errors.append(f"line {n_line}: divergence endpoints require ordered positive APS/APE/CPS/CPE")
    if "bko[-]" in line:
      if any(int(debug_ids.group(name)) != 0 for name in ("bko", "blp", "brp", "stl", "str", "stf")):
        errors.append(f"line {n_line}: bko[-] conflicts with BKO/BLP/BRP/STL/STR/STF debug ids")
      continue
    breakout_field = BREAKOUT_FIELD.search(line)
    bko_context = BKO_CONTEXT.search(line)
    if breakout_field is None:
      errors.append(f"line {n_line}: candidate missing breakout id")
      continue
    if bko_context is None:
      errors.append(f"line {n_line}: malformed bko context")
      continue

    n_breakout = int(breakout_field.group("breakout"))
    n_bko = int(debug_ids.group("bko"))
    n_blp = int(debug_ids.group("blp"))
    n_brp = int(debug_ids.group("brp"))
    n_leave = int(bko_context.group("leave"))
    n_retest = int(bko_context.group("retest"))
    if is_third_signal and (n_bko <= 0 or n_blp <= 0 or n_brp <= 0):
      errors.append(f"line {n_line}: third signal requires positive BKO/BLP/BRP")
    if n_breakout >= 0 and n_bko != n_breakout + 1:
      errors.append(f"line {n_line}: BKO{n_bko} does not match breakout {n_breakout}")
    if n_blp != n_leave:
      errors.append(f"line {n_line}: BLP{n_blp} does not match leave P{n_leave}")
    if n_brp != n_retest:
      errors.append(f"line {n_line}: BRP{n_brp} does not match retest P{n_retest}")
    if n_small_turn != 0 and (n_stl != n_blp or n_str != n_brp):
      errors.append(f"line {n_line}: STL/STR must match BLP/BRP for small turn signals")
    if n_retest != n_pid:
      errors.append(f"line {n_line}: retest P{n_retest} does not match PID{n_pid}")
    if n_leave >= n_retest:
      errors.append(f"line {n_line}: leave P{n_leave} must precede retest P{n_retest}")
    if is_third_signal and not (n_blp < n_brp == n_pid):
      errors.append(f"line {n_line}: third signal requires BLP < BRP == PID")
    if bko_context.group("retest_date") != line_date:
      errors.append(f"line {n_line}: retest date {bko_context.group('retest_date')} does not match signal date {line_date}")
    if (bko_context.group("first"), bko_context.group("back"), bko_context.group("third")) != ("1", "0", "1"):
      errors.append(f"line {n_line}: bko flags must be first=1 back=0 third=1 for emitted third signals")
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
    actual_text = "".join(actual)
    context_errors = validate_point_structure(actual_text)
    context_errors.extend(validate_candidate_context(actual_text))
    if context_errors:
      print(f"{actual_file} has invalid SSE structure.", file=sys.stderr)
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
