#include "core/engine.h"

#include "core/morphology.h"
#include "core/structure.h"

#include <map>
#include <utility>
#include <memory>
#include <stdexcept>

namespace chan
{
namespace
{

using SignalKey = std::pair<int, int>;  // (信号K线, 信号码)

// 当下可确认的信号：其端点之后已有下一端点（末端点仍可能延伸，不输出）；同键取优先级最高者
std::map<SignalKey, Signal> Confirmed(const Snapshot &s, bool early)
{
  std::map<SignalKey, Signal> out;
  for (const Signal &sig : s.signals)
  {
    if (sig.pivot < 0 || (early && s.pivots[static_cast<std::size_t>(sig.pivot)].extensionOnly) ||
        (!early && static_cast<std::size_t>(sig.pivot) + 1 >= s.pivots.size())) continue;
    SignalKey key{sig.index, static_cast<int>(sig.type)};
    auto it = out.find(key);
    if (it == out.end() || it->second.priority < sig.priority)
    {
      out[key] = sig;
      if (early) out[key].stop = s.pivots[static_cast<std::size_t>(sig.pivot)].Price();
    }
  }
  return out;
}

int MinDirty(int a, int b)
{
  if (a < 0) return b;
  if (b < 0) return a;
  return a < b ? a : b;
}

// 共享形态流：所有级别读取同一笔链及线段链，MACD端点能量只赋值一次。
class MorphologyStream
{
public:
  MorphologyStream(const std::vector<Fractal> &fractals, const LevelConfig &config, const Series &source,
                   const SharedAnalysisInputs &inputs, bool needSegments)
    : strokes(fractals,config,&source,&inputs.bars), segments(config.analysis.segment.method),
      tables_(inputs.energy), needSegments_(needSegments) {}
  // 加入第 k 个分型；只登记变化，不重算下游
  void AddFractal(std::size_t k)
  {
    int changed = strokes.Add(k);
    if (changed < 0) return;
    const std::vector<Fractal> &ends = strokes.Ends();
    if (ends.size() < 2)
    {
      if (!strokePivots.empty()) strokeDirty = MinDirty(strokeDirty, 0);
      strokePivots.clear();
      return;
    }
    std::size_t from = strokePivots.empty() ? 0 : static_cast<std::size_t>(changed);
    strokePivots.resize(ends.size());
    for (std::size_t i = from; i < ends.size(); i++)
    {
      Pivot p;
      p.kind = ends[i].kind;
      p.index = ends[i].index;
      p.high = ends[i].high;
      p.low = ends[i].low;
      p.fractalAt = ends[i].extensionOnly ? -1 : ends[i].confirmedAt;
      p.extensionOnly = ends[i].extensionOnly;
      strokePivots[i] = p;
    }
    strokeDirty = MinDirty(strokeDirty, static_cast<int>(from));
  }


  void Step()
  {
    segmentDirty = -1;
    if (strokeDirty < 0) return;
    std::size_t from = static_cast<std::size_t>(strokeDirty);
    if (from < strokePivots.size())
    {
      std::vector<Pivot> tail(strokePivots.begin()+static_cast<std::ptrdiff_t>(from),strokePivots.end());
      AssignEnergy(tail,tables_);
      std::copy(tail.begin(),tail.end(),strokePivots.begin()+static_cast<std::ptrdiff_t>(from));
    }
    if (needSegments_) segmentDirty = segments.Update(strokePivots,from);
  }
  void ClearDirty() { strokeDirty = segmentDirty = -1; }

  StrokeStream strokes;
  SegmentStream segments;
  std::vector<Pivot> strokePivots;
  int strokeDirty = -1, segmentDirty = -1;
private:
  const EnergyTables &tables_;
  bool needSegments_;
};

// 增量状态：笔流 → (线段流) → 端点能量 → 中枢流 → 走势 → 买卖点流，每层只重算受上游变化影响的部分
class Incremental
{
public:
  Incremental(MorphologyStream &morphology, const EnergyTables &tables, const LevelConfig &config)
    : morphology_(morphology), tables_(tables), config_(config)
  {
    signals_.SetTables(&tables_);
    signals_.SetEarlySignals(config_.analysis.signals.publication == SignalPublication::Early);
  }

