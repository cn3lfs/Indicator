import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = [ROOT / "README.md", ROOT / "formulas" / "README.md"]
FORMULA_REF = re.compile(r"(?:formulas[\\/])?(chan-[A-Za-z0-9_-]+\.txt)")
FUNC30_REF = re.compile(r"TDXDLL1\s*\(\s*30\s*,\s*H\s*,\s*L\s*,\s*([0-9]+)\s*\)")
FUNC30_CALL = re.compile(r"TDXDLL1\s*\(\s*30\s*,")
FUNC40_CALL = re.compile(r"TDXDLL1\s*\(\s*40\s*,\s*C\s*,\s*V\s*,\s*0\s*\)")
FUNC30_SWITCH = re.compile(r"void\s+Func30\s*\([^)]*\)\s*\{(?P<body>.*?)\n\}\s*\n//=+\n// 输出函数40号", re.S)
CASE_REF = re.compile(r"\bcase\s+([0-9]+)\s*:")


def is_valid_config(n_config: int) -> bool:
  for _ in range(4):
    if (n_config % 10) not in (0, 1):
      return False
    n_config //= 10
  return n_config == 0


def parse_func30_outputs(text: str):
  match = FUNC30_SWITCH.search(text)
  if match is None:
    return set(), ["unable to locate Func30 switch in CzscCore.cpp"]

  outputs = {int(item) for item in CASE_REF.findall(match.group("body"))}
  if not outputs:
    return outputs, ["unable to parse Func30 case labels"]
  return outputs, []


def validate_func30_outputs(outputs):
  expected = set(range(0, max(outputs) + 1))
  missing = sorted(expected - outputs)
  if missing:
    return [f"Func30 output cases are not contiguous, missing: {missing}"]
  return []


def read_func30_outputs():
  text = (ROOT / "CzscCore.cpp").read_text(encoding="utf-8")
  outputs, errors = parse_func30_outputs(text)
  if errors:
    return outputs, errors
  return outputs, validate_func30_outputs(outputs)


def validate_func30_mode(n_mode: int, outputs):
  n_output = (n_mode % 1000) // 10
  n_config = n_mode // 1000
  if (n_mode % 10) != 0:
    return "mode must end with 0"
  if n_output not in outputs:
    return f"unknown output {n_output}"
  if not is_valid_config(n_config):
    return f"invalid config {n_config}"
  return ""


def validate_readme_func30_docs(readme_text: str, outputs):
  errors = []
  for n_output in sorted(n for n in outputs if n >= 14):
    marker = f"30 号输出 {n_output}"
    if marker not in readme_text:
      errors.append(f"README.md missing Func30 output doc: {marker}")
    mode_ref = f"TDXDLL1(30,H,L,{n_output * 10})"
    if mode_ref not in readme_text:
      errors.append(f"README.md missing Func30 mode example: {mode_ref}")
  return errors


def self_test() -> int:
  sample = (
    "void Func30(int nCount)\n"
    "{\n"
    "  switch (nCount)\n"
    "  {\n"
    "    case 0: break;\n"
    "    case 1: break;\n"
    "    case 2: break;\n"
    "  }\n"
    "}\n"
    "\n"
    "//=============================================================================\n"
    "// 输出函数40号\n"
  )
  outputs, errors = parse_func30_outputs(sample)
  if errors or outputs != {0, 1, 2}:
    print("self-test failed: parse contiguous cases", file=sys.stderr)
    return 1

  gapped = sample.replace("case 1: break;\n", "")
  outputs, errors = parse_func30_outputs(gapped)
  if errors or not validate_func30_outputs(outputs):
    print("self-test failed: detect missing case", file=sys.stderr)
    return 1

  outputs, errors = parse_func30_outputs("void Func29() {}")
  if not errors:
    print("self-test failed: detect missing Func30", file=sys.stderr)
    return 1

  outputs = {0, 1}
  checks = [
    (validate_func30_mode(11, outputs), "mode must end with 0"),
    (validate_func30_mode(90, outputs), "unknown output 9"),
    (validate_func30_mode(2000, outputs), "invalid config 2"),
    (validate_func30_mode(1010, outputs), ""),
  ]
  for actual, expected in checks:
    if actual != expected:
      print(f"self-test failed: mode validation {actual!r} != {expected!r}", file=sys.stderr)
      return 1

  doc_errors = validate_readme_func30_docs(
    "30 号输出 14 `TDXDLL1(30,H,L,140)`\n30 号输出 16\n",
    {0, 13, 14, 15, 16})
  expected_doc_errors = [
    "README.md missing Func30 output doc: 30 号输出 15",
    "README.md missing Func30 mode example: TDXDLL1(30,H,L,150)",
    "README.md missing Func30 mode example: TDXDLL1(30,H,L,160)",
  ]
  if doc_errors != expected_doc_errors:
    print(f"self-test failed: README output docs {doc_errors!r}", file=sys.stderr)
    return 1
  return 0


