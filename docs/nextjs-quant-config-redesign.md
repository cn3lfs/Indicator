# 配置体系重构计划（允许与 v10 及以前不兼容）

> 需求方：`D:\github\nextjs-quant`。起草：2026-10-01，基于 `2ef2998`（api v10）。
> 目标：停止“每加一个口径就往十进制配置码上再加一位”的做法，换成结构化配置，让项目结构明显变好。**允许破坏旧接口**；唯一硬约束是默认分析结果不变（见 §5）。
> 执行顺序：先完成 v6 死锁修复 → 本重构 → 再在新体系下实现 v7（笔中缺口）。v7 不要再用千万位实现。

## 1. 现状盘点

`core/config.h` 的十进制配置码目前有 7 位，计划中还要加第 8 位：

| 位 | 含义 | 取值 | 作用对象 | 问题 |
|---|---|---|---|---|
| 个位 | 笔算法 | 0 严格 / 1 新笔 / 2 czsc / 3 4K / 4 分型 | 分析 | — |
| 十位 | 收笔端点 | 0 极值 / 1 次高次低 | 分析 | 与百万位语义冲突，只能靠“非法组合”挡住 |
| 百位 | 中枢构件 | 0 笔 / 1 线段 | 分析 | 本质是“要哪一级的中枢”，却迫使调用方对同一行情**构建两次**（0 和 1100），笔、分型、MACD 全部重复计算 |
| 千位 | 线段算法 | 0 启发式 / 1 特征序列 | 分析 | 笔级配置（百位 0）里也有这一位，大多数情况下无用，但十万位又依赖它当“父线段” |
| 万位 | 线段分界点 | 0 / 1 / 2 | **仅显示** | 显示选项混进分析配置：改它会重算、占缓存、改变配置身份 |
| 十万位 | 笔中枢构成 | 0 进入段 / 1 服从所在线段 | 分析（仅笔中枢） | 对线段中枢无效，却占用全局一位 |
| 百万位 | 笔内包络 | 0 / 1 | 分析 | 与十位冲突 |
| （计划）千万位 | 笔中缺口 | 0 / 1 / 2 | 分析 | 超出通达信 float 精确范围 2^24 |
| 隐式 | `earlySignals` | 布尔 | 信号发布策略 | 不在配置码里，靠缓存键另外区分 |
| `flags` | EVENTS / HIGHER | 位 | 输出选择 | 与配置码平行的第二套开关 |
| `czsc_nested_build(low, high)` | 区间套 | — | 跨级别 | 需要调用方先分别构建 0 和 1100 两个快照再拼 |

由此带来的问题：

1. **重复计算**：图表、监控、信号台账每只证券都要构建 0 和 1100 两次，笔层完全重复。
2. **分析与显示混在一起**：万位只影响画线，却改变配置身份和缓存。
3. **靠“非法组合”维持语义**：十位与百万位、千位 0 与万位非零、分型笔与跨度类选项，规则散落在 `Decode` 和 `czsc_config_valid` 里；前端只能自己硬编码这些依赖（nextjs-quant 现在就在硬编码“启发式线段时禁用分界点”）。
4. **编码到顶**：十进制位数有限，通达信又以 float 传参，千万位起就会失真。
5. **研究身份不透明**：配置码是一个整数，看不出含义，也难以平滑扩展。

## 2. 目标设计

### 2.1 配置拆成三层

```
分析配置 AnalysisConfig   —— 决定结构，进入缓存身份和研究版本
  stroke   : rule(严格/新笔/czsc/4K/分型)
             endpoint(极值 / 次高次低 / 区间包络)      ← 合并原十位与百万位，冲突消失
             gap(不处理 / 计作1根 / 大缺口成笔), gapThreshold(默认 0.02)   ← v7 落在这里
  segment  : method(启发式 / 特征序列)
  center   : strokeFormation(进入段 / 服从所在线段)    ← 原十万位，只挂在笔中枢上
             （线段中枢目前无可选项，以后有了再加，不影响笔级）
  signals  : publication(标准 / 快速候选)               ← 原隐式 earlySignals，显式化

输出选择 Outputs          —— 只决定生成哪些表，不改变结构
  levels   : 位集合 {笔级, 线段级}                      ← 取代“百位 + 构建两次”
  events, recursion, nested : 布尔                       ← 取代 flags 与 czsc_nested_build

显示投影 Projection        —— 只影响端点/中枢框的显示坐标
  segmentBoundary : 极值 / 合并首笔 / 合并末笔           ← 原万位
  centerBox       : 前三笔 / 含延伸                       ← 现在 nextjs-quant 自己做，统一到这里
```

