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
TDXDLL_REF = re.compile(r"TDXDLL1\s*\(\s*([0-9]+)\s*,")
REGISTERED_FUNC_REF = re.compile(r"\{\s*([0-9]+)\s*,\s*&Func[0-9]+\s*\}")
CASE_REF = re.compile(r"\bcase\s+([0-9]+)\s*:")
TDX_COMMENT = re.compile(r"\{.*?\}", re.S)
FORMULA_ALIASES = [
  ("chan-main.txt", "chan-all-buys.txt"),
]
README_REQUIRED_SNIPPETS = [
  "formulas/README.md",
  "formulas/chan-main.txt",
  "formulas/chan-debug.txt",
]
README_MODE_RULE_SNIPPETS = [
  "配置码四位只能由 `0/1` 组成",
  "非法 mode 会输出全 0",
]
EXPECTED_REGISTERED_FUNCS = set(range(1, 21)) | {30, 40}

EXPECTED_FORMULA_SNIPPETS = {
  "chan-main.txt": [
    "DLL:=TDXDLL1(30,H,L,0);",
    "SEG:=TDXDLL1(30,H,L,1100000);",
    "BSP:=TDXDLL1(30,H,L,40);",
    "QLT:=TDXDLL1(30,H,L,50);",
    "DRAWTEXT(BSP=1,LOW,'一买'),COLORWHITE;",
    "DRAWTEXT(BSP=11,HIGH,'一卖'),COLORGREEN;",
  ],
  "chan-first-buy.txt": [
    "BSP:=TDXDLL1(30,H,L,40);",
    "BARSLAST(BSP=1)<10;",
  ],
  "chan-first-buy-abc.txt": [
    "ABX:=TDXDLL1(30,H,L,160);",
    "BARSLAST(ABX=1)<10;",
  ],
  "chan-first-buy-context.txt": [
    "CTX:=TDXDLL1(30,H,L,210);",
    "QST:=MOD(INTPART(CTX/1),2)=1;",
    "ABC:=MOD(INTPART(CTX/2),2)=1;",
    "MZP:=MOD(INTPART(CTX/4),2)=1;",
    "MLW:=MOD(INTPART(CTX/8),2)=1;",
    "STD:=MOD(INTPART(CTX/32),2)=1;",
    "BARSLAST(BSP=1 AND QST AND ABC AND MZP AND MLW AND STD)<10;",
  ],
  "chan-first-buy-original.txt": [
    "POS:=TDXDLL1(30,H,L,220);",
    "MOV:=TDXDLL1(30,H,L,230);",
    "BARSLAST(BSP=1 AND POS=-1 AND MOV=-1)<10;",
  ],
  "chan-first-buy-standard.txt": [
    "STD:=TDXDLL1(30,H,L,200);",
    "BARSLAST(STD=1)<10;",
  ],
  "chan-small-turn-buy.txt": [
    "XC:=TDXDLL1(40,C,V,0);",
    "STB:=TDXDLL1(30,H,L,140);",
    "BARSLAST(STB=1)<10;",
  ],
  "chan-first-sell.txt": [
    "BSP:=TDXDLL1(30,H,L,40);",
    "BARSLAST(BSP=11)<10;",
  ],
  "chan-second-buy.txt": [
    "BSP:=TDXDLL1(30,H,L,40);",
    "BARSLAST(BSP=2)<10;",
  ],
  "chan-third-buy.txt": [
    "BSP:=TDXDLL1(30,H,L,40);",
    "BARSLAST(BSP=3)<10;",
  ],
  "chan-multi-buy.txt": [
    "XC:=TDXDLL1(40,C,V,0);",
    "BSP:=TDXDLL1(30,H,L,40);",
    "日二买:=BARSLAST(BSP=2)<10;",
    "BSP30:=TDXDLL1(30,H,L,40)#MIN30;",
    "小买点:=BARSLAST(BSP30=2 OR BSP30=3)<16;",
    "日二买 AND 小买点;",
  ],
  "chan-first-sell-abc.txt": [
    "ABX:=TDXDLL1(30,H,L,160);",
    "BARSLAST(ABX=11)<10;",
  ],
  "chan-first-sell-context.txt": [
    "CTX:=TDXDLL1(30,H,L,210);",
    "QST:=MOD(INTPART(CTX/1),2)=1;",
    "ABC:=MOD(INTPART(CTX/2),2)=1;",
    "MZP:=MOD(INTPART(CTX/4),2)=1;",
    "MLW:=MOD(INTPART(CTX/8),2)=1;",
    "STD:=MOD(INTPART(CTX/32),2)=1;",
    "BARSLAST(BSP=11 AND QST AND ABC AND MZP AND MLW AND STD)<10;",
  ],
  "chan-first-sell-original.txt": [
    "POS:=TDXDLL1(30,H,L,220);",
    "MOV:=TDXDLL1(30,H,L,230);",
    "BARSLAST(BSP=11 AND POS=1 AND MOV=1)<10;",
  ],
  "chan-second-sell.txt": [
    "BSP:=TDXDLL1(30,H,L,40);",
    "BARSLAST(BSP=12)<10;",
  ],
  "chan-third-sell.txt": [
    "BSP:=TDXDLL1(30,H,L,40);",
    "BARSLAST(BSP=13)<10;",
  ],
  "chan-first-sell-standard.txt": [
    "STD:=TDXDLL1(30,H,L,200);",
    "BARSLAST(STD=-1)<10;",
  ],
  "chan-small-turn-sell.txt": [
    "XC:=TDXDLL1(40,C,V,0);",
    "STB:=TDXDLL1(30,H,L,140);",
    "BARSLAST(STB=-1)<10;",
  ],
  "chan-overlap-buy.txt": [
    "CTX:=TDXDLL1(30,H,L,210);",
    "OVL:=MOD(INTPART(CTX/2048),2)=1;",
    "BARSLAST((BSP=2 OR BSP=3) AND OVL)<10;",
  ],
  "chan-overlap-sell.txt": [
    "CTX:=TDXDLL1(30,H,L,210);",
    "OVL:=MOD(INTPART(CTX/2048),2)=1;",
    "BARSLAST((BSP=12 OR BSP=13) AND OVL)<10;",
  ],
  "chan-third-buy-original.txt": [
    "POS:=TDXDLL1(30,H,L,220);",
    "CTX:=TDXDLL1(30,H,L,210);",
    "BRK:=MOD(INTPART(CTX/4096),2)=1;",
    "BARSLAST(BSP=3 AND POS=1 AND BRK)<10;",
  ],
  "chan-third-buy-expanded.txt": [
    "AFT:=TDXDLL1(30,H,L,80);",
    "BARSLAST(BSP=3 AND AFT=1)<10;",
  ],
  "chan-third-buy-newborn.txt": [
    "AFT:=TDXDLL1(30,H,L,80);",
    "BARSLAST(BSP=3 AND AFT=2)<10;",
  ],
  "chan-third-buy-strong.txt": [
    "QLT:=TDXDLL1(30,H,L,50);",
    "CTX:=TDXDLL1(30,H,L,210);",
    "BRK:=MOD(INTPART(CTX/4096),2);",
    "BARSLAST(BSP=3 AND QLT=2 AND BRK=1)<10;",
  ],
  "chan-third-sell-original.txt": [
    "POS:=TDXDLL1(30,H,L,220);",
    "CTX:=TDXDLL1(30,H,L,210);",
    "BRK:=MOD(INTPART(CTX/4096),2)=1;",
    "BARSLAST(BSP=13 AND POS=-1 AND BRK)<10;",
  ],
  "chan-third-sell-expanded.txt": [
    "AFT:=TDXDLL1(30,H,L,80);",
    "BARSLAST(BSP=13 AND AFT=1)<10;",
  ],
  "chan-third-sell-newborn.txt": [
    "AFT:=TDXDLL1(30,H,L,80);",
    "BARSLAST(BSP=13 AND AFT=2)<10;",
  ],
  "chan-third-sell-strong.txt": [
    "QLT:=TDXDLL1(30,H,L,50);",
    "CTX:=TDXDLL1(30,H,L,210);",
    "BRK:=MOD(INTPART(CTX/4096),2);",
    "BARSLAST(BSP=13 AND QLT=2 AND BRK=1)<10;",
  ],
}


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


