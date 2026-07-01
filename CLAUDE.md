# CLAUDE.md

本文件是 czsc-tdx 的 Claude Code 专用补充指引。**先与 `AGENTS.md` 一起阅读**：`AGENTS.md`
是跨工具的工程规则（结构、构建命令、命名风格、提交规范）；本文件只补充缠论领域知识、
架构地图与本机特有事项，不重复 `AGENTS.md`。

## 项目是什么

通达信（TDX）缠论可视化插件，编译为 32 位 Windows DLL（`CZSC.dll`）。核心算法在
`CzscCore.cpp/.h`（可测试、无 Windows 依赖）；`Main.cpp` 把函数按编号注册给通达信。
设计目标是尽量贴近缠师原文的线段、中枢、买卖点等概念。

## TDX DLL 数据契约（官方规范）

- 计算函数签名固定 `void Func(int DataLen, float*pOut, float*a, float*b, float*c)` —— **只有 3 个输入序列**，
  无回调可取更多数据（HISDAT/REPORTDAT2 等结构是「选股插件」用的，计算型 DLL 取不到）。3 个序列可为公式里
  任意表达式（`H/L/O/C/VOL/AMOUNT`）。
- **无效数 `0xF8F8F8F8`**（约 -4e34 的巨大负 float）：官方要求计算前判断并跳过。已在 4 个 H/L 摄入点
  （`BuildMergedBars`/`BuildSignalPoints`/`ComputeShortLongMa`/`AssignSegmentEnergy`）用 `SanitizeSeries`
  前向填充清洗（无效 pIn 视作无端点）。新增读 H/L/pIn 的入口须同样清洗。
- **3 槽已被 H/L+（pIn 或 mode）占满**，真实收盘价 C 与成交量 V 进不了主入口。默认用 `(H+L)/2` 代理 C；
  二者改由下方「旁路注册」按需启用。输出逐根写 `pOut[i]`，`0`=无信号 + 公式端 `IF/DRAWNULL/DRAWICON`，符合规范。

### 旁路注册数据契约（Func40 / 真实 C、V）

- `Func40`（`XC:=TDXDLL1(40,C,V,0)`）调 `RegisterAuxData` 把清洗后的 C/V 存入进程内全局单槽 `g_Aux`，
  输出透传 C；须在公式里**置于消费函数之前**（通达信按行序求值）。
- 消费端经 `GetValidatedClose/GetValidatedVolume` 取值：仅当 `g_Aux.bValid && nCount 匹配 && 每根 Close∈[L,H]`
  （真实 OHLC 必然成立）才返回真实序列，否则返回 0 → **回落 `(H+L)/2` 代理**。该内容校验天然否决跨股票/过期
  的错配数据，故未注册或注册错误都不会算错，只退化为现状（零回归）。
- 消费点：`AssignSegmentEnergy`、`ComputeShortLongMa` 用真实 C 算 MACD/均线；`An.Volume` 供湿吻放量骗线过滤
  （`ClassifyMaKissesWithVolume`，Func30 输出 13）。
- **缓存正确性**：`GetOrBuild*` 缓存键加一维 `hAux = FNV(校验通过的 C[+V])`；注册的 C 或 V 变化即失效重算，
  撤销（`RegisterAuxData(0,0,0)`）则 `hAux=0` 回落。测试用 g_Aux 全局，aux 用例须首尾 `RegisterAuxData(0,0,0)` 清场防泄漏。

## 计算流水线（形态学 → 动力学 → 买卖点）

```
原始K线 H/L
  └ BuildMergedBars   包含处理(第62/65课)
      └ BuildFractals 顶/底分型(第62课)
          └ BuildStrokes 笔(第62/65课, 同型取极值延伸/中继; 异型须跨度达标才成端点, 不足则忽略不弹出达标笔)
              └ BuildSegmentPoints 笔端点 / BuildLineSegmentPoints 启发式线段 /
                BuildLineSegmentPointsByFeature 线段(第64/67课, 逆向笔 higher high+higher low/lower low+lower
                high 即反向线段破坏; 终点是逆向笔内端, 不一定是极值)
                  └ BuildCenters 中枢(第17/20课: 不含进入段第1笔, 由第2/3/4笔重叠成枢, 带方向
                    nDirection ±1[进入向上=上升中枢/向下=下降中枢], 不与后中枢共用端点)
                      ├ BuildTrendStructures 走势类型(盘整/趋势=≥2同向中枢, 第17课)
                      ├ BuildCenterBreakouts 中枢首次离开+回试 → 三类买卖点(第20课)
                      └ BuildTradingSignalCandidates 三类买卖点候选(第20/21课)
AssignSegmentEnergy / ComputeMacdHistogram 给线段点附 MACD 柱面积(动力学, 第24课)
MeasureStrength / MeasureDivergence 力度与背驰(第15/24/27课)
```

