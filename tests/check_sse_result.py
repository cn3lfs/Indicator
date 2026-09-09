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
  r"PID(?P<pid>[0-9]+) TID(?P<trend>[0-9]+) "
  r"DGS(?P<dgs>[0-9]+) RVP(?P<rvp>[0-9]+)"
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
NESTED_LINE = re.compile(
  r"^  (?P<date>[0-9]{4}-[0-9]{2}-[0-9]{2})  "
  r"级别(?P<level>[0-9]+)  语义(?P<semantic>[0-9]+)  确认(?P<flags>[0-9]+)  源H(?P<source>[0-9]+)  "
  r"低P(?P<start>[0-9]+)/(?P<start_date>[0-9]{4}-[0-9]{2}-[0-9]{2})"
  r"->P(?P<end>[0-9]+)/(?P<end_date>[0-9]{4}-[0-9]{2}-[0-9]{2})  "
  r"方向(?P<direction>-?[0-9]+)  小转大(?P<small>[0-9]+)$"
)
NESTED_SEMANTIC_HEADER = re.compile(
  r"^========== 区间套语义摘要\(3,total=(?P<total>[0-9]+),new_extreme=(?P<new>[0-9]+),"
  r"buy=(?P<buy>[0-9]+),sell=(?P<sell>[0-9]+)\) ==========$"
)
NESTED_SEMANTIC_LINE = re.compile(
  r"^  语义(?P<semantic>[1-3])  (?P<name>趋势背驰|盘整背驰|小转大)  (?P<count>[0-9]+)$"
)
NESTED_SEMANTIC_NAMES = {
  1: "趋势背驰",
  2: "盘整背驰",
  3: "小转大",
}
FILTER_HEADER = re.compile(
  r"^========== (?P<title>买卖点过滤原因\((?:笔|线段)中枢\))\(8,total=(?P<total>[0-9]+),unknown=(?P<unknown>[0-9]+)\) ==========$"
)
FILTER_REASON_LINE = re.compile(
  r"^  原因(?P<reason>[1-8])  (?P<name>[^ ]+)  (?P<count>[0-9]+)"
  r"(?:  样例 (?P<examples>-|[0-9]{4}-[0-9]{2}-[0-9]{2}(?:,[0-9]{4}-[0-9]{2}-[0-9]{2})*))?$"
)
FILTER_REASON_NAMES = {
  1: "无趋势",
  2: "非趋势背驰",
  3: "二类顺序失败",
  4: "非首次回试",
  5: "回中枢",
  6: "方向不匹配",
  7: "ABC未对齐",
  8: "缺中枢",
}
NO_TREND_ATTR_HEADER = re.compile(
  r"^========== (?P<title>买卖点无趋势归因\((?:笔|线段)中枢\))\(3,total=(?P<total>[0-9]+)\) ==========$"
)
NO_TREND_ATTR_LINE = re.compile(
  r"^  归因(?P<attr>[1-3])  (?P<name>[^ ]+)  (?P<count>[0-9]+)"
  r"  样例 (?P<examples>-|[0-9]{4}-[0-9]{2}-[0-9]{2}(?:,[0-9]{4}-[0-9]{2}-[0-9]{2})*)$"
)
NO_TREND_ATTR_NAMES = {
  1: "早期中心不足",
  2: "同向趋势不足",
  3: "不在已完成趋势",
}
TREND_STRUCTURE_HEADER = re.compile(
  r"^========== (?P<title>走势结构摘要\((?:笔|线段)中枢\))"
  r"\(3,total=(?P<total>[0-9]+),invalid_center=(?P<invalid_center>[0-9]+),"
  r"invalid_span=(?P<invalid_span>[0-9]+)\) ==========$"
)
TREND_STRUCTURE_LINE = re.compile(
  r"^  走势 (?P<name>上涨|下跌|盘整)  (?P<count>[0-9]+)$"
)
LIFECYCLE_LINE = re.compile(r"^(?P<prefix>BZ|SZ)[0-9]+->(?P=prefix)[0-9]+\s+[^()]+\((?P<code>-?[0-9]+)\)\s+")
LIFECYCLE_SUMMARY = re.compile(
  r"^延伸(?P<extension>[0-9]+) 扩展(?P<expansion>[0-9]+) "
  r"上涨新生(?P<up>[0-9]+) 下跌新生(?P<down>[0-9]+) 未知(?P<unknown>[0-9]+)$"
)