要点：

- **一次构建出两级**：同一份笔，在上面同时构建线段、笔中枢、线段中枢、两级买卖点、区间套。以前的 0 和 1100 成为同一快照里的两个 level。区间套不再需要 `czsc_nested_build` 拼两个快照。
- **冲突用枚举消除**，不再用“非法组合”：endpoint 的三个取值本来就互斥。剩下真正的依赖（如分型笔对 gap 无意义、启发式线段对 segmentBoundary 无意义）由 schema 描述（见 2.3），调用方据此禁用，不再各自硬编码。
- **显示投影不进缓存身份**：同一个分析结果可以按不同投影读出，不必重算。

### 2.2 C 接口

```c
typedef struct czsc_config { uint32_t size; /* 各字段为 int32 枚举或 float 阈值，按 §2.1 排列 */ } czsc_config;
CZSC_API int32_t czsc_config_default(czsc_config *out);
CZSC_API int32_t czsc_config_validate(const czsc_config *c);          /* 0 合法；否则 czsc_last_error() 给出中文原因 */
CZSC_API int32_t czsc_config_id(const czsc_config *c, char *out, int32_t cap);
        /* 规范化文本身份，如 "stroke=strict;end=extreme;gap=none;seg=feature;center=entry;pub=standard"，
           只含分析配置，用于缓存与研究版本；字段新增时有默认值，旧身份可解析 */

CZSC_API void *czsc_build(const czsc_input *input, const czsc_config *c, uint32_t outputs);
/* 表访问增加 level 参数：0 笔级，1 线段级 */
CZSC_API const czsc_pivot  *czsc_level_pivots (void *s, int32_t level, int32_t *count);
CZSC_API const czsc_center *czsc_level_centers(void *s, int32_t level, int32_t *count);
/* … signals / movements / breakouts / events / recursive_* 同理；nested 直接从快照读 */
CZSC_API int32_t czsc_set_projection(void *s, const czsc_projection *p);  /* 只改显示坐标，不重算 */
```

- `czsc_input` 不再携带 `config` / `flags`。
- 删除：十进制 `Encode/Decode`、`czsc_config_valid(int)`、`czsc_nested_build`、`CZSC_FLAG_*`、旧的单级表访问函数。
- `extremeIndex` 保留，含义仍为分析端点；`index` 为投影后的显示端点。

### 2.3 自描述 schema（取代 place/value 选项表）

```c
typedef struct czsc_config_field {
  uint32_t size;
  char key[32];        /* "stroke.rule" */
  char label[32];      /* "笔算法" */
  int32_t layer;       /* 0 分析 / 1 输出 / 2 显示 */
  int32_t kind;        /* 0 枚举 / 1 浮点 */
  int32_t defaultValue; float defaultFloat, minFloat, maxFloat;
} czsc_config_field;
typedef struct czsc_config_choice {
  uint32_t size;
  char field[32]; int32_t value;
  char key[32]; char label[32]; char lessons[32];
  int32_t original;    /* 1 原文 / 0 社区 */
  char note[256];      /* 一两句说明，供界面 tooltip，免得调用方另写一份 */
} czsc_config_choice;
typedef struct czsc_config_rule {       /* 依赖：当 whenField==whenValue 时，field 不适用或只能取 onlyValue */
  uint32_t size;
  char whenField[32]; int32_t whenValue;
  char field[32]; int32_t onlyValue;    /* -1 表示该字段不适用（界面禁用） */
  char reason[128];
} czsc_config_rule;
CZSC_API int32_t czsc_config_fields (czsc_config_field  *out, int32_t cap);
CZSC_API int32_t czsc_config_choices(czsc_config_choice *out, int32_t cap);
CZSC_API int32_t czsc_config_rules  (czsc_config_rule   *out, int32_t cap);
```

key 全局唯一（v9 出现过 `center.segment` 同时用于百位和十万位，nextjs-quant 只能按 place 区分）。`note` 让 tooltip 文案由 DLL 统一提供；nextjs-quant 现在自己维护的 `czscOptionNotes` 会删除。

### 2.4 通达信侧

float 传参的上限问题改用**预设号**解决：

