// 动力学（第11-15/24/27课）：MACD 累积表、走势段力度、背驰判定、均线与吻。
// MACD 与均线都是因果的（只用截至当根的数据），前缀取值与前缀重算一致。
#pragma once

#include "core/model.h"
#include "core/series.h"

#include <vector>

namespace chan
{

// 逐根 MACD 累积表：柱代数和 / 红柱 / 绿柱，以及 DIF、DEA（EMA 12/26/9，柱=(DIF-DEA)*2）
struct EnergyTables
{
  std::vector<float> cumulative, red, green, dif, dea;
};

EnergyTables BuildEnergyTables(const Series &s);
void AssignEnergy(std::vector<Pivot> &pivots, const EnergyTables &t);

// 走势段力度：价差、平均速度、同色 MACD 面积（向上看红柱、向下看绿柱，第24课）
Strength MeasureStrength(const Pivot &start, const Pivot &end);

// 背驰（第15/24课）：c 段创新高/新低，且 MACD 面积变小，或价差与速度同时变小
Divergence MeasureDivergence(const Pivot &prevStart, const Pivot &prevEnd,
                             const Pivot &curStart, const Pivot &curEnd, int direction);

// 均线系统（第11-15课）
std::vector<float> MovingAverage(const std::vector<float> &price, int period);

enum class Kiss : int8_t
{
  None = 0,
  Fly = 1,      // 飞吻：短均线略走平后继续
  Lip = 2,      // 唇吻：靠近不破
  Wet = 3,      // 湿吻：升破/跌破
  WetTrap = 4,  // 放量湿吻：骗线嫌疑（第12课，需成交量）
};

std::vector<Kiss> ClassifyKisses(const std::vector<float> &shortMa, const std::vector<float> &longMa,
                                 const std::vector<float> &volume);

struct MovingAverages
{
  std::vector<float> shortMa, longMa;
  std::vector<Kiss> kisses;
};

MovingAverages BuildMovingAverages(const Series &s);

// 缺口（借鉴 czsc check_gap_info，只给当下可知部分）：H[i-1]<L[i] 为 +1，L[i-1]>H[i] 为 -1，否则 0
std::vector<int8_t> Gaps(const Series &s);

// 分型强弱（第82课：第三根扩大战果），写在分型成立那根K线：其收盘（无真实收盘价时顶用最低、底用最高）
// 跌破左侧合并K线低点为强顶 2，否则 1；底分型对称 -2/-1；其余 0
std::vector<int8_t> FractalStrengths(const Series &s, const std::vector<MergedBar> &bars,
                                     const std::vector<Fractal> &fractals);

}  // namespace chan
