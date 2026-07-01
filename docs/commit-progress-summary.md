# czsc-tdx 近 300 个提交完成情况汇总

生成日期：2026-07-02
基准提交：`346322e refactor: split czsc core modules`
统计范围：原始近 300 个提交覆盖 `81c131c` 到 `1bf2780`；当前近 20 个提交覆盖 `ef79aa3` 到 `346322e`，其中 `346322e` 为结构重构，`ef79aa3` 到 `cf8fd7f` 为结构重构前的算法/发布增量。
依据：`git log -20 --date=short --stat`、`git log -20 --name-only`、`346322e` 提交 diff、当前源码/公式/测试入口。

## 总体结论

这 300 个提交已经把项目从“核心逻辑可读化 + 少量 DLL 输出”推进到一个可回归、可发布、可在通达信公式层直接使用的 CZSC 插件体系。当前主线已经具备：

- 32/64 位 TongDaXin DLL 发布产物：`build/CZSC.dll`、`build/CZSC64.dll`。
- 统一入口 `Func30(mode)`，旧 `Func1-20` 和 `Func40(C,V)` 继续保留。
- 源码已按职责拆分为 `include/` 接口层和 `src/` 实现层，`CzscCore.h` 保留兼容聚合入口。
- 可配置的分型/笔/线段/中枢/走势/买卖点分析管线。
- 买卖点候选的上下文、质量、所属中枢、突破、走势、背驰动力学和端点编号诊断。
- 主图、调试、三类买卖点、动力学、标准背驰、小转大、多级别共振等公式包。
- `make test`、公式检查、SSE golden 文件、release DLL 检查组成的回归链路。

## 最新结构重构增量补充

提交 `346322e refactor: split czsc core modules` 已完成一次不改变功能的工程结构拆分。该提交没有新增或删除 TDX 公共函数、没有改变 `Func30` 输出编号、没有刷新正式 release DLL；它只把原来集中在 `CzscCore.cpp` / `CzscCore.h` 的核心实现拆到更清晰的模块边界，便于下一轮计划继续开发和测试。

### 新源码结构

| 层级 | 路径 | 责任 |
| --- | --- | --- |
| 兼容入口 | `CzscCore.h` | 聚合 include，保证既有测试和 `Main.cpp` 调用不需要迁移 |
| 类型层 | `include/CzscTypes.h` | 枚举、配置、K 线、分型、笔、线段点、中枢、走势、候选、区间套、`CzscAnalyzer` |
| 公共接口层 | `include/Czsc*.h` | 按形态、中枢、动力学、买卖点、区间套、分析器、TDX 导出拆分声明 |
| 内部共享层 | `include/CzscInternal.h` | 跨实现文件共享的常量、输入清洗、输出清零、端点/区间 helper |
| 实现层 | `src/CzscCommon.cpp` | 通达信无效值清洗、旁路 C/V 注册、配置解码 |
| 实现层 | `src/CzscMorphology.cpp` | 包含处理、分型、笔、启发式线段、特征序列线段、配置端点 |
| 实现层 | `src/CzscCenter.cpp` | 中枢、走势结构、中枢关系/生命周期、中枢投影 |
| 实现层 | `src/CzscDynamics.cpp` | MACD、均线、吻、力度、背驰、即时背驰 |
| 实现层 | `src/CzscTrading.cpp` | 一二三类买卖点候选、质量、上下文、投影输出 |
| 实现层 | `src/CzscNestedDivergence.cpp` | 区间套背驰上下文和输出 |
| 实现层 | `src/CzscAnalyzer.cpp` | 中心化分析器和缓存 |
| 实现层 | `src/CzscTdxExports.cpp` | `Parse1/2`、`Func1-20/30/40` TDX 适配层 |

### 结构重构完成情况