def parse_point_sections(text: str):
  samples = []
  current = None
  current_sample = None
  for line in text.splitlines():
    header = SECTION_HEADER.match(line)
    if header is not None:
      title = header.group("title")
      if title == "线段端点":
        current_sample = {}
        samples.append(current_sample)
        current = title
        current_sample[current] = {
          "declared": int(header.group("count")),
          "points": [],
        }
      elif title == "笔端点":
        if current_sample is None:
          current_sample = {}
          samples.append(current_sample)
        current = title
        current_sample[current] = {
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
    current_sample[current]["points"].append((
      point.group("date"),
      point.group("kind"),
      point.group("price"),
    ))
  return samples


def validate_point_structure(text: str):
  errors = []
  samples = parse_point_sections(text)
  if not samples:
    return ["missing point sections"]

  for n_sample, sections in enumerate(samples, start=1):
    for title in ("线段端点", "笔端点"):
      if title not in sections:
        errors.append(f"sample {n_sample}: missing point section: {title}")
        continue
      declared = sections[title]["declared"]
      actual = len(sections[title]["points"])
      if declared != actual:
        errors.append(f"sample {n_sample} {title}: declared {declared} points but parsed {actual}")
    if any(title not in sections for title in ("线段端点", "笔端点")):
      continue

    segment_points = sections["线段端点"]["points"]
    stroke_points = sections["笔端点"]["points"]
    if len(segment_points) >= len(stroke_points):
      errors.append(
        f"sample {n_sample}: line segments should reduce point count: segments {len(segment_points)} >= strokes {len(stroke_points)}"
      )
    stroke_set = set(stroke_points)
    stroke_index = {}
    for n_stroke, point in enumerate(stroke_points):
      stroke_index.setdefault(point, n_stroke)
    for point in segment_points:
      if point not in stroke_set:
        errors.append(
          f"sample {n_sample}: line segment point not found in stroke points: {point[0]} {point[1]} {point[2]}"
        )
    for n_point in range(1, len(segment_points)):
      prev = segment_points[n_point - 1]
      curr = segment_points[n_point]
      if prev[1] == curr[1]:
        errors.append(
          f"sample {n_sample}: line segment endpoints must alternate top/bottom: {prev[0]} {prev[1]} -> {curr[0]} {curr[1]}"
        )
      if prev not in stroke_index or curr not in stroke_index:
        continue
      n_prev = stroke_index[prev]
      n_curr = stroke_index[curr]
      if n_curr <= n_prev:
        errors.append(
          f"sample {n_sample}: line segment endpoints must follow stroke order: {prev[0]} -> {curr[0]}"
        )
      elif (n_curr - n_prev) < 3:
        errors.append(
          f"sample {n_sample}: line segment must span at least three strokes: {prev[0]} -> {curr[0]} spans {n_curr - n_prev}"
        )
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
    n_dgs = int(debug_ids.group("dgs"))
    n_rvp = int(debug_ids.group("rvp"))
    is_first_signal = ("  一买  " in line) or ("  一卖  " in line)
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
    if n_dgs not in (0, 1, 2, 3):
      errors.append(f"line {n_line}: invalid DGS{n_dgs}")
    if n_dgs == 1 and (not is_first_signal or "创新" not in line or "成立" not in line):
      errors.append(f"line {n_line}: trend divergence DGS1 requires first signal and confirmed new extreme")
    if n_dgs == 2 and (is_first_signal or "成立" not in line):
      errors.append(f"line {n_line}: consolidation divergence DGS2 requires non-first confirmed divergence")
    if n_dgs == 3 and (not is_third_signal or n_small_turn == 0):
      errors.append(f"line {n_line}: small-turn DGS3 requires third signal and small turn context")
    if n_rvp != 0 and (not is_first_signal or n_rvp != n_pid + 1):
      errors.append(f"line {n_line}: RVP{n_rvp} requires first signal and next point after PID{n_pid}")
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


def validate_nested_context(text: str):
  errors = []
  in_nested = False
  declared = None
  parsed = 0
  for n_line, line in enumerate(text.splitlines(), start=1):
    header = SECTION_HEADER.match(line)
    if header is not None:
      if in_nested and declared != parsed:
        errors.append(f"nested context section declared {declared} rows but parsed {parsed}")
      in_nested = header.group("title") == "区间套背驰上下文"
      declared = int(header.group("count")) if in_nested else None
      parsed = 0
      continue
    if not in_nested:
      continue
    if not line.strip():
      continue
    nested = NESTED_LINE.match(line)
    if nested is None:
      errors.append(f"line {n_line}: malformed nested divergence context")
      continue
    parsed += 1
    n_level = int(nested.group("level"))
    n_semantic = int(nested.group("semantic"))
    n_flags = int(nested.group("flags"))
    n_source = int(nested.group("source"))
    n_start = int(nested.group("start"))
    n_end = int(nested.group("end"))
    n_direction = int(nested.group("direction"))
    n_small = int(nested.group("small"))
    if n_level not in (0, 1, 2):
      errors.append(f"line {n_line}: nested level must be 0, 1 or 2")
    if n_semantic not in (1, 2, 3):
      errors.append(f"line {n_line}: nested semantic must be 1, 2 or 3")
    if (n_flags & 3) != 3:
      errors.append(f"line {n_line}: nested flags must include inside and divergence bits")
    if n_source <= 0:
      errors.append(f"line {n_line}: nested source must be positive")
    if not (0 < n_start < n_end):
      errors.append(f"line {n_line}: nested low endpoints must be ordered")
    if n_direction not in (-1, 1):
      errors.append(f"line {n_line}: nested direction must be -1 or 1")
    if n_small not in (0, 1):
      errors.append(f"line {n_line}: nested small-turn flag must be 0 or 1")
    if n_level == 0 and (n_semantic != 2 or (n_flags & 4) != 0):
      errors.append(f"line {n_line}: nested consolidation context must be semantic 2 without new-extreme bit")
    if n_level > 0 and (n_semantic not in (1, 3) or (n_flags & 4) == 0):
      errors.append(f"line {n_line}: nested trend context requires trend/small-turn semantic and new-extreme bit")
    if n_small == 1 and (n_level != 2 or n_semantic != 3 or (n_flags & 8) == 0):
      errors.append(f"line {n_line}: nested small-turn confirmation requires level 2, semantic 3 and small-turn bit")
  if in_nested and declared != parsed:
    errors.append(f"nested context section declared {declared} rows but parsed {parsed}")
  return errors


def validate_nested_semantic_summary(text: str):
  errors = []
  sample_count = text.count("########## 样本:")
  semantic_sections = 0
  in_context = False
  in_summary = False
  pending = None
  expected = None
  summary = None
  summary_line = 0

  def empty_counts():
    return {
      "total": 0,
      "new": 0,
      "buy": 0,
      "sell": 0,
      "semantic": {
        1: 0,
        2: 0,
        3: 0,
      },
    }

  def close_summary():
    nonlocal in_summary, pending, summary
    if not in_summary:
      return
    if pending is None:
      errors.append(f"line {summary_line}: nested semantic summary has no preceding context section")
      pending = empty_counts()
    if sorted(summary["semantic"].keys()) != [1, 2, 3]:
      errors.append(f"line {summary_line}: nested semantic summary must list semantics 1..3")
    semantic_total = sum(summary["semantic"].values())
    if semantic_total != summary["total"]:
      errors.append(f"line {summary_line}: nested semantic summary total {summary['total']} but rows count {semantic_total}")
    if summary["buy"] + summary["sell"] != summary["total"]:
      errors.append(f"line {summary_line}: nested semantic summary buy/sell counts do not match total")
    if summary["new"] > summary["total"]:
      errors.append(f"line {summary_line}: nested semantic summary new_extreme exceeds total")
    if summary != pending:
      errors.append(f"line {summary_line}: nested semantic summary {summary} does not match context rows {pending}")
    in_summary = False
    pending = None
    summary = None

  for n_line, line in enumerate(text.splitlines(), start=1):
    if line.startswith("========== "):
      if in_context:
        pending = expected
        in_context = False
        expected = None
      else:
        close_summary()

      semantic_header = NESTED_SEMANTIC_HEADER.match(line)
      if semantic_header is not None:
        semantic_sections += 1
        in_summary = True
        summary_line = n_line
        summary = {
          "total": int(semantic_header.group("total")),
          "new": int(semantic_header.group("new")),
          "buy": int(semantic_header.group("buy")),
          "sell": int(semantic_header.group("sell")),
          "semantic": {},
        }
        continue

      header = SECTION_HEADER.match(line)
      if header is not None and header.group("title") == "区间套背驰上下文":
        if pending is not None:
          errors.append(f"line {n_line}: missing nested semantic summary after context section")
        in_context = True
        expected = empty_counts()
        continue

      if pending is not None:
        errors.append(f"line {n_line}: missing nested semantic summary after context section")
        pending = None
      continue

    if in_context:
      if not line.strip():
        continue
      nested = NESTED_LINE.match(line)
      if nested is None:
        continue
      n_semantic = int(nested.group("semantic"))
      n_flags = int(nested.group("flags"))
      n_direction = int(nested.group("direction"))
      expected["total"] += 1
      if n_semantic in expected["semantic"]:
        expected["semantic"][n_semantic] += 1
      if (n_flags & 4) != 0:
        expected["new"] += 1
      if n_direction > 0:
        expected["buy"] += 1
      elif n_direction < 0:
        expected["sell"] += 1
      continue

    if in_summary:
      if not line.strip():
        continue
      semantic_line = NESTED_SEMANTIC_LINE.match(line)
      if semantic_line is None:
        errors.append(f"line {n_line}: malformed nested semantic summary row")
        continue
      n_semantic = int(semantic_line.group("semantic"))
      if semantic_line.group("name") != NESTED_SEMANTIC_NAMES[n_semantic]:
        errors.append(f"line {n_line}: semantic {n_semantic} name should be {NESTED_SEMANTIC_NAMES[n_semantic]}")
      summary["semantic"][n_semantic] = int(semantic_line.group("count"))

  if in_context:
    pending = expected
  close_summary()
  if pending is not None:
    errors.append("missing nested semantic summary after final context section")
  if sample_count > 0 and semantic_sections != sample_count:
    errors.append(f"expected {sample_count} nested semantic summary sections for {sample_count} samples, found {semantic_sections}")
  return errors


def validate_filter_reason_summary(text: str):
  errors = []
  sample_count = text.count("########## 样本:")
  filter_sections = 0
  in_filter = False
  title = ""
  line_start = 0
  declared_total = 0
  declared_unknown = 0
  seen_reasons = []
  parsed_total = 0

  def close_section():
    nonlocal in_filter
    if not in_filter:
      return
    if len(seen_reasons) != 8:
      errors.append(f"line {line_start}: {title} should list 8 filter reasons, parsed {len(seen_reasons)}")
    if sorted(seen_reasons) != list(range(1, 9)):
      errors.append(f"line {line_start}: {title} reasons must be exactly 1..8")
    if parsed_total != declared_total:
      errors.append(f"line {line_start}: {title} declared total {declared_total} but counted {parsed_total}")
    if declared_unknown != 0:
      errors.append(f"line {line_start}: {title} has unknown filter reasons: {declared_unknown}")
    in_filter = False

  for n_line, line in enumerate(text.splitlines(), start=1):
    header = SECTION_HEADER.match(line)
    if header is not None:
      close_section()
      filter_header = FILTER_HEADER.match(line)
      if filter_header is None:
        continue
      filter_sections += 1
      in_filter = True
      title = filter_header.group("title")
      line_start = n_line
      declared_total = int(filter_header.group("total"))
      declared_unknown = int(filter_header.group("unknown"))
      seen_reasons = []
      parsed_total = 0
      continue

    if not in_filter:
      continue
    if not line.strip():
      continue
    reason_line = FILTER_REASON_LINE.match(line)
    if reason_line is None:
      errors.append(f"line {n_line}: malformed filter reason summary row")
      continue
    n_reason = int(reason_line.group("reason"))
    n_count = int(reason_line.group("count"))
    examples = reason_line.group("examples")
    seen_reasons.append(n_reason)
    parsed_total += n_count
    if reason_line.group("name") != FILTER_REASON_NAMES[n_reason]:
      errors.append(f"line {n_line}: reason {n_reason} name should be {FILTER_REASON_NAMES[n_reason]}")
    if examples is None:
      errors.append(f"line {n_line}: reason {n_reason} missing example dates")
    elif n_count == 0 and examples != "-":
      errors.append(f"line {n_line}: reason {n_reason} has examples but zero count")
    elif n_count > 0:
      if examples == "-":
        errors.append(f"line {n_line}: reason {n_reason} should include at least one example date")
      else:
        example_dates = examples.split(",")
        if len(example_dates) > 5:
          errors.append(f"line {n_line}: reason {n_reason} should include at most 5 example dates")
        if len(example_dates) > n_count:
          errors.append(f"line {n_line}: reason {n_reason} has more examples than count")

  close_section()
  if sample_count > 0 and filter_sections != sample_count * 2:
    errors.append(f"expected {sample_count * 2} filter summary sections for {sample_count} samples, found {filter_sections}")
  return errors


def validate_no_trend_attribution_summary(text: str):
  errors = []
  sample_count = text.count("########## 样本:")
  attribution_sections = 0
  in_attribution = False
  title = ""
  line_start = 0
  declared_total = 0
  seen_attrs = []
  parsed_total = 0

  def close_section():
    nonlocal in_attribution
    if not in_attribution:
      return
    if len(seen_attrs) != 3:
      errors.append(f"line {line_start}: {title} should list 3 no-trend attributions, parsed {len(seen_attrs)}")
    if sorted(seen_attrs) != [1, 2, 3]:
      errors.append(f"line {line_start}: {title} attributions must be exactly 1..3")
    if parsed_total != declared_total:
      errors.append(f"line {line_start}: {title} declared total {declared_total} but counted {parsed_total}")
    in_attribution = False

  for n_line, line in enumerate(text.splitlines(), start=1):
    header = SECTION_HEADER.match(line)
    if header is not None:
      close_section()
      attribution_header = NO_TREND_ATTR_HEADER.match(line)
      if attribution_header is None:
        continue
      attribution_sections += 1
      in_attribution = True
      title = attribution_header.group("title")
      line_start = n_line
      declared_total = int(attribution_header.group("total"))
      seen_attrs = []
      parsed_total = 0
      continue

    if not in_attribution:
      continue
    if not line.strip():
      continue
    attr_line = NO_TREND_ATTR_LINE.match(line)
    if attr_line is None:
      errors.append(f"line {n_line}: malformed no-trend attribution summary row")
      continue

    n_attr = int(attr_line.group("attr"))
    n_count = int(attr_line.group("count"))
    examples = attr_line.group("examples")
    seen_attrs.append(n_attr)
    parsed_total += n_count
    if attr_line.group("name") != NO_TREND_ATTR_NAMES[n_attr]:
      errors.append(f"line {n_line}: attribution {n_attr} name should be {NO_TREND_ATTR_NAMES[n_attr]}")
    if n_count == 0 and examples != "-":
      errors.append(f"line {n_line}: attribution {n_attr} has examples but zero count")
    elif n_count > 0:
      if examples == "-":
        errors.append(f"line {n_line}: attribution {n_attr} should include at least one example date")
      else:
        example_dates = examples.split(",")
        if len(example_dates) > 5:
          errors.append(f"line {n_line}: attribution {n_attr} should include at most 5 example dates")
        if len(example_dates) > n_count:
          errors.append(f"line {n_line}: attribution {n_attr} has more examples than count")

  close_section()
  if sample_count > 0 and attribution_sections != sample_count * 2:
    errors.append(
      f"expected {sample_count * 2} no-trend attribution sections for {sample_count} samples, "
      f"found {attribution_sections}")
  return errors


def validate_trend_structure_summary(text: str):
  errors = []
  sample_count = text.count("########## 样本:")
  trend_sections = 0
  in_trend = False
  title = ""
  line_start = 0
  declared_total = 0
  invalid_center = 0
  invalid_span = 0
  seen_names = []
  parsed_total = 0

  def close_section():
    nonlocal in_trend
    if not in_trend:
      return
    if sorted(seen_names) != ["上涨", "下跌", "盘整"]:
      errors.append(f"line {line_start}: {title} should list 上涨/下跌/盘整 rows")
    if parsed_total != declared_total:
      errors.append(f"line {line_start}: {title} declared total {declared_total} but counted {parsed_total}")
    if invalid_center != 0:
      errors.append(f"line {line_start}: {title} has invalid center ranges: {invalid_center}")
    if invalid_span != 0:
      errors.append(f"line {line_start}: {title} has invalid point spans: {invalid_span}")
    in_trend = False

  for n_line, line in enumerate(text.splitlines(), start=1):
    if line.startswith("========== "):
      close_section()
      trend_header = TREND_STRUCTURE_HEADER.match(line)
      if trend_header is None:
        continue
      trend_sections += 1
      in_trend = True
      title = trend_header.group("title")
      line_start = n_line
      declared_total = int(trend_header.group("total"))
      invalid_center = int(trend_header.group("invalid_center"))
      invalid_span = int(trend_header.group("invalid_span"))
      seen_names = []
      parsed_total = 0
      continue

    if not in_trend:
      continue
    if not line.strip():
      continue
    trend_line = TREND_STRUCTURE_LINE.match(line)
    if trend_line is None:
      errors.append(f"line {n_line}: malformed trend structure summary row")
      continue
    seen_names.append(trend_line.group("name"))
    parsed_total += int(trend_line.group("count"))

  close_section()
  if sample_count > 0 and trend_sections != sample_count * 2:
    errors.append(f"expected {sample_count * 2} trend summary sections for {sample_count} samples, found {trend_sections}")
  return errors


def validate_center_lifecycle_summary(text: str):
  errors = []
  current = None
  counts = None
  pending = {}

  def empty_counts():
    return {
      "extension": 0,
      "expansion": 0,
      "up": 0,
      "down": 0,
      "unknown": 0,
    }

  def add_lifecycle(code: int):
    if code == 1:
      counts["extension"] += 1
    elif code == 2:
      counts["expansion"] += 1
    elif code == 3:
      counts["up"] += 1
    elif code == -3:
      counts["down"] += 1
    else:
      counts["unknown"] += 1

  for n_line, line in enumerate(text.splitlines(), start=1):
    header = SECTION_HEADER.match(line)
    if header is not None:
      title = header.group("title")
      if title in ("笔中枢生命周期", "线段中枢生命周期"):
        current = title
        counts = empty_counts()
        pending[title] = counts
      else:
        current = None
        counts = None
      continue

    if line == "========== 笔中枢生命周期摘要 ==========":
      current = "笔中枢生命周期摘要"
      continue
    if line == "========== 线段中枢生命周期摘要 ==========":
      current = "线段中枢生命周期摘要"
      continue

    if current in ("笔中枢生命周期", "线段中枢生命周期") and line.strip():
      lifecycle = LIFECYCLE_LINE.match(line)
      if lifecycle is None:
        errors.append(f"line {n_line}: malformed center lifecycle row")
        continue
      add_lifecycle(int(lifecycle.group("code")))
      continue

    if current in ("笔中枢生命周期摘要", "线段中枢生命周期摘要") and line.strip():
      summary = LIFECYCLE_SUMMARY.match(line)
      if summary is None:
        errors.append(f"line {n_line}: malformed center lifecycle summary")
        continue
      source_title = current.replace("摘要", "")
      expected = pending.get(source_title)
      if expected is None:
        errors.append(f"line {n_line}: lifecycle summary without source rows: {source_title}")
        continue
      actual = {key: int(summary.group(key)) for key in expected}
      if actual != expected:
        errors.append(f"line {n_line}: {source_title} summary {actual} does not match rows {expected}")
      pending.pop(source_title, None)
      current = None

  for title in pending:
    errors.append(f"missing lifecycle summary for {title}")
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
    context_errors.extend(validate_nested_context(actual_text))
    context_errors.extend(validate_nested_semantic_summary(actual_text))
    context_errors.extend(validate_filter_reason_summary(actual_text))
    context_errors.extend(validate_no_trend_attribution_summary(actual_text))
    context_errors.extend(validate_trend_structure_summary(actual_text))
    context_errors.extend(validate_center_lifecycle_summary(actual_text))
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
