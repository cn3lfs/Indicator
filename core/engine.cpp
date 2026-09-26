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
      p.fractalAt = ends[i].confirmedAt;
      strokePivots_[i] = p;
    }
    strokeDirty_ = MinDirty(strokeDirty_, static_cast<int>(from));
  }

  // 一个时刻的全部分型加入后，推进下游并产出事件，再推进各层定型边界
  void Step(int bar, bool emit, std::vector<SignalEvent> &events)
  {
    if (strokeDirty_ < 0) return;
    int dirty = strokeDirty_;
    strokeDirty_ = -1;
    bool changed = true;
    if (config_.unit == CenterUnit::Segment)
    {
      dirty = segments_.Update(strokePivots_, static_cast<std::size_t>(dirty));
      changed = dirty >= 0;
      if (changed) Energize(segments_.Pivots(), static_cast<std::size_t>(dirty));
    }
    else
    {
      Energize(strokePivots_, static_cast<std::size_t>(dirty));
    }
    if (changed)
    {
      int dc = centers_.Update(pivots_, static_cast<std::size_t>(dirty));
      int dm = UpdateMovements(centers_.Centers(), moves_, dc);
      signals_.Update(pivots_, centers_.Centers(), moves_, dirty, dc, dm, bar, emit, events);
    }
    AdvanceFinality(bar);
  }

  // 定型边界只增不减：笔端点 → (线段端点) → 中枢 → 走势 / 突破。
  // 已定型的输入此后不再改变，视界落在其内的检查点之前的输出也就不再改变。
  void AdvanceFinality(int bar)
  {
    std::size_t strokeFinal = strokes_.FinalCount();
    std::size_t pivotFinal = config_.unit == CenterUnit::Segment ? segments_.FinalCount(strokeFinal) : strokeFinal;
    pivotFinal = std::min(pivotFinal, pivots_.size());
    std::size_t centerFinal = std::min(centers_.FinalCount(pivotFinal), centers_.Centers().size());
    // 走势 [a,b] 由关系 (b,b+1) 截止：其后一个中枢也已定型才定型
    std::size_t moveFinal = 0;
    while (moveFinal < moves_.size() && static_cast<std::size_t>(moves_[moveFinal].lastCenter) + 1 < centerFinal)
      moveFinal++;
    Mark(pivotFinalAt, pivotFinal, bar);
    Mark(centerFinalAt, centerFinal, bar);
    Mark(movementFinalAt, moveFinal, bar);
    if (breakoutFinalAt.size() < centerFinal) breakoutFinalAt.resize(centerFinal, -1);
    for (std::size_t ci = breakoutScan_; ci < centerFinal; ci++)
      if (breakoutFinalAt[ci] < 0 && signals_.BreakoutFinal(ci, pivotFinal)) breakoutFinalAt[ci] = bar;
    while (breakoutScan_ < breakoutFinalAt.size() && breakoutFinalAt[breakoutScan_] >= 0) breakoutScan_++;
  }

  std::vector<int> pivotFinalAt, centerFinalAt, movementFinalAt, breakoutFinalAt;

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
  static void Mark(std::vector<int> &at, std::size_t count, int bar)
  {
    if (at.size() < count) at.resize(count, bar);
  }

  std::size_t breakoutScan_ = 0;
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
  auto fit = [](std::vector<int> v, std::size_t n) {
    v.resize(n, -1);
    return v;
  };
  a.pivotFinalAt = fit(inc.pivotFinalAt, a.snapshot.pivots.size());
  a.centerFinalAt = fit(inc.centerFinalAt, a.snapshot.centers.size());
  a.movementFinalAt = fit(inc.movementFinalAt, a.snapshot.movements.size());
  a.breakoutFinalAt = fit(inc.breakoutFinalAt, a.snapshot.centers.size());
  a.energy = std::move(tables);
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
