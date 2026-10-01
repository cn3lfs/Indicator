#include "check.h"
#include "core/engine.h"
#include "core/morphology.h"
#include "adapter/czsc_api.h"
#include "sse_data.h"
#include <algorithm>
#include <cstring>
#include <vector>
#include <stdexcept>
namespace
{
chan::Series Quotes(std::initializer_list<float> h,std::initializer_list<float> l,bool mirror)
{
  chan::Series s;s.high=h;s.low=l;
  if(mirror)for(std::size_t i=0;i<s.high.size();++i){float old=s.high[i];s.high[i]=200-s.low[i];s.low[i]=200-old;}
  for(std::size_t i=0;i<s.high.size();++i){s.close.push_back((s.high[i]+s.low[i])*.5f);s.volume.push_back(1);}
  return s;
}
std::size_t Ends(const chan::Series &s,chan::GapRule gap,float threshold=.02f)
{
  chan::LevelConfig c;c.analysis.stroke.gap=gap;c.analysis.stroke.gapThreshold=threshold;
  auto f=chan::DetectFractals(chan::MergeBars(s));return chan::BuildStrokeEnds(f,c,&s).size();
}
bool EventsEqual(const std::vector<chan::SignalEvent> &a,const std::vector<chan::SignalEvent> &b)
{
  if(a.size()!=b.size())return false;
  for(std::size_t i=0;i<a.size();++i)
    if(a[i].bar!=b[i].bar || a[i].revoked!=b[i].revoked || a[i].signal.index!=b[i].signal.index ||
       a[i].signal.type!=b[i].signal.type || a[i].signal.stop!=b[i].signal.stop || a[i].signal.pivot!=b[i].signal.pivot ||
       a[i].signal.center!=b[i].signal.center || a[i].signal.quality!=b[i].signal.quality || a[i].signal.context!=b[i].signal.context)return false;
  return true;
}
}
TEST(StrokeGapsSymmetricCountsLargeAndExactThreshold)
{
  for(bool mirror:{false,true})
  {
    auto one=Quotes({101,100,102,104,105,104},{99.5f,99,99.5f,102.5f,103.5f,102.5f},mirror);
    REQUIRE(chan::DetectFractals(chan::MergeBars(one)).size()==2);
    CHECK(Ends(one,chan::GapRule::None)==1);CHECK(Ends(one,chan::GapRule::AsBar)==2);CHECK(Ends(one,chan::GapRule::Large)==2);
    auto big=Quotes({101,100,103,104,103},{99.5f,99,102.5f,103,102.5f},mirror);
    CHECK(Ends(big,chan::GapRule::None)==1);CHECK(Ends(big,chan::GapRule::AsBar)==1);CHECK(Ends(big,chan::GapRule::Large)==2);
    auto small=Quotes({101,100,102,103,102},{99.5f,99,101.5f,102,101.5f},mirror);
    CHECK(Ends(small,chan::GapRule::Large)==1);
    auto equal=Quotes({101,100,102.5f,103,102.5f},{99.5f,99,102,102.5f,102},mirror);
    CHECK(Ends(equal,chan::GapRule::Large)==1);CHECK(Ends(equal,chan::GapRule::Large,.019f)==2);
  }
  // 原始区间边界/反向：使用明确的端点分型值，只测缺口计数，避免将其他分型选择误作计数规则。
  auto s=Quotes({110,100,99,108,110},{109,95,94,102,105},false);
  std::vector<chan::Fractal> f(2);f[0]={chan::Kind::Bottom,1,1,100,95,2};f[1]={chan::Kind::Top,3,3,108,102,4};
  chan::LevelConfig c;c.analysis.stroke.gap=chan::GapRule::AsBar;
  // i=1是区间外前根到左端点的反向大缺口；区间内i=2反向、i=3同向。严格差2+1仍不足。
  CHECK(chan::BuildStrokeEnds(f,c,&s).size()==1);
  c.analysis.stroke.gap=chan::GapRule::Large;CHECK(chan::BuildStrokeEnds(f,c,&s).size()==2);
  s.high[2]=106;s.low[2]=100;s.low[3]=99; // 区间内仅反向缺口，不能给向上笔放宽跨度。
  CHECK(chan::BuildStrokeEnds(f,c,&s).size()==1);
}
TEST(StrokeGapsValidationIdentityAndMissingSource)
{
  czsc_config c{};czsc_config_default(&c);c.strokeRule=4;c.strokeGap=1;
  CHECK(czsc_config_validate(&c)!=0);c.strokeGap=2;CHECK(czsc_config_validate(&c)!=0);c.strokeGap=0;CHECK(czsc_config_validate(&c)==0);
  c.strokeRule=0;c.strokeGap=2;c.gapThreshold=.025f;CHECK(czsc_config_validate(&c)==0);
  char id[512];CHECK(czsc_config_id(&c,id,sizeof(id))>0);CHECK(std::strstr(id,"stroke.gapThreshold=")!=nullptr);
  czsc_config d{};CHECK(czsc_config_parse(id,&d)==0);CHECK(d.gapThreshold==c.gapThreshold);
  chan::LevelConfig cfg;cfg.analysis.stroke.gap=chan::GapRule::AsBar;std::vector<chan::Fractal> f;
  bool rejected=false;try{chan::StrokeStream stream(f,cfg);}catch(const std::invalid_argument &){rejected=true;}
  CHECK(rejected);
}
TEST(StrokeGapsSseBothLevelsReferenceCausalityAndFinality)
{
  auto source=chan::Series::FromRaw(SSE_DAILY_COUNT,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME);
  for(int gap:{1,2})for(int rule=0;rule<4;++rule)for(int endpoint=0;endpoint<3;++endpoint)
  {
    chan::AnalysisConfig cfg;cfg.stroke.gap=static_cast<chan::GapRule>(gap);cfg.stroke.rule=static_cast<chan::StrokeRule>(rule);
    cfg.stroke.endpoint=static_cast<chan::StrokeEnd>(endpoint);
    auto full=chan::AnalyzeFamily(source,cfg);
    for(int level=0;level<2;++level)
    {
      const auto &a=full.levels[level];const auto &p=a.snapshot.pivots;
      for(std::size_t i=1;i<p.size();++i)CHECK(p[i].kind!=p[i-1].kind && p[i].index>p[i-1].index);
      chan::LevelConfig view;view.analysis=cfg;view.level=static_cast<chan::CenterUnit>(level);
      if(endpoint==0)CHECK(EventsEqual(a.events,chan::AnalyzeReference(source,view).events));
    }
    for(int n:{170,176,240,500,1000,1700})
    {
      auto pre=source;pre.high.resize(n);pre.low.resize(n);pre.close.resize(n);pre.volume.resize(n);
      auto partial=chan::AnalyzeFamily(pre,cfg);
      for(int level=0;level<2;++level)
      {
        std::vector<chan::SignalEvent> expected;for(const auto &e:full.levels[level].events)if(e.bar<n)expected.push_back(e);
        CHECK(EventsEqual(partial.levels[level].events,expected));
        const auto &a=partial.levels[level],&b=full.levels[level];
        for(std::size_t i=0;i<a.snapshot.pivots.size();++i)if(a.pivotFinalAt[i]>=0)
          CHECK(i<b.snapshot.pivots.size() && a.snapshot.pivots[i].index==b.snapshot.pivots[i].index && a.snapshot.pivots[i].Price()==b.snapshot.pivots[i].Price());
      }
    }
  }
}