def strip_tdx_comments(text: str):
  return TDX_COMMENT.sub("", text)


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


def parse_registered_tdx_funcs(text: str):
  funcs = {int(item) for item in REGISTERED_FUNC_REF.findall(text)}
  if not funcs:
    return funcs, ["unable to parse registered TDX functions in Main.cpp"]
  return funcs, []


def validate_registered_tdx_funcs(funcs):
  errors = []
  missing = sorted(EXPECTED_REGISTERED_FUNCS - funcs)
  extra = sorted(funcs - EXPECTED_REGISTERED_FUNCS)
  if missing:
    errors.append(f"Main.cpp missing registered TDX functions: {missing}")
  if extra:
    errors.append(f"Main.cpp has unexpected registered TDX functions: {extra}")
  return errors


def read_registered_tdx_funcs():
  text = (ROOT / "Main.cpp").read_text(encoding="utf-8")
  return parse_registered_tdx_funcs(text)


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


def validate_readme_entry_docs(readme_text: str):
  errors = []
  for snippet in README_REQUIRED_SNIPPETS:
    if snippet not in readme_text:
      errors.append(f"README.md missing formula pack entry: {snippet}")
  return errors


def validate_readme_mode_rules(readme_text: str):
  errors = []
  for snippet in README_MODE_RULE_SNIPPETS:
    if snippet not in readme_text:
      errors.append(f"README.md missing Func30 mode rule: {snippet}")
  return errors


