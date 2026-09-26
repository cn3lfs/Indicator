// 缠论领域模型：只含不可变值类型，不含任何算法、输出或全局状态。
// 层次（第72课懒人线路图）：K线 → 合并K线 → 分型 → 笔 → 线段 → 中枢 → 走势类型 → 买卖点。
// 分型带 confirmedAt（右侧首根非包含K线出现即成立）；端点、中枢等何时定型、此后不再改变，由引擎另行给出（Analysis::*FinalAt）。
#pragma once

#include <cstdint>
#include <vector>

namespace chan
{

// 顶/底（分型、端点）。值与 TDX 输出约定一致：顶 +1、底 -1。
enum class Kind : int8_t
{
  Top = 1,
  Bottom = -1,
};

inline Kind Opposite(Kind k) { return k == Kind::Top ? Kind::Bottom : Kind::Top; }

// 包含处理后的K线（第62/65课）。[first,last] 为覆盖的原始K线区间。
struct MergedBar
{
  int first = 0;
  int last = 0;
  int highAt = 0;   // 高点所在原始K线
  int lowAt = 0;    // 低点所在原始K线
  float high = 0;
  float low = 0;
};

// 分型（第62课）。index 为极值原始K线；merged 为中间合并K线下标。
struct Fractal
{
  Kind kind = Kind::Top;
  int index = 0;
  int merged = 0;
  float high = 0;   // 中间合并K线的高
  float low = 0;    // 中间合并K线的低
  int confirmedAt = -1;  // 右侧首根非包含K线出现即成立
};

// 端点：笔或线段的转折点，附该点累积 MACD 数据（动力学）。
struct Pivot
{
  Kind kind = Kind::Top;
  int index = 0;
  float high = 0;
  float low = 0;
  int fractalAt = -1;  // 端点所在分型成立的K线（端点此后仍可能被延伸替换，定型时刻见 Analysis::pivotFinalAt）
  // 动力学：截至 index 的累积 MACD 柱（代数和/红/绿）与 DIF/DEA
  float energy = 0;
  float energyRed = 0;
  float energyGreen = 0;
  float dif = 0;
  float dea = 0;

  float Price() const { return kind == Kind::Top ? high : low; }
};

// 中枢（第17/18/20课）。[firstPivot,lastPivot] 为构成中枢的端点下标；
// zg/zd 由成枢前两个 Zn 固定，gg/dd 随延伸扩张。direction 为进入段方向。
struct Center
{
  int firstPivot = 0;
  int lastPivot = 0;
  int start = 0;  // 起点原始K线
  int end = 0;    // 终点原始K线
  float zg = 0;
  float zd = 0;
  float gg = 0;
  float dd = 0;
  int direction = 0;
};

// 相邻同级中枢关系（第20课中心定理二）。
enum class CenterRelation : int8_t
{
  Up = 1,
  Down = -1,
  Expansion = 2,
};

// 走势类型（第17课）：盘整=1 个中枢，趋势=≥2 个依次同向中枢。
enum class MovementType : int8_t
{
  Consolidation = 0,
  Up = 1,
  Down = -1,
};

struct Movement
{
  MovementType type = MovementType::Consolidation;
  int firstCenter = 0;
  int lastCenter = 0;
  int start = 0;
  int end = 0;
};

// 买卖点类别（第20/21课）。数值即 TDX 输出码。
enum class SignalType : int8_t
{
  Buy1 = 1, Buy2 = 2, Buy3 = 3,
  Sell1 = 11, Sell2 = 12, Sell3 = 13,
};

inline bool IsBuy(SignalType t) { return static_cast<int>(t) < 10; }

// 背驰度量（第15/24课）：c 段相对 b 段。
struct Strength
{
  float space = 0;   // 价差
  float speed = 0;   // 价差/K线数
  float area = 0;    // 同色 MACD 柱面积
};

struct Divergence
{
  Strength previous;  // b 段（或 A 段）
  Strength current;   // c 段
  int previousStart = -1, previousEnd = -1, currentStart = -1, currentEnd = -1;  // 端点下标
  int previousStartIndex = -1, previousEndIndex = -1, currentStartIndex = -1, currentEndIndex = -1;  // 对应K线下标
  bool newExtreme = false;
  bool weakSpace = false;
  bool weakSpeed = false;
  bool weakArea = false;
  bool holds = false;  // newExtreme && (weakArea || (weakSpace && weakSpeed))
};

struct Signal
{
  SignalType type = SignalType::Buy1;
  int pivot = 0;       // 信号端点
  int index = 0;       // 信号所在原始K线
  int center = -1;     // 所属中枢
  int centerStart = -1;   // 所属中枢起点K线（跨时刻引用中枢用）
  int basedOnIndex = -1;  // 二类：所依一类买卖点所在K线
  int priority = 0;    // 同根取胜优先级：一类>二类>三类
  float stop = 0;      // 失效价（第20/21/27课）
  Divergence divergence;
};

// 当下事件：信号在 bar 这根K线上出现或失效。
struct SignalEvent
{
  int bar = 0;
  Signal signal;
  bool revoked = false;
};

}  // namespace chan
