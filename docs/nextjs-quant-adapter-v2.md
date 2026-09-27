# nextjs-quant 适配层需求 v2（在 api v4 之上）

> 需求方：`D:\github\nextjs-quant`。起草：2026-09-27，基于 `dev` @ `4ffcdc0`（api v4）。
> v1 需求（`docs/nextjs-quant-adapter.md`）的 P0、P1 已全部接入：监控、信号台账、图表、原生一二三类、二三重合、区间套、反弹、周线盘整背驰、均线类策略都已改用 `czsc_api.h`，SSE 与 `tests/unit/golden/sse.txt` 逐行一致。
> 本文只列**还缺什么**。缠论口径以 108 课原文为准，新口径登记进 `chan-ambiguity-decisions.md`。

## 1. 现状

nextjs-quant 有两组策略现在只能报“结构缺口”：

| 策略 | 课文 | 用到的级别 |
|---|---|---|
| CH06 已完成趋势背驰、CH08 同级别分解、CH09 下跌+盘整+下跌、CH18 小转大回抽（日线/五分钟） | 16/17/18/24/27/33/38/40/43/44 | 配置级别**往上一层**（递归 level 1） |
| CH13 月线底部（月线序列） | 18/108 | 配置级别（level 0） |
| CH10 中阴结束 + 三买（日线/五分钟） | 89/90 附近（中阴阶段） | 配置级别（level 0） |

日期锚已按 P2 结论由我们自己重采样（周/月/五分钟分别构建快照），这里不再需要 DLL 处理日期。

## 2. 需求

### R1 递归中枢表（最主要的缺口）

v4 的 `czsc_recursive_node` 给出了每层走势，但 **level ≥ 1 的中枢**没有价格和成员，我们无法：
- 核对“一类买卖点所属中枢 = 该趋势最后一个中枢”；
- 判断回抽是否进入最后中枢 [ZD,ZG]（CH18）；
- 给递归对象建立跨前缀的稳定身份（我们用“层级 + 首中枢起点 + ZD/ZG”作键）。

请新增表 `czsc_recursive_center`（flags 位1 时生成），按 (level, ordinal) 排序：

| 字段 | 说明 |
|---|---|
| `level` / `ordinal` | 与节点同一套层级编号 |
| `start` / `end` | 中枢起止K线 |
| `zg` / `zd` / `gg` / `dd` | 与 `czsc_center` 同义（第20课：[ZD,ZG] 成枢即固定） |
| `direction` | 进入段方向 |
| `firstMember` / `memberCount` | 构成该中枢的**下一层**节点（在 `czsc_recursive_node` 表中的下标区间或子表），第17课：至少三个次级别走势重叠 |
| `established` | 中枢成立的K线（第三个成员定型） |
| `confirmedAt` | 定型K线，未定型 -1 |

`czsc_recursive_node` 末尾追加 `firstCenter` / `lastCenter`（本表下标）和 `high` / `low`（节点区间内最高、最低价）。level 0 的中枢就是 `czsc_centers`，可以不重复输出，但请在头文件里说明 level 0 节点的中枢引用指向哪张表。

### R2 同级别连接段

CH08、CH09 需要“走势 A 与走势 B 之间的连接段”及其低一级成员链，用来核对“下跌 + 盘整 + 下跌”等同级别分解是否首尾相接、没有缺口（第17/33课结合律）。

请新增表 `czsc_recursive_connection`：`level`、`left`/`right`（相邻两节点下标）、`start`/`end`（K线）、`firstMember`/`memberCount`（覆盖该连接段的低一级节点链）、`confirmedAt`。
level 0 的连接段已由 `czsc_movement.connectionStart/End` 给出，可复用，不必重复。

### R3 中阴阶段（请先给口径结论，再实现）

v3 规定 `completedAt = successorEstablishedAt`，即“前走势完成”和“后继首中枢成立”是同一根K线。但中阴阶段指的正是**前一走势完成之后、后一走势类型尚未确立之前**的区间。按当前定义，这个区间长度为零。

请按原文回答并在 `chan-ambiguity-decisions.md` 登记：
1. 前一走势“完成”最早能在哪根K线被当下确认？旧实现的口径是“离开最后中枢且观察到不回”（leave-and-observed-nonreturn）。请确认该口径是否符合原文；若不符合，给出替代口径。
2. 中阴的结束 = 后继走势首个中枢成立（现有的 `successorEstablishedAt`），请确认。

确认后，在 `czsc_movement` 和 `czsc_recursive_node` 末尾追加 `zhongyinStart`（中阴开始的K线，未知 -1），`zhongyinEnd` 复用 `successorEstablishedAt`。BOLL 辅助版由我们在 TS 里计算，不需要 DLL 提供。

### R4 小事项

- `czsc_recursive_node` 的 `level` 请在头文件写明与配置级别的关系（level 0 = 本配置级别的走势，对应 `czsc_movements`），以及 `children` 指向的是下一层节点还是本层中枢。
- 如果 R1 实现后 level ≥ 1 的一类买卖点可以直接给出（第17课递归后同样适用第24课背驰），请评估是否在 `czsc_signal` 中新增 `level` 字段，或单独输出高层信号表。评估结论即可，不强求实现。

## 3. 验收

1. 新增表在 SSE 日线上写入 `tests/unit/golden/sse.txt`（新段落），nextjs-quant 同步副本并逐行核对。
2. 因果：对任意前缀 k，`confirmedAt ≤ k` 的新对象在更长数据中逐字段不变。
3. 只在结构体末尾追加字段，`CZSC_API_VERSION` 递增。

## 4. nextjs-quant 侧

交付后，我们用新表重建 C4 的走势、连接和关联结构（signal→level 0 节点的关联用 `czsc_signal.movement`，已可得），恢复上述 7 个策略（日线与五分钟各算一个），移除“结构缺口”。在此之前，这些策略会明确报出缺口，不伪造数据。
