/*
 * czsc_api.h —— 缠论结构化快照导出（面向程序调用方，如 nextjs-quant 经 koffi FFI 调用 CZSC64.dll）。
 * 本文件是唯一契约。缠论口径以 108 课原文为准，原文未定义处的取舍见 docs/chan-ambiguity-decisions.md。
 *
 * 约定
 *  - 纯 C ABI（extern "C"、cdecl），只用定长整数、float、POD 结构与指针；不抛异常。
 *  - 无全局可变状态、可重入：每次构建独立，输入相同则结果逐字节相同（结构体无填充字节）。
 *  - 内存由 DLL 分配与释放：czsc_build 返回句柄，czsc_snapshot_free 释放；取表函数返回的指针在
 *    句柄释放或下一次czsc_set_projection前有效，调用方不得写入或释放。
 *  - 版本：czsc_api_version() 返回整数，结构体布局或语义一改就递增；每个结构体首字段 size = sizeof(该结构体)，
 *    v20为不兼容主版本；业务行布局保留v10，配置/input接口重建。
 *  - 下标：凡“K线位置/K线”均为 0 基原始K线下标；凡“…表下标”均为 0 基的本快照内表下标；-1 表示无。
 *  - 两种口径：
 *      结构（端点/中枢/走势/突破）是用全部输入数据得出的当前结构，末尾对象会随新数据改变；
 *      confirmedAt = 该对象“定型”的最早K线：此后无论再追加什么数据它都不会改变，尚未定型为 -1。
 *      因此截取前 k+1 根构建的快照中 confirmedAt<=k 的对象，与更长数据快照中的同一对象逐字段相同。
 *      买卖点按“当下”记录：每一行是一段信号生命，内容冻结为信号确认那一刻所知的数据。
 *  - 错误：非法入参返回 NULL，czsc_last_error() 给出原因（线程局部，下次调用前有效）。任何输入都不会崩溃或越界写。
 */
#ifndef CZSC_API_H
#define CZSC_API_H

#include <stdint.h>

#if defined(_WIN32) && defined(CZSC_BUILDING)
#define CZSC_API __declspec(dllexport)
#else
#define CZSC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* v20不兼容主版本：结构化配置与一次构建两级，删除整数配置/flags/独立区间套。
 * 所有业务行保持v10布局；packed仅用于schema，其他POD按4字节字段布局。
 * 默认分析level0/1分别保留旧配置0/1100的业务字节。 */
#define CZSC_API_VERSION 20

/* czsc_signal.context 位定义（研判语义，信号确认当时计算、随信号冻结） */
#define CZSC_CTX_ABC 0x01u            /* a+A+b+B+c 完整：一类的 c 段内含 B 中枢的三类点（第37课） */
#define CZSC_CTX_ZERO_PULLBACK 0x02u  /* 一类：B 中枢期间黄白线回拉零轴（DIF 穿越零轴或 |DIF| 最小值 <= b 段峰值 10%，第24/25课） */
#define CZSC_CTX_LINE_WEAK 0x04u      /* 一类：c 段黄白线（DIF）极值不及 b 段（第25课） */
#define CZSC_CTX_STANDARD 0x08u       /* 一类：标准背驰 = 同色面积背驰 且 黄白线回零（第24课） */
#define CZSC_CTX_SMALL_TURN 0x10u     /* 三类：同中枢此前有同向一类点（小转大必要条件，第44课） */
#define CZSC_CTX_OVERLAP 0x20u        /* 二类与三类重合（同中枢、同向、同一回试端点，第21/61课） */
#define CZSC_CTX_FIRST_RETEST 0x40u   /* 三类：首次回试（第20课“必须是第一次”） */

