// 结构层不变量（第17/18/20课）与关键原文规则。
#include "check.h"
#include "oracle_data.h"
#include "core/dynamics.h"
#include "core/morphology.h"
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
  for (int code : kOracleConfigs)
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
