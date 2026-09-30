// 结构层不变量（第17/18/20课）与关键原文规则。
#include "check.h"
#include "sse_data.h"
#include "core/dynamics.h"
#include "core/morphology.h"
#include "core/signals.h"
#include "core/structure.h"

using namespace chan;

namespace
{
Pivot P(Kind k, int i, float price)
{
  Pivot p;
  p.kind = k;
  p.index = i;
  p.high = p.low = price;
  return p;
}
}  // namespace

TEST(CentersRespectBoundsAndDoNotShareEndpoints)
{
  Series s = Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW);
  std::vector<Fractal> f = DetectFractals(MergeBars(s));
  for (int code : kConfigs)
  {
    std::vector<Pivot> p = BuildPivots(f, *Config::Decode(code));
    std::vector<Center> c = BuildCenters(p);
    for (std::size_t i = 0; i < c.size(); i++)
    {
      CHECK(c[i].dd <= c[i].zd && c[i].zd <= c[i].zg && c[i].zg <= c[i].gg);
      CHECK(c[i].lastPivot - c[i].firstPivot >= 3);
      CHECK(p[static_cast<std::size_t>(c[i].firstPivot)].index == c[i].start);
      CHECK(p[static_cast<std::size_t>(c[i].lastPivot)].index == c[i].end);
      if (i > 0) CHECK(c[i].firstPivot > c[i - 1].lastPivot);
    }
    for (const Movement &m : BuildMovements(c))
      if (m.type != MovementType::Consolidation) CHECK(m.lastCenter > m.firstCenter);  // 趋势 ≥2 中枢
  }
}

// 第18/20课：离开段后回抽不回 [ZD,ZG] → 中枢破坏，离开段不属于前中枢；后DD>前GG → 上涨
TEST(LeavingSegmentBelongsToNextCenter)
{
  const float px[] = {1, 10, 4, 9, 5, 20, 15, 19, 16, 18, 17};
  std::vector<Pivot> p;
  for (int i = 0; i < 11; i++) p.push_back(P(i % 2 ? Kind::Top : Kind::Bottom, i * 4, px[i]));
  std::vector<Center> c = BuildCenters(p);
  REQUIRE(c.size() == 2);
  CHECK(c[0].start == 4 && c[0].end == 16 && c[0].zg == 9 && c[0].zd == 5 && c[0].gg == 10);
  CHECK(c[1].start == 20 && c[1].end == 40 && c[1].zg == 19 && c[1].zd == 16);
  CHECK(Relate(c[0], c[1]) == CenterRelation::Up);
}

// 第24课：向上段只看红柱、向下段只看绿柱
TEST(StrengthUsesSameColorBars)
{
  Pivot lo = P(Kind::Bottom, 0, 10), hi = P(Kind::Top, 10, 20);
  lo.energyRed = 10; lo.energyGreen = 5;
  hi.energy = 3; hi.energyRed = 22; hi.energyGreen = 14;
  CHECK(MeasureStrength(lo, hi).area == 12.0f);
  CHECK(MeasureStrength(hi, lo).area == 9.0f);
}

// 第37课 a+A+b+B+c：c 段内含 B 的三类卖点（离开 B 后回抽不回），再创新低且力度背驰 → 一买带 abc 上下文
TEST(FirstBuyWithAbcContext)
{
  //            A 中枢(ZG104/ZD92)        B 中枢(ZG79/ZD72)      c：离开65 回抽68(三卖) 新低60
  const float px[] = {110, 90, 105, 92, 104, 70, 80, 72, 79, 65, 68, 60, 62};
  const float en[] = {0, 0, 0, 0, 0, -100, -100, -100, -100, -120, -120, -150, -150};
  std::vector<Pivot> p;
  for (int i = 0; i < 13; i++)
  {
    Pivot x = P(i % 2 ? Kind::Bottom : Kind::Top, i * 4, px[i]);
    x.energy = en[i];
    p.push_back(x);
  }
  std::vector<Center> c = BuildCenters(p);
  REQUIRE(c.size() == 2);
  CHECK(Relate(c[0], c[1]) == CenterRelation::Down);
  std::vector<Movement> m = BuildMovements(c);
  std::vector<Breakout> b = BuildBreakouts(p, c);
  std::vector<Signal> s = BuildSignals(p, c, m, b);
  const Signal *buy1 = nullptr, *sell3 = nullptr;
  for (const Signal &x : s)
  {
    if (x.type == SignalType::Buy1) buy1 = &x;
    if (x.type == SignalType::Sell3) sell3 = &x;
  }
  REQUIRE(buy1 != nullptr && sell3 != nullptr);
  CHECK(buy1->pivot == 11 && buy1->divergence.holds && (buy1->context & kContextAbc));
  CHECK(buy1->quality == 1);  // 无 MACD 表：回零/标准背驰不置，故非强质
  CHECK(sell3->pivot == 10 && (sell3->context & kContextFirstRetest));
}

TEST(ParentSegmentCenterDirectionAndBounds)
{
  for (int mirror : {1, -1})
  {
    const float prices[] = {0,10,5,20,12,18,13,25,15,24};
    std::vector<Pivot> p;
    for (int i = 0; i < 10; ++i)
      p.push_back(P(((i % 2 == 0) == (mirror == 1)) ? Kind::Bottom : Kind::Top, i, prices[i] * mirror));
    std::vector<Pivot> parents{p[0], p[7]};
    auto old = BuildCenters(p), now = BuildCentersInSegments(p, parents);
    REQUIRE(!old.empty() && !now.empty());
    // 第一组反向三笔不重叠；旧规则成反方向枢，新规则跳到下一反向首笔。
    CHECK(old[0].firstPivot == 2 && old[0].direction == -mirror);
    CHECK(now[0].firstPivot == 3 && now[0].direction == mirror && now[0].lastPivot <= 7);
    CHECK(now[0].zd == (mirror == 1 ? 13 : -18) && now[0].zg == (mirror == 1 ? 18 : -13));
    CHECK(now[0].lastPivot == 7); // 延伸不可越过父线段终点
    auto crossed = BuildCentersInSegments(p, {p[0], p[5]});
    for (const auto &c : crossed) CHECK(c.firstPivot >= 6); // 三笔跨父边界不能成枢
    CHECK(BuildCentersInSegments(p, {}).empty()); // 无父起点，不猜方向
    CenterStream stream;
    auto scopes = CenterScopes(p, parents);
    stream.UpdateScoped(p, scopes);
    CHECK(stream.FinalCount(p.size()) == 0); // 父线段尚未定型
    stream.UpdateScoped(p, scopes, 7);
    CHECK(stream.FinalCount(p.size()) >= 1); // 父段两端稳定后才允许定型
  }
}
