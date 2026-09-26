#include "core/engine.h"

#include "core/morphology.h"
#include "core/structure.h"

#include <map>
#include <utility>

namespace chan
{
namespace
{

using SignalKey = std::pair<int, int>;  // (信号K线, 信号码)

// 当下可确认的信号：其端点之后已有下一端点（末端点仍可能延伸，不输出）；同键取优先级最高者
std::map<SignalKey, Signal> Confirmed(const Snapshot &s)
{
  std::map<SignalKey, Signal> out;
  for (const Signal &sig : s.signals)
  {
    if (sig.pivot < 0 || static_cast<std::size_t>(sig.pivot) + 1 >= s.pivots.size()) continue;
    SignalKey key{sig.index, static_cast<int>(sig.type)};
    auto it = out.find(key);
    if (it == out.end() || it->second.priority < sig.priority) out[key] = sig;
  }
  return out;
}

}  // namespace

Snapshot BuildSnapshot(const std::vector<Fractal> &fractals, std::size_t count, const EnergyTables &tables,
                       const Config &config)
{
  Snapshot s;
  std::vector<Fractal> prefix(fractals.begin(), fractals.begin() + static_cast<std::ptrdiff_t>(count));
  s.pivots = BuildPivots(prefix, config);
  AssignEnergy(s.pivots, tables);
  s.centers = BuildCenters(s.pivots);
  s.movements = BuildMovements(s.centers);
  s.breakouts = BuildBreakouts(s.pivots, s.centers);
  s.signals = BuildSignals(s.pivots, s.centers, s.movements, s.breakouts);
  return s;
}

Analysis Analyze(const Series &series, const Config &config, int window)
{
  Analysis a;
  a.config = config;
  a.bars = MergeBars(series);
  a.fractals = DetectFractals(a.bars);
  a.ma = BuildMovingAverages(series);
  EnergyTables tables = BuildEnergyTables(series);

  int from = window > 0 ? series.Size() - window : 0;
  std::map<SignalKey, Signal> active;
  const std::vector<Fractal> &f = a.fractals;
  for (std::size_t k = 0; k < f.size(); k++)
  {
    int bar = f[k].confirmedAt;
    if (k + 1 < f.size() && f[k + 1].confirmedAt == bar) continue;  // 同一时刻成立的分型一并处理
    bool baseline = bar < from;
    if (baseline && k + 1 < f.size() && f[k + 1].confirmedAt < from) continue;  // 窗口前只算最后一步作基线

    std::map<SignalKey, Signal> now = Confirmed(BuildSnapshot(f, k + 1, tables, config));
    if (!baseline)
    {
      for (const auto &kv : now)
        if (!active.count(kv.first)) a.events.push_back({bar, kv.second, false});
      for (const auto &kv : active)
        if (!now.count(kv.first)) a.events.push_back({bar, kv.second, true});
    }
    active.swap(now);
  }
  a.snapshot = BuildSnapshot(f, f.size(), tables, config);
  return a;
}

}  // namespace chan