/* v20断代：分析、输出、显示三层独立。枚举值见schema；字符串UTF-8/NUL。 */
typedef struct czsc_config
{
  uint32_t size;
  int32_t strokeRule;       /* 0严格/1新笔/2czsc/3社区4K/4社区分型 */
  int32_t strokeEndpoint;   /* 0极值/1次高低/2合并K线包络 */
  int32_t strokeGap;        /* 0不处理/1计作一根/2大缺口成笔 */
  float gapThreshold;      /* 大缺口相对前根高/低阈值，默认0.02 */
  int32_t segmentMethod;    /* 0启发式/1特征序列，默认1 */
  int32_t centerStrokeFormation; /* 0进入段/1所属线段 */
  int32_t signalsPublication;    /* 0标准/1快速，默认0 */
} czsc_config; /* size32，align4，两种指针宽度一致 */
typedef struct czsc_projection
{
  uint32_t size;
  int32_t segmentBoundary; /* 0极值/1合并首笔/2合并末笔，仅特征序列允许非0 */
  int32_t centerBox;       /* 0前三构件/1含延伸，默认1；只改start/end显示坐标 */
} czsc_projection; /* size12，align4 */
#define CZSC_OUTPUT_STROKE 1u
#define CZSC_OUTPUT_SEGMENT 2u
#define CZSC_OUTPUT_EVENTS 4u
#define CZSC_OUTPUT_RECURSION 8u
#define CZSC_OUTPUT_NESTED 16u
#define CZSC_OUTPUT_DEFAULT 31u
#pragma pack(push, 1)
typedef struct czsc_config_field
{
  uint32_t size;
  char key[32], label[32];
  int32_t layer, kind, defaultValue; /* layer0分析/1输出/2显示；kind0枚举/1浮点 */
  float defaultFloat, minFloat, maxFloat;
} czsc_config_field; /* packed size92 */
typedef struct czsc_config_choice
{
  uint32_t size;
  char field[32]; int32_t value;
  char key[32], label[32], lessons[32];
  int32_t original;
  char note[256];
} czsc_config_choice; /* packed size396 */
typedef struct czsc_config_rule
{
  uint32_t size;
  char whenField[32]; int32_t whenValue;
  char field[32]; int32_t onlyValue; /* -1不适用；其余为唯一合法值 */
  char reason[128];
} czsc_config_rule; /* packed size204 */
#pragma pack(pop)
CZSC_API int32_t czsc_config_default(czsc_config *out);
CZSC_API int32_t czsc_config_validate(const czsc_config *c); /* 0合法，其他值中文原因 */
CZSC_API int32_t czsc_config_id(const czsc_config *c, char *out, int32_t cap);
/* id返回含NUL所需容量；NULL/0查询，容量不足不写入；非法返回-1。 */
CZSC_API int32_t czsc_config_parse(const char *id, czsc_config *out); /* 缺失字段用默认，未知/重复字段拒绝 */
CZSC_API int32_t czsc_projection_default(czsc_projection *out);
/* schema：NULL/0查询条数；否则写min(cap,总数)；非法容量返回0并设置错误。 */
CZSC_API int32_t czsc_config_fields(czsc_config_field *out, int32_t cap);
CZSC_API int32_t czsc_config_choices(czsc_config_choice *out, int32_t cap);
CZSC_API int32_t czsc_config_rules(czsc_config_rule *out, int32_t cap);

typedef struct czsc_input
{
  uint32_t size;          /* sizeof(czsc_input) */
  int32_t n;              /* K线根数，0..16777216 */
  const float *high;      /* n 个最高价（n>0 时必填，须有限且 >= low） */
  const float *low;       /* n 个最低价 */
  const float *close;     /* n 个收盘价（必填，须落在 [low,high]；MACD 用真实收盘价） */
  const float *volume;    /* n 个成交量（必填，须有限且 >= 0；用于放量湿吻判定） */
} czsc_input;

/* 端点：level0笔端点或level1线段端点，顶底交替 */
typedef struct czsc_pivot
{
  uint32_t size;          /* sizeof(czsc_pivot) */
  int32_t index;          /* 显示分界K线，segmentBoundary=0为极值；笔级不受分界投影影响 */
  int32_t kind;           /* +1 顶 / -1 底 */
  float price;            /* 显示index的原始K线高/低；用于画线，分析价位见extremeIndex */
  int32_t fractalAt;      /* 真实极值分型成立的K线；endpoint=bounded未确认延伸候选为-1；显示分界见extremeIndex */
  int32_t confirmedAt;    /* 端点定型K线；默认其后第二个端点出现即定型；endpoint=bounded允许尾部回退，暂全部为-1 */
  /* ---- v8：社区显示口径只改index/price；下游分析仍按真实极值 ---- */
  int32_t extremeIndex;   /* 真实分析端点K线；默认/笔级等于index。offset24，size28，无填充 */
} czsc_pivot;