- `Makefile` 已改为 `CORE_OBJECTS`，`make test`、SSE dump、DLL 链接都使用同一组 `src/*.o`。
- `tests/check_formulas.py` 已从 `src/CzscTdxExports.cpp` 解析 `Func30` switch，并保留旧 `CzscCore.cpp` 回退路径。
- `AGENTS.md`、`CLAUDE.md` 已同步新目录结构和本机 clang 验证命令。
- 已验证 `python tests/check_formulas.py --self-test`、`python tests/check_formulas.py`、`make test`、临时 MinGW 32 位 DLL 链接和 `git diff --check`。
- 根目录 `CzscCore.cpp`、`CzscAnalyzer.cpp` 现在只是迁移说明壳；后续实现不要继续写回这两个根文件。

## 算法/发布最近 19 个提交增量补充

以下 19 个提交是当前近 20 个提交中除 `346322e` 结构重构之外的算法/发布增量，完成了上一轮“形态学 → 中枢生命周期 → 买卖点质量 → 区间套 → 回归 → 公式发布”和本轮“背驰语义 → 背驰后回拉 → ABC 边界 → SSE 动力学回归 → DLL 发布”的连续演进。整体上，公共输出从 `Func30` 0-46 扩展到 0-54，release DLL 在 `cf8fd7f` 已刷新到最新算法实现。

### 增量清单

| 提交 | 类型 | 主要内容 | 影响范围 |
| --- | --- | --- | --- |
| `cf8fd7f` | build | 刷新 32/64 位 release DLL | `build/CZSC.dll`、`build/CZSC64.dll` |
| `2fe4f96` | test | SSE dump 增加 `DGS/RVP`，校验背驰语义、回拉端点、区间套上下文结构 | `tests/DumpSseResult.cpp`、`tests/check_sse_result.py`、SSE golden |
| `5d25181` | fix | 收紧第37课 ABC c 段关联突破，要求回试点对齐当前 C 段起点 | `CzscCore.cpp`、C++ 测试 |
| `aea2c58` | feat | 新增 `Func30` 输出 54：一类背驰后首段回拉/反弹端点编号 | 核心、README、调试公式、公式校验、投影测试 |
| `5e21a29` | feat | 新增 `Func30` 输出 53：胜出候选背驰语义，区分趋势/盘整/小转大 | 核心、README、调试公式、公式校验、投影测试 |
| `10ed77f` | docs | 瘦身公式发布说明，明确默认推荐、调试公式和兼容公式边界 | README、`formulas/README.md`、公式校验 |
| `a71e10c` | test | 强化 SSE 结构断言，人工 notes 保持结构化 | `tests/check_sse_result.py`、`tests/czsc_sse_golden_notes.md` |
| `110e4be` | build | 刷新区间套输出后的 release DLL | `build/` |
| `c597928` | feat | 新增区间套上下文输出 49-52：层级、源高级别段、低级别起止端点 | 核心、README、调试公式、SSE dump、测试 |
| `8aa2d7d` | fix | 收紧三类买卖点候选质量与方向/级别过滤 | `CzscCore.cpp`、C++ 测试 |
| `4a7aa72` | build | 刷新中枢生命周期输出后的 release DLL | `build/` |
| `8b2d18e` | feat | 新增中枢生命周期输出 47/48：延伸、扩展、上涨/下跌新生 | 核心、README、调试公式、SSE dump、测试 |
| `6862162` | build | 刷新形态学校准后的 release DLL | `build/` |
| `54ce3a7` | fix | 校准严格笔与特征序列线段，强化线段至少三笔、前三笔重叠、端点归属等约束 | `CzscCore.cpp`、C++ 测试、SSE golden |
| `1bf2780` | build | 刷新背驰端点输出后的 release DLL | `build/` |
| `fd9c7fb` | feat | 新增背驰 A/C 段起止端点输出 43-46，并同步动力学公式 | 核心、README、公式、SSE dump、测试 |
| `ce88c51` | build | 刷新小转大源点输出后的 release DLL | `build/` |
| `e597697` | feat | 新增小转大关联一类端点输出 42，完善小转大调试/选股上下文 | 核心、README、公式、SSE dump、测试 |
| `ef79aa3` | test | 校验多周期公式中周期级 `Func40` 辅助数据注册顺序 | `tests/check_formulas.py` |