- 公式只传一个小整数“预设号” `TDXDLL1(编号, H, L, 预设号)`。
- 内置预设：`0` 默认笔级视图、`1` 默认线段级视图、`2` 当前主图口径（原 101000 等价）……
- 用户自定义预设：DLL 同目录 `czsc-presets.ini`，按 `[3] stroke=4k gap=asbar …` 的格式写，键名与 §2.3 的 key 一致；读不到或不合法时该预设输出全 0，并在一个诊断编号上给出错误码。
- 每个导出函数按“预设号 + 输出层级”取对应 level 的表，内部一次构建、缓存共享。
- `earlySignals` 成为预设中的 `signals.publication=early`，缓存键不再需要额外字段。

### 2.5 研究版本与旧配置

- 研究、监控、信号台账在 nextjs-quant 中记录的“配置 0 / 1100”改为记录 `czsc_config_id` 文本。
- 请提供一个**一次性映射**：旧整数码 → (新 AnalysisConfig, level, Projection)，写进文档和测试，供需求方迁移旧记录的标签；DLL 本身不必保留旧解码入口。

## 3. 收益

| 收益 | 说明 |
|---|---|
| 计算量 | 图表、监控、信号台账每只证券少构建一次，笔层与 MACD 只算一遍 |
| 接口面 | 删除 `czsc_nested_build`、`flags`、单级表访问、整数编解码；跨级别关系直接在一个快照内 |
| 语义清晰 | 分析／输出／显示分层；互斥项是枚举，不再有“非法组合”这一概念 |
| 扩展性 | 新口径加枚举值或字段，不再受十进制位数和 float 精度限制 |
| 调用方简化 | 依赖规则、说明文案由 schema 提供；nextjs-quant 删除 `czscCodes` 的拼码逻辑、按 place 去重的特判和本地说明表 |
| 研究可追溯 | 文本身份可读、可比较、可扩展 |

## 4. 迁移步骤（每步单独提交，`make test` 全绿）

1. 内部重构 `Config` 为分层结构，保留旧 `Decode` 作为测试桥；证明全部旧合法码经映射后输出不变。
2. 单次构建两级：引擎内部共享笔层；用 SSE 证明 level 0 = 旧配置 0、level 1 = 旧配置 1100（及其他旧码对应组合）逐字节一致，区间套与旧 `czsc_nested_build` 一致。
3. 新 C 接口与 schema；删除旧接口；头文件、README、CLAUDE.md 更新，api 版本记为新的主版本（建议 v20，以示断代）。
4. 通达信预设号与 `czsc-presets.ini`；公式包改写；静态校验脚本同步。
5. 在新体系下实现 v7（缺口），`gapThreshold` 作为浮点字段。

## 5. 硬约束与验收

- **默认分析结果不变**：新默认配置下，level 0 与旧配置 0、level 1 与旧配置 1100 在 SSE（真实 C/V，含事件与递归）上所有业务字段逐字节一致；`tests/unit/golden` 不改，只改读取方式。
- 所有旧合法配置码，经 §2.5 映射后结果一致（全量跑一遍）。
- 性能：给出 SSE 和一只长历史个股上“旧：构建 0 + 1100 两次”与“新：一次构建两级”的耗时对比。
- 在本文件末尾追加“答复”：最终字段表、接口清单、映射表、性能数字、未做项。

## 6. nextjs-quant 侧配合（需求方实现）

- FFI 改为新接口；`CzscFamily` 由“两次构建的两个 family”改为“一个快照的两个 level”。
- 设置面板完全由 schema 生成（字段、选项、说明、依赖禁用），删除本地硬编码。
- 研究、监控、信号台账的配置记录迁移为文本身份；旧记录按映射表补标签。


## 执行记录

1. 分层配置：AnalysisConfig / OutputSelection / Projection已拆开，端点三枚举替代十位与百万位，
   信号发布策略显式化；旧Decode暂作迁移测试桥。完整映射见legacy-config-map.csv。
   先固定b21eb2d下全部240个合法旧码（真实C/V、含事件与递归）的11张C表字节摘要和默认区间套摘要，
   经映射全部一致，负对照能发现单个业务字节变化。make test：64 cases，0 failed checks。
   默认分析线段法选feature以同时对应旧0/1100；默认C中枢框投影为含延伸以保留旧end字节。
   v20按需求明确断代，替代CLAUDE.md中旧版本“只末尾追加字段”的约束，业务表默认字节仍保持。

