#!/usr/bin/env python3
"""公式包静态校验：每个 TDXDLL1 调用的编号须在 Main.cpp 注册，配置码须合法，注释花括号成对。"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CALL = re.compile(r"TDXDLL1\s*\(\s*(\d+)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*\)")
REGISTERED = re.compile(r"\{\s*(\d+)\s*,\s*&tdx::")


def registered_numbers():
    return {int(n) for n in REGISTERED.findall((ROOT / "Main.cpp").read_text(encoding="utf-8"))}


def valid_config(text):
    if not re.fullmatch(r"\d+", text):
        return False
    code = int(text)
    digits = [code // 10 ** k % 10 for k in range(4)]
    return code <= 9999 and digits[0] <= 2 and all(d <= 1 for d in digits[1:])


def check(path, numbers):
    errors = []
    text = path.read_text(encoding="utf-8")
    if text.count("{") != text.count("}"):
        errors.append("花括号注释不成对")
    calls = CALL.findall(text)
    if not calls:
        errors.append("没有 TDXDLL1 调用")
    for number, a, b, c in calls:
        n = int(number)
        if n not in numbers:
            errors.append(f"编号 {n} 未在 Main.cpp 注册")
        elif n == 40:
            if (a, b, c) != ("C", "V", "0"):
                errors.append("40 号须写成 TDXDLL1(40,C,V,0)")
        else:
            if (a, b) != ("H", "L"):
                errors.append(f"{n} 号前两个输入须为 H,L")
            if not valid_config(c):
                errors.append(f"{n} 号配置码 {c} 非法")
    return errors


def main():
    numbers = registered_numbers()
    failed = False
    files = sorted((ROOT / "formulas").glob("*.txt"))
    for path in files:
        for error in check(path, numbers):
            failed = True
            print(f"{path.name}: {error}")
    print(f"checked {len(files)} formulas against functions {sorted(numbers)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
