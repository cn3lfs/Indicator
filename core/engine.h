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

struct Analysis
{
  Config config;
  std::vector<MergedBar> bars;
  std::vector<Fractal> fractals;
  Snapshot snapshot;                 // 全部数据下的结构（画线/中枢用）
  std::vector<SignalEvent> events;   // 当下信号的出现/失效（回测/选股用）
  MovingAverages ma;
};

// window>0 时只对最近 window 根K线内的时刻产生事件（其前的状态作为基线，不产生事件）
Analysis Analyze(const Series &series, const Config &config, int window = 0);

// 由分型前缀得到结构快照（纯函数；引擎每一步与全量结果共用）
Snapshot BuildSnapshot(const std::vector<Fractal> &fractals, std::size_t count, const EnergyTables &tables,
                       const Config &config);

}  // namespace chan
