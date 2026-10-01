# 通达信公式配置指南（api v20）

当前公式使用小整数预设号。预设的分析参数、输出级别及显示投影写在 DLL 同目录的 UTF-8 `czsc-presets.ini` 中；公式通过 `TDXDLL1(函数号,H,L,预设号)` 选择预设。
本文适用于当前 [公式包](../formulas/README.md)。旧公式中的 `1100`、`101000` 等十进制配置码已经停用，升级 v20 DLL 时须一并换用当前公式。

## 安装与生效

1. 安装与通达信位数匹配的 `CZSC.dll` 或 `CZSC64.dll`，绑定到1号DLL插件。
2. 将 [预设示例](../formulas/czsc-presets.ini) 复制到实际加载的 DLL 同目录，文件名固定为 `czsc-presets.ini`，使用UTF-8保存。不是放在宿主程序目录或公式目录，除非它们恰好也是DLL目录。
3. 在INI中新增预设，例如 `[5]`、`[6]`；编号允许0..9999。同一编号只能定义一次，同一预设内字段不能重复。新增预设未写字段使用默认值，不继承相邻预设；覆盖内置0/1/2时，未写字段保留该内置预设的原值。
4. 在主图及选股公式中同步修改调用的最后一个参数。
5. 修改INI后重载DLL，通常重启通达信。仅重新计算公式不会重新读取INI。

没有INI文件时使用内置0/1/2。文件存在但不可读时输出诊断错误，不静默使用内置默认。

## 内置预设与默认值

| 预设号 | 视图 | 中枢构成 | 买卖点发布 |
|---|---|---|---|
| 0 | 笔级 `stroke` | 按进入段 `entry` | 标准 `standard` |
| 1 | 线段级 `segment` | 按进入段 `entry` | 标准 `standard` |
| 2 | 笔级 `stroke` | 服从所属线段 `segment` | 快速 `early` |

三者均为严格笔、极值端点、不处理缺口、特征序列线段；通达信预设默认画最初三构件的中枢框。0/1共享同一分析，2的中枢与发布策略不同。
当前主图的黄笔用0、红线段用1、中枢/买卖点/止损用2；这意味着默认线条与信号使用不同的中枢/发布策略。若改变笔算法或笔端点，建议用下方成对预设同步所有图层。

INI可以显式覆盖0/1/2。新增预设未写字段的分析默认值与0相同，视图为笔级、中枢框为initial。C接口的配置/输出默认与通达信预设不同：C默认可同时输出两级，中枢投影为extended。

## 可配置字段

### 分析参数

| 字段 | 取值 | 默认 | 含义 |
|---|---|---|---|
| `stroke.rule` | `strict` / `new` / `czsc` / `4k` / `fractal` | `strict` | 严格笔 / 新笔 / czsc笔 / 4K笔 / 分型笔 |
| `stroke.endpoint` | `extreme` / `first` / `bounded` | `extreme` | 同型取更极值延伸 / 保留首个同型分型、允许次高次低 / 合并K线包络约束 |
| `stroke.gap` | `none` / `asbar` / `large` | `none` | 不处理 / 同向原始缺口计入跨度 / 计入跨度且大缺口可跳过跨度门槛 |
| `stroke.gapThreshold` | 0与1之间的有限小数 | `0.02` | large策略的比例阈值，0.02为2%，严格超过才算大缺口 |
| `segment.method` | `feature` / `heuristic` | `feature` | 特征序列法 / 保护点启发式 |
| `center.strokeFormation` | `entry` / `segment` | `entry` | 按进入段 / 笔中枢服从所属线段方向 |
| `signals.publication` | `standard` / `early` | `standard` | 标准等待下一端点 / 真实端点分型确认且结构条件满足即提示 |

bounded以包含处理后的合并K线判包络：两端极值闭区间内不得超过端点，相等允许。被包含处理舍弃的原始影线不作阻塞条件，所以它不等于“所有原始影线都不得越界”。同型末端继续延伸，未确认极值仅作延伸候选，不能反向成笔或确认信号；尾部失效时保留合法前缀并重建，当前不承诺bounded端点定型。

`center.strokeFormation=segment`只改变笔中枢：上升父线段取下上下，下降父线段取上下上，进入/离开笔不作为这三笔构件，构件及延伸不跨父线段。线段级中枢和高层递归仍按进入段。

快速发布是允许失败、后续可结构撤销的概率信号，不等待下一反向笔完成；提示位于当下可知K线，不回填到历史极值K线。快速止损价取原买卖点顶底分型极值，并在发布时冻结；标准三类失效价仍使用ZG/ZD。结构撤销与价格触及止损是两件事，公式不自动执行交易。

