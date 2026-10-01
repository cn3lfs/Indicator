// 当下引擎：按时间逐根推进，每当新分型成立（当下可知的最早时刻）就更新端点→中枢→走势→买卖点，
// 并对“端点已被下一端点确认”的买卖点记录出现/失效事件。全量结果=推进到最后一根时的快照。
//
// 因果性由构造保证：分型在右侧首根非包含K线出现即定型，故 t 时刻可见的分型恰是 confirmedAt<=t 的前缀；
// MACD 为因果 EMA，只读取下标 <=t 的值。引擎从不读取 t 之后的数据。
#pragma once

#include "core/config.h"
#include "core/dynamics.h"
#include "core/model.h"
#include "core/series.h"
#include "core/signals.h"

#include <vector>
#include <array>
#include <memory>

namespace chan
{

struct Snapshot
{
  std::vector<Pivot> pivots;
  std::vector<Center> centers;
  std::vector<Movement> movements;
  std::vector<Breakout> breakouts;
  std::vector<Signal> signals;
};

// 两级共同的包含处理、真实分型、均线与MACD表；同一族只有一份不可变数据。
struct SharedAnalysisInputs
{
  std::vector<MergedBar> bars;
  std::vector<Fractal> fractals;
  MovingAverages ma;
  EnergyTables energy;
};

struct Analysis
{
  Config config;  // 单级视图配置，不参与共享分析身份。
  std::shared_ptr<const SharedAnalysisInputs> inputs;
  Snapshot snapshot;
  std::vector<SignalEvent> events;

  // 定型时刻：对象此后无论再来什么数据都不会改变的最早K线；尚未定型为 -1。
  // 与快照表逐行对应；breakoutFinalAt 按中枢下标。
  std::vector<int> pivotFinalAt;
  std::vector<int> centerFinalAt;
  std::vector<int> movementFinalAt;
  std::vector<int> breakoutFinalAt;

  // 即时背驰预警（第24课，逐根、当下可知）：当前未完成段自最后端点起创新极值且相对前一同向段背驰时，
  // 向上段为 +1（见顶预警）、向下段为 -1（见底预警），否则 0
  std::vector<int8_t> instantWarning;
};

struct FamilyAnalysis
{
  std::array<Analysis,2> levels;
};

// 单次推进：共享笔与线段流，每级独立维护中枢/走势/信号/定型。
FamilyAnalysis AnalyzeFamily(const Series &series, const AnalysisConfig &config, int window = 0);

// window>0 时只对最近 window 根K线内的时刻产生事件（其前的状态作为基线，不产生事件）
Analysis Analyze(const Series &series, const Config &config, int window = 0);

// 参照实现：每个时刻从分型前缀整条重算快照再差分。与 Analyze 逐事件一致，仅供测试对照增量逻辑。
Analysis AnalyzeReference(const Series &series, const Config &config, int window = 0);

// 由分型前缀得到结构快照（纯函数；引擎每一步与全量结果共用）
Snapshot BuildSnapshot(const std::vector<Fractal> &fractals, std::size_t count, const EnergyTables &tables,
                       const Config &config, const Series *source = nullptr);

}  // namespace chan
