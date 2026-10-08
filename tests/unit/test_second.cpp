// 社区/非原文：相邻次高低仅补足跨度，不冻结首个端点。
#include "check.h"
#include "sse_data.h"
#include "core/morphology.h"
#include "core/engine.h"
#include "adapter/czsc_api.h"
#include "tdx/presets.h"
#include "tdx/exports.h"
#include <algorithm>
#include <cstring>

using namespace chan;
namespace
{
Fractal F(Kind kind, int merged, float price, bool mirror = false, int index = -1)
{
  Fractal f;
  f.kind = mirror ? (kind == Kind::Top ? Kind::Bottom : Kind::Top) : kind;
  f.merged = merged; f.index = index < 0 ? merged : index; f.confirmedAt = f.index+1;
  f.high = mirror ? -price+0.2f : price+0.2f;
  f.low = mirror ? -price-0.2f : price-0.2f;
  return f;
}
LevelConfig Second(StrokeRule rule = StrokeRule::Strict)
{
  LevelConfig c; c.analysis.stroke.endpoint = StrokeEnd::Second; c.analysis.stroke.rule = rule;
  return c;
}
std::vector<int> Indices(const std::vector<Fractal> &f)
{
  std::vector<int> out; for (const auto &x:f) out.push_back(x.index); return out;
}
}

TEST(SecondAdjacentCandidateAndNormalExtension)
{
  for (bool mirror:{false,true})
  {
    std::vector<Fractal> f{F(Kind::Bottom,0,8,mirror),F(Kind::Top,3,11,mirror),
      F(Kind::Bottom,4,9,mirror),F(Kind::Top,5,10,mirror),F(Kind::Top,7,10.5f,mirror),
      F(Kind::Top,8,10.5f,mirror),F(Kind::Bottom,12,7,mirror),F(Kind::Top,16,12,mirror)};
    StrokeStream s(f,Second());
    CHECK(s.Add(0)==0); CHECK(s.Add(1)==-1); CHECK(s.Add(2)==-1);
    CHECK(s.Ends().size()==1);
    CHECK(s.Add(3)==1); CHECK(s.Ends().back().index==5);
    CHECK(s.Add(4)==1); CHECK(s.Ends().back().index==7);
    CHECK(s.Add(5)==1); CHECK(s.Ends().back().index==8);
    s.Add(6); s.Add(7);
    CHECK(Indices(s.Ends())==std::vector<int>({0,8,12,16}));
    CHECK(s.FinalCount()==2);
    CHECK(Indices(BuildStrokeEnds(f,Second()))==Indices(s.Ends()));
  }
}

TEST(SecondOnlyOneCandidateAndNewExtremeRestarts)
{
  for (bool mirror:{false,true})
  {
    std::vector<Fractal> f{F(Kind::Bottom,0,8,mirror),F(Kind::Top,2,11,mirror),
      F(Kind::Top,3,10,mirror),F(Kind::Top,5,9,mirror),F(Kind::Top,7,12,mirror)};
    StrokeStream s(f,Second());
    for (int i=0;i<4;++i) s.Add(i);
    CHECK(s.Ends().size()==1);  // 第三个较弱分型即使跨度足够也不能凑笔。
    s.Add(4); CHECK(Indices(s.Ends())==std::vector<int>({0,7}));
    f={F(Kind::Bottom,0,8,mirror),F(Kind::Top,1,11,mirror),F(Kind::Top,2,10,mirror),
      F(Kind::Top,3,12,mirror),F(Kind::Top,5,11.5f,mirror)};
    CHECK(Indices(BuildStrokeEnds(f,Second()))==std::vector<int>({0,5}));
    // 相等极值取后，重新允许其后相邻候选。
    f[3]=F(Kind::Top,3,11,mirror); f[4]=F(Kind::Top,5,10.5f,mirror);
    CHECK(Indices(BuildStrokeEnds(f,Second()))==std::vector<int>({0,5}));
  }
}