2. 单次两级：AnalyzeFamily共享包含/分型、笔/线段流、均线和MACD，一份不可变SharedAnalysisInputs供两级读取；
   两级独立维护中枢/走势/信号与定型边界，最终快照复用共享端点，不再重跑形态链。
   两级均与独立参照重算及前缀事件一致，全部旧码C表字节基线不变。
   make test：66 cases，0 failed checks；原golden不变。

3. api v20：czsc_build单次family，level取表；czsc_config/default/validate/id/parse、projection与fields/choices/rules落地。
   删除旧C导出，整数迁移移至migration/测试桥；TDX暂用迁移工具，下一步替换为预设。
   显示分界只改pivot.index/price，中枢框坐标仍按真实极值以保留旧映射字节；centerBox独立选择前三或延伸。
   schema布局packed size92/396/204；config size32、projection size12。
   make test：67 cases，0 failed checks；全部240旧码11表及区间套基线一致，原golden未改。

4. 通达信已用0..9999预设号；0笔级/1线段级共享默认family，2为所属线段中枢+快速发布。
   DLL地址定位其目录，首次读取UTF-8 czsc-presets.ini，未知/重复字段及依赖冲突拒绝该预设，46号诊断。
   缓存仅含分析身份与行情，level/projection不入键；测试证明两级及不同投影只构建一次，publication确实隔离。
   production core/adapter/tdx不再引用整数迁移工具；公式与校验同步，附INI示例。
   make test：69 cases，0 failed checks；原golden及240映射字节保持。

5. v7按结构字段实现：同向原始缺口计数、large浮点阈值、分型笔禁用、细化窗口及bounded组合；choices/rules与INI同步。
   第62/65/77/79课正文核实结论登记为社区/非原文。73个测试全绿，社区回归只追加新段，旧golden及240映射字节不变。

## 答复

已按§4顺序完成五步，每步make test全绿后独立提交；前置v6死锁修复已先提交b21eb2d。
api最终为**v20不兼容主版本**；默认两级用同一笔/线段流与均线/MACD输入，输出和投影不进入分析身份。

### 字段表与布局

| 层 | schema key | 取值/默认 |
|---|---|---|
| 分析 | stroke.rule | strict默认/new/czsc/4k/fractal，C值0..4 |
| 分析 | stroke.endpoint | extreme默认/first/bounded，C值0..2；替代旧十位与百万位 |
| 分析 | stroke.gap | none默认/asbar/large，C值0..2；分型笔只能none |
| 分析 | stroke.gapThreshold | float默认0.02，有限开区间(0,1)；只有large进入身份 |
| 分析 | segment.method | heuristic=0/feature=1默认 |
| 分析 | center.strokeFormation | entry=0默认/segment=1，只作用笔中枢 |
| 分析 | signals.publication | standard=0默认/early=1 |
| 输出 | outputs.levels | stroke=1/segment=2/both=3默认；TDX预设只选一个视图 |
| 输出 | outputs.events / recursion / nested | 各自off/on，默认on；nested必须两级 |
| 显示 | projection.segmentBoundary | extreme=0默认/first=1/last=2；heuristic仅extreme |
| 显示 | projection.centerBox | initial=0/extended=1默认；TDX内置initial |

czsc_config（自然align4，size32）offset：size0、strokeRule4、strokeEndpoint8、strokeGap12、gapThreshold16、segmentMethod20、centerStrokeFormation24、signalsPublication28。
czsc_projection（align4，size12）：size0、segmentBoundary4、centerBox8。
czsc_input只含size/n/H/L/C/V：x86 size24、指针offset8/12/16/20；x64 size40、指针offset8/16/24/32。
schema三个结构pack(1)：field size92（key4,label36,layer68,kind72,defaultValue76,defaultFloat80,min84,max88）；
choice size396（field4,value36,key40,label72,lessons104,original136,note140）；rule size204（whenField4,whenValue36,field40,onlyValue72,reason76）。
业务行全部保留v10：pivot28/center56/movement60/divergence68/breakout96/signal144/event16/bar40/nested52/recursive_node84/recursive_center56/recursive_connection40字节。

### 接口清单

