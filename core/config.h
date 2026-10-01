// 分析配置：原文多义处（第33课）的可选口径，全部显式化。默认值=严格笔+严格收笔+笔中枢+启发式线段。
#pragma once

#include <optional>
#include <string>

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
  Bounded = 2,  // 社区合并K线包络与未确认末端延伸（v6修复口径）
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

// 万位：社区显示口径；第67/71课的极值划分判定不变。
enum class SegmentEnd : int
{
  Extreme = 0,
  First = 1,
  Last = 2,
};

// 十万位：按父线段筛选笔中枢是社区/非原文口径；线段级保持进入段。
enum class CenterFormation : int
{
  Entry = 0,
  Segment = 1,
};

// 结构化配置：分析身份不包含输出选择或显示投影，缺口由v7实现。
enum class GapRule : int { None = 0, AsBar = 1, Large = 2 };
enum class SignalPublication : int { Standard = 0, Early = 1 };
enum class CenterBox : int { Initial = 0, Extended = 1 };

struct StrokeConfig
{
  StrokeRule rule = StrokeRule::Strict;
  StrokeEnd endpoint = StrokeEnd::Extreme;
  GapRule gap = GapRule::None;
  float gapThreshold = 0.02f;
};
struct SegmentConfig { SegmentMethod method = SegmentMethod::Feature; };
struct CenterConfig { CenterFormation strokeFormation = CenterFormation::Entry; };
struct SignalConfig { SignalPublication publication = SignalPublication::Standard; };

struct AnalysisConfig
{
  StrokeConfig stroke;
  SegmentConfig segment;
  CenterConfig center;
  SignalConfig signals;
  bool operator==(const AnalysisConfig &o) const;
};

struct OutputSelection
{
  unsigned levels = 3;  // bit0笔级，bit1线段级；与分析身份独立。
  bool events = true;
  bool recursion = true;
  bool nested = true;
  bool operator==(const OutputSelection &o) const
  {
    return levels == o.levels && events == o.events && recursion == o.recursion && nested == o.nested;
  }
};

struct Projection
{
  SegmentEnd segmentBoundary = SegmentEnd::Extreme;
  CenterBox centerBox = CenterBox::Extended;  // 默认保留旧C表end，前三构件仅改变显示。
  bool operator==(const Projection &o) const
  {
    return segmentBoundary == o.segmentBoundary && centerBox == o.centerBox;
  }
};

std::string Validate(const AnalysisConfig &config);  // 空串合法，否则中文原因。
AnalysisConfig Normalize(const AnalysisConfig &config);
std::string AnalysisId(const AnalysisConfig &config);
std::string ApplyAnalysisField(AnalysisConfig &config, const std::string &key, const std::string &value);
std::string ParseAnalysisId(const std::string &id, AnalysisConfig &config);

// 单级引擎视图；level不是分析选项。旧整数桥只供迁移期间验证，v20移至测试工具。
struct LevelConfig
{
  AnalysisConfig analysis;
  OutputSelection outputs;
  Projection projection;
  CenterUnit level = CenterUnit::Stroke;
  bool operator==(const LevelConfig &o) const
  {
    return analysis == o.analysis && outputs == o.outputs && projection == o.projection && level == o.level;
  }
};

}  // namespace chan