### 增量完成情况

- **形态学原文校准已完成一轮**：`54ce3a7` 对严格笔、特征序列线段、线段端点归属和 SSE golden 做了实质校准，后续结构都基于这轮输出。
- **中枢生命周期已公开输出**：`8b2d18e` 增加 `Func30` 47/48，把相邻中枢关系从兼容的上涨/下跌/扩展，提升为延伸、扩展、上涨新生、下跌新生的生命周期语境。
- **三类买卖点候选进一步收紧**：`8aa2d7d` 在现有一二三类规则上减少方向、级别、上下文不匹配候选。
- **区间套上下文已连续输出**：`c597928` 增加 `Func30` 49-52，记录区间套层级、源高级别背驰段、低级别背驰段起止端点。
- **动力学调试语义继续增强**：`fd9c7fb`、`e597697`、`5e21a29`、`aea2c58` 先后补齐 A/C 段端点、小转大源点、背驰语义和背驰后回拉端点。
- **ABC c 段边界已收紧**：`5d25181` 要求 ABC 关联的三买/三卖回试点对齐当前 C 段起点，过滤发生在 C 段之前的伪 ABC。
- **SSE 回归已覆盖新增动力学上下文**：`a71e10c`、`2fe4f96` 把结构断言扩展到候选调试字段、背驰语义、回拉端点和区间套上下文。
- **公式发布治理完成一轮瘦身**：`10ed77f` 明确主图轻量、调试公式承载完整上下文、选股公式保持推荐/兼容分层。
- **发布产物已刷新**：`cf8fd7f` 的 DLL 是当前最新 release，包含 `Func30` 0-54 的公共输出。

## 提交分布

按提交消息类型粗分：

| 类型 | 数量 | 含义 |
| --- | ---: | --- |
| `test` | 78 | 核心回归、公式校验、SSE golden、上下文断言 |
| `build` | 62 | DLL 刷新、release 构建、交叉编译、产物校验 |
| `fix` | 44 | 形态、中枢、买卖点、诊断输出的边界修正 |
| `feat` | 40 | 新输出、新公式、新上下文和动力学能力 |
| `docs` | 21 | README、公式指南、构建说明和使用说明 |
| 其他 | 55 | 早期无前缀提交、数据、revert、少量重构 |

按主要路径热度粗分：

| 路径 | 出现次数 | 说明 |
| --- | ---: | --- |
| `tests/` | 256 | 回归体系是主要投入点 |
| `build/` | 151 | 发布 DLL 多次随功能刷新 |
| `formulas/` | 136 | TDX 使用体验和选股公式持续扩展 |
| `CzscCore.cpp` | 92 | 历史核心算法和投影函数主要承载处，现已拆入 `src/` |
| `README.md` | 54 | 用户安装、公式和输出说明持续同步 |
| `CzscCore.h` | 41 | 历史公共结构、配置、Func30 输出说明承载处，现为兼容聚合头 |

时间分布上，2026-07-01 有 195 个提交，主要是在已有核心能力上密集补齐诊断输出、公式校验、边界 guard 和 release 产物。

## 已完成的主线能力

### 1. 核心架构与统一入口

已完成：

- 抽出 `CzscAnalyzer`，把端点、中枢、走势、候选、均线、成交量辅助数据等集中到一次分析管线里。
- 增加分析器缓存，避免多个 DLL 输出重复计算。
- 增加 `CzscConfig` 配置维度，支持笔类型、线段算法、中枢单位等配置。
- 增加 `Func30(mode)` 统一入口，当前输出编号从 `0` 到 `46` 连续覆盖。
- 保留 `Func1-20` 旧输出兼容，`RegisterTdxFunc` 仍注册 `1-20`、`30`、`40`。
- 增加 `Func40(C,V)` 旁路注册真实收盘价和成交量，供动力学和均线类输出优先使用。
- 对 `Func30` mode 做合法性校验，非法配置和未知输出输出全 0。
- 针对多周期公式增加周期级 C/V 注册校验，避免日线和 30 分钟数据互相污染。