TEST(SecondDoesNotBypassOtherRulesOrSearchBackward)
{
  for (bool mirror:{false,true})
  {
    auto c=Second();
    std::vector<Fractal> f{F(Kind::Bottom,0,10,mirror),F(Kind::Top,3,9,mirror),F(Kind::Top,5,8,mirror)};
    CHECK(BuildStrokeEnds(f,c).size()==1); // 极值失败不止跨度。
    f={F(Kind::Bottom,0,8,mirror),F(Kind::Top,3,11,mirror),F(Kind::Top,5,7,mirror),F(Kind::Top,7,10,mirror)};
    CHECK(BuildStrokeEnds(f,c).size()==1); // 相邻候选价位不合法，不能继续找。
    f={F(Kind::Bottom,0,8,mirror),F(Kind::Top,4,10,mirror),F(Kind::Top,6,11,mirror),F(Kind::Bottom,8,7,mirror)};
    CHECK(Indices(BuildStrokeEnds(f,c))==std::vector<int>({0,6})); // 不回选顶4以接底8。
    f={F(Kind::Bottom,0,8,mirror),F(Kind::Top,3,11,mirror,10),F(Kind::Top,3,10,mirror,12)};
    CHECK(BuildStrokeEnds(f,c).size()==1); // 原始K线足够，合并K线仍不足。
  }
  auto c=Second(StrokeRule::Czsc);
  auto a=F(Kind::Bottom,0,8), e=F(Kind::Top,2,11), b=F(Kind::Top,4,10);
  e.low=7; // 极值分型包含起点，不能以跨度补选掩盖互含失败。
  CHECK(BuildStrokeEnds({a,e,b},c).size()==1);
  e=F(Kind::Top,2,11); b.low=7;
  CHECK(BuildStrokeEnds({a,e,b},c).size()==1); // 候选自身互含。
}

TEST(SecondResetsWhenStartExtendsOrStrokeChanges)
{
  std::vector<Fractal> f{F(Kind::Bottom,0,8),F(Kind::Top,3,11),F(Kind::Bottom,4,7),
    F(Kind::Top,7,10),F(Kind::Top,9,9),F(Kind::Bottom,13,6),F(Kind::Bottom,15,5)};
  CHECK(Indices(BuildStrokeEnds(f,Second()))==std::vector<int>({4,9,15}));
  // czsc同型延伸也须维护已有笔的互含约束。
  auto bad=F(Kind::Top,6,12); bad.low=7;
  f={F(Kind::Bottom,0,8),F(Kind::Top,4,11),bad};
  CHECK(Indices(BuildStrokeEnds(f,Second(StrokeRule::Czsc)))==std::vector<int>({0,4}));
}

TEST(SecondUsesSelectedStrokeAndGapRules)
{
  for (auto rule:{StrokeRule::Strict,StrokeRule::New,StrokeRule::Czsc,StrokeRule::FourK,StrokeRule::Fractal})
  {
    auto f=std::vector<Fractal>{F(Kind::Bottom,0,8),F(Kind::Top,2,11),F(Kind::Top,4,10)};
    auto ends=BuildStrokeEnds(f,Second(rule));
    CHECK(Indices(ends)==std::vector<int>({0,rule==StrokeRule::Fractal?2:4}));
  }
  // 第62课严格笔跨度沿用既有计数；缺口奖励为独立社区策略，不由second新增。
  Series source; source.high={8.2f,9.2f,10.2f,11.2f,10.2f};
  source.low={7.8f,8.8f,9.8f,10.8f,9.8f};
  auto f=std::vector<Fractal>{F(Kind::Bottom,0,8),F(Kind::Top,3,11),F(Kind::Top,4,10)};
  for (auto gap:{GapRule::None,GapRule::AsBar,GapRule::Large})
  {
    auto c=Second(); c.analysis.stroke.gap=gap;
    CHECK(Indices(BuildStrokeEnds(f,c,&source))==std::vector<int>({0,gap==GapRule::None?4:3}));
  }
}