/* 下游中枢/走势/背驰/信号/递归表均按extremeIndex计算，表序号不受显示投影影响。
 * 信号index可不同于其pivot行的显示index；fractalAt和established仍按真实极值。 */

/* 中枢（第17/18/20课）：进入段之后三段重叠成枢，[ZD,ZG] 成枢即固定，与之重叠的段延伸 GG/DD */
typedef struct czsc_center
{
  uint32_t size;          /* sizeof(czsc_center) */
  int32_t start;          /* 起点K线（首个成员端点） */
  int32_t end;            /* 终点K线（最后成员端点；离开段不属于本中枢） */
  int32_t firstPivot;     /* 首个成员端点，端点表下标 */
  int32_t lastPivot;      /* 最后成员端点，端点表下标 */
  float zg;               /* 中枢上沿 ZG = 成枢三段高点的最小值 */
  float zd;               /* 中枢下沿 ZD = 成枢三段低点的最大值 */
  float gg;               /* 波动上沿 GG = 成员段最高点 */
  float dd;               /* 波动下沿 DD = 成员段最低点 */
  int32_t direction;      /* 默认/线段级：进入段方向；strokeFormation=segment笔级：父分析线段方向。进入段仍firstPivot-1 */
  int32_t confirmedAt;    /* 中枢定型的K线（终点、GG/DD 不再改变）；未定型 -1 */
  int32_t relationToPrev; /* 与前一中枢（第20课中心定理二）：1 上涨 / -1 下跌 / 2 扩展 / 0 首个中枢 */
  /* ---- v2 ---- */
  int32_t lifecycle;      /* 相对前一中枢（第18/20课）：0 延伸（[ZD,ZG] 重叠）/ 1 扩展（仅 GG/DD 重叠）/
                             2 新生上 / 3 新生下；首个中枢 -1 */
  /* ---- v6 ---- */
  int32_t established;    /* 中枢成立的K线 = 第三个成员段终端点（端点表 firstPivot+3）的分型成立；
                             与第 0 层递归节点 established、czsc_recursive_center.established 同口径 */
} czsc_center;

/* 走势类型（第17课）：盘整 = 1 个中枢；趋势 = 连续同向关系的 >=2 个中枢 */
typedef struct czsc_movement
{
  uint32_t size;          /* sizeof(czsc_movement) */
  int32_t type;           /* 0 盘整 / 1 上涨 / -1 下跌 */
  int32_t firstCenter;    /* 首个中枢，中枢表下标 */
  int32_t lastCenter;     /* 最后中枢，中枢表下标 */
  int32_t start;          /* 起点K线（首个中枢起点） */
  int32_t end;            /* 终点K线（最后中枢终点） */
  int32_t confirmedAt;    /* 走势定型的K线（其后一个中枢也已定型，分组不再改变）；未定型 -1 */
  /* ---- v3：完成证据（第17课走势终完美：一个走势类型完成即转化为另一走势类型；第29课背驰-转折） ---- */
  int32_t connectionStart;        /* 与后继走势的连接段起点 = 本走势最后中枢的末端点，端点表下标 */
  int32_t connectionEnd;          /* 连接段终点 = 后继走势首个中枢的首端点，端点表下标；无后继 -1 */
  int32_t successor;              /* 后继走势，走势表下标；无 -1 */
  int32_t successorEstablishedAt; /* 后继走势首个中枢成立的K线（其第三段终点的分型成立）；无后继 -1 */
  int32_t completedAt;            /* 本走势完成的K线 = successorEstablishedAt；无后继 -1 */
  int32_t completedByIndex;       /* 趋势末端的一类买卖点所在K线（上涨看一卖、下跌看一买，须在最后中枢上）；盘整或无 -1 */
  int32_t completedBy;            /* 同上，信号表下标（事后行）；-1。与信号引用一样随快照解析，非因果字段 */
  /* ---- v5：中阴阶段（第89课：前走势完成后、后继走势类型确立前）。中阴结束 = successorEstablishedAt ---- */
  int32_t zhongyinStart;          /* 中阴开始的K线 = 与后继的连接极值点之后的下一端点分型成立（此后前走势不破该点即不能延续）；
                                     无后继 -1。口径见 chan-ambiguity-decisions.md「中阴阶段」 */
} czsc_movement;

