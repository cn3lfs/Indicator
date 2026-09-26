// 形态层不变量（第62/65/67/71课）：与实现细节无关的长期规格。
#include "check.h"
#include "sse_data.h"
#include "core/morphology.h"

#include <algorithm>

using namespace chan;

namespace
{
Series Sse()
{
  return Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW);
}

bool Alternates(const std::vector<Pivot> &p)
{
  for (std::size_t i = 1; i < p.size(); i++)
    if (p[i].kind == p[i - 1].kind || p[i].index <= p[i - 1].index) return false;
  return true;
}

bool PriceProgresses(const std::vector<Pivot> &p)
{
  for (std::size_t i = 1; i < p.size(); i++)
  {
    bool up = p[i - 1].kind == Kind::Bottom;
    if (up ? p[i].Price() <= p[i - 1].Price() : p[i].Price() >= p[i - 1].Price()) return false;
  }
  return true;
}
}  // namespace

TEST(MergedBarsHaveNoInclusion)
{
  std::vector<MergedBar> b = MergeBars(Sse());
  for (std::size_t i = 1; i < b.size(); i++)
  {
    bool inc = (b[i].high <= b[i - 1].high && b[i].low >= b[i - 1].low) ||
               (b[i].high >= b[i - 1].high && b[i].low <= b[i - 1].low);
    CHECK(!inc);
    CHECK(b[i].first == b[i - 1].last + 1);
  }
}

TEST(FractalsAlternateAndAreCausal)
{
  Series s = Sse();
  std::vector<Fractal> full = DetectFractals(MergeBars(s));
  for (std::size_t i = 1; i < full.size(); i++) CHECK(full[i].kind != full[i - 1].kind);
  // 任意前缀 [0,t] 的分型 = 全量分型中 confirmedAt<=t 的那一段
  for (int t = 40; t < s.Size(); t += 53)
  {
    Series p;
    p.high.assign(s.high.begin(), s.high.begin() + t + 1);
    p.low.assign(s.low.begin(), s.low.begin() + t + 1);
    std::vector<Fractal> pre = DetectFractals(MergeBars(p));
    std::size_t k = 0;
    while (k < full.size() && full[k].confirmedAt <= t) k++;
    REQUIRE(pre.size() == k);
    for (std::size_t i = 0; i < k; i++) CHECK(pre[i].index == full[i].index && pre[i].confirmedAt == full[i].confirmedAt);
  }
}

TEST(PivotsAlternateAndProgress)
{
  std::vector<Fractal> f = DetectFractals(MergeBars(Sse()));
  for (int code : kConfigs)
  {
    Config c = *Config::Decode(code);
    std::vector<Pivot> p = BuildPivots(f, c);
    CHECK(p.size() >= 2);
    CHECK(Alternates(p));
    CHECK(PriceProgresses(p));
    if (c.unit == CenterUnit::Segment)
    {
      // 线段端点是笔端点的子集（第67课），且级别更高
      Config sc = c;
      sc.unit = CenterUnit::Stroke;
      std::vector<Pivot> strokes = BuildPivots(f, sc);
      CHECK(p.size() < strokes.size());
      for (const Pivot &x : p)
        CHECK(std::any_of(strokes.begin(), strokes.end(), [&](const Pivot &y) { return y.index == x.index && y.kind == x.kind; }));
    }
  }
}

// 第62课：向下笔的底须低于起点顶的低点；czsc 笔另否决分型K线互相包含（ab_include）
TEST(StrokeRules)
{
  auto frac = [](Kind k, int i, float h, float l) { Fractal f; f.kind = k; f.index = i; f.merged = i; f.high = h; f.low = l; return f; };
  std::vector<Fractal> f = {frac(Kind::Top, 0, 100, 96), frac(Kind::Bottom, 4, 99, 97), frac(Kind::Top, 8, 105, 101),
                            frac(Kind::Bottom, 12, 95, 90)};
  std::vector<Fractal> e = BuildStrokeEnds(f, Config{});
  REQUIRE(e.size() == 2);
  CHECK(e[0].index == 8 && e[1].index == 12);

  std::vector<Fractal> g = {frac(Kind::Bottom, 0, 10, 8), frac(Kind::Top, 4, 20, 7), frac(Kind::Bottom, 8, 12, 6),
                            frac(Kind::Top, 12, 18, 14)};
  Config czsc;
  czsc.stroke = StrokeRule::Czsc;
  CHECK(BuildStrokeEnds(g, Config{}).size() == 4);
  std::vector<Fractal> ce = BuildStrokeEnds(g, czsc);
  REQUIRE(ce.size() == 2);
  CHECK(ce[0].index == 8 && ce[1].index == 12);
}
