# nextjs-quant 适配层需求

> 需求方：`D:\github\nextjs-quant`（观澜量化，Node/Electron，通过 koffi 以 FFI 调用 `CZSC64.dll`）
> 起草：2026-09-27，针对 `dev` @ `74d8c8d`（新 core/ + tdx/ 引擎）
> 缠论口径以 108 课原文为准，边界口径沿用 `docs/chan-ambiguity-decisions.md`。本文只规定**要什么数据、什么形状**，不规定算法。

## 1. 背景

nextjs-quant 此前通过旧 `Func30`（mode = 配置码×1000 + 输出×10）逐根读取约 50 个输出（0–108），包括逐根投影表、对象编号表与带锚快照。`74d8c8d` 删除了旧实现，新 `tdx/` 只保留 13 个逐根序列，nextjs-quant 无法继续使用。

逐根铺序列的方式本来就是为通达信公式服务的；程序化调用方需要的是**对象**（端点、中枢、走势、信号及其关联）。所以不要求恢复旧 Func30，只要求新增一个面向程序的**结构化快照导出**，与现有 `tdx/` 并列。建议放在 `adapter/`（或 `api/`），不影响通达信公式。

## 2. 总体约定（全部必须满足）

1. **纯 C ABI，x64**：`extern "C"`，`__cdecl`；只用定长整数、`float`/`double`、POD 结构与指针，不暴露 C++ 类型或异常。
2. **无全局可变状态、可重入**：每次调用独立。输入相同则结果逐字节相同，与此前调用过什么无关。旧实现的全局缓存与 40 号旁路注册导致过取值串扰；新接口把 close/volume 作为入参直接传入。
3. **内存由 DLL 分配、DLL 释放**：`czsc_snapshot_build(...) -> handle`，`czsc_snapshot_free(handle)`。调用方通过取表函数拿「指针 + 条数」读数组。handle 释放前指针一直有效。
4. **版本化**：`czsc_api_version()` 返回整数，结构体布局或语义一改就递增。每个结构体首字段是 `uint32 size`（`sizeof`），调用方据此校验。
5. **下标约定**：凡是“K 线位置”都是 0 基原始 K 线下标（`int32`），凡是“对象引用”都是 0 基的表内下标，`-1` 表示无。同一个值不得两种含义混用。
6. **因果字段**：每个会被“当下”改写的对象都带 `confirmedAt`，即该对象在当下能被确认的最早原始 K 线下标（core 的 `confirmedAt` 已有这个概念）。调用方用它做无未来函数的回测，不再自己按数据前缀逐根重跑。
7. **错误**：非法入参返回空 handle，由 `czsc_last_error()`（线程局部）给出原因字符串。任何输入都不得崩溃或越界写。
8. **规模**：`n ≤ 16,777,216`。SSE 日线 2038 根、两套配置的一次完整构建，目标 < 50 ms。

## 3. 入口

```c
typedef struct {
  uint32_t size;
  int32_t  n;
  const float *high, *low, *close, *volume;   // close/volume 必填（MACD 用真实收盘）
  int32_t  config;                            // 沿用配置码：个位笔、十位笔结束、百位中枢构件、千位线段法
  int32_t  flags;                             // 位0 = 生成当下事件流（§4.7）；位1 = 生成高级别结构（§5，P2）
} czsc_input;

void*  czsc_snapshot_build(const czsc_input*);
void   czsc_snapshot_free(void*);
int    czsc_api_version(void);
const char* czsc_last_error(void);
// 取表：返回首元素指针，*count 写条数
const czsc_pivot*    czsc_pivots(void*, int32_t* count);
const czsc_center*   czsc_centers(void*, int32_t* count);
/* ... 以下各表同形 ... */
```

nextjs-quant 目前固定使用两个配置：**0**（笔中枢）和 **1100**（线段中枢，特征序列法），分别调用两次即可。

## 4. P0：基础结构（监控、信号台账、图表、一二三类策略，缺一不可）

### 4.1 端点 `czsc_pivot`（笔或线段端点，按配置）
| 字段 | 说明 |
|---|---|
| `index` | 极值所在 K 线 |
| `kind` | +1 顶 / -1 底 |
| `price` | 顶取 high、底取 low |
| `confirmedAt` | 分型成立的 K 线 |

### 4.2 中枢 `czsc_center`
`start/end`（K 线）、`firstPivot/lastPivot`（端点表下标）、`zg/zd/gg/dd`、`direction`（进入段方向 ±1）、`confirmedAt`（第三段成立使中枢成立的 K 线）、`relationToPrev`（第20课：1 上涨 / -1 下跌 / 2 扩展 / 0 首个）。

### 4.3 走势类型 `czsc_movement`
`type`（0 盘整 / 1 上涨 / -1 下跌）、`firstCenter/lastCenter`（中枢表下标）、`start/end`（K 线）、`confirmedAt`。

### 4.4 离开与回试 `czsc_breakout`（core `Breakout` 直出）
`center`、`direction`、`leavePivot`、`retestPivot`、`third`（回试不回 [ZD,ZG]），以及离开段相对前一同向段的背驰度量（§4.6 同构）。

### 4.5 买卖点 `czsc_signal`（事后全量，与现 13 号 `HindsightSignals` 同一集合）
| 字段 | 说明 |
|---|---|
| `index` / `pivot` | 信号 K 线 / 端点表下标 |
| `type` | 1/2/3 买，-1/-2/-3 卖（**不要用 11/12/13**） |
| `center` | 所属中枢（中枢表下标） |
| `movement` | 所属走势（走势表下标，没有则 -1） |
| `breakout` | 三类所依的离开/回试（没有则 -1） |
| `basedOn` | 二类所依的一类信号（信号表下标，没有则 -1） |
| `stop` | 失效价（第20/21/27课，同 7 号） |
| `divergence` | §4.6 结构体内嵌（一类为 b/c 段，二、三类为对应离开段） |
| `confirmedAt` | 端点被下一端点确认、信号成立的 K 线 |
| `revokedAt` | 当下口径下被撤销的 K 线，没被撤销则 -1 |