def validate_formula_snippets(formula_texts):
  errors = []
  for name, snippets in EXPECTED_FORMULA_SNIPPETS.items():
    text = formula_texts.get(name, "")
    for snippet in snippets:
      if snippet not in text:
        errors.append(f"{name}: missing snippet {snippet}")
  return errors


def validate_formula_aliases(formula_texts):
  errors = []
  for primary, alias in FORMULA_ALIASES:
    if formula_texts.get(primary, "") != formula_texts.get(alias, ""):
      errors.append(f"{primary} must match {alias}")
  return errors


def validate_aux_order(formula_texts):
  errors = []
  for name, text in formula_texts.items():
    active_text = strip_tdx_comments(text)
    first_func30 = FUNC30_CALL.search(active_text)
    if first_func30 is None:
      continue
    first_func40 = FUNC40_CALL.search(active_text)
    if first_func40 is None:
      errors.append(f"{name}: missing TDXDLL1(40,C,V,0) before Func30")
    elif first_func40.start() > first_func30.start():
      errors.append(f"{name}: TDXDLL1(40,C,V,0) must appear before Func30")
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

  funcs, errors = parse_registered_tdx_funcs("{1, &Func1},\n{30, &Func30},\n{40, &Func40},\n")
  if errors or funcs != {1, 30, 40}:
    print(f"self-test failed: parse registered functions {funcs!r} {errors!r}", file=sys.stderr)
    return 1
  registry_errors = validate_registered_tdx_funcs(EXPECTED_REGISTERED_FUNCS)
  if registry_errors:
    print(f"self-test failed: registered function validation {registry_errors!r}", file=sys.stderr)
    return 1
  registry_errors = validate_registered_tdx_funcs(set(range(1, 20)) | {30, 40, 50})
  expected_registry_errors = [
    "Main.cpp missing registered TDX functions: [20]",
    "Main.cpp has unexpected registered TDX functions: [50]",
  ]
  if registry_errors != expected_registry_errors:
    print(f"self-test failed: registered function mismatch {registry_errors!r}", file=sys.stderr)
    return 1
  funcs, errors = parse_registered_tdx_funcs("static PluginTCalcFuncInfo Info[] = {{0, NULL}};")
  if not errors:
    print("self-test failed: detect missing registered functions", file=sys.stderr)
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
  entry_errors = validate_readme_entry_docs("formulas/README.md formulas/chan-main.txt formulas/chan-debug.txt")
  if entry_errors:
    print(f"self-test failed: README entry docs {entry_errors!r}", file=sys.stderr)
    return 1
  entry_errors = validate_readme_entry_docs("formulas/README.md formulas/chan-main.txt")
  if entry_errors != ["README.md missing formula pack entry: formulas/chan-debug.txt"]:
    print(f"self-test failed: README entry missing {entry_errors!r}", file=sys.stderr)
    return 1
  mode_rule_errors = validate_readme_mode_rules("配置码四位只能由 `0/1` 组成 非法 mode 会输出全 0")
  if mode_rule_errors:
    print(f"self-test failed: README mode rules {mode_rule_errors!r}", file=sys.stderr)
    return 1
  mode_rule_errors = validate_readme_mode_rules("配置码四位只能由 `0/1` 组成")
  if mode_rule_errors != ["README.md missing Func30 mode rule: 非法 mode 会输出全 0"]:
    print(f"self-test failed: README mode rule missing {mode_rule_errors!r}", file=sys.stderr)
    return 1

  snippet_errors = validate_formula_snippets({
    "chan-main.txt": (
      "DLL:=TDXDLL1(30,H,L,0);\n"
      "SEG:=TDXDLL1(30,H,L,1100000);\n"
      "BSP:=TDXDLL1(30,H,L,40);\n"
      "QLT:=TDXDLL1(30,H,L,50);\n"
      "DRAWTEXT(BSP=1,LOW,'一买'),COLORWHITE;\n"
      "DRAWTEXT(BSP=11,HIGH,'一卖'),COLORGREEN;\n"
    ),
    "chan-first-buy.txt": (
      "BSP:=TDXDLL1(30,H,L,40);\n"
      "BARSLAST(BSP=1)<10;\n"
    ),
    "chan-first-buy-abc.txt": (
      "ABX:=TDXDLL1(30,H,L,160);\n"
      "BARSLAST(ABX=1)<10;\n"
    ),
    "chan-first-buy-context.txt": (
      "CTX:=TDXDLL1(30,H,L,210);\n"
      "QST:=MOD(INTPART(CTX/1),2)=1;\n"
      "ABC:=MOD(INTPART(CTX/2),2)=1;\n"
      "MZP:=MOD(INTPART(CTX/4),2)=1;\n"
      "MLW:=MOD(INTPART(CTX/8),2)=1;\n"
      "STD:=MOD(INTPART(CTX/32),2)=1;\n"
      "BARSLAST(BSP=1 AND QST AND ABC AND MZP AND MLW AND STD)<10;\n"
    ),
    "chan-first-buy-original.txt": (
      "POS:=TDXDLL1(30,H,L,220);\n"
      "MOV:=TDXDLL1(30,H,L,230);\n"
      "BARSLAST(BSP=1 AND POS=-1 AND MOV=-1)<10;\n"
    ),
    "chan-first-buy-standard.txt": (
      "STD:=TDXDLL1(30,H,L,200);\n"
      "BARSLAST(STD=1)<10;\n"
    ),
    "chan-small-turn-buy.txt": (
      "XC:=TDXDLL1(40,C,V,0);\n"
      "STB:=TDXDLL1(30,H,L,140);\n"
      "BARSLAST(STB=1)<10;\n"
    ),
    "chan-first-sell.txt": (
      "BSP:=TDXDLL1(30,H,L,40);\n"
      "BARSLAST(BSP=11)<10;\n"
    ),
    "chan-second-buy.txt": (
      "BSP:=TDXDLL1(30,H,L,40);\n"
      "BARSLAST(BSP=2)<10;\n"
    ),
    "chan-third-buy.txt": (
      "BSP:=TDXDLL1(30,H,L,40);\n"
      "BARSLAST(BSP=3)<10;\n"
    ),
    "chan-multi-buy.txt": (
      "XC:=TDXDLL1(40,C,V,0);\n"
      "BSP:=TDXDLL1(30,H,L,40);\n"
      "日二买:=BARSLAST(BSP=2)<10;\n"
      "BSP30:=TDXDLL1(30,H,L,40)#MIN30;\n"
      "小买点:=BARSLAST(BSP30=2 OR BSP30=3)<16;\n"
      "日二买 AND 小买点;\n"
    ),
    "chan-first-sell-abc.txt": (
      "ABX:=TDXDLL1(30,H,L,160);\n"
      "BARSLAST(ABX=11)<10;\n"
    ),
    "chan-first-sell-context.txt": (
      "CTX:=TDXDLL1(30,H,L,210);\n"
      "QST:=MOD(INTPART(CTX/1),2)=1;\n"
      "ABC:=MOD(INTPART(CTX/2),2)=1;\n"
      "MZP:=MOD(INTPART(CTX/4),2)=1;\n"
      "MLW:=MOD(INTPART(CTX/8),2)=1;\n"
      "STD:=MOD(INTPART(CTX/32),2)=1;\n"
      "BARSLAST(BSP=11 AND QST AND ABC AND MZP AND MLW AND STD)<10;\n"
    ),
    "chan-first-sell-original.txt": (
      "POS:=TDXDLL1(30,H,L,220);\n"
      "MOV:=TDXDLL1(30,H,L,230);\n"
      "BARSLAST(BSP=11 AND POS=1 AND MOV=1)<10;\n"
    ),
    "chan-second-sell.txt": (
      "BSP:=TDXDLL1(30,H,L,40);\n"
      "BARSLAST(BSP=12)<10;\n"
    ),
    "chan-third-sell.txt": (
      "BSP:=TDXDLL1(30,H,L,40);\n"
      "BARSLAST(BSP=13)<10;\n"
    ),
    "chan-first-sell-standard.txt": (
      "STD:=TDXDLL1(30,H,L,200);\n"
      "BARSLAST(STD=-1)<10;\n"
    ),
    "chan-small-turn-sell.txt": (
      "XC:=TDXDLL1(40,C,V,0);\n"
      "STB:=TDXDLL1(30,H,L,140);\n"
      "BARSLAST(STB=-1)<10;\n"
    ),
    "chan-overlap-buy.txt": (
      "CTX:=TDXDLL1(30,H,L,210);\n"
      "OVL:=MOD(INTPART(CTX/2048),2)=1;\n"
      "BARSLAST((BSP=2 OR BSP=3) AND OVL)<10;\n"
    ),
    "chan-overlap-sell.txt": (
      "CTX:=TDXDLL1(30,H,L,210);\n"
      "OVL:=MOD(INTPART(CTX/2048),2)=1;\n"
      "BARSLAST((BSP=12 OR BSP=13) AND OVL)<10;\n"
    ),
    "chan-third-buy-expanded.txt": (
      "AFT:=TDXDLL1(30,H,L,80);\n"
      "BARSLAST(BSP=3 AND AFT=1)<10;\n"
    ),
    "chan-third-buy-newborn.txt": (
      "AFT:=TDXDLL1(30,H,L,80);\n"
      "BARSLAST(BSP=3 AND AFT=2)<10;\n"
    ),
    "chan-third-buy-strong.txt": (
      "QLT:=TDXDLL1(30,H,L,50);\n"
      "CTX:=TDXDLL1(30,H,L,210);\n"
      "BRK:=MOD(INTPART(CTX/4096),2);\n"
      "BARSLAST(BSP=3 AND QLT=2 AND BRK=1)<10;\n"
    ),
    "chan-third-sell-original.txt": (
      "POS:=TDXDLL1(30,H,L,220);\n"
      "CTX:=TDXDLL1(30,H,L,210);\n"
      "BRK:=MOD(INTPART(CTX/4096),2)=1;\n"
      "BARSLAST(BSP=13 AND POS=-1 AND BRK)<10;\n"
    ),
    "chan-third-sell-expanded.txt": (
      "AFT:=TDXDLL1(30,H,L,80);\n"
      "BARSLAST(BSP=13 AND AFT=1)<10;\n"
    ),
    "chan-third-sell-newborn.txt": (
      "AFT:=TDXDLL1(30,H,L,80);\n"
      "BARSLAST(BSP=13 AND AFT=2)<10;\n"
    ),
    "chan-third-sell-strong.txt": (
      "QLT:=TDXDLL1(30,H,L,50);\n"
      "CTX:=TDXDLL1(30,H,L,210);\n"
      "BRK:=MOD(INTPART(CTX/4096),2);\n"
      "BARSLAST(BSP=13 AND QLT=2 AND BRK=1)<10;\n"
    ),
    "chan-third-buy-original.txt": "POS:=TDXDLL1(30,H,L,220);\n",
  })
  expected_snippet_errors = [
    "chan-third-buy-original.txt: missing snippet CTX:=TDXDLL1(30,H,L,210);",
    "chan-third-buy-original.txt: missing snippet BRK:=MOD(INTPART(CTX/4096),2)=1;",
    "chan-third-buy-original.txt: missing snippet BARSLAST(BSP=3 AND POS=1 AND BRK)<10;",
  ]
  if snippet_errors != expected_snippet_errors:
    print(f"self-test failed: formula snippets {snippet_errors!r}", file=sys.stderr)
    return 1

  alias_errors = validate_formula_aliases({
    "chan-main.txt": "same",
    "chan-all-buys.txt": "same",
  })
  if alias_errors:
    print(f"self-test failed: formula aliases {alias_errors!r}", file=sys.stderr)
    return 1
  alias_errors = validate_formula_aliases({
    "chan-main.txt": "new",
    "chan-all-buys.txt": "old",
  })
  if alias_errors != ["chan-main.txt must match chan-all-buys.txt"]:
    print(f"self-test failed: formula alias mismatch {alias_errors!r}", file=sys.stderr)
    return 1
  aux_errors = validate_aux_order({
    "ok.txt": "XC:=TDXDLL1(40,C,V,0);\nBSP:=TDXDLL1(30,H,L,40);\n",
    "comment_only.txt": "{XC:=TDXDLL1(40,C,V,0);}\nBSP:=TDXDLL1(30,H,L,40);\n",
    "missing.txt": "BSP:=TDXDLL1(30,H,L,40);\n",
    "late.txt": "BSP:=TDXDLL1(30,H,L,40);\nXC:=TDXDLL1(40,C,V,0);\n",
    "plain.txt": "CLOSE>0;\n",
  })
  expected_aux_errors = [
    "comment_only.txt: missing TDXDLL1(40,C,V,0) before Func30",
    "missing.txt: missing TDXDLL1(40,C,V,0) before Func30",
    "late.txt: TDXDLL1(40,C,V,0) must appear before Func30",
  ]
  if aux_errors != expected_aux_errors:
    print(f"self-test failed: aux order {aux_errors!r}", file=sys.stderr)
    return 1
  return 0


