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

TEST(CommunityStrokeBoundaries)
{
  auto pair = [](int merged, int raw, float topHigh = 20, float topLow = 15) {
    Fractal a, b;
    a.kind = Kind::Bottom; a.index = 1; a.merged = 1; a.low = 8; a.high = 10;
    b.kind = Kind::Top; b.index = 1 + raw; b.merged = 1 + merged; b.high = topHigh; b.low = topLow;
    return std::vector<Fractal>{a, b};
  };
  Config four = *Config::Decode(3), fractal = *Config::Decode(4);
  // 原始极值含两端4根，合并分型不共用K线；没有独立合并K线仍允许。
  CHECK(BuildStrokeEnds(pair(3, 3), four).size() == 2);
  CHECK(BuildStrokeEnds(pair(3, 3), Config{}).size() == 1);
  CHECK(BuildStrokeEnds(pair(3, 3), *Config::Decode(1)).size() == 1);
  CHECK(BuildStrokeEnds(pair(2, 9), four).size() == 1);
  CHECK(BuildStrokeEnds(pair(3, 2), four).size() == 1);
  CHECK(BuildStrokeEnds(pair(4, 4), four).size() == 2);
  // 分型笔允许共用分型K线，无最小跨度；只按顶底极值判断。
  CHECK(BuildStrokeEnds(pair(2, 2, 9, 7), fractal).size() == 2);
  CHECK(BuildStrokeEnds(pair(2, 2, 8, 7), fractal).size() == 1);
  auto f = pair(2, 2);
  Fractal c = f[0]; c.index = 5; c.merged = 5; c.low = 6;
  f.push_back(c);
  auto ends = BuildStrokeEnds(f, fractal);
  REQUIRE(ends.size() == 3);
  CHECK(ends[0].index == 1 && ends[1].index == 3 && ends[2].index == 5);
}

// 三根包含特征笔；社区投影不改变第67/71课极值判定（方案a）。
TEST(SegmentMergedBoundaryExamples)
{
  auto pivots = [](std::vector<float> prices, bool mirror) {
    std::vector<Pivot> out;
    for (std::size_t i = 0; i < prices.size(); ++i)
    {
      Pivot p; p.index = static_cast<int>(i);
      p.kind = (i % 2 == 0) != mirror ? Kind::Bottom : Kind::Top;
      p.high = p.low = mirror ? -prices[i] : prices[i];
      out.push_back(p);
    }
    return out;
  };
  for (bool mirror : {false, true})
  {
    for (int test = 0; test < 3; ++test)
    {
      std::vector<float> prices = test == 0
        ? std::vector<float>{0,10,5,15,7,18,6,20,4,14,3,13,2}
        : std::vector<float>{0,10,5,15,7,20,4,18,8,14,3,13,2};
      if (test == 2) prices[7] = 20; // 等值极值沿用后一笔
      auto s = pivots(prices, mirror);
      auto ends = SegmentPivotsFeature(s);
      REQUIRE(ends.size() >= 2);
      const auto &p = ends[1];
      CHECK(p.index == (test == 1 ? 5 : 7));
      CHECK(p.firstFeatureIndex == 3 && p.lastFeatureIndex == 7);
      for (int digit = 0; digit < 3; ++digit)
      {
        auto config = *Config::Decode(1100 + digit * 10000);
        CHECK(DisplayPivotIndex(p, config) == (digit == 0 ? p.index : digit == 1 ? 3 : 7));
      }
      // 从非极值首笔开始重算会被组内更高/低点破坏；投影仍保留3，不偷偷回退到极值。
      SegmentStream stream(SegmentMethod::Feature);
      for (std::size_t n = 4; n <= s.size(); ++n)
      {
        std::vector<Pivot> prefix(s.begin(), s.begin() + n);
        stream.Update(prefix, n - 1);
        auto batch = SegmentPivotsFeature(prefix);
        REQUIRE(stream.Pivots().size() == batch.size());
        for (std::size_t i = 0; i < batch.size(); ++i)
          CHECK(stream.Pivots()[i].index == batch[i].index &&
                stream.Pivots()[i].firstFeatureIndex == batch[i].firstFeatureIndex &&
                stream.Pivots()[i].lastFeatureIndex == batch[i].lastFeatureIndex);
      }
    }
    // 无包含时三种输出位置相同。
    auto plain = SegmentPivotsFeature(pivots({0,10,5,15,7,12,4,11,3}, mirror));
    REQUIRE(plain.size() >= 2);
    CHECK(plain[1].index == plain[1].firstFeatureIndex && plain[1].index == plain[1].lastFeatureIndex);
    // 缺口确认从真实极值7开始，不从显示首笔3开始；新极值则撤销该候选。
    auto gap = pivots({0,10,5,15,12,18,11,20,10,14,3,13,6,14,5,13}, mirror);
    auto confirmed = SegmentPivotsFeature(gap);
    REQUIRE(confirmed.size() >= 2);
    CHECK(confirmed[1].index == 7 && confirmed[1].firstFeatureIndex == 3 && confirmed[1].lastFeatureIndex == 7);
    gap[9].high = gap[9].low = mirror ? -21 : 21;
    auto invalidated = SegmentPivotsFeature(gap);
    CHECK(invalidated.size() < 2 || invalidated[1].index != 7);
  }
}
