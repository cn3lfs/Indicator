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