TEST(StrokeGapsCExportsAndOtherConstraints)
{
  czsc_input in{sizeof(in),SSE_DAILY_COUNT,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME};
  auto source=chan::Series::FromRaw(in.n,in.high,in.low,in.close,in.volume);
  for(int gap:{1,2})
  {
    czsc_config c{};czsc_config_default(&c);c.strokeGap=gap;
    void *h=czsc_build(&in,&c,CZSC_OUTPUT_DEFAULT);REQUIRE(h);
    chan::AnalysisConfig cfg;cfg.stroke.gap=static_cast<chan::GapRule>(gap);auto family=chan::AnalyzeFamily(source,cfg);
    for(int level=0;level<2;++level)
    {
      int n=0;const auto *p=czsc_level_pivots(h,level,&n);CHECK(n==static_cast<int>(family.levels[level].snapshot.pivots.size()));
      for(int i=0;i<n;++i)CHECK(p[i].extremeIndex==family.levels[level].snapshot.pivots[i].index);
      czsc_level_centers(h,level,&n);CHECK(n==static_cast<int>(family.levels[level].snapshot.centers.size()));
    }
    czsc_snapshot_free(h);
  }
  auto shortGap=Quotes({101,100,103,104,103},{99.5f,99,102.5f,103,102.5f},false);
  auto f=chan::DetectFractals(chan::MergeBars(shortGap));chan::LevelConfig c;c.analysis.stroke.gap=chan::GapRule::Large;
  c.analysis.stroke.endpoint=chan::StrokeEnd::Bounded;CHECK(chan::BuildStrokeEnds(f,c,&shortGap).size()==2);
  c.analysis.stroke.endpoint=chan::StrokeEnd::Extreme;c.analysis.stroke.rule=chan::StrokeRule::Czsc;
  f[1].low=98; // 顶分型包住底分型，即使大缺口也不绕过czsc互含约束。
  CHECK(chan::BuildStrokeEnds(f,c,&shortGap).size()==1);
}
