// 形态学（第62/65/67/71课）：包含处理 → 分型 → 笔端点 → 线段端点。
// 包含处理与分型是流式的（BarMerger / DetectFractals 的结果对任意前缀都稳定），
// 笔与线段是对分型/端点前缀的纯函数，供全量计算与增量引擎共用。
#pragma once

#include "core/config.h"
#include "core/model.h"
#include "core/series.h"

#include <vector>

namespace chan
{

// 流式包含处理：逐根喂入原始K线；最后一根合并K线在下一根非包含K线到来前仍可能变化。
class BarMerger
{
public:
  // 返回 true 表示新增了一根合并K线（此前的最后一根由此定型）
  bool Push(int index, float high, float low);
  const std::vector<MergedBar> &Bars() const { return bars_; }

private:
  std::vector<MergedBar> bars_;
  int direction_ = 0;
};

std::vector<MergedBar> MergeBars(const Series &s);

// 分型：合并K线无包含 → 顶底严格交替，且分型在右侧首根非包含K线出现时即定型（confirmedAt）。
std::vector<Fractal> DetectFractals(const std::vector<MergedBar> &bars);

// 笔端点流：按顺序逐个加入分型（同型更极端者延伸、异型须跨度与价位达标），并即时细化倒数第二个端点。
// 细化只读取早于下一端点的分型，故 Add 到第 k 个分型时的 Ends() 恰等于对前 k+1 个分型的批量结果。
class StrokeStream
{
public:
  StrokeStream(const std::vector<Fractal> &fractals, const Config &config) : fractals_(&fractals), config_(config) {}
  // 加入 fractals[k]（须按顺序）；返回首个发生变化的端点下标，无变化返回 -1
  int Add(std::size_t k);
  const std::vector<Fractal> &Ends() const { return ends_; }

private:
  const std::vector<Fractal> *fractals_;
  Config config_;
  std::vector<Fractal> raw_;   // 未细化端点
  std::vector<Fractal> ends_;  // 细化后端点
};

// 笔端点（批量）：把全部分型依次加入 StrokeStream
std::vector<Fractal> BuildStrokeEnds(const std::vector<Fractal> &fractals, const Config &config);

// 读取视界：一个增量阶段读到的最远输入下标；判断若依赖“数据到头”则为无界。
// 续算时只有视界早于输入变化位置（dirty）的检查点可复用。
struct Horizon
{
  std::size_t max = 0;
  bool unbounded = false;
  void Read(std::size_t i) { if (i > max) max = i; }
  void Bound() { unbounded = true; }
  bool Before(std::size_t dirty) const { return !unbounded && max < dirty; }
};

// 线段流：输入为笔端点序列，Update(strokes, dirty) 表示 strokes 从 dirty 起可能变化；返回首个变化的输出下标（无变化 -1）
class SegmentStream
{
public:
  explicit SegmentStream(SegmentMethod method) : method_(method) {}
  int Update(const std::vector<Pivot> &strokes, std::size_t dirty);
  const std::vector<Pivot> &Pivots() const { return out_; }

private:
  struct Checkpoint
  {
    std::size_t start = 0, i = 0, candidate = 0, protect = 0, outSize = 0;
    bool has = false;
    Pivot outBack;
    Horizon horizon;
  };
  void Save();
  void RunFeature(const std::vector<Pivot> &s, bool fresh);
  void RunHeuristic(const std::vector<Pivot> &s, bool fresh);

  SegmentMethod method_;
  std::vector<Pivot> out_;
  std::vector<Checkpoint> checkpoints_;
  Horizon horizon_;
  std::size_t start_ = 0, i_ = 0, candidate_ = 0, protect_ = 0;
  bool has_ = false;
};

// 端点序列：笔端点，或按配置进一步划分的线段端点
std::vector<Pivot> StrokePivots(const std::vector<Fractal> &ends);
std::vector<Pivot> SegmentPivotsHeuristic(const std::vector<Pivot> &strokes);
std::vector<Pivot> SegmentPivotsFeature(const std::vector<Pivot> &strokes);
std::vector<Pivot> BuildPivots(const std::vector<Fractal> &fractals, const Config &config);

}  // namespace chan