  // 一个时刻的全部分型加入后，推进下游并产出事件，再推进各层定型边界
  void Step(int bar, bool emit, std::vector<SignalEvent> &events)
  {
    if (morphology_.strokeDirty < 0) return;
    int dirty = config_.level == CenterUnit::Segment ? morphology_.segmentDirty : morphology_.strokeDirty;
    bool changed = dirty >= 0;
    if (changed)
    {
      const auto &source = config_.level == CenterUnit::Segment ? morphology_.segments.Pivots() : morphology_.strokePivots;
      pivots_.resize(source.size());
      for (std::size_t i=static_cast<std::size_t>(dirty); i<source.size(); ++i) pivots_[i] = source[i];
    }
    if (changed)
    {
      int dc;
      if (config_.level == CenterUnit::Stroke && config_.analysis.center.strokeFormation == CenterFormation::Segment)
      {
        std::size_t stable = morphology_.segments.FinalCount(morphology_.strokes.FinalCount());
        int finalBar = stable >= 2 ? morphology_.segments.Pivots()[stable - 1].index : -1;
        dc = centers_.UpdateScoped(pivots_, CenterScopes(pivots_, morphology_.segments.Pivots()), finalBar);
      }
      else dc = centers_.Update(pivots_, static_cast<std::size_t>(dirty));
      int dm = UpdateMovements(centers_.Centers(), moves_, dc);
      signals_.Update(pivots_, centers_.Centers(), moves_, dirty, dc, dm, bar, emit, events);
    }
    AdvanceFinality(bar);
  }

