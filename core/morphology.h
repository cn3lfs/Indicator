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

// 笔端点（顶底交替）：同型更极端者延伸、异型须跨度与价位达标，最后按窗口细化收笔点。
std::vector<Fractal> BuildStrokeEnds(const std::vector<Fractal> &fractals, const Config &config);

// 端点序列：笔端点，或按配置进一步划分的线段端点
std::vector<Pivot> StrokePivots(const std::vector<Fractal> &ends);
std::vector<Pivot> SegmentPivotsHeuristic(const std::vector<Pivot> &strokes);
std::vector<Pivot> SegmentPivotsFeature(const std::vector<Pivot> &strokes);
std::vector<Pivot> BuildPivots(const std::vector<Fractal> &fractals, const Config &config);

}  // namespace chan
