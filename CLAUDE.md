# CLAUDE.md

本文件是 czsc-tdx 的 Claude Code 补充指引，**先与 `AGENTS.md` 一起阅读**（结构、命令、风格、提交规范在那里）。
这里只补充缠论领域知识、架构要点与本机特有事项。

## 项目是什么

通达信缠论插件（`build/CZSC.dll` 32 位 / `build/CZSC64.dll` 64 位）。`core/` 是纯领域引擎，`tdx/` 是投影层，
`Main.cpp` 按函数号注册。设计目标：贴近缠师原文，且信号**当下可知、无未来函数**。

## 计算流水线（形态学 → 结构 → 动力学 → 买卖点）

```
Series(H,L,[C,V])
  └ BarMerger 包含处理(第62/65课) → DetectFractals 顶底分型(第62课，右侧首根非包含K线即定型 confirmedAt)
      └ StrokeStream 笔端点(第62/65课：同型取极值延伸；异型须跨度+价位推进；czsc 笔另否决分型K线互含)
          └ SegmentStream 线段(第65/67/71课：特征序列/启发式；新段确立前起点被破则前段延续)
              └ CenterStream 中枢(第17/18/20课：[ZD,ZG] 成枢即固定；离开段后回抽不回即破坏，离开段归下一中枢)
                  └ UpdateMovements 走势类型(第17课：连续同向中枢关系=趋势) · Relate 中心定理二(第20课)
                      └ SignalStream 买卖点(第20/21/24/27/29/37课) + 出现/失效事件
BuildEnergyTables：MACD 累积/红/绿柱与 DIF/DEA（因果 EMA）；MeasureStrength 向上看红柱、向下看绿柱(第24课)
```

买卖点：**一类**=至少两个同向中枢的趋势，最后中枢外创新低/新高且 c（最后一次回到中枢后的离开段）相对
b（前中枢终点→最后中枢起点）背驰；同一中枢区只留最极端者（全量口径）。**二类**=一类后第二段不创新低/新高。
**三类**=首次离开中枢后回试不回 [ZD,ZG]。同根取胜：一类 30 > 三类 20 > 二类 10。

## 两种口径（最重要的设计约束）

- **当下事件**：`Analyze` 在每个分型成立时刻推进各层流，对“端点已被下一端点确认”的信号做出现/失效差分。
  全量结果只保留事后胜出的信号（幸存者偏差），当下失败的一类点只在事件里可见——回测/选股只能用事件。
- **当前结构**：最终快照（笔、线段、中枢、走势），最后一段会随新数据改变，只用于画图。
- 流的续算靠**读取视界**（`Horizon`）：检查点记录至此读到的最远输入；输入从 d 变化时，从视界 < d 的最后
  检查点续算。判断若依赖“数据到头”则标无界。视界过宽会退化为平方级（曾因线段缺口情形标无界而发生）。

## 通达信接口约束

- 签名固定 `void f(int count, float *out, float *a, float *b, float *c)`，只有 3 个输入：H、L、配置码。
- 真实收盘价/成交量走 40 号注册（`tdx/exports.cpp` 边界全局），须逐根落在 [L,H] 才被采用，否则回落 (H+L)/2。
- 无效数 `0xF8F8F8F8` 由 `Series::FromRaw` 清洗（向前填充）。
- 配置码十进制位：个位笔(0/1/2)、十位笔结束、百位中枢构件、千位线段法；非法输出全 0。

## 本机构建与测试（无 make/g++/mingw，只有 clang）

```bash
cd D:/github/czsc-tdx
"/c/Program Files/LLVM/bin/clang++" -std=c++17 -O2 -finput-charset=UTF-8 -I. \
  -o tests/unit/ChanTests.exe core/*.cpp tdx/*.cpp tests/unit/*.cpp
./tests/unit/ChanTests.exe; echo $?      # 退出码 = 失败检查数；可加子串参数只跑部分用例
```

- 更新 golden：`CHAN_UPDATE_GOLDEN=1 ./tests/unit/ChanTests.exe Golden`，并人工核对 `tests/unit/golden/sse.txt` 的 diff。
- DLL 与完整 `make test`/`make release` 走 WSL：`wsl.exe -e bash -lc 'cd /mnt/d/github/czsc-tdx && make release'`。
- `Main.cpp` 含 windows.h，clang 只能 `-fsyntax-only`；不要为此改 pack/windows 头。`build/` 不入库。
- `compile_flags.txt` 供 clangd 以 C++17 解析头文件。

## 缠论知识来源

以 `chan-theory` skill（chzhshch-108-plus 原文要点）为准，注释引用课文编号。原文未定义处的口径集中在
`docs/chan-ambiguity-decisions.md`，改口径须同步该表、注释与测试。

## 进度跟踪

`todos.json`（gitignore）记录各轮路线图；提交按“一个特性一组提交”。