- czsc_config_default / validate（0合法，失败中文原因）/ id（返回含NUL所需容量）/ parse（缺失字段用默认，重复/未知拒绝）。
- czsc_config_fields / choices / rules（NULL/0查询条数）；13字段、31选项、5依赖，field与choice的key全局唯一，note由DLL提供。
- czsc_build(input,config,outputs)，输出位STROKE1/SEGMENT2/EVENTS4/RECURSION8/NESTED16，默认31；czsc_snapshot_free释放。
- czsc_level_pivots / centers / movements / breakouts / signals / events / bars / recursive_nodes / recursive_children / recursive_centers / recursive_connections：level参数0笔级/1线段级。
- czsc_nested_rows从同family读取，删除独立nested_build。递归行的level为所属视图内部递归层，不能与访问参数level混淆。
- czsc_projection_default / czsc_set_projection，只改变显示表，getter指针在释放或下一次set_projection前有效。
- czsc_api_version / build_commit / last_error。

删除整数Encode/Decode、config_valid/options、CZSC_FLAG_*、旧single getters及snapshot_build。migration/legacy_config.h仅作一次性记录迁移与测试工具，production core/adapter/tdx均不引用。
分析ID采用稳定字段名文本；规范化none/asbar无效阈值，输出/投影不在ID中。配置parse支持旧身份省略后增字段，schema让界面自行禁用依赖。
TDX预设0/1共享同一分析，2为所属线段中枢+early；INI在DLL首次使用从自身目录读取，修改需重载。46号0合法/1编号非法/2不存在/3文件不可读/4字段非法。
内置无文件可用；存在但无法读取的INI拒绝，防止静默回落默认。events=off禁用事件类输出；TDX没有递归/区间套导出，二者字段只用于C调用方。

### 一次性映射表

完整240行见[legacy-config-map.csv](legacy-config-map.csv)，字段包含legacyCode/analysisId/level/segmentBoundary/centerBox。其生成规则如下（仅历史迁移，不提供DLL整数接口）：

| 旧项 | 新项 |
|---|---|
| 个位0..4 | stroke.rule0..4 |
| 十位0/1，百万位0 | stroke.endpoint=extreme/first |
| 百万位1（原十位须0） | stroke.endpoint=bounded |
| 百位0/1 | 访问level0/1 |
| 千位0/1 | segment.method=heuristic/feature |
| 万位0/1/2 | projection.segmentBoundary=extreme/first/last |
| 十万位0/1 | center.strokeFormation=entry/segment |
| 隐式earlySignals | 明确stroke之外的signals.publication；旧C记录映射为standard，旧快速TDX记录须另记early |
| 原flags EVENTS/HIGHER | 新outputs EVENTS/RECURSION，level另选；NESTED按需求选择 |
| 原centerBox无开关 | C映射extended以保留end字节，TDX主图initial |

新默认segment.method=feature使level1等于旧1100；在默认entry口径下，笔级不受线段法影响，所以level0同时等于旧0。
全部240旧合法码的11表count+完整业务行字节与重构前冻结基线一致，负对照能发现单字节变化；默认nested字节同旧两次快照拼接。
原tests/unit/golden未改；社区fixture旧205655字节保留，只追加缺口50630字节。v6默认0/1100已核对v9字节基线，重构继续保留。

### 性能

同一WSL native g++ -O2，真实C/V，含事件、递归及区间套；各5次预热，每轮100次，3轮取单次耗时中位数。
旧程序保留重构前构建产物（两次0/1100+独立nested），新为一次czsc_build outputs31并释放；不比较缓存命中。

| 样本 | 旧两次+区间套 | 新单次两级+区间套 | 耗时降低 |
|---|---:|---:|---:|
| SSE日线2038根 | 1508.664μs | 1247.643μs | 17.30% |
| 000001日线8464根，1991-04-03至2026-09-30 | 6185.545μs | 5260.556μs | 14.95% |

单次共享笔层有收益，但两级中枢/信号/递归与C投表仍须分别执行，不能声称减半。新工具tests/bench_family.cpp可复跑；个股行情及本机路径未入库。
原始3轮SSE旧1507.995/1508.664/1532.575，新1328.639/1247.643/1139.803；个股旧6751.580/6185.545/6116.950，新5860.132/5081.062/5260.556μs。

未做：nextjs-quant FFI、研究标签及设置面板修改属§6需求方工作，本仓库提供契约/映射/说明；未在通达信GUI导入公式，仅公式静态校验。
缺口社区口径不标原文，未添加千万位或旧整数兼容导出。v6 bounded可回退，继续不承诺端点定型；无待实现的本轮必需项。