买卖点判定（第20/21课，CzscCore.h/cpp）：**一类**针对趋势(≥2同向中枢)末端背驰，且**同一趋势只保留价格
最极端的一个**(去重，否则趋势内每个创新高/低背驰都各成一类→泛滥)；**二类**紧随一类(一买后第一个不
创新低=二买)；**三类**=中枢被首次离开(突破ZG/ZD)后回试不破ZG/ZD(上升中枢→三买、下降中枢→三卖)。
数据结构与买卖点上下文字段（质量/中枢位置/背驰-转折/三买后续、优先级、中枢/突破/端点/走势编号等）
见 `CzscCore.h` 注释。

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
| 10 | Func10 | 信号质量(0观察/1确认/2强质量) | 第24/27课 |
| 11 | Func11 | 相邻中枢关系(1上涨/-1下跌/2扩展) | 第20课中心定理二 |
| 12 | Func12 | 一类买卖点背驰-转折(1扩展/2盘整/3反趋势) | 第29课 |
| 13 | Func13 | 三类买卖点后续(1扩张/2新生) | 第21课 |
| 14 | Func14 | 一类买卖点背驰段(买1/2, 卖-1/-2) | 第27课 |
| 15 / 16 | Func15 / Func16 | 均线差(体位/力度) / 均线吻(飞/唇/湿) | 第11/15课 |
| 17 | Func17 | 即时背驰预警(1见顶/-1见底) | 第15课 |
| 18 / 19 | Func18 / Func19 | 笔(新笔标准) / 线段(特征序列法) | 第62-67课 |
| 20 | Func20 | 配置驱动端点(笔/线段中枢) | 见上「架构」 |
| 30 | Func30 | mode 统一入口(配置+输出，一步算全链路；输出21-28为胜出候选上下文/位置/走势/优先级/编号) | 见上「架构」 |
| 40 | Func40 | 旁路注册真实 C/V(透传 C)，供后续函数启用 | 见上「旁路注册数据契约」 |

新增输出函数时：在 `CzscCore.h` 声明、`CzscCore.cpp` 实现、`Main.cpp` 注册 `{n,&Funcn}`、
`README.md` 补公式、`tests/` 加用例。可配置的分支优先并入 `CzscConfig` 经 `Func20` 暴露。

## 本机构建与测试（重要：无 make/g++/mingw）

本 Windows 机器只装了 clang，Makefile 的 `make test` 用不了。跑核心回归：

```bash
cd D:/github/czsc-tdx
"/c/Program Files/LLVM/bin/clang++" -O2 -finput-charset=UTF-8 \
  -o tests/CzscCoreTests.exe CzscCore.cpp CzscAnalyzer.cpp tests/CzscCoreTests.cpp
./tests/CzscCoreTests.exe; echo $?   # exit 0 = 全过
```

源码已部分模块化：`CzscAnalyzer.cpp` 持有中心化分析器与缓存层（`CzscCore.cpp` 仍含其余流水线
与 Func 导出）。新增模块时同步 `Makefile` 的 `OBJECT1`/`TEST_OBJECTS` 与上面的 clang 命令。

- `Main.cpp` 因 `FxIndicator.h` 含 windows.h，clang 仅能 `-fsyntax-only` 检查；真正的 DLL 构建
  走 WSL2 MinGW（`make mingw32`，本机 `wsl.exe -e bash -lc 'cd /mnt/d/github/czsc-tdx && make mingw32'`）。
  **不要为让 clang 过编译而改 pack/windows 头。**
- **发布产物统一在 `build/`，32/64 双版本并存**：`make mingw32` → `build/CZSC.dll`（PE32，给 32 位通达信）；
  `make mingw64` → `build/CZSC64.dll`（PE32+，给 64 位通达信，`x86_64-w64-mingw32-` 工具链，`TARGET1` 覆盖为
  CZSC64.dll）。两版同源，导出/公式一致，仅指针宽度不同（`pack(1)`+cdecl 随架构自然移植，无需改头）。
  链接均加 `-static -static-libgcc -static-libstdc++` 使 DLL 自包含（仅依赖 `KERNEL32`/`msvcrt`），
  免在通达信机器另装 MinGW 运行时。两个 DLL 作为正式包**已纳入 git 跟踪**（覆盖发布即重新构建提交）。
  打包/校验：`sh scripts/build-mingw32.sh` / `sh scripts/build-mingw64.sh`（均先 `make clean` 再 check→原生测试→
  Win 测试构建→打 DLL）；`<prefix>objdump -p build/CZSC*.dll | grep "DLL Name"` 应只剩 KERNEL32/msvcrt。

## 测试约定

`tests/CzscCoreTests.cpp` 用纯返回码风格：`main()` 里 `if(!TestX()) return N;`，新用例返回码递增。
信号编码约定：买 1/2/3、卖 11/12/13；构造测试中枢用 `MakeTestCenter`（4参，自动令 GG=fHigh、
DD=fLow）或 `MakeTestCenterFull`（6参，可指定 GG/DD）。

**真实数据测试**：`tests/SseIndexDaily.h` 是从通达信拉取的上证指数(000001.SH) 500 根日线(前复权)，
笔/线段类测试优先用它验证**结构性质**（顶底交替、方向、严格笔合并跨度≥4、线段是笔端点子集且更高级别），
而非脆弱的小手工 fixture（手工 fixture 易因笔/线段判据调整而需重算）。刷新数据见 `scripts/fetch-sse-data.py`
（须在通达信 PYPlugins 环境运行）。新增笔/线段算法改动后，跑诊断看真实数据上笔数/线段数是否合理。

## 缠论知识来源

理论以 `chan-theory` skill（chzhshch-108-plus 原文要点）为准，所有定义/定理标注课文编号可溯源。
注释里引用课文编号（如「第24课」）即指该体系，避免互联网二手解读。

## 进度跟踪

`todos.json`（已 gitignore，本地文件）记录多轮优化路线图的「做了/测过/提交了/暂缓」状态，
可作为后续轮次的延续锚点。提交按「一个特性一组提交」拆分（特性代码与测试可分两次提交）。
