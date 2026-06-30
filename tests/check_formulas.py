import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = [ROOT / "README.md", ROOT / "formulas" / "README.md"]
FORMULA_REF = re.compile(r"(?:formulas[\\/])?(chan-[A-Za-z0-9_-]+\.txt)")


def main() -> int:
  missing = []
  empty = []
  for doc in DOCS:
    text = doc.read_text(encoding="utf-8")
    for name in sorted(set(FORMULA_REF.findall(text))):
      path = ROOT / "formulas" / name
      if not path.exists():
        missing.append(f"{doc.relative_to(ROOT)} -> {name}")
      elif path.stat().st_size == 0:
        empty.append(str(path.relative_to(ROOT)))

  if missing or empty:
    for item in missing:
      print(f"missing formula: {item}", file=sys.stderr)
    for item in empty:
      print(f"empty formula: {item}", file=sys.stderr)
    return 1
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