### 4.6 背驰度量 `czsc_divergence`（core `Divergence` 直出，每项给原值，不只给比值）
`prevStart/prevEnd/curStart/curEnd`（端点表下标）；`prev/cur` 各自的 `space/speed/area`（同色 MACD 面积）；四个 bool：`newExtreme`、`weakSpace`、`weakSpeed`、`weakArea`；以及 `holds`。另加 `semantic`：0 无 / 1 趋势背驰 / 2 盘整背驰。

### 4.7 当下事件流 `czsc_event`（flags 位0）
逐根重放得到的出现/撤销事件，与 5/6 号序列等价：`bar`（发生的 K 线）、`op`（+1 出现 / -1 撤销）、`signal`（§4.5 表下标）。若撤销后又出现的信号在事后表里不存在，需要单独列出，保证每个事件都能引用到一条完整的 `czsc_signal`。

### 4.8 逐根序列（可直接复用现有 tdx 实现，作为表输出）
MACD（dif/dea/柱，用真实 close）、均线吻（10 号）、缺口（11 号）、分型强弱（12 号）。

## 5. P1：研究判据所需的附加语义（旧实现有，新 core 已删）

下列语义被 nextjs-quant 的 16 个缠论研究策略（`research-chan-*.ts`）使用。**请逐项按原文重新实现**，旧实现只作参考，不作权威。每项实现时把口径登记进 `chan-ambiguity-decisions.md`。

| 语义 | 原文 | 挂在哪 | 旧输出（仅供对照） |
|---|---|---|---|
| 信号强质（确认 / 强质量过滤） | 第24、37课 | `czsc_signal.quality`：1 确认 / 2 强质 | 5 |
| 上下文标志位：a+A+b+B+c 结构、黄白线回零、黄白线弱、标准形态、小转大必要条件、二三类重合、首次回试 | 第24/25/37/44/53课 | `czsc_signal.context`（位掩码，位定义写进头文件） | 21、14、15 |
| 二买基点、二买转折端点 | 第21课 | `czsc_signal.secondBasePivot` / `secondTurnPivot` | 40、41 |
| 小转大：基点、离开、回试端点 | 第44课 | `czsc_signal.smallTurnBase/Leave/RetestPivot` | 38、39、42 |
| 前一同向段、当前段的起止端点 | 第24/37课 | 就是 §4.6 的 `prev*/cur*`，无需另加 | 43–46 |
| 中枢生命周期（延伸、扩展、新生上/下） | 第18/20课 | `czsc_center.lifecycle`：0 延伸 / 1 扩展 / 2 新生上 / 3 新生下 | 47、48 |
| 区间套（低级别背驰落在高级别背驰段内） | 第27、37课 | 表 `czsc_nested`：低配置信号 → 高配置信号，含 `insideHighSegment`、`confirmed`、`newExtreme`、`smallTurn` | 49–52、56–58 |
| 即时背驰预警 | 第24课 | 逐根序列 | 12 |

## 6. P2：高级别与递归走势（可以最后做，但需要先确认可行性）

nextjs-quant 的走势递归类策略（`research-chan-movements/recursive.ts`）用到：

- **趋势对象**：走势表以外，还要趋势的成员中枢列表，以及“趋势完成证据”：完成所需的连接端点、观察到的后继走势、后继成立的 K 线（第17、29课，走势终完美）。
- **高级别候选**：线段中枢（1100）上的一类候选，带它所依的趋势、中枢，以及低级别段的起止端点。
- **递归节点**：以某级别中枢为元素的上一级走势，含 `level/start/end/centerStart/centerEnd/established/connection/completed/successor/children`（第17课递归定义）。
- **带锚快照**：旧 93–108 号以日/周/月为锚。建议 DLL **不再处理日期锚**：由 nextjs-quant 自己把 K 线重采样成周/月后分别构建快照，DLL 只做单一序列的递归。请评估这样是否丢失原文语义，给出结论。

## 7. 验收

1. `tests/unit/golden/sse.txt` 中的端点、中枢、信号，与本接口在 SSE 日线上的输出逐项一致（同一引擎，不得出现两套结果）。
2. **因果一致性**：对任意前缀 `k`，截取前 `k+1` 根构建出的快照中，`confirmedAt ≤ k` 的对象与全量快照中同一批对象逐字段相同；事件流与 5/6 号序列逐根等价。
3. **可重入**：交错构建不同输入和配置、多次释放后，结果不变；未释放的 handle 互不影响。
4. **边界**：`n = 0/1/2`、全平价格、NaN、非法配置码，都返回可解释的错误或空表，不崩溃。
5. 头文件 `adapter/czsc_api.h` 自带每个字段的注释，nextjs-quant 以它作为唯一契约。

## 8. nextjs-quant 侧的承诺

- 只经 `czsc_api.h` 访问 DLL，不再依赖 `RegisterTdxFunc` / 通达信编号。
- 接入 P0 后，监控、信号台账、图表和一二三类策略改用 `confirmedAt` 与事件流，不再在 TS 中做前缀重跑或推断中枢归属。
- P1、P2 未交付前，依赖它们的研究策略继续使用旧 DLL（`718defa`，两个 DLL 改名并存），交付后逐项迁移，全部迁完就删掉旧 DLL。
