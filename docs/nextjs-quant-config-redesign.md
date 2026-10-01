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
