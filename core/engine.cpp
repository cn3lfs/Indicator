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

int MinDirty(int a, int b)
{
  if (a < 0) return b;
  if (b < 0) return a;
  return a < b ? a : b;
}

// 增量状态：笔流 → (线段流) → 端点能量 → 中枢流 → 走势 → 买卖点流，每层只重算受上游变化影响的部分
class Incremental
{
public:
  Incremental(const std::vector<Fractal> &fractals, const EnergyTables &tables, const Config &config)
    : strokes_(fractals, config), segments_(config.segment), tables_(tables), config_(config)
  {
  }

  // 加入第 k 个分型；只登记变化，不重算下游
  void AddFractal(std::size_t k)
  {
    int changed = strokes_.Add(k);
    if (changed < 0) return;
    const std::vector<Fractal> &ends = strokes_.Ends();
    if (ends.size() < 2)
    {
      if (!strokePivots_.empty()) strokeDirty_ = MinDirty(strokeDirty_, 0);
      strokePivots_.clear();
      return;
    }
    std::size_t from = strokePivots_.empty() ? 0 : static_cast<std::size_t>(changed);
    strokePivots_.resize(ends.size());
    for (std::size_t i = from; i < ends.size(); i++)
    {
      Pivot p;
      p.kind = ends[i].kind;
      p.index = ends[i].index;
      p.high = ends[i].high;
      p.low = ends[i].low;
      p.confirmedAt = ends[i].confirmedAt;
      strokePivots_[i] = p;
    }
    strokeDirty_ = MinDirty(strokeDirty_, static_cast<int>(from));
  }

  // 一个时刻的全部分型加入后，推进下游并产出事件
  void Step(int bar, bool emit, std::vector<SignalEvent> &events)
  {
    if (strokeDirty_ < 0) return;
    int dirty = strokeDirty_;
    strokeDirty_ = -1;
    if (config_.unit == CenterUnit::Segment)
    {
      dirty = segments_.Update(strokePivots_, static_cast<std::size_t>(dirty));
      if (dirty < 0) return;
      Energize(segments_.Pivots(), static_cast<std::size_t>(dirty));
    }
    else
    {
      Energize(strokePivots_, static_cast<std::size_t>(dirty));
    }
    int dc = centers_.Update(pivots_, static_cast<std::size_t>(dirty));
    int dm = UpdateMovements(centers_.Centers(), moves_, dc);
    signals_.Update(pivots_, centers_.Centers(), moves_, dirty, dc, dm, bar, emit, events);
  }

private:
  // 端点从 from 起更新并赋能量（MACD 因果，只读 <= 端点下标的累积值）
  void Energize(const std::vector<Pivot> &src, std::size_t from)
  {
    pivots_.resize(src.size());
    for (std::size_t i = from; i < src.size(); i++) pivots_[i] = src[i];
    if (from < pivots_.size())
    {
      std::vector<Pivot> tail(pivots_.begin() + static_cast<std::ptrdiff_t>(from), pivots_.end());
      AssignEnergy(tail, tables_);
      std::copy(tail.begin(), tail.end(), pivots_.begin() + static_cast<std::ptrdiff_t>(from));
    }
  }

  StrokeStream strokes_;
  SegmentStream segments_;
  CenterStream centers_;
  SignalStream signals_;
  const EnergyTables &tables_;
  Config config_;
  std::vector<Pivot> strokePivots_;
  std::vector<Pivot> pivots_;
  std::vector<Movement> moves_;
  int strokeDirty_ = -1;
};

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
  Incremental inc(a.fractals, tables, config);
  const std::vector<Fractal> &f = a.fractals;
  for (std::size_t k = 0; k < f.size(); k++)
  {
    inc.AddFractal(k);
    int bar = f[k].confirmedAt;
    if (k + 1 < f.size() && f[k + 1].confirmedAt == bar) continue;  // 同一时刻成立的分型一并处理
    inc.Step(bar, bar >= from, a.events);
  }
  a.snapshot = BuildSnapshot(f, f.size(), tables, config);
  return a;
}

Analysis AnalyzeReference(const Series &series, const Config &config, int window)
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
    if (k + 1 < f.size() && f[k + 1].confirmedAt == bar) continue;
    bool baseline = bar < from;
    if (baseline && k + 1 < f.size() && f[k + 1].confirmedAt < from) continue;
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
