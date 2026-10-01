#include "config.h"

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <set>

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
  if (!Validate(config).empty()) return {};
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

std::string ApplyAnalysisField(AnalysisConfig &c, const std::string &key, const std::string &value)
{
  auto choice = [&](const char *const *names, int count) {
    for (int i=0; i<count; ++i) if (value==names[i]) return i;
    return -1;
  };
  const char *stroke[]={"strict","new","czsc","4k","fractal"};
  const char *endpoint[]={"extreme","first","bounded"};
  const char *gap[]={"none","asbar","large"};
  const char *method[]={"heuristic","feature"};
  const char *formation[]={"entry","segment"};
  const char *publication[]={"standard","early"};
  int v=-1;
  if (key=="stroke.rule") { v=choice(stroke,5); if(v>=0)c.stroke.rule=static_cast<StrokeRule>(v); }
  else if (key=="stroke.endpoint") { v=choice(endpoint,3); if(v>=0)c.stroke.endpoint=static_cast<StrokeEnd>(v); }
  else if (key=="stroke.gap") { v=choice(gap,3); if(v>=0)c.stroke.gap=static_cast<GapRule>(v); }
  else if (key=="segment.method") { v=choice(method,2); if(v>=0)c.segment.method=static_cast<SegmentMethod>(v); }
  else if (key=="center.strokeFormation") { v=choice(formation,2); if(v>=0)c.center.strokeFormation=static_cast<CenterFormation>(v); }
  else if (key=="signals.publication") { v=choice(publication,2); if(v>=0)c.signals.publication=static_cast<SignalPublication>(v); }
  else if (key=="stroke.gapThreshold")
  {
    std::istringstream in(value); in.imbue(std::locale::classic()); float f=0;
    if (!(in>>f) || !in.eof()) return "缺口阈值格式无效";
    c.stroke.gapThreshold=f; return {};
  }
  else return "未知分析字段："+key;
  return v>=0 ? std::string{} : "字段取值无效："+key;
}

std::string ParseAnalysisId(const std::string &id, AnalysisConfig &config)
{
  AnalysisConfig c; std::set<std::string> seen;
  std::istringstream in(id); std::string field;
  while (std::getline(in,field,';'))
  {
    auto at=field.find('=');
    if (at==std::string::npos || !seen.insert(field.substr(0,at)).second) return "配置身份字段格式错误或重复";
    auto error=ApplyAnalysisField(c,field.substr(0,at),field.substr(at+1));
    if (!error.empty()) return error;
  }
  auto error=Validate(c);
  if (error.empty()) config=Normalize(c);
  return error;
}

}  // namespace chan