def main() -> int:
  missing = []
  empty = []
  undocumented = []
  invalid_modes = []
  stale_comments = []
  aux_order_errors = []
  formula_snippet_errors = []
  formula_alias_errors = []
  func30_errors = []
  func30_outputs, func30_errors = read_func30_outputs()
  registered_funcs, registered_func_errors = read_registered_tdx_funcs()
  tdx_func_errors = []
  readme_text = (ROOT / "README.md").read_text(encoding="utf-8")
  func30_doc_errors = validate_readme_func30_docs(readme_text, func30_outputs)
  readme_entry_errors = validate_readme_entry_docs(readme_text)
  readme_mode_rule_errors = validate_readme_mode_rules(readme_text)
  guide_text = (ROOT / "formulas" / "README.md").read_text(encoding="utf-8")
  registered_func_errors.extend(validate_registered_tdx_funcs(registered_funcs))
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
    scan_text = strip_tdx_comments(text) if doc.parent == (ROOT / "formulas") else text
    for match in TDXDLL_REF.finditer(scan_text):
      n_func = int(match.group(1))
      if n_func not in registered_funcs:
        tdx_func_errors.append(f"{doc.relative_to(ROOT)} -> {n_func}")
    for match in FUNC30_REF.finditer(scan_text):
      n_mode = int(match.group(1))
      error = validate_func30_mode(n_mode, func30_outputs)
      if error:
        invalid_modes.append(f"{doc.relative_to(ROOT)} -> {n_mode}: {error}")

  formula_texts = {
    path.name: path.read_text(encoding="utf-8")
    for path in formula_files
  }
  aux_order_errors = validate_aux_order({
    str(path.relative_to(ROOT)): formula_texts[path.name]
    for path in formula_files
  })
  formula_snippet_errors = validate_formula_snippets(formula_texts)
  formula_alias_errors = validate_formula_aliases(formula_texts)

  debug_text = (ROOT / "formulas" / "chan-debug.txt").read_text(encoding="utf-8")
  expected_debug_comments = [
    "中枢关系：1上涨/-1下跌/2扩展",
    "一类背驰转折：1扩展/2盘整/3反趋势",
    "三买三卖后续：1扩张/2新生",
    "上下文位图：含2048二三重合/4096首次回试",
    "胜出候选优先级：二10三20一30",
    "胜出候选中枢编号：一基",
    "胜出候选突破编号：一基",
    "胜出候选端点编号：一基",
    "胜出候选走势编号：一基",
    "C/A段MACD面积比：小于100为柱面积走弱",
    "C/A段价差力度比：小于100为空间走弱",
    "C/A段平均力度比：小于100为速度走弱",
  ]
  expected_debug_lines = [
    "DRAWNUMBER(BSP<>0 AND POS<>2,L*0.995,POS)",
    "DRAWNUMBER(BSP<>0,H*1.005,MOV)",
    "DRAWNUMBER(BSP<>0,H*1.010,PRI)",
    "DRAWNUMBER(BSP<>0 AND CEN>0,L*0.990,CEN)",
    "DRAWNUMBER(BSP<>0 AND BKO>0,H*1.015,BKO)",
    "DRAWNUMBER(BSP<>0 AND PID>0,L*0.985,PID)",
    "DRAWNUMBER(BSP<>0 AND TID>0,H*1.020,TID)",
    "DRAWNUMBER(BSP<>0 AND MAR>0,L*0.980,MAR)",
    "DRAWNUMBER(BSP<>0 AND SPR>0,H*1.025,SPR)",
    "DRAWNUMBER(BSP<>0 AND VPR>0,L*0.975,VPR)",
    "DRAWLINE(BSG=1,L,BSG=2,L,0),COLORBLUE",
    "DRAWLINE(BSG=-1,H,BSG=-2,H,0),COLORBLUE",
    "DRAWLINE(NST=1,L,NST=2,L,0),COLORCYAN",
    "DRAWLINE(NST=-1,H,NST=-2,H,0),COLORCYAN",
  ]
  for comment in expected_debug_comments:
    if comment not in debug_text:
      stale_comments.append(f"chan-debug.txt missing comment: {comment}")
  for line in expected_debug_lines:
    if line not in debug_text:
      stale_comments.append(f"chan-debug.txt missing debug line: {line}")

  if (missing or empty or undocumented or invalid_modes or stale_comments or
      aux_order_errors or formula_snippet_errors or formula_alias_errors or func30_errors or
      func30_doc_errors or readme_entry_errors or readme_mode_rule_errors or
      registered_func_errors or tdx_func_errors):
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
    for item in formula_snippet_errors:
      print(f"formula snippet error: {item}", file=sys.stderr)
    for item in formula_alias_errors:
      print(f"formula alias error: {item}", file=sys.stderr)
    for item in func30_errors:
      print(f"Func30 parser error: {item}", file=sys.stderr)
    for item in func30_doc_errors:
      print(f"Func30 documentation error: {item}", file=sys.stderr)
    for item in readme_entry_errors:
      print(f"README entry error: {item}", file=sys.stderr)
    for item in readme_mode_rule_errors:
      print(f"README mode rule error: {item}", file=sys.stderr)
    for item in registered_func_errors:
      print(f"TDX registry parser error: {item}", file=sys.stderr)
    for item in tdx_func_errors:
      print(f"unknown TDX function reference: {item}", file=sys.stderr)
    return 1
  return 0


if __name__ == "__main__":
  if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
    raise SystemExit(self_test())
  raise SystemExit(main())