def main() -> int:
  missing = []
  empty = []
  undocumented = []
  invalid_modes = []
  stale_comments = []
  aux_order_errors = []
  func30_errors = []
  func30_outputs, func30_errors = read_func30_outputs()
  readme_text = (ROOT / "README.md").read_text(encoding="utf-8")
  func30_doc_errors = validate_readme_func30_docs(readme_text, func30_outputs)
  guide_text = (ROOT / "formulas" / "README.md").read_text(encoding="utf-8")
  formula_files = sorted((ROOT / "formulas").glob("chan-*.txt"))
  for doc in DOCS:
    text = doc.read_text(encoding="utf-8")
    for name in sorted(set(FORMULA_REF.findall(text))):
      path = ROOT / "formulas" / name
      if not path.exists():
        missing.append(f"{doc.relative_to(ROOT)} -> {name}")
      elif path.stat().st_size == 0:
        empty.append(str(path.relative_to(ROOT)))

  for path in formula_files:
    if path.name not in guide_text:
      undocumented.append(path.name)

  for doc in DOCS + formula_files:
    text = doc.read_text(encoding="utf-8")
    for match in FUNC30_REF.finditer(text):
      n_mode = int(match.group(1))
      error = validate_func30_mode(n_mode, func30_outputs)
      if error:
        invalid_modes.append(f"{doc.relative_to(ROOT)} -> {n_mode}: {error}")

  for path in formula_files:
    text = path.read_text(encoding="utf-8")
    first_func30 = FUNC30_CALL.search(text)
    if first_func30 is None:
      continue
    first_func40 = FUNC40_CALL.search(text)
    if first_func40 is None:
      aux_order_errors.append(f"{path.relative_to(ROOT)}: missing TDXDLL1(40,C,V,0) before Func30")
    elif first_func40.start() > first_func30.start():
      aux_order_errors.append(f"{path.relative_to(ROOT)}: TDXDLL1(40,C,V,0) must appear before Func30")

  debug_text = (ROOT / "formulas" / "chan-debug.txt").read_text(encoding="utf-8")
  expected_debug_comments = [
    "中枢关系：1上涨/-1下跌/2扩展",
    "一类背驰转折：1扩展/2盘整/3反趋势",
    "三买三卖后续：1扩张/2新生",
    "上下文位图：含4096首次离开/回试",
    "胜出候选优先级：二10三20一30",
    "胜出候选中枢编号：一基",
    "胜出候选突破编号：一基",
    "胜出候选端点编号：一基",
    "胜出候选走势编号：一基",
  ]
  expected_debug_lines = [
    "DRAWNUMBER(BSP<>0 AND POS<>2,L*0.995,POS)",
    "DRAWNUMBER(BSP<>0,H*1.005,MOV)",
    "DRAWNUMBER(BSP<>0,H*1.010,PRI)",
    "DRAWNUMBER(BSP<>0 AND CEN>0,L*0.990,CEN)",
    "DRAWNUMBER(BSP<>0 AND BKO>0,H*1.015,BKO)",
    "DRAWNUMBER(BSP<>0 AND PID>0,L*0.985,PID)",
    "DRAWNUMBER(BSP<>0 AND TID>0,H*1.020,TID)",
  ]
  for comment in expected_debug_comments:
    if comment not in debug_text:
      stale_comments.append(f"chan-debug.txt missing comment: {comment}")
  for line in expected_debug_lines:
    if line not in debug_text:
      stale_comments.append(f"chan-debug.txt missing debug line: {line}")

  if (missing or empty or undocumented or invalid_modes or stale_comments or
      aux_order_errors or func30_errors or func30_doc_errors):
    for item in missing:
      print(f"missing formula: {item}", file=sys.stderr)
    for item in empty:
      print(f"empty formula: {item}", file=sys.stderr)
    for item in undocumented:
      print(f"undocumented formula: formulas/README.md -> {item}", file=sys.stderr)
    for item in invalid_modes:
      print(f"invalid Func30 mode: {item}", file=sys.stderr)
    for item in stale_comments:
      print(f"stale formula comment: {item}", file=sys.stderr)
    for item in aux_order_errors:
      print(f"aux order error: {item}", file=sys.stderr)
    for item in func30_errors:
      print(f"Func30 parser error: {item}", file=sys.stderr)
    for item in func30_doc_errors:
      print(f"Func30 documentation error: {item}", file=sys.stderr)
    return 1
  return 0


if __name__ == "__main__":
  if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
    raise SystemExit(self_test())
  raise SystemExit(main())
