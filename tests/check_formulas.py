import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = [ROOT / "README.md", ROOT / "formulas" / "README.md"]
FORMULA_REF = re.compile(r"(?:formulas[\\/])?(chan-[A-Za-z0-9_-]+\.txt)")
FUNC30_REF = re.compile(r"TDXDLL1\s*\(\s*30\s*,\s*H\s*,\s*L\s*,\s*([0-9]+)\s*\)")
FUNC30_OUTPUTS = set(range(0, 24))


def is_valid_config(n_config: int) -> bool:
  for _ in range(4):
    if (n_config % 10) not in (0, 1):
      return False
    n_config //= 10
  return n_config == 0


def main() -> int:
  missing = []
  empty = []
  undocumented = []
  invalid_modes = []
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
      n_output = (n_mode % 1000) // 10
      n_config = n_mode // 1000
      if (n_mode % 10) != 0:
        invalid_modes.append(f"{doc.relative_to(ROOT)} -> {n_mode}: mode must end with 0")
      elif n_output not in FUNC30_OUTPUTS:
        invalid_modes.append(f"{doc.relative_to(ROOT)} -> {n_mode}: unknown output {n_output}")
      elif not is_valid_config(n_config):
        invalid_modes.append(f"{doc.relative_to(ROOT)} -> {n_mode}: invalid config {n_config}")

  if missing or empty or undocumented or invalid_modes:
    for item in missing:
      print(f"missing formula: {item}", file=sys.stderr)
    for item in empty:
      print(f"empty formula: {item}", file=sys.stderr)
    for item in undocumented:
      print(f"undocumented formula: formulas/README.md -> {item}", file=sys.stderr)
    for item in invalid_modes:
      print(f"invalid Func30 mode: {item}", file=sys.stderr)
    return 1
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
