# -*- coding: utf-8 -*-
"""
拉取上证指数(000001.SH)日线，生成 tests/SseIndexDaily.h 供笔/线段真实数据测试。

依赖通达信的 tqcenter 库（随通达信 PYPlugins 提供），须在能 import tqcenter 的环境运行：
  1. 启动通达信客户端并登录、下载好上证指数日线盘后数据；
  2. 把本脚本复制到 通达信目录\\PYPlugins\\user\\ 下（与 tqcenter.py 同级）；
  3. 用该环境的 Python（如 C:\\Python312\\python）运行：python fetch-sse-data.py
脚本会把生成的头文件写回本仓库 tests/SseIndexDaily.h（OUT 为绝对路径，按需修改）。
"""
from tqcenter import tq

OUT = r"D:\github\czsc-tdx\tests\SseIndexDaily.h"
COUNT = 500
CODE = "000001.SH"

tq.initialize(__file__)
df = tq.get_market_data(
    field_list=["High", "Low", "Close", "Volume", "Amount"],
    stock_list=[CODE],
    count=COUNT,
    period="1d",
    dividend_type="front",
    fill_data=True,
)
tq.close()


def col(name):
    if name not in df:
        return None
    return [float(v) for v in df[name][CODE].values]


highs = col("High")
lows = col("Low")
closes = col("Close")
vols = col("Volume") or col("Amount")
n = len(highs)


def arr(name, data):
    return "static const float %s[SSE_DAILY_COUNT] = {%s};" % (
        name, ",".join("%.2f" % x for x in data))


lines = [
    "// 上证指数(000001.SH) %d 根日线(前复权)，从通达信拉取，供笔/线段真实数据测试" % n,
    "#ifndef SSE_INDEX_DAILY_H",
    "#define SSE_INDEX_DAILY_H",
    "static const int SSE_DAILY_COUNT = %d;" % n,
    arr("SSE_DAILY_HIGH", highs),
    arr("SSE_DAILY_LOW", lows),
    arr("SSE_DAILY_CLOSE", closes),
]
if vols is not None:
    lines.append(arr("SSE_DAILY_VOLUME", vols))
lines.append("#endif")

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
print("wrote", n, "bars to", OUT)
