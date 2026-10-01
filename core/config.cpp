#include "config.h"

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace chan
{

AnalysisConfig Normalize(const AnalysisConfig &config)
{
  AnalysisConfig c = config;
  if (c.stroke.gap != GapRule::Large) c.stroke.gapThreshold = 0.02f;
  return c;
}

bool AnalysisConfig::operator==(const AnalysisConfig &o) const
{
  auto a = Normalize(*this), b = Normalize(o);
  return a.stroke.rule == b.stroke.rule && a.stroke.endpoint == b.stroke.endpoint &&
    a.stroke.gap == b.stroke.gap && a.stroke.gapThreshold == b.stroke.gapThreshold &&
    a.segment.method == b.segment.method && a.center.strokeFormation == b.center.strokeFormation &&
    a.signals.publication == b.signals.publication;
}

std::string Validate(const AnalysisConfig &c)
{
  if (static_cast<int>(c.stroke.rule) < 0 || static_cast<int>(c.stroke.rule) > 4) return "笔算法无效";
  if (static_cast<int>(c.stroke.endpoint) < 0 || static_cast<int>(c.stroke.endpoint) > 2) return "笔端点策略无效";
  if (c.stroke.gap != GapRule::None) return "缺口策略尚未实现，请使用不处理";
  if (!std::isfinite(c.stroke.gapThreshold) || c.stroke.gapThreshold <= 0 || c.stroke.gapThreshold >= 1)
    return "缺口阈值须为0到1之间的有限小数";
  if (c.segment.method != SegmentMethod::Heuristic && c.segment.method != SegmentMethod::Feature) return "线段算法无效";
  if (c.center.strokeFormation != CenterFormation::Entry && c.center.strokeFormation != CenterFormation::Segment) return "笔中枢构成无效";
  if (c.signals.publication != SignalPublication::Standard && c.signals.publication != SignalPublication::Early) return "信号发布策略无效";
  return {};
}

std::string AnalysisId(const AnalysisConfig &config)
{
  auto c = Normalize(config);
  const char *stroke[] = {"strict","new","czsc","4k","fractal"};
  const char *endpoint[] = {"extreme","first","bounded"};
  const char *gap[] = {"none","asbar","large"};
  if (!Validate(c).empty()) return {};
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << "stroke.rule=" << stroke[static_cast<int>(c.stroke.rule)]
      << ";stroke.endpoint=" << endpoint[static_cast<int>(c.stroke.endpoint)]
      << ";stroke.gap=" << gap[static_cast<int>(c.stroke.gap)];
  if (c.stroke.gap == GapRule::Large) out << ";stroke.gapThreshold=" << std::setprecision(9) << c.stroke.gapThreshold;
  out << ";segment.method=" << (c.segment.method == SegmentMethod::Feature ? "feature" : "heuristic")
      << ";center.strokeFormation=" << (c.center.strokeFormation == CenterFormation::Entry ? "entry" : "segment")
      << ";signals.publication=" << (c.signals.publication == SignalPublication::Standard ? "standard" : "early");
  return out.str();
}

int Config::Encode() const
{
  int endpoint = analysis.stroke.endpoint == StrokeEnd::First ? 1 : 0;
  return static_cast<int>(analysis.stroke.rule) + endpoint*10 + static_cast<int>(level)*100 +
    static_cast<int>(analysis.segment.method)*1000 + static_cast<int>(projection.segmentBoundary)*10000 +
    static_cast<int>(analysis.center.strokeFormation)*100000 + (analysis.stroke.endpoint == StrokeEnd::Bounded ? 1000000 : 0);
}

std::optional<Config> Config::Decode(int code)
{
  if (code < 0 || code > 1129999) return std::nullopt;
  int d0=code%10, d1=(code/10)%10, d2=(code/100)%10, d3=(code/1000)%10;
  int d4=(code/10000)%10, d5=(code/100000)%10, d6=code/1000000;
  if (d6>1 || (d6==1 && d1==1) || d5>1 || d4>2 || (d3==0 && d4!=0) || d0>4 || d1>1 || d2>1 || d3>1)
    return std::nullopt;
  Config c;
  c.analysis.stroke.rule = static_cast<StrokeRule>(d0);
  c.analysis.stroke.endpoint = d6 ? StrokeEnd::Bounded : static_cast<StrokeEnd>(d1);
  c.level = static_cast<CenterUnit>(d2);
  c.analysis.segment.method = static_cast<SegmentMethod>(d3);
  c.projection.segmentBoundary = static_cast<SegmentEnd>(d4);
  c.analysis.center.strokeFormation = static_cast<CenterFormation>(d5);
  return c;
}

}  // namespace chan
