// 递归走势（第17课递归定义：某级别中枢 = 至少三个连续次级别走势类型的重叠）。
// 第 0 层是配置级别的走势；把相邻走势之间的连接点作为上一层的“端点”，再构中枢、分走势，逐层向上直到不能成枢。
// 多义处按第33课取一种满足结合律的确定分解（口径见 docs/chan-ambiguity-decisions.md）。
#pragma once

#include "core/engine.h"

#include <vector>

namespace chan
{

struct RecursiveLevel
{
  std::vector<Pivot> pivots;           // 本层端点（第 0 层为配置级别端点，上层为下层走势的连接点）
  std::vector<int> pivotFinalAt;       // 端点定型K线，-1 未定型
  std::vector<Center> centers;
  std::vector<int> centerFinalAt;
  std::vector<Movement> movements;
  std::vector<int> movementFinalAt;
  std::vector<int> boundaries;         // 走势 m 的起止为 pivots[boundaries[m]] → pivots[boundaries[m+1]]（端点下标）
};

// 相邻走势的分界端点下标（口径见 Boundaries）：走势 m 为 [b[m], b[m+1]]，最后一个暂止于最后端点
std::vector<int> MovementBoundaries(const std::vector<Pivot> &pivots, const std::vector<Center> &centers,
                                    const std::vector<Movement> &movements);

// 自 Analysis 的最终快照与定型时刻逐层递归；maxLevels 为总层数上限（含第 0 层）
std::vector<RecursiveLevel> BuildRecursion(const Analysis &analysis, int maxLevels = 16);

}  // namespace chan
