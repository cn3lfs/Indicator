// 分析配置：原文多义处（第33课）的可选口径，全部显式化。默认值=严格笔+严格收笔+笔中枢+启发式线段。
#pragma once

#include <optional>

namespace chan
{

enum class StrokeRule : int
{
  Strict = 0,  // 合并K线跨度 ≥4（顶底之间至少一根独立K线，第62课）
  New = 1,     // 合并K线跨度 ≥3 且原始K线跨度 ≥4（新笔）
  FourK = 3,   // 社区/非原文：原始极值下标差≥3，两分型不共用合并K线
  Fractal = 4, // 社区/非原文：相邻顶底直接连接，仅须顶高于底
  Czsc = 2,    // 合并K线跨度 ≥3 且两端分型K线区间互不包含（借鉴 waditu/czsc check_bi）
};

enum class StrokeEnd : int
{
  Extreme = 0,  // 同型分型取更极端者延伸端点
  First = 1,    // 保留首个同型分型（允许次高/次低收笔）
};

enum class CenterUnit : int
{
  Stroke = 0,   // 笔中枢
  Segment = 1,  // 线段中枢
};

enum class SegmentMethod : int
{
  Heuristic = 0,  // 保护点启发式
  Feature = 1,    // 特征序列（第67/71课）
};

struct Config
{
  StrokeRule stroke = StrokeRule::Strict;
  StrokeEnd strokeEnd = StrokeEnd::Extreme;
  CenterUnit unit = CenterUnit::Stroke;
  SegmentMethod segment = SegmentMethod::Heuristic;

  // 十进制位编码：个位笔(0/1/2/3/4)、十位笔结束(0/1)、百位中枢构件(0/1)、千位线段法(0/1)
  int Encode() const;
  static std::optional<Config> Decode(int code);

  bool operator==(const Config &o) const
  {
    return stroke == o.stroke && strokeEnd == o.strokeEnd && unit == o.unit && segment == o.segment;
  }
};

}  // namespace chan