  // 定型边界只增不减：笔端点 → (线段端点) → 中枢 → 走势 / 突破。
  // 已定型的输入此后不再改变，视界落在其内的检查点之前的输出也就不再改变。
  void AdvanceFinality(int bar)
  {
    std::size_t strokeFinal = morphology_.strokes.FinalCount();
    std::size_t pivotFinal = config_.level == CenterUnit::Segment ? morphology_.segments.FinalCount(strokeFinal) : strokeFinal;
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

  // 即时背驰预警：当前段 = 最后端点 P 到此刻的极值；与前一同向段（倒数第三→倒数第二端点）比较力度
  int8_t InstantWarning(int bar, const Series &s)
  {
    if (pivots_.size() < 3) return 0;
    const Pivot &last = pivots_.back();
    if (last.index != anchor_)
    {
      anchor_ = last.index;
      extremeAt_ = -1;
      scanned_ = last.index;
    }
    int dir = last.kind == Kind::Bottom ? 1 : -1;
    for (int i = scanned_ + 1; i <= bar; i++)
    {
      float v = dir > 0 ? s.high[static_cast<std::size_t>(i)] : s.low[static_cast<std::size_t>(i)];
      if (extremeAt_ < 0 || (dir > 0 ? v > extreme_ : v < extreme_))
      {
        extreme_ = v;
        extremeAt_ = i;
      }
    }
    scanned_ = bar;
    if (extremeAt_ < 0) return 0;
    Pivot now;
    now.kind = dir > 0 ? Kind::Top : Kind::Bottom;
    now.index = extremeAt_;
    now.high = now.low = extreme_;
    std::vector<Pivot> tmp(1, now);
    AssignEnergy(tmp, tables_);  // 取极值K线处的能量
    std::size_t b = static_cast<std::size_t>(bar);
    tmp[0].energy = tables_.cumulative[b];  // 面积累积到此刻
    tmp[0].energyRed = tables_.red[b];
    tmp[0].energyGreen = tables_.green[b];
    const Pivot &ps = pivots_[pivots_.size() - 3], &pe = pivots_[pivots_.size() - 2];
    return MeasureDivergence(ps, pe, last, tmp[0], dir).holds ? static_cast<int8_t>(dir) : 0;
  }

private:
  MorphologyStream &morphology_;
  CenterStream centers_;
  SignalStream signals_;
  static void Mark(std::vector<int> &at, std::size_t count, int bar)
  {
    if (at.size() < count) at.resize(count, bar);
  }

  std::size_t breakoutScan_ = 0;
  int anchor_ = -1, extremeAt_ = -1, scanned_ = -1;
  float extreme_ = 0;
  const EnergyTables &tables_;
  LevelConfig config_;
  std::vector<Pivot> pivots_;
  std::vector<Movement> moves_;
};

}  // namespace

Snapshot BuildSnapshot(const std::vector<Fractal> &fractals, std::size_t count, const EnergyTables &tables,
                       const LevelConfig &config, const Series *source)
{
  Snapshot s;
  std::vector<Fractal> prefix(fractals.begin(), fractals.begin() + static_cast<std::ptrdiff_t>(count));
  s.pivots = BuildPivots(prefix, config, source);
  AssignEnergy(s.pivots, tables);
  if (config.level == CenterUnit::Stroke && config.analysis.center.strokeFormation == CenterFormation::Segment)
  {
    auto segments = config.analysis.segment.method == SegmentMethod::Feature ? SegmentPivotsFeature(s.pivots) : SegmentPivotsHeuristic(s.pivots);
    s.centers = BuildCentersInSegments(s.pivots, segments);
  }
  else s.centers = BuildCenters(s.pivots);
  s.movements = BuildMovements(s.centers);
  s.breakouts = BuildBreakouts(s.pivots, s.centers);
  s.signals = BuildSignals(s.pivots, s.centers, s.movements, s.breakouts, &tables);
  return s;
}

namespace
{
std::shared_ptr<SharedAnalysisInputs> SharedInputs(const Series &series)
{
  auto inputs = std::make_shared<SharedAnalysisInputs>();
  inputs->bars = MergeBars(series);
  inputs->fractals = DetectFractals(inputs->bars);
  inputs->ma = BuildMovingAverages(series);
  inputs->energy = BuildEnergyTables(series);
  return inputs;
}

Snapshot SnapshotFromPivots(const std::vector<Pivot> &pivots, const std::vector<Pivot> &segments,
                            const EnergyTables &tables, const LevelConfig &config)
{
  Snapshot s;
  s.pivots = pivots;
  s.centers = config.level == CenterUnit::Stroke && config.analysis.center.strokeFormation == CenterFormation::Segment
    ? BuildCentersInSegments(pivots,segments) : BuildCenters(pivots);
  s.movements = BuildMovements(s.centers);
  s.breakouts = BuildBreakouts(s.pivots,s.centers);
  s.signals = BuildSignals(s.pivots,s.centers,s.movements,s.breakouts,&tables);
  return s;
}

FamilyAnalysis AnalyzeLevels(const Series &series, const LevelConfig &config, unsigned mask, int window)
{
  auto inputs = SharedInputs(series);
  const auto f = StrokeInputs(inputs->fractals,series,config);
  bool needSegments = (mask&2) != 0 || config.analysis.center.strokeFormation == CenterFormation::Segment;
  MorphologyStream morphology(f,config,series,*inputs,needSegments);
  FamilyAnalysis family;
  std::array<std::unique_ptr<Incremental>,2> streams;
  for (int level=0; level<2; ++level)
  {
    auto &view = family.levels[level];
    view.config = config; view.config.level = static_cast<CenterUnit>(level); view.inputs = inputs;
    if (!(mask&(1u<<level))) continue;
    streams[level] = std::make_unique<Incremental>(morphology,inputs->energy,view.config);
    view.instantWarning.assign(static_cast<std::size_t>(series.Size()),0);
  }
  int from = window>0 ? series.Size()-window : 0;
  std::size_t k = 0;
  for (int bar=0; bar<series.Size(); ++bar)
  {
    for (; k<f.size() && f[k].confirmedAt==bar; ++k) morphology.AddFractal(k);
    morphology.Step();
    for (int level=0; level<2; ++level)
      if (streams[level])
      {
        auto &view = family.levels[level];
        streams[level]->Step(bar,bar>=from,view.events);
        view.instantWarning[static_cast<std::size_t>(bar)] = streams[level]->InstantWarning(bar,series);
      }
    morphology.ClearDirty();
  }
  auto fit = [](std::vector<int> v, std::size_t n) { v.resize(n,-1); return v; };
  for (int level=0; level<2; ++level)
    if (streams[level])
    {
      auto &view = family.levels[level]; auto &stream = *streams[level];
      const auto &pivots = level==0 ? morphology.strokePivots : morphology.segments.Pivots();
      view.snapshot = SnapshotFromPivots(pivots,morphology.segments.Pivots(),inputs->energy,view.config);
      view.pivotFinalAt = fit(stream.pivotFinalAt,view.snapshot.pivots.size());
      view.centerFinalAt = fit(stream.centerFinalAt,view.snapshot.centers.size());
      view.movementFinalAt = fit(stream.movementFinalAt,view.snapshot.movements.size());
      view.breakoutFinalAt = fit(stream.breakoutFinalAt,view.snapshot.centers.size());
    }
  return family;
}
}  // namespace

FamilyAnalysis AnalyzeFamily(const Series &series, const AnalysisConfig &config, int window)
{
  auto error = Validate(config);
  if (!error.empty()) throw std::invalid_argument(error);
  LevelConfig view; view.analysis = Normalize(config);
  return AnalyzeLevels(series,view,3,window);
}

Analysis Analyze(const Series &series, const LevelConfig &config, int window)
{
  auto family = AnalyzeLevels(series,config,1u<<static_cast<unsigned>(config.level),window);
  return std::move(family.levels[static_cast<std::size_t>(config.level)]);
}

Analysis AnalyzeReference(const Series &series, const LevelConfig &config, int window)
{
  Analysis a;
  a.config = config;
  auto inputs = SharedInputs(series);
  a.inputs = inputs;
  const EnergyTables &tables = inputs->energy;

  int from = window > 0 ? series.Size() - window : 0;
  std::map<SignalKey, Signal> active;
  std::set<SignalKey> seen;
  const std::vector<Fractal> f = StrokeInputs(inputs->fractals, series, config);
  for (std::size_t k = 0; k < f.size(); k++)
  {
    int bar = f[k].confirmedAt;
    if (k + 1 < f.size() && f[k + 1].confirmedAt == bar) continue;
    bool baseline = bar < from;
    if (!(config.analysis.signals.publication == chan::SignalPublication::Early) && baseline && k + 1 < f.size() && f[k + 1].confirmedAt < from) continue;
    Snapshot snapshot = BuildSnapshot(f, k + 1, tables, config, &series);
    std::map<SignalKey, Signal> now = Confirmed(snapshot, (config.analysis.signals.publication == chan::SignalPublication::Early));
    if ((config.analysis.signals.publication == chan::SignalPublication::Early))
      for (auto it = now.begin(); it != now.end();)
      {
        if (!active.count(it->first) && (seen.count(it->first) ||
            it->second.pivot + 1 != static_cast<int>(snapshot.pivots.size()))) it = now.erase(it);
        else { seen.insert(it->first); ++it; }
      }
    if (!baseline)
    {
      for (const auto &kv : now)
        if (!active.count(kv.first)) a.events.push_back({bar, kv.second, false});
      for (const auto &kv : active)
        if (!now.count(kv.first)) a.events.push_back({bar, kv.second, true});
    }
    active.swap(now);
  }
  a.snapshot = BuildSnapshot(f, f.size(), tables, config, &series);
  return a;
}

}  // namespace chan