### 最终DLL复核

32/64位DLL均实际LoadLibrary成功：v20新导出、旧导出删除、schema13/31/5、projection、两级表、缺口及DLL自身目录INI（宿主EXE目录不同）通过。
默认两级11表共174492字节：Win64与v9基线SHA256为1003760a081c3da02965ac46fb3ae44b8d7e2030d1960764e0c31762f2af13f9；
Win32与从v9 cf0159c重建的同架构程序SHA256均为bc2610e95063df88c99d7200e546593c6c05b996df24daed28e888a42271c656。
两架构各自逐字节一致；32位x87与64位浮点字节差异本来已存在，不把跨架构输出混作一个基线，也未为此改业务算法或golden。
release-check通过：PE32/PE32+、仅KERNEL32/msvcrt导入、PE时间戳0。smoke工具见tests/smoke_api20.cpp；未做通达信GUI公式导入。

## 需求方复核（2026-10-01，构建 9846686，api v20）

复核方式：WSL `make mingw64` 产物，用独立 koffi 脚本同时加载 nextjs-quant 现用的 v9 DLL（cf0159c）与本版 DLL，直接比较 C 表原始字节。

### 已确认

- 默认配置一次构建：level0 与旧 0、level1 与旧 1100 的 11 张表（含事件、递归）及区间套，SSE 上**逐字节一致**。
- 需求方图表正在使用的非默认组合迁移后一致：旧 `101000` = `center.strokeFormation=segment` 的 level0；旧 `11100` / `21100` = 默认构建 + `projection.segmentBoundary=first/last` 的 level1（端点、中枢、信号均一致）。
- v6 原死锁已解除：`stroke.endpoint=bounded` 时，前 170 根末端点越过 157B 到 169B，全样本 126 个端点。
- v7 缺口统计与答复一致（严格笔 158→166→166 等）；分型笔 + 缺口、阈值越界被 `validate` 以中文原因拒绝。
- schema：13 字段 / 31 选项 / 5 条依赖，key 全局唯一，说明文案可直接用于 tooltip。

### 问题 1（严重）：bounded 模式丢失整段早期历史

同一 SSE 样本（2038 根，真实 C/V），默认配置除 `stroke.endpoint=bounded` 外不变：

| 输入 | bounded 端点 | 默认端点 |
|---|---|---|
| 前 160 根 | 3 个：`136B 142T 157B` | 13 个：`10B 16T 36B 46T 51B 72T 79B 85T 105B 119T 136B 142T 157B` |
| 前 240 根 | 6 个 | — |
| 全样本 | 126 个，**第一个端点为 174B** | 158 个，第一个为 10B |

即第 174 根之前的所有笔都消失，前缀输入时更早的笔也被截掉。答复中“端点约为默认 80%、同量级”的统计掩盖了这一点：少掉的端点里有一整段开头历史。判断为 v6 修复引入的“尾部链回退”在回退时截断了链头，且之后不再重建。

修复要求：

1. 回退只能撤销真正受影响的尾部端点，回退点之前已合法的笔必须保留；回退后要能从回退点重新成笔。
2. 回归测试（对 bounded 及其与各笔算法、缺口的组合）：
   - 第一个端点与默认配置第一个端点相距不超过一笔的合理跨度（如 SSE 中不得晚于第 30 根）；
   - 任意前缀 n 的端点序列，其已定型部分不得在 n 增大时整体消失（bounded 虽不承诺定型，也不得把链头清空）；
   - 默认配置有笔的任意 300 根窗口内，bounded 也至少有 1 笔。
3. 修复后重做 bounded 的 SSE 统计，并按“开头 / 中段 / 结尾”分段给出端点数，避免总数掩盖局部缺失。

### 问题 2（次要）：DLL 体积 976KB → 3.2MB

`objdump -h`：`.debug_*` 段约 600KB 未剥离；`.text` 1MB，新增的 `tdx/presets.cpp`（`<fstream> <sstream> <filesystem>`）与 `core/config.cpp`（`<locale> <iomanip> <sstream>`）把 iostream/locale/filesystem 整套静态链接进来。

建议：release 构建剥离调试信息（`-s` 或 `strip`）；预设 INI 读取与配置身份格式化改用 Win32 API / C stdio 和手写格式化，不引入 iostream、locale、filesystem。体积目标回到 1.2MB 以内，并在 `release-check` 中加体积上限检查。