/* 背驰度量（第15/24/37课）：当前段 cur 相对前一同向段 prev */
typedef struct czsc_divergence
{
  uint32_t size;          /* sizeof(czsc_divergence) */
  int32_t prevStart;      /* 前一同向段起点，端点表下标（无则 -1） */
  int32_t prevEnd;        /* 前一同向段终点，端点表下标 */
  int32_t curStart;       /* 当前段起点，端点表下标 */
  int32_t curEnd;         /* 当前段终点，端点表下标 */
  float prevSpace;        /* 前段价差（绝对值） */
  float prevSpeed;        /* 前段价差 / K线数 */
  float prevArea;         /* 前段同色 MACD 柱面积（向上看红柱、向下看绿柱，第24课） */
  float curSpace;         /* 当前段价差 */
  float curSpeed;         /* 当前段速度 */
  float curArea;          /* 当前段同色 MACD 柱面积 */
  int32_t newExtreme;     /* 1 = 当前段创新高/新低（背驰前提，第61课） */
  int32_t weakSpace;      /* 1 = curSpace < prevSpace */
  int32_t weakSpeed;      /* 1 = curSpeed < prevSpeed */
  int32_t weakArea;       /* 1 = 两段面积均 >0 且 curArea < prevArea */
  int32_t holds;          /* 1 = 背驰成立：一类为 newExtreme 且 (weakArea 或 weakSpace 且 weakSpeed)；
                             二类/离开段为 weakArea 或 (weakSpace 且 weakSpeed) */
  int32_t semantic;       /* 0 不成立 / 1 趋势背驰（一类）/ 2 盘整背驰（二、三类与离开段） */
} czsc_divergence;

/* 中枢的首次离开与回试（第20课“必须是第一次”） */
typedef struct czsc_breakout
{
  uint32_t size;          /* sizeof(czsc_breakout) */
  int32_t center;         /* 中枢表下标 */
  int32_t direction;      /* +1 向上离开 / -1 向下离开 */
  int32_t leavePivot;     /* 离开段终点，端点表下标 */
  int32_t retestPivot;    /* 首次回试终点，端点表下标 */
  int32_t third;          /* 1 = 回试不回 [ZD,ZG]，构成三类买卖点 */
  int32_t confirmedAt;    /* 定型的K线；未定型 -1 */
  czsc_divergence divergence; /* 离开段相对前一同向段的盘整背驰（第24课） */
} czsc_breakout;

/* 买卖点：每行是一段信号生命。
 *  - hindsight=1 的行与事后全量集合（通达信 13 号输出）一一对应，顺序为 二类、三类、一类；
 *  - 其余行是当下出现过、此后被撤销（或同键非胜出）的信号生命，按出现顺序排在后面。
 * 已确认的行（confirmedAt>=0）内容冻结为确认那一刻所知；未确认的事后行取全部数据下的内容。
 * 引用字段（pivot/center/movement/breakout/basedOn/divergence.*）指向本快照的表，按K线身份解析，
 * 被引用对象已不存在时为 -1（常见于已撤销的行：结构改写使原端点/中枢消失，信号也因此失效）。
 * 因果保证：confirmedAt<=k 的行，其值字段（index/type/stop/confirmedAt/divergence 数值）在更长数据的快照中不变，
 * revokedAt 只会从 -1 变为 >k 的值；引用只在所引对象未被改写时保持同一身份。 */