4K、分型直连、包络、缺口等可选社区策略，以及父方向成枢工程口径的来源与边界见 [歧义决策表](chan-ambiguity-decisions.md)，不把全部选项当作108课原文规则。

### 输出与显示

| 字段 | 取值 | 通达信预设默认 | 含义 |
|---|---|---|---|
| `outputs.levels` | `stroke` / `segment` | `stroke` | 该预设输出笔级或线段级；TDX单次调用只取一个级别 |
| `outputs.events` | `on` / `off` | `on` | 事件输出；主图买卖点及有效状态使用的预设应保持on |
| `outputs.recursion` | `on` / `off` | `on` | 递归结构输出 |
| `outputs.nested` | `on` / `off` | `on` | 区间套输出 |
| `projection.segmentBoundary` | `extreme` / `first` / `last` | `extreme` | 线段显示分界为极值笔 / 合并特征元素的起始原始笔 / 最后原始笔 |
| `projection.centerBox` | `initial` / `extended` | `initial` | 框画到最初三构件末端 / 最后成员末端 |

projection只改变显示位置，不改变线段划分、真实极值、力度区间、信号和止损口径。框只画前三构件也不会关闭中枢结构延伸。

依赖约束：

- `stroke.rule=fractal`只能使用`stroke.gap=none`。
- `segment.method=heuristic`只能使用`projection.segmentBoundary=extreme`。
- `stroke.endpoint`一次只能选一种策略；first与bounded不能同时开启。
- `stroke.gapThreshold`即使在none/asbar下也须为合法有限小数；只有large策略使用其数值参与分析身份。

## 示例：同步改为bounded，并保留快速买卖点

以下是一组完整的5/6预设。两个预设的分析字段相同，只切换输出级别，因此笔、线段、中枢及买卖点来自同一次分析。

```ini
[5]
stroke.rule=strict
stroke.endpoint=bounded
stroke.gap=none
segment.method=feature
center.strokeFormation=segment
signals.publication=early
outputs.levels=stroke
projection.segmentBoundary=extreme
projection.centerBox=initial

[6]
stroke.rule=strict
stroke.endpoint=bounded
stroke.gap=none
segment.method=feature
center.strokeFormation=segment
signals.publication=early
outputs.levels=segment
projection.segmentBoundary=extreme
projection.centerBox=initial
```

在现有主图中替换这些赋值，保留后面的画线、文字及止损显示语句：

```text
XC:=TDXDLL1(40,C,V,0);
BI:=TDXDLL1(1,H,L,5);
SEG:=TDXDLL1(1,H,L,6);
ZG:=TDXDLL1(2,H,L,5);
ZD:=TDXDLL1(3,H,L,5);
BS:=TDXDLL1(41,H,L,5);
ST:=TDXDLL1(43,H,L,5);
```

40号注册真实收盘价和成交量，不按预设选择分析，继续保留这行。买点选股的44号、卖点选股的45号也同步改用5：

```text
{买点选股}
XC:=TDXDLL1(40,C,V,0);
LIVE:=TDXDLL1(44,H,L,5);
LIVE>=1 AND LIVE<=3;
```

```text
{卖点选股}
XC:=TDXDLL1(40,C,V,0);
LIVE:=TDXDLL1(45,H,L,5);
LIVE>=11 AND LIVE<=13;
```

若要切换4K笔，两节都改`stroke.rule=4k`；若要启用大缺口，两节都改`stroke.gap=large`并添加`stroke.gapThreshold=0.02`。不要只改黄笔预设而保留中枢/买卖点的旧笔算法。
函数41并不强制开启early：5/41、6/42、7/43各组同类函数都遵从预设的`signals.publication`。

## 配置诊断

可在临时指标中加入：

```text
配置状态:TDXDLL1(46,H,L,5),NODRAW;
```

| 返回值 | 含义 | 检查项 |
|---|---|---|
| 0 | 合法 | 预设已加载 |
| 1 | 编号非法 | 须为0..9999整数，不能传旧整数配置码 |
| 2 | 预设不存在 | INI是否在DLL目录、是否已重载、是否有该节 |
| 3 | 文件无法读取 | INI路径、访问权限、文件是否可读 |
| 4 | 字段或依赖非法 | 拼写、枚举值、重复字段/节、阈值及依赖约束 |

非法预设输出0，可能表现为没有线条或买卖点，先用46号检查。空白图表不代表结构算法没有找到信号。
接口开发与旧配置一次性迁移见 [配置体系重构文档](nextjs-quant-config-redesign.md) 和 [映射表](legacy-config-map.csv)。旧映射表用于迁移，不能直接将旧码传给当前DLL。
