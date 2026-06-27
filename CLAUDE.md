# CLAUDE.md

本文件是 czsc-tdx 的 Claude Code 专用补充指引。**先与 `AGENTS.md` 一起阅读**：`AGENTS.md`
是跨工具的工程规则（结构、构建命令、命名风格、提交规范）；本文件只补充缠论领域知识、
架构地图与本机特有事项，不重复 `AGENTS.md`。

## 项目是什么

通达信（TDX）缠论可视化插件，编译为 32 位 Windows DLL（`CZSC.dll`）。核心算法在
`CzscCore.cpp/.h`（可测试、无 Windows 依赖）；`Main.cpp` 把函数按编号注册给通达信。
设计目标是尽量贴近缠师原文的线段、中枢、买卖点等概念。

## 计算流水线（形态学 → 动力学 → 买卖点）

```
原始K线 H/L
  └ BuildMergedBars   包含处理(第62/65课)
      └ BuildFractals 顶/底分型(第62课)
          └ BuildStrokes 笔(第62课)
              └ BuildSegmentPoints / BuildLineSegmentPoints 线段点 / 线段(第67/71课)
                  └ BuildCenters 中枢(第17/18课, 含 ZG/ZD 与全幅 GG/DD)
                      ├ BuildTrendStructures 走势类型(盘整/趋势, 第17课)
                      ├ BuildCenterBreakouts 中枢突破 → 三类买卖点(第20课)
                      └ BuildTradingSignalCandidates 三类买卖点候选(第20/21课)
AssignSegmentEnergy / ComputeMacdHistogram 给线段点附 MACD 柱面积(动力学, 第24课)
MeasureStrength / MeasureDivergence 力度与背驰(第15/24/27课)
```

数据结构与买卖点上下文字段（质量/中枢位置/背驰-转折/三买后续等）见 `CzscCore.h` 注释。

## 架构：CzscAnalyzer + 缓存 + 配置 + Func30

- **中心化 `CzscAnalyzer`**（`CzscCore.h`）一次算成全部结果（Points/Centers/Structures/Breakouts/
  Candidates/MaShort/MaLong/Kiss）。两个 Build 入口：`BuildAnalyzerFromSignal`（pIn 家族）与
  `BuildAnalyzerFromPrice`（H/L+config 家族），`Points` 就绪后共用私有 `BuildCentersStage`。
  各 Func 只做**投影**，不再各自重跑流水线。
- **缓存层** `GetOrBuildSignalAnalyzer` / `GetOrBuildPriceAnalyzer`：按 nCount + 全字节 FNV-1a 指纹
  （H/L 与 pIn 或 config 码）做**单槽**缓存；通达信就同一序列连调多 Func 时，首调用算、其余命中。
  统一在 analyzer 内 `AssignSegmentEnergy`（不读 fEnergy 的输出不受影响 → 等价）。
- **配置 `CzscConfig`**（四维：`nStrokeType` 笔类型 / `nStrokeEnd` 笔结束 / `nCenterUnit` 中枢构件 /
  `nSegmentMethod` 线段法）。`DefaultConfig()` 复现现状，`DecodeConfig(码)` 按 个位/十位/百位/千位 解码。
- **`Func30` mode 统一入口**（`TDXDLL1(30,H,L,mode)`，`mode = 配置码*1000 + 输出*10`）：内部走带缓存的
  `BuildAnalyzerFromPrice` 一步算全链路再投影，等价于旧 Func1→Func2..5 连调（`TestFunc30MatchesLegacyPipeline`）。

新增可选分支时**优先扩展 `CzscConfig` 并经 `Func30`/`Func20` 暴露，保持默认=现状以零回归**；
新增信号/中枢类计算时让 Func 走 `GetOrBuild*` 投影，而非自建流水线。

## 通达信导出函数（编号见 `Main.cpp` 的 `Info[]`，公式示例见 `README.md`）