typedef struct czsc_signal
{
  uint32_t size;          /* sizeof(czsc_signal) */
  int32_t index;          /* 信号所在K线（信号端点的极值K线） */
  int32_t pivot;          /* 信号端点，端点表下标 */
  int32_t type;           /* 1/2/3 = 一二三类买点，-1/-2/-3 = 一二三类卖点 */
  int32_t center;         /* 所属中枢，中枢表下标（一类=趋势最后中枢；二类=信号K线所在中枢；三类=被离开的中枢） */
  int32_t movement;       /* 所属中枢所在的走势，走势表下标 */
  int32_t breakout;       /* 三类所依的离开/回试，突破表下标；其他类 -1 */
  int32_t basedOn;        /* 二类所依的一类信号，信号表下标；其他类 -1 */
  float stop;             /* 失效价：一/二买为信号点低点、三买为 ZG（第20/21/27课）；卖点对称为高点/ZD */
  int32_t confirmedAt;    /* 信号成立的K线：信号端点已被下一端点确认（=事件流的出现）；未确认 -1 */
  int32_t revokedAt;      /* 当下口径下被撤销的K线（新低/新高否定背驰等）；未撤销 -1 */
  int32_t hindsight;      /* 1 = 属于事后全量集合 */
  czsc_divergence divergence; /* 一类：b 段 vs c 段；二类：一买后两段；三类：离开段 vs 前一同向段 */
  /* ---- v2 ---- */
  int32_t quality;        /* 1 确认 / 2 强质。一类：标准背驰且 abc 完整为 2（第24/37课）；二类：自身盘整背驰或二三重合为 2
                             （第27/61课）；三类：离开段无盘整背驰（有力离开）或二三重合为 2（第20/53/61课） */
  uint32_t context;       /* CZSC_CTX_* 位掩码 */
  int32_t secondBasePivot;      /* 二类：所依一类的端点，端点表下标；其他 -1（第21课） */
  int32_t secondTurnPivot;      /* 二类：一类之后的转折端点（次级别第一段终点），端点表下标；其他 -1 */
  int32_t smallTurnBasePivot;   /* 三类且 CZSC_CTX_SMALL_TURN：同中枢此前同向一类的端点；其他 -1（第44课） */
  int32_t smallTurnLeavePivot;  /* 同上：离开段终点 */
  int32_t smallTurnRetestPivot; /* 同上：回试终点 */
} czsc_signal;

/* 当下事件流（outputs EVENTS）：与通达信 5/6 号序列逐根等价 */
typedef struct czsc_event
{
  uint32_t size;          /* sizeof(czsc_event) */
  int32_t bar;            /* 事件发生的K线 */
  int32_t op;             /* +1 出现 / -1 撤销 */
  int32_t signal;         /* 信号表下标 */
} czsc_event;

/* 逐根序列：MACD（用真实收盘价，EMA 12/26/9）、均线吻、缺口、分型强弱，均为当下可知 */
typedef struct czsc_bar
{
  uint32_t size;          /* sizeof(czsc_bar) */
  float dif;              /* DIF = EMA12 - EMA26 */
  float dea;              /* DEA = EMA9(DIF) */
  float macd;             /* 柱 = (DIF - DEA) * 2 */
  int32_t kiss;           /* MA5 与 MA20 的吻（第11/12课）：0 无 / 1 飞吻 / 2 唇吻 / 3 湿吻 / 4 放量湿吻 */
  int32_t gap;            /* 缺口：1 向上（前根最高 < 本根最低）/ -1 向下 / 0 无 */
  int32_t fractalStrength;/* 写在分型成立那根：2 强顶 / 1 顶 / -1 底 / -2 强底 / 0 无（第62/82课） */
  /* ---- v2 ---- */
  int32_t instantDivergence; /* 即时背驰预警（第24课）：当时已知的最后端点起的未完成段创新极值且相对前一同向段背驰，
                                向上段 +1（见顶预警）/ 向下段 -1（见底预警）/ 0 */
  /* ---- v6：均线吻所用的同一组均线（第11课），价格 = 收盘价；前 N-1 根为已有K线的算术平均（窗口不足时不补空） ---- */
  float maShort;          /* MA5 */
  float maLong;           /* MA20 */
} czsc_bar;

/* 区间套（第27/61课；小转大候选见第43/44课）：低级别一类信号 → 高级别结构。由同一个快照两级结构生成。 */
typedef struct czsc_nested
{
  uint32_t size;               /* sizeof(czsc_nested) */
  int32_t lowSignal;           /* 低级别快照的信号表下标（一类买卖点） */
  int32_t highSignal;          /* 高级别同向一类信号（信号表下标），其背驰段 c 包含 lowSignal 的K线；无则 -1 */
  int32_t highSegmentStart;    /* 高级别快照中包含 lowSignal K线的那一段：起点端点表下标（无则 -1） */
  int32_t highSegmentEnd;      /* 同上：终点端点表下标（该段尚未结束为 -1） */
  int32_t insideHighSegment;   /* 1 = 低级别背驰落在高级别背驰段内（highSignal>=0） */
  int32_t confirmed;           /* 1 = inside 且高低两级信号均已确认（confirmedAt>=0） */
  int32_t newExtreme;          /* 1 = 低级别背驰创新高/新低（第61课：无新高新低只可能是盘整背驰） */
  int32_t smallTurn;           /* 1 = 小转大候选：低级别背驰落在方向一致的高级别段内、但该段无高级别背驰（第43课）；
                                  其必要条件（最后次级别中枢出现同向三类点，第44课）由调用方结合后续三类信号判断 */
  /* ---- v3：高级别背驰段映射到低级别端点（线段端点必为笔端点，第67课），低级别快照端点表下标；无 -1 ---- */
  int32_t highPrevStartLow;    /* 高级别 b 段起点 */
  int32_t highPrevEndLow;      /* 高级别 b 段终点 */
  int32_t highCurStartLow;     /* 高级别 c 段起点 */
  int32_t highCurEndLow;       /* 高级别 c 段终点 */
} czsc_nested;