代表提交：

- `c0e2be6 centralize pipeline in CzscAnalyzer`
- `3c28849 cache analyzer to avoid redundant recompute`
- `a576e08 add Func30 mode-driven unified entry`
- `5bf22c3 consume real close via side-channel registration`
- `90f5e2c fix: reject invalid Func30 configs`
- `49e6b4c fix: register aux data in multi-period formula`

### 2. 形态学：包含、笔、线段

已完成：

- 增加 K 线无效值处理，识别通达信 `0xF8F8F8F8` 类异常值并清洗。
- 支持均线位置与吻分类，为早期动力学输出打基础。
- 支持严格笔/新笔配置，笔跨度从原始 K 线改到合并 K 线口径。
- 对中继分型、短反向、笔端点延伸、回滚做了多轮修正和测试。
- 增加线段特征序列法配置，并要求线段端点落在笔端点上。
- 对线段破坏规则做了多轮校准：反向特征分型、第三笔破第一笔端、higher-high/higher-low、保护点等边界。
- 保留启发式线段/旧配置作为兼容路径，同时推进特征序列法作为推荐配置。

代表提交：

- `f14ea11 add strict new-bi stroke option`
- `4ee1647 add feature sequence line segment option`
- `b923f65 redefine bi span by merged-bar count; scan reversal for segment break`
- `fb4be9b require reversal feature fractal to break a segment`
- `7cde78e redefine segment break by third-stroke through first-stroke end`
- `6ed9062 break segment by higher-high+higher-low; keep valid bi over short reversal`
- `5b83844 test: require segment endpoints match stroke points`

### 3. 中枢、走势结构与中枢关系

已完成：

- 重做中枢构造：排除入口段、不共享端点、增加方向信息。
- 恢复并修正中枢延伸逻辑，改用 `ZG/ZD` 判断重叠。
- 增加“穿越”线段封口逻辑，避免回试扩展继续误落入原中枢。
- 增加中枢关系分类，使用全幅 `GG/DD` 判断同级中枢上涨、下跌、扩展关系。
- 增加笔中枢和线段中枢 SSE 输出，并在 snapshot 中分开展示，便于人工核对。
- 增加中心关系边界测试、SSE 中枢分离测试、SSE 线段中枢测试。

代表提交：

- `33a857f rework centers: exclude entry segment, add direction, no shared endpoints; detect third buy/sell`
- `4f9766f fix: restore center extension, use ZG/ZD for overlap (not GG/DD)`
- `c2da368 fix: seal center on穿越 segment; skip retest-extension fallthrough`
- `373470b fix: classify trends by full center extent`
- `319e4ab test: cover center relation boundaries`
- `cf4fafa test: lock sse segment centers`

### 4. 三类买卖点体系

已完成：

- 三类买卖点候选统一为 `TradingSignalCandidate`，包含信号、来源、优先级、中枢、突破、走势、质量、位置、后续等上下文。
- 第一类买卖点绑定趋势结构，避免无趋势或未来走势误参与。
- 第一类买卖点按趋势极值去重，避免同一趋势内信号泛滥。
- 第二类买卖点保留“一类后第二段次级别走势”的端点关系，并输出二类关联一类端点和中间反向端点。
- 第三类买卖点绑定“首次离开 + 首次回试不回中枢”，并输出突破编号、离开端点、回试端点。
- 增加二三类重合标记和上下文位图。
- 增加三买/三卖后续中枢扩张、新生输出。
- 增加信号质量、相对中枢位置、所属走势类型、候选优先级、所属中枢编号、信号端点编号、走势编号等诊断输出。
- 对无效信号、方向不匹配、缺中枢、缺突破、缺走势、非首次回试、边界回试等情况增加 guard。