在本文件末尾追加修复答复；默认两级与 v9 逐字节一致的约束不变。

## 修复答复（2026-10-01，api v20）

### 问题1：bounded早期历史

已修复。回退保留较早同型锚点及合法前缀，撤销受影响尾部；无法直接延伸锚点时，重选此前因跨度不足被跳过的已确认反向分型，逐笔复核跨度、价格和合并包络。禁止删除锚点后把新末端强接到更早历史。此工程口径已登记决策表，非原文规则。
第127根保留105B并重接119T；126B未满足119T之后的跨度，暂不成笔。第175根保留早期历史，仅把136B同型延伸到174B。
前160根恢复13个端点（10B起），前240根16个，全样本136个，末端继续延伸，未以锁住末端换取历史数量。

SSE日线2038根，以下统计区间按原始下标：开头[0,300)、中段[300,1738)、结尾[1738,2038)。每格为endpoint=extreme→bounded，gap.large阈值0.02。

| 笔算法 | gap | 开头 | 中段 | 结尾 | 全部 |
|---|---|---:|---:|---:|---:|
| strict | none | 24→16 | 110→98 | 24→22 | 158→136 |
| new | none | 28→18 | 126→110 | 26→22 | 180→150 |
| czsc / 4k | none | 28→18 | 154→128 | 26→22 | 208→168 |
| fractal | none | 104→104 | 507→507 | 107→107 | 718→718 |
| strict | asbar / large | 24→16 | 118→114 | 24→22 | 166→152 |
| new | asbar / large | 30→18 | 140→124 | 26→22 | 196→164 |
| czsc / 4k | asbar / large | 32→22 | 168→148 | 26→22 | 226→192 |

13个合法组合的首点均不晚于30（strict/new/czsc/4k为10，fractal为5）。逐输入检查100根后的链头与开头四点不消失；每个组合的1739个300根滑动窗口均在默认有完整笔时至少保留一笔。保留上升/下降对称包络、相等、候选、防死锁及批量/增量因果测试。负对照换回9846686实现时新增历史回归失败。
仅更新社区fixture中的bounded段；其他社区段及原文tests/unit/golden不改。旧240码冻结文件保留，bounded独立使用修复后字节基线；非bounded及默认nested继续核对旧基线。api版本、结构体布局均保持v20不变。

验证：`make test`全部74例及公式检查通过；默认两级、递归、事件和区间套的旧字节回归通过。

### 问题2：DLL体积

发布链接新增`--strip-all`，移除符号及调试段。配置身份改为字符串拼接+C stdio的9位有效数字格式化，小数分隔符规范为ASCII点；数值及分号字段用ASCII解析。INI用Win32定位DLL自身目录，宽字符`_wfopen`+分块`fread`读取，手工逐行解析。保留UTF-8 BOM、CRLF、末行无换行、重复字段、缺失文件与存在但不可读的诊断行为。core/tdx/adapter不再引入fstream/sstream/iostream/locale/iomanip/filesystem。

| 发布产物 | 修复前字节 | 修复后字节 | 上限 |
|---|---:|---:|---:|
| CZSC.dll（32位） | 2,933,228 | 458,752 | 1,200,000 |
| CZSC64.dll（64位） | 3,221,087 | 398,336 | 1,200,000 |

`release-check`新增每个DLL≤1,200,000字节及无`.debug_*`段断言，继续检查PE架构、仅KERNEL32/msvcrt导入及零时间戳。负对照使用旧2,933,228字节DLL时被体积检查拒绝。
`make test`全部76例及公式检查通过；新增约5000个有效float阈值的9位格式/精确往返、错误数值、BOM/CRLF及无尾换行INI测试。
两种发布DLL实际LoadLibrary通过（`tests/smoke_api20.cpp`），包含German小数locale下的身份往返、DLL同目录INI、缺口及bounded前160根13端点/全样本136端点/首点10B。两架构的默认两级11表各174492字节，分别与先前核对v9的同架构基线逐字节一致：Win32 SHA256为bc2610e95063df88c99d7200e546593c6c05b996df24daed28e888a42271c656，Win64为1003760a081c3da02965ac46fb3ae44b8d7e2030d1960764e0c31762f2af13f9。默认区间套的旧字节回归也通过，tests/unit/golden未改。

api仍为v20，结构体布局不变。本轮两项均完成；未在通达信GUI导入公式，验证覆盖真实DLL ABI与静态公式检查。