| 编号 | 函数 | 输出 | 缠论依据 |
|------|------|------|----------|
| 1 / 9 | Func1 / Func9 | 线段点 / 线段(笔) | 第62-71课 |
| 2 / 3 / 4 | Func2-4 | 中枢上沿 / 下沿 / 起止 | 第17/18课 |
| 5 / 6 | Func5 / Func6 | 三类买卖点 / 形态买卖点 | 第20/21课 |
| 7 / 8 | Func7 / Func8 | 线段强度 / 斜率 | — |
| 10 | Func10 | 信号质量(0观察/1确认/2标准背驰) | 第24/27课 |
| 11 | Func11 | 相邻中枢关系(1上涨/-1下跌/2扩展) | 第20课中心定理二 |
| 12 | Func12 | 一类买卖点背驰-转折(1扩展/2盘整/3反趋势) | 第29课 |
| 13 | Func13 | 三类买卖点后续(1扩张/2新生) | 第21课 |
| 14 | Func14 | 一类买卖点背驰段(买1/2, 卖-1/-2) | 第27课 |
| 15 / 16 | Func15 / Func16 | 均线差(体位/力度) / 均线吻(飞/唇/湿) | 第11/15课 |
| 17 | Func17 | 即时背驰预警(1见顶/-1见底) | 第15课 |
| 18 / 19 | Func18 / Func19 | 笔(新笔标准) / 线段(特征序列法) | 第62-67课 |
| 20 | Func20 | 配置驱动端点(笔/线段中枢) | 见上「架构」 |
| 30 | Func30 | mode 统一入口(配置+输出，一步算全链路) | 见上「架构」 |

新增输出函数时：在 `CzscCore.h` 声明、`CzscCore.cpp` 实现、`Main.cpp` 注册 `{n,&Funcn}`、
`README.md` 补公式、`tests/` 加用例。可配置的分支优先并入 `CzscConfig` 经 `Func20` 暴露。

## 本机构建与测试（重要：无 make/g++/mingw）

本 Windows 机器只装了 clang，Makefile 的 `make test` 用不了。跑核心回归（当前 85 用例）：

```bash
cd D:/github/czsc-tdx
"/c/Program Files/LLVM/bin/clang++" -O2 -finput-charset=UTF-8 \
  -o tests/CzscCoreTests.exe CzscCore.cpp CzscAnalyzer.cpp tests/CzscCoreTests.cpp
./tests/CzscCoreTests.exe; echo $?   # exit 0 = 全过
```

源码已部分模块化：`CzscAnalyzer.cpp` 持有中心化分析器与缓存层（`CzscCore.cpp` 仍含其余流水线
与 Func 导出）。新增模块时同步 `Makefile` 的 `OBJECT1`/`TEST_OBJECTS` 与上面的 clang 命令。

- `Main.cpp` 因 `FxIndicator.h` 含 windows.h，clang 仅能 `-fsyntax-only` 检查；真正的 DLL 构建
  走 WSL2 MinGW（`make mingw32`）。**不要为让 clang 过编译而改 pack/windows 头。**
- `CZSC.dll` 是构建产物，非发布不要替换/提交。

## 测试约定

`tests/CzscCoreTests.cpp` 用纯返回码风格：`main()` 里 `if(!TestX()) return N;`，新用例返回码递增。
信号编码约定：买 1/2/3、卖 11/12/13；构造测试中枢用 `MakeTestCenter`（4参，自动令 GG=fHigh、
DD=fLow）或 `MakeTestCenterFull`（6参，可指定 GG/DD）。

## 缠论知识来源

理论以 `chan-theory` skill（chzhshch-108-plus 原文要点）为准，所有定义/定理标注课文编号可溯源。
注释里引用课文编号（如「第24课」）即指该体系，避免互联网二手解读。

## 进度跟踪

`todos.json`（已 gitignore，本地文件）记录多轮优化路线图的「做了/测过/提交了/暂缓」状态，
可作为后续轮次的延续锚点。提交按「一个特性一组提交」拆分（特性代码与测试可分两次提交）。