代表提交：

- `f794c43 dedup first buy/sell: one per trend at price extreme`
- `b33e936 fix: require trend for first signals`
- `5552c0a test: enforce first retest for third signals`
- `f547cdf fix: require center for third candidates`
- `898130d feat: expose second signal context ids`
- `5e063b9 feat: expose breakout point ids`
- `1b96d91 feat: expose signal point trend ids`
- `225b2ca feat: expose signal center context ids`

### 5. 背驰、动力学、ABC、小转大

已完成：

- 均线体系：短长均线差、吻分类、带成交量校验的湿吻骗线提示。
- 即时背驰预警：用未完成末段与前一同向完成段做第15课趋势平均力度预警。
- 趋势背驰：第一类买卖点必须有趋势上下文，标准趋势背驰要求方向匹配、基础背驰成立、创新极值、MACD 柱面积走弱、黄白线高度走弱、B 中枢回零和 ABC 结构。
- MACD 辅助诊断：黄白线高度走弱、B 中枢回拉 0 轴、MACD 柱面积比。
- 力度诊断：C/A 价差力度比、平均力度比、背驰要素位图。
- 第37课 ABC 结构：输出一类背驰的 ABC 结构标记、严格 ABC 买卖点、ABC 关联突破编号及其离开/回试端点。
- 第44课小转大：输出小转大必要条件标记、三买/三卖突破离开/回试端点、小级别一类源点编号。
- 区间套：输出高级别背驰段内低级别一类背驰段。
- 最新完成：输出胜出候选背驰 A 段起点/终点、C 段起点/终点编号，便于图上核对 A-B-C 的两段同向走势边界。

代表提交：

- `e229a94 add instant trend force divergence warning`
- `71f310b feat: track macd line weakness`
- `cd72b8b feat: mark macd zero pullback`
- `dde8110 feat: add standard divergence output`
- `3d44570 feat: mark abc divergence structure`
- `c59ffc0 feat: add strict abc signal output`
- `dbb35e4 feat: mark small-turn conditions`
- `964e358 feat: expose MACD area ratio diagnostic`
- `dfa43ef feat: expose divergence strength ratios`
- `74b988b feat: expose divergence condition flags`
- `25e92b4 feat: expose abc breakout point ids`
- `7884b9c feat: expose small turn point ids`
- `e597697 feat: expose small turn source point id`
- `fd9c7fb feat: expose divergence segment point ids`

### 6. 通达信公式包

已完成：

- 主图公式：`chan-main.txt`，旧名 `chan-all-buys.txt` 保持兼容。
- 调试公式：`chan-debug.txt`，展示买卖点、质量、走势、中枢、突破、背驰、ABC、小转大、端点编号等上下文。
- 基础选股：一买、二买、三买、一卖、二卖、三卖。
- 原文位置口径公式：一买/一卖、三买/三卖 original 版本。
- 标准背驰公式：一买/一卖 standard，要求标准趋势背驰和 ABC 端点上下文。
- 动力学公式：一买/一卖 dynamics，要求创新极值、空间/速度/MACD 柱面积均走弱，并且 A/C 端点顺序可复核。
- 上下文位图公式：一买/一卖 context。
- 小转大公式：买/卖小转大。
- 二三类重合公式：overlap buy/sell。
- 三买/三卖强质量、扩张、新生公式。
- 多级别共振公式：日线二买叠加 30 分钟二/三买，且已修正多周期 C/V 注册顺序。
- 所有公式由 `tests/check_formulas.py` 校验引用、mode、辅助数据顺序、文档入口和关键片段。

当前公式目录包含 30 个以上可导入公式，核心入口统一推荐 `TDXDLL1(30,H,L,mode)`。

### 7. 测试与回归框架