CZSC_API int32_t czsc_api_version(void);
/* v6：构建时写入的源码 git commit（12 位短哈希，构建时工作区有未提交改动则加 "-dirty"）；未经 make 构建为 "unknown"。
 * 返回静态字符串，勿释放 */
CZSC_API const char *czsc_build_commit(void);
CZSC_API const char *czsc_last_error(void);
CZSC_API void *czsc_build(const czsc_input *input, const czsc_config *config, uint32_t outputs);
CZSC_API int32_t czsc_set_projection(void *snapshot, const czsc_projection *projection);
CZSC_API void czsc_snapshot_free(void *snapshot);

/* 取表：返回首元素指针（空表返回非 NULL 的有效地址或 NULL，均以 *count 为准），*count 写条数；
 * snapshot 为 NULL 时返回 NULL、*count=0 并设置错误。count 可为 NULL。 */
CZSC_API const czsc_pivot *czsc_level_pivots(void *snapshot, int32_t level, int32_t *count);
CZSC_API const czsc_center *czsc_level_centers(void *snapshot, int32_t level, int32_t *count);
CZSC_API const czsc_movement *czsc_level_movements(void *snapshot, int32_t level, int32_t *count);
CZSC_API const czsc_breakout *czsc_level_breakouts(void *snapshot, int32_t level, int32_t *count);
CZSC_API const czsc_signal *czsc_level_signals(void *snapshot, int32_t level, int32_t *count);
CZSC_API const czsc_event *czsc_level_events(void *snapshot, int32_t level, int32_t *count);   /* 未请求EVENTS 时为空 */
CZSC_API const czsc_bar *czsc_level_bars(void *snapshot, int32_t level, int32_t *count);        /* n 条 */

/* 递归走势节点（outputs RECURSION，第17课：某级别中枢由至少三个连续次级别走势类型重叠构成）。
 * level 与配置级别的关系：level 0 = 本配置级别的走势（与 czsc_movements 按 ordinal 一一对应，其中枢即 czsc_centers）；
 * level L+1 以 level L 相邻走势之间的连接点为端点，再构中枢、分走势，逐层向上直到不能成枢。
 * 节点按 (level, ordinal) 升序排列；表内下标随数据长度变化，跨快照比对请用 (level, ordinal)。
 * children（czsc_recursive_children）指向下一层（level-1）的节点，不是本层中枢；本层中枢见 firstCenter/lastCenter。
 * 多义处的取舍见 chan-ambiguity-decisions.md。 */
typedef struct czsc_recursive_node
{
  uint32_t size;          /* sizeof(czsc_recursive_node) */
  int32_t level;          /* 层级，0 = 配置级别 */
  int32_t ordinal;        /* 本层内序号（跨快照比对用：同层同序号即同一节点） */
  int32_t type;           /* 0 盘整 / 1 上涨 / -1 下跌 */
  int32_t start;          /* 起点K线（与前一走势的连接点；首个走势为首中枢的进入点） */
  int32_t end;            /* 终点K线（与后一走势的连接点；最后一个走势暂为最后端点） */
  int32_t centerStart;    /* 首个本层中枢起点K线 */
  int32_t centerEnd;      /* 最后本层中枢终点K线 */
  int32_t centerCount;    /* 本层中枢个数（盘整 1，趋势 >=2） */
  int32_t established;    /* 首个中枢成立的K线（第 0 层为第三段终点分型成立；上层为该端点定型）；未知 -1 */
  int32_t connection;     /* 与后继走势的连接点K线（= end）；无后继 -1 */
  int32_t completed;      /* 完成的K线 = 后继节点的 established（走势终完美）；无后继或未知 -1 */
  int32_t successor;      /* 同层后继节点，节点表下标；无 -1 */
  int32_t firstChild;     /* 子节点在 czsc_recursive_children 中的起始位置；第 0 层无子节点为 -1 */
  int32_t childCount;     /* 子节点个数（下一层的节点，按时间顺序） */
  int32_t confirmedAt;    /* 节点定型的K线（起止与分组都不再改变）；未定型 -1 */
  /* ---- v5 ---- */
  int32_t firstCenter;    /* 首个本层中枢：level 0 为 czsc_centers 下标，level >= 1 为 czsc_recursive_centers 下标 */
  int32_t lastCenter;     /* 最后本层中枢，同上 */
  float high;             /* 节点 [start, end] 内最高价 */
  float low;              /* 节点 [start, end] 内最低价 */
  int32_t zhongyinStart;  /* 中阴开始的K线（口径同 czsc_movement.zhongyinStart；上层为该端点定型）；无后继 -1。中阴结束 = completed */
} czsc_recursive_node;