TEST(SecondRealFractalsAppearOnlyAtConfirmationAndFinalPrefixStaysStable)
{
  Series source;
  for (float p:std::initializer_list<float>{10,8,9,10,11,9,10,8.8f,9.5f,11,10,9,8,9,10,9,11,10,8,7,8})
  { source.high.push_back(p+.2f); source.low.push_back(p-.2f); }
  auto c=Second(); auto all=Analyze(source,c);
  for (int n=1;n<=source.Size();++n)
  {
    Series pre; pre.high.assign(source.high.begin(),source.high.begin()+n); pre.low.assign(source.low.begin(),source.low.begin()+n);
    auto a=Analyze(pre,c);
    if (n==7)
    {
      CHECK(a.snapshot.pivots.empty()); // 单个起点不构成笔，公开端点表为空。
      auto ends=BuildStrokeEnds(a.inputs->fractals,c);
      REQUIRE(ends.size()==1); CHECK(ends[0].index==1);
    }
    if (n==8)
    {
      REQUIRE(a.snapshot.pivots.size()==2);
      CHECK(a.snapshot.pivots[1].index==6 && a.snapshot.pivots[1].fractalAt==7);
    }
    for (std::size_t i=0;i<a.pivotFinalAt.size();++i)
      if (a.pivotFinalAt[i]>=0)
      {
        REQUIRE(i<all.snapshot.pivots.size());
        CHECK(a.snapshot.pivots[i].index==all.snapshot.pivots[i].index);
        CHECK(a.pivotFinalAt[i]==all.pivotFinalAt[i]);
      }
    for (const auto &event:a.events) CHECK(event.bar<n);
  }
}

TEST(SecondConfigApiAndTdxPreset)
{
  CHECK(CZSC_API_VERSION==21 && czsc_api_version()==21);
  czsc_config config{};
  REQUIRE(czsc_config_parse("stroke.endpoint=second",&config)==0);
  CHECK(config.strokeEndpoint==1);
  char id[512]; REQUIRE(czsc_config_id(&config,id,sizeof id)>0);
  CHECK(std::strstr(id,"stroke.endpoint=second;")!=nullptr);
  czsc_config parsed{}; REQUIRE(czsc_config_parse(id,&parsed)==0);
  CHECK(std::memcmp(&config,&parsed,sizeof config)==0);
  CHECK(czsc_config_parse("stroke.endpoint=first",&parsed)!=0);
  auto map=tdx::ParsePresets("[5]\nstroke.endpoint=second\n[6]\nstroke.endpoint=first\n");
  CHECK(map.at(5).error==0 && map.at(5).view.analysis.stroke.endpoint==StrokeEnd::Second);
  CHECK(map.at(6).error==4);
  int count=czsc_config_choices(nullptr,0); std::vector<czsc_config_choice> choices(count);
  czsc_config_choices(choices.data(),count); bool found=false;
  for (const auto &choice:choices)
  {
    CHECK(std::strcmp(choice.key,"stroke.endpoint.first")!=0);
    if (std::strcmp(choice.key,"stroke.endpoint.second")==0)
    { found=true; CHECK(choice.value==1 && choice.original==0); }
  }
  CHECK(found);
  czsc_input input{sizeof(input),SSE_DAILY_COUNT,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME};
  void *h=czsc_build(&input,&config,CZSC_OUTPUT_DEFAULT); REQUIRE(h);
  int rows=0; const auto *p=czsc_level_pivots(h,0,&rows);
  tdx::ResetForTesting(); tdx::PresetTextForTesting("[5]\nstroke.endpoint=second\n");
  std::vector<float> high(SSE_DAILY_HIGH,SSE_DAILY_HIGH+SSE_DAILY_COUNT),low(SSE_DAILY_LOW,SSE_DAILY_LOW+SSE_DAILY_COUNT);
  std::vector<float> close(SSE_DAILY_CLOSE,SSE_DAILY_CLOSE+SSE_DAILY_COUNT),volume(SSE_DAILY_VOLUME,SSE_DAILY_VOLUME+SSE_DAILY_COUNT),out(SSE_DAILY_COUNT);
  tdx::RegisterCloseVolume(SSE_DAILY_COUNT,out.data(),close.data(),volume.data(),nullptr);
  float preset=5; tdx::Pivots(SSE_DAILY_COUNT,out.data(),high.data(),low.data(),&preset);
  CHECK(std::count_if(out.begin(),out.end(),[](float v){return v!=0;})==rows);
  for (int i=0;i<rows;++i) CHECK(out[p[i].index]==p[i].kind);
  czsc_snapshot_free(h); tdx::ResetForTesting();
}
