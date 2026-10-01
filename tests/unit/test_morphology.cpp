#include "migration/legacy_config.h"
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
    LevelConfig c = *migration::MapLegacyConfig(code);
    std::vector<Pivot> p = BuildPivots(f, c);
    CHECK(p.size() >= 2);
    CHECK(Alternates(p));
    CHECK(PriceProgresses(p));
    if (c.level == CenterUnit::Segment)
    {
      // 线段端点是笔端点的子集（第67课），且级别更高
      LevelConfig sc = c;
      sc.level = CenterUnit::Stroke;
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
  std::vector<Fractal> e = BuildStrokeEnds(f, LevelConfig{});
  REQUIRE(e.size() == 2);
  CHECK(e[0].index == 8 && e[1].index == 12);

  std::vector<Fractal> g = {frac(Kind::Bottom, 0, 10, 8), frac(Kind::Top, 4, 20, 7), frac(Kind::Bottom, 8, 12, 6),
                            frac(Kind::Top, 12, 18, 14)};
  LevelConfig czsc;
  czsc.analysis.stroke.rule = StrokeRule::Czsc;
  CHECK(BuildStrokeEnds(g, LevelConfig{}).size() == 4);
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
  LevelConfig four = *migration::MapLegacyConfig(3), fractal = *migration::MapLegacyConfig(4);
  // 原始极值含两端4根，合并分型不共用K线；没有独立合并K线仍允许。
  CHECK(BuildStrokeEnds(pair(3, 3), four).size() == 2);
  CHECK(BuildStrokeEnds(pair(3, 3), LevelConfig{}).size() == 1);
  CHECK(BuildStrokeEnds(pair(3, 3), *migration::MapLegacyConfig(1)).size() == 1);
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
        auto config = *migration::MapLegacyConfig(1100 + digit * 10000);
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

TEST(BoundedStrokesUseMergedEnvelopeAndAlwaysExtendTail)
{
  for (int mirror : {1, -1})
  {
    auto frac = [&](Kind kind, int i, float h, float l) {
      Fractal f; f.kind = mirror == 1 ? kind : Opposite(kind); f.index = f.merged = i;
      f.high = mirror == 1 ? h : -l; f.low = mirror == 1 ? l : -h; f.confirmedAt = i + 1; return f;
    };
    auto raw = [&](int n) { Series s; for (int i=0; i<n; ++i) { s.high.push_back(mirror == 1 ? 3+2*i : -(1+2*i)); s.low.push_back(mirror == 1 ? 1+2*i : -(3+2*i)); } return s; };
    auto set = [&](Series &s, int i, float h, float l) { s.high[i] = mirror == 1 ? h : -l; s.low[i] = mirror == 1 ? l : -h; };
    for (int stroke = 0; stroke <= 4; ++stroke)
    {
      auto bounded = *migration::MapLegacyConfig(1000000 + stroke);
      auto allowed = *migration::MapLegacyConfig(stroke);
      auto s = raw(10); set(s,0,3,1); set(s,8,20,18);
      std::vector<Fractal> f{frac(Kind::Bottom,0,3,1),frac(Kind::Top,8,20,18)};
      set(s,4,10,0); // 向上包含丢弃低影线，不能强造已确认分型。
      CHECK(BuildStrokeEnds(f,allowed,&s).size() == 2);
      CHECK(BuildStrokeEnds(f,bounded,&s).size() == 2);
      set(s,4,8,0); // 非包含合并K线低于起点，阻止反向成笔。
      CHECK(BuildStrokeEnds(f,bounded,&s).size() == 1);
      set(s,4,21,9); // 未形成可用顶分型的高影线。
      CHECK(BuildStrokeEnds(f,bounded,&s).size() == 1);
      set(s,4,20,1); // 等于两端极值允许。
      CHECK(BuildStrokeEnds(f,bounded,&s).size() == 2);
      set(s,8,20,0); // 端点原始影线被向上包含处理舍弃。
      CHECK(BuildStrokeEnds(f,bounded,&s).size() == 2);
    }
    auto s = raw(16);
    s.high.assign(16,mirror == 1 ? 10 : -4); s.low.assign(16,mirror == 1 ? 4 : -10);
    set(s,0,3,1); set(s,4,15,12); set(s,6,2,0); set(s,10,20,18); set(s,14,0,-1);
    std::vector<Fractal> f{frac(Kind::Bottom,0,3,1),frac(Kind::Top,4,15,12),frac(Kind::Bottom,6,2,0),
      frac(Kind::Top,10,20,18),frac(Kind::Bottom,14,0,-1)};
    // 第6根分型跨度不足被跳过；第10根同型延伸不能冻结，旧尾部包络被破则退回。
    auto e = BuildStrokeEnds(f,*migration::MapLegacyConfig(1000000),&s);
    REQUIRE(e.size() == 3);
    CHECK(e[0].index == 6 && e[1].index == 10 && e[2].index == 14);
  }
  CHECK(!migration::MapLegacyConfig(1000010).has_value());
  CHECK(!migration::MapLegacyConfig(1001114).has_value());
  CHECK(migration::LegacyCode(*migration::MapLegacyConfig(1001104)) == 1001104);
}

TEST(BoundedStrokesSseAlwaysStayInsideBothEndpoints)
{
  auto s = Series::FromRaw(SSE_DAILY_COUNT,SSE_DAILY_HIGH,SSE_DAILY_LOW);
  auto bars = MergeBars(s);
  auto f = DetectFractals(bars);
  for (int stroke = 0; stroke <= 4; ++stroke)
  {
    auto c = *migration::MapLegacyConfig(1000000 + stroke);
    auto ends = BuildStrokeEnds(StrokeInputs(f,s,c),c,&s);
    CHECK(ends.size() > 2);
    for (std::size_t k = 1; k < ends.size(); ++k)
    {
      CHECK(ends[k].kind != ends[k-1].kind && ends[k].index > ends[k-1].index);
      float top = ends[k].kind == Kind::Top ? ends[k].high : ends[k-1].high;
      float bottom = ends[k].kind == Kind::Bottom ? ends[k].low : ends[k-1].low;
      if (ends[k].extensionOnly) { CHECK(k+1 == ends.size()); continue; }
      for (const auto &bar : bars)
        if (bar.last >= ends[k-1].index && bar.first <= ends[k].index)
          CHECK(bar.high <= top && bar.low >= bottom);
    }
  }
}


// 独立按文档检查成笔规则，不调用实现中的ValidStroke；候选与已确认端点分开验收。
TEST(AllLegalConfigurationsCannotFreezeEligibleOppositeStroke)
{
  auto s = Sse();
  auto bars = MergeBars(s);
  auto f = DetectFractals(bars);
  int configurations = 0, eligible = 0, extended = 0;
  for (int million=0; million<=1; ++million)
    for (int scope=0; scope<=1; ++scope)
      for (int boundary=0; boundary<=2; ++boundary)
        for (int method=0; method<=1; ++method)
          for (int unit=0; unit<=1; ++unit)
            for (int end=0; end<=1; ++end)
              for (int stroke=0; stroke<=4; ++stroke)
              {
                auto c = migration::MapLegacyConfig(million*1000000+scope*100000+boundary*10000+method*1000+unit*100+end*10+stroke);
                if (!c) continue;
                ++configurations;
                auto input = StrokeInputs(f,s,*c);
                StrokeStream stream(input,*c,&s);
                std::vector<Fractal> confirmed;
                for (std::size_t k=0; k<input.size(); ++k)
                {
                  auto before = confirmed;
                  bool mustForm = false, mustExtend = false;
                  if (!before.empty() && !input[k].extensionOnly)
                  {
                    const auto &a = before.back(), &b = input[k];
                    int merged = b.merged-a.merged, raw = b.index-a.index;
                    bool span = stroke==4 || (stroke==0 ? merged>=4 : merged>=3 && (stroke==1 ? raw>=4 : stroke==3 ? raw>=3 : true));
                    bool progress = stroke==4 ? (a.kind==Kind::Bottom ? b.high>a.low : a.high>b.low) :
                      (a.kind==Kind::Bottom ? b.high>a.high : b.low<a.low);
                    bool nested = stroke==2 && ((a.high>b.high && a.low<b.low) || (a.high<b.high && a.low>b.low));
                    bool envelope = true;
                    float top = a.kind==Kind::Top ? a.high : b.high;
                    float bottom = a.kind==Kind::Bottom ? a.low : b.low;
                    if (million)
                      for (const auto &bar : bars)
                        if (bar.last>=a.index && bar.first<=b.index && (bar.high>top || bar.low<bottom)) envelope=false;
                    mustForm = a.kind!=b.kind && span && progress && !nested && envelope;
                    mustExtend = end==0 && a.kind==b.kind && (a.kind==Kind::Top ? b.high>=a.high : b.low<=a.low);
                  }
                  int dirty = stream.Add(k);
                  if (!input[k].extensionOnly && dirty>=0) confirmed = stream.Ends();
                  if (mustForm) { ++eligible; CHECK(stream.Ends().size()==before.size()+1); CHECK(stream.Ends().back().index==input[k].index); }
                  if (mustExtend) { ++extended; CHECK(stream.Ends().back().index==input[k].index); }
                }
              }
  CHECK(configurations==240);
  CHECK(eligible>10000 && extended>1000);
}

TEST(BoundedSsePrefixesShowPendingExtensionThenConfirmedFractal)
{
  for (int n : {160,166,170,175,200,240,SSE_DAILY_COUNT})
  {
    auto s = Series::FromRaw(n,SSE_DAILY_HIGH,SSE_DAILY_LOW);
    auto c = *migration::MapLegacyConfig(1000000);
    auto f = DetectFractals(MergeBars(s));
    auto e = BuildStrokeEnds(StrokeInputs(f,s,c),c,&s);
    REQUIRE(!e.empty());
    if (n>=170) CHECK(e.back().index>157);
    if (n==170) { CHECK(e.back().index==169); CHECK(e.back().extensionOnly); }
    if (n==175) { CHECK(e.back().index==174); CHECK(e.back().extensionOnly); }
    if (n==SSE_DAILY_COUNT)
    {
      auto base = BuildStrokeEnds(f,LevelConfig{},&s);
      CHECK(e.size()*2>=base.size());
      CHECK(e.back().index==base.back().index);
    }
  }
}


TEST(BoundedPendingExtensionIsSymmetricAndCannotFormStroke)
{
  for (int mirror : {1,-1})
  {
    auto s = Series::FromRaw(SSE_DAILY_COUNT,SSE_DAILY_HIGH,SSE_DAILY_LOW);
    if (mirror<0)
      for (int i=0; i<s.Size(); ++i) { float h=s.high[i]; s.high[i]=-s.low[i]; s.low[i]=-h; }
    auto c = *migration::MapLegacyConfig(1000000);
    auto input = StrokeInputs(DetectFractals(MergeBars(s)),s,c);
    StrokeStream stream(input,c,&s);
    int candidates = 0;
    for (std::size_t k=0; k<input.size(); ++k)
    {
      auto before=stream.Ends();
      int dirty=stream.Add(k);
      if (!input[k].extensionOnly) continue;
      CHECK(stream.Ends().size()==before.size());
      if (dirty>=0)
      {
        ++candidates;
        REQUIRE(!before.empty());
        CHECK(stream.Ends().back().kind==before.back().kind);
        CHECK(stream.Ends().back().extensionOnly);
        auto pivots = StrokePivots(stream.Ends());
        if (stream.Ends().size()<2) CHECK(pivots.empty());
        else CHECK(pivots.back().fractalAt==-1 && pivots.back().extensionOnly);
      }
    }
    CHECK(candidates>100);
  }
}