已完成：

- `tests/CzscCoreTests.cpp` 覆盖核心算法、配置、输出投影、买卖点上下文和大量边界条件。
- `tests/SseIndexDaily.h` 引入真实上证指数日线数据，当前数据从 2018-01-26 附近开始，用于实际走势核对。
- `tests/DumpSseResult.cpp` 生成完整 SSE 人工核对输出，包括笔端点、线段端点、笔中枢、线段中枢、买卖点候选和上下文。
- `tests/czsc_sse_result.txt` 作为可复核 golden 输出，`tests/czsc_sse_golden_notes.md` 保留人工标注/关注点。
- `tests/check_sse_result.py` 自动比对 SSE golden，并校验候选上下文、突破端点、ABC 端点、小转大端点、A/C 背驰端点顺序等结构一致性。
- `tests/check_formulas.py` 自动校验公式包、README 引用、Func30 输出文档、注册函数范围、TDX 函数引用、Func40 先于 Func30 的辅助数据顺序。
- `make test` 已串联公式检查、SSE golden 检查和 C++ 测试。

代表提交：

- `27039ac test: add reproducible SSE baseline`
- `d800cd0 test: check sse golden output`
- `c43967f test: derive func30 formula outputs`
- `e50c506 test: validate tdx function references`
- `b651883 test: require func30 output docs`
- `64f4e9b test: dump breakout point ids`
- `e4d501f test: validate abc breakout context`
- `ef79aa3 test: validate period-specific aux registration`

### 8. 构建、发布和产物校验

已完成：

- WSL MinGW 32 位构建脚本：`scripts/build-mingw32.sh`。
- WSL MinGW 64 位构建脚本：`scripts/build-mingw64.sh`。
- release 构建脚本：`scripts/build-release.sh`。
- release DLL 校验脚本：`scripts/check-release-dlls.sh`。
- `make release` 会清理、跑核心测试、交叉构建测试代码、生成 `build/CZSC.dll` 和 `build/CZSC64.dll`，并检查 PE32/PE32+ 产物和静态依赖。
- release DLL timestamp 归零，构建趋于可复现。
- `build/CZSC.dll`、`build/CZSC64.dll` 随功能变更多次刷新，最新 release 刷新点是 `cf8fd7f`；`346322e` 仅做源码结构拆分，未刷新 DLL。

代表提交：

- `483d4db package self-contained dll under build/`
- `e712e74 build 64-bit dll for x64 tongdaxin`
- `bc9124d build: add release dll checks`
- `126ca0d build: make release dlls reproducible`
- `2a9f62e build: add release build target`
- `1bf2780 build: refresh release dlls`

### 9. 文档与用户使用体验

已完成：

- README 已覆盖 DLL 安装、32/64 位选择、公式导入、`Func30` mode 规则、`Func40` 使用、主要输出说明。
- `formulas/README.md` 写成面向普通通达信用户的导入指南。
- 主图、调试、选股公式分层，避免看盘公式过重。
- 多次同步 README 与公式包，确保文档中的 mode 和公式实际存在。
- 项目级 `AGENTS.md`、`CLAUDE.md` 记录构建、测试、编码风格、发布注意事项。

## 当前公共输出能力快照

`Func30(mode)` 当前主要输出：

- `0-9`：端点、中枢、买卖点、质量、中枢关系、背驰转折、三买后续、背驰段。
- `10-13`：均线差、吻、即时背驰预警、带量校验吻。
- `14-20`：小转大、ABC、严格 ABC、区间套、MACD 黄白线走弱、B 中枢回零、标准趋势背驰。
- `21-28`：候选上下文位图、位置、走势类型、优先级、中枢编号、突破编号、端点编号、走势编号。
- `29-32`：MACD 面积比、价差力度比、平均力度比、背驰要素位图。
- `33-42`：突破、ABC、小转大、二类上下文相关端点编号。
- `43-46`：背驰 A/C 段起止端点编号。
- `47-48`：胜出候选所属中枢生命周期、相邻中枢生命周期关系。
- `49-52`：区间套层级、源高级别背驰段编号、低级别背驰段起止端点编号。
- `53-54`：胜出候选背驰语义、一类背驰后首段回拉/反弹端点编号。