/* 递归中枢（outputs RECURSION，v5）：level >= 1 的中枢，按 (level, ordinal) 升序；level 0 的中枢即 czsc_centers，不在此表 */
typedef struct czsc_recursive_center
{
  uint32_t size;          /* sizeof(czsc_recursive_center) */
  int32_t level;          /* 层级（>= 1），与节点同一套编号 */
  int32_t ordinal;        /* 本层内序号 */
  int32_t start;          /* 起点K线（首个成员端点） */
  int32_t end;            /* 终点K线（最后成员端点） */
  float zg;               /* 与 czsc_center 同义（第20课：[ZD,ZG] 成枢即固定） */
  float zd;
  float gg;
  float dd;
  int32_t direction;      /* 进入段方向：+1 / -1 */
  int32_t firstMember;    /* 首个成员 = 下一层（level-1）节点，czsc_recursive_nodes 下标；成员在节点表中连续 */
  int32_t memberCount;    /* 成员个数（第17课：至少三个次级别走势类型） */
  int32_t established;    /* 中枢成立的K线（第三个成员段的终端点定型）；未定型 -1 */
  int32_t confirmedAt;    /* 定型K线（自身与全部成员均定型）；未定型 -1 */
} czsc_recursive_center;

/* 同级别连接段（outputs RECURSION，v5，第18课中枢定理一、第17/33课结合律）：level >= 1 相邻两个走势之间的连接段，
 * 前走势最后中枢末端点 → 后走势首中枢首端点。level 0 的连接段见 czsc_movement.connectionStart/End，不在此表 */
typedef struct czsc_recursive_connection
{
  uint32_t size;          /* sizeof(czsc_recursive_connection) */
  int32_t level;          /* 层级（>= 1） */
  int32_t ordinal;        /* = left 节点的 ordinal */
  int32_t left;           /* 前走势，czsc_recursive_nodes 下标 */
  int32_t right;          /* 后走势，czsc_recursive_nodes 下标 */
  int32_t start;          /* 起点K线 */
  int32_t end;            /* 终点K线 */
  int32_t firstMember;    /* 覆盖连接段的下一层节点链，czsc_recursive_nodes 下标（连续，首尾相接） */
  int32_t memberCount;
  int32_t confirmedAt;    /* 定型K线（两侧分组与全部成员均定型）；未定型 -1 */
} czsc_recursive_connection;

/* 区间套直接读取family快照；须请求NESTED与两级。递归行level为本级内部递归层，非访问参数level。 */
CZSC_API const czsc_recursive_node *czsc_level_recursive_nodes(void *snapshot, int32_t level, int32_t *count);  /* 未请求RECURSION时为空 */
CZSC_API const int32_t *czsc_level_recursive_children(void *snapshot, int32_t level, int32_t *count);          /* 子节点的节点表下标 */
CZSC_API const czsc_recursive_center *czsc_level_recursive_centers(void *snapshot, int32_t level, int32_t *count);          /* v5，未请求RECURSION时为空 */
CZSC_API const czsc_recursive_connection *czsc_level_recursive_connections(void *snapshot, int32_t level, int32_t *count);  /* v5，未请求RECURSION时为空 */
CZSC_API const czsc_nested *czsc_nested_rows(void *nested, int32_t *count);

#ifdef __cplusplus
}
#endif

#endif /* CZSC_API_H */
