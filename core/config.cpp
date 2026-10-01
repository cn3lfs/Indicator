#include "config.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cctype>
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
  if (static_cast<int>(c.stroke.gap)<0 || static_cast<int>(c.stroke.gap)>2) return "缺口策略无效";
  if(c.stroke.rule==StrokeRule::Fractal && c.stroke.gap!=GapRule::None) return "分型笔无跨度门槛，缺口策略必须为不处理";
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
  std::string out="stroke.rule=";
  out+=stroke[static_cast<int>(c.stroke.rule)];
  out+=";stroke.endpoint=";out+=endpoint[static_cast<int>(c.stroke.endpoint)];
  out+=";stroke.gap=";out+=gap[static_cast<int>(c.stroke.gap)];
  if (c.stroke.gap==GapRule::Large)
  {
    // C stdio保留旧9位有效数字身份；把宿主locale小数分隔符规范为ASCII点。
    char buffer[128];std::snprintf(buffer,sizeof buffer,"%.9g",static_cast<double>(c.stroke.gapThreshold));
    std::string decimal=buffer;
    auto point=decimal.find_first_not_of("0123456789eE+-");
    if(point!=std::string::npos)
    {
      auto next=decimal.find_first_of("0123456789eE+-",point);
      decimal.replace(point,next==std::string::npos ? decimal.size()-point : next-point,".");
    }
    out+=";stroke.gapThreshold=";out+=decimal;
  }
  out+=";segment.method=";out+=c.segment.method==SegmentMethod::Feature ? "feature" : "heuristic";
  out+=";center.strokeFormation=";out+=c.center.strokeFormation==CenterFormation::Entry ? "entry" : "segment";
  out+=";signals.publication=";out+=c.signals.publication==SignalPublication::Standard ? "standard" : "early";
  return out;
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
    // ASCII十进制解析，接受科学计数，不受进程setlocale影响。
    std::size_t i=0;
    while (i<value.size() && std::isspace(static_cast<unsigned char>(value[i]))) ++i;
    bool negative=false;
    if (i<value.size() && (value[i]=='+' || value[i]=='-')) negative=value[i++]=='-';
    double number=0;int fractional=0,count=0;bool dot=false;
    while (i<value.size())
    {
      char ch=value[i];
      if (ch=='.' && !dot) { dot=true;++i;continue; }
      if (ch<'0' || ch>'9') break;
      number=number*10+(ch-'0');if(dot)++fractional;++count;++i;
    }
    int exponent=0;bool minus=false;
    if (i<value.size() && (value[i]=='e' || value[i]=='E'))
    {
      ++i;
      if (i<value.size() && (value[i]=='+' || value[i]=='-')) minus=value[i++]=='-';
      std::size_t begin=i;
      while (i<value.size() && value[i]>='0' && value[i]<='9')
      { exponent=std::min(10000,exponent*10+value[i++]-'0'); }
      if (i==begin) return "缺口阈值格式无效";
    }
    if (!count || i!=value.size()) return "缺口阈值格式无效";
    double result=number*std::pow(10.0,(minus?-exponent:exponent)-fractional);
    float f=static_cast<float>(negative?-result:result);
    if (!std::isfinite(f)) return "缺口阈值格式无效";
    c.stroke.gapThreshold=f;return {};
  }
  else return "未知分析字段："+key;
  return v>=0 ? std::string{} : "字段取值无效："+key;
}

std::string ParseAnalysisId(const std::string &id, AnalysisConfig &config)
{
  AnalysisConfig c; std::set<std::string> seen;
  std::size_t start=0;
  while (start<id.size())
  {
    auto end=id.find(';',start);
    std::string field=id.substr(start,end==std::string::npos ? end : end-start);
    start=end==std::string::npos ? id.size() : end+1;
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