`Func40(C,V)` 已作为真实收盘价/成交量旁路入口；需要动力学或多周期公式时，公式层已要求先注册对应周期的 C/V。

## 当前源码结构快照

当前 HEAD 的开发入口已经从单一 `CzscCore.cpp` 转为分层模块：

- 类型和公共结构：`include/CzscTypes.h`。
- 形态学：`include/CzscMorphology.h` / `src/CzscMorphology.cpp`。
- 中枢与走势：`include/CzscCenter.h` / `src/CzscCenter.cpp`。
- 动力学与背驰度量：`include/CzscDynamics.h` / `src/CzscDynamics.cpp`。
- 买卖点候选与输出投影：`include/CzscTrading.h` / `src/CzscTrading.cpp`。
- 区间套：`include/CzscNestedDivergence.h` / `src/CzscNestedDivergence.cpp`。
- 分析器与缓存：`include/CzscAnalyzer.h` / `src/CzscAnalyzer.cpp`。
- TDX 导出适配：`include/CzscTdxExports.h` / `src/CzscTdxExports.cpp`。
- 通用输入/配置/旁路数据：`include/CzscCommon.h` / `src/CzscCommon.cpp`。
- 内部 helper：`include/CzscInternal.h`，不要把只给实现文件使用的 helper 暴露到用户文档。
- 兼容入口：`CzscCore.h` 继续给旧调用方 include；不要在根目录 `CzscCore.cpp` 或 `CzscAnalyzer.cpp` 继续新增实现。

## 当前已知质量门禁

后续改动应继续保持以下门禁：

- `python tests/check_formulas.py --self-test`
- `python tests/check_formulas.py`
- `wsl.exe -e bash -lc 'cd /mnt/d/github/czsc-tdx && make test'`
- release 前：`wsl.exe -e bash -lc 'cd /mnt/d/github/czsc-tdx && make release'`
- 修改 SSE 输出后：先 `make sse-result`，人工核对 diff，再跑 `make test`。
- 修改 `Func30` 输出后：同步 `include/CzscTdxExports.h` 注释、`src/CzscTdxExports.cpp` switch、README、公式/调试公式、`tests/check_formulas.py`、C++ projection 测试。
- 修改类型/候选字段后：同步 `include/CzscTypes.h`，并检查 `src/CzscAnalyzer.cpp`、`src/CzscTrading.cpp`、SSE dump 和结构校验脚本。
- 修改实现模块边界后：同步 `Makefile` 的 `CORE_OBJECTS`、`CLAUDE.md` 本机 clang 命令和公式校验脚本的源码定位。

## 后续 plan 的入口观察

以下不是已经完成项，而是基于 `346322e` 之后的当前结构整理出的下一步规划入口：

- 源码结构已拆分，下一轮优化应优先沿 `src/` 职责边界落点；不要把新算法继续堆回 `CzscCore.cpp`。
- 形态学已经完成一轮原文校准，下一步更适合引入第二个真实行情样本，防止 SSE 单样本过拟合。
- 中枢生命周期已输出 47/48，后续可继续做多级别生命周期迁移校验，而不是继续增加旧式散装函数。
- 买卖点候选已经按一二三类来源和上下文收紧，下一步可以在调试层保留更多“被过滤原因”，辅助人工核对。
- 动力学已具备 A/C 端点、MACD 面积、黄白线、B 中枢回零、小转大、区间套、背驰语义和回拉端点；下一步重点应放在更多真实样本和多级别递归断言。
- 公式体验已经较完整，后续应继续控制主图复杂度，把新增诊断尽量放在调试公式和结构校验里。
