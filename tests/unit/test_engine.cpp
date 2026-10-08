#include "migration/legacy_config.h"
// 增量引擎 = 参照实现（每步整条重算后差分），逐事件一致；这是增量逻辑的长期规格。
#include "check.h"
#include "sse_data.h"
#include "core/engine.h"

#include <algorithm>
#include <cstdio>

namespace
{
bool Same(const std::vector<chan::SignalEvent> &a, const std::vector<chan::SignalEvent> &b)
{
  if (a.size() != b.size())
  {
    std::printf("    %zu vs %zu events\n", a.size(), b.size());
    return false;
  }
  for (std::size_t i = 0; i < a.size(); i++)
  {
    const chan::Signal &x = a[i].signal, &y = b[i].signal;
    if (a[i].bar != b[i].bar || a[i].revoked != b[i].revoked || x.index != y.index || x.type != y.type ||
        x.stop != y.stop || x.priority != y.priority || x.pivot != y.pivot || x.center != y.center ||
        x.quality != y.quality || x.context != y.context || x.secondBaseIndex != y.secondBaseIndex ||
        x.secondTurnIndex != y.secondTurnIndex || x.smallTurnBaseIndex != y.smallTurnBaseIndex ||
        x.smallTurnRetestIndex != y.smallTurnRetestIndex || x.centerStart != y.centerStart)
    {
      std::printf("    event %zu differs: bar %d/%d idx %d/%d type %d/%d rev %d/%d\n", i, a[i].bar, b[i].bar, x.index,
                  y.index, (int)x.type, (int)y.type, (int)a[i].revoked, (int)b[i].revoked);
      return false;
    }
  }
  return true;
}
}  // namespace

TEST(IncrementalEngineMatchesReference)
{
  std::vector<SseSample> samples = SseSamples();
  samples[1].high.resize(9000);
  samples[1].low.resize(9000);
  int total = 0;
  for (SseSample &s : samples)
  {
    int n = static_cast<int>(s.high.size());
    chan::Series series = chan::Series::FromRaw(n, &s.high[0], &s.low[0]);
    for (auto c : AnalysisConfigs({0,1,2,101,1100,1101,3,4,1103,1104,11100,11101,11102,11103,11104,21100,21101,21102,21103,21104}, {0,1,2,3,4,1100}))
    {
      chan::Analysis inc = chan::Analyze(series, c);
      chan::Analysis ref = chan::AnalyzeReference(series, c);
      bool same = Same(inc.events, ref.events);
      if (!same) std::printf("  %s cfg %s level%d\n", s.name, chan::AnalysisId(c.analysis).c_str(), static_cast<int>(c.level));
      CHECK(same);
      total += static_cast<int>(inc.events.size());
    }
    CHECK(Same(chan::Analyze(series, chan::LevelConfig{}, 700).events, chan::AnalyzeReference(series, chan::LevelConfig{}, 700).events));
  }
  CHECK(total > 100);
}

// 无未来函数的定义：只拿 [0,t] 数据分析得到的事件 = 全量分析中发生在 t 及以前的事件
TEST(EngineIsCausalOnEveryPrefix)
{
  chan::Series full = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW);
  for (auto c : AnalysisConfigs({0,2,1100,3,4,1103,1104,11100,11101,11102,11103,11104,21100,21101,21102,21103,21104,1000000,1000001,1000002,1000003,1000004,1001100,1001101,1001102,1001103,1001104,1100000,1101000,1101100}, {0,1,2,3,4,1100}))
  {
    std::vector<chan::SignalEvent> all = chan::Analyze(full, c).events;
    for (int t = 120; t < full.Size(); t += 37)
    {
      chan::Series pre;
      pre.high.assign(full.high.begin(), full.high.begin() + t + 1);
      pre.low.assign(full.low.begin(), full.low.begin() + t + 1);
      std::vector<chan::SignalEvent> expected;
      for (const chan::SignalEvent &e : all)
        if (e.bar <= t) expected.push_back(e);
      CHECK(Same(chan::Analyze(pre, c).events, expected));
    }
  }
}

// 定型口径：任意前缀中 finalAt<=k 的对象与全量中同下标对象逐字段相同，且定型时刻相同
TEST(FinalizedObjectsNeverChange)
{
  chan::Series full = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW);
  int checked = 0;
  for (auto c : AnalysisConfigs({0,1,2,101,1100,1101,3,4,1103,1104,11100,11101,11102,11103,11104,21100,21101,21102,21103,21104,1000000,1000001,1000002,1000003,1000004,1001100,1001101,1001102,1001103,1001104,1100000,1101000,1101100}, {0,1,2,3,4,1100}))
  {
    chan::Analysis all = chan::Analyze(full, c);
    for (int k = 60; k < full.Size(); k += 29)
    {
      chan::Series pre;
      pre.high.assign(full.high.begin(), full.high.begin() + k + 1);
      pre.low.assign(full.low.begin(), full.low.begin() + k + 1);
      chan::Analysis a = chan::Analyze(pre, c);
      for (std::size_t i = 0; i < a.pivotFinalAt.size(); i++)
      {
        if (a.pivotFinalAt[i] < 0) continue;
        REQUIRE(a.pivotFinalAt[i] <= k && i < all.snapshot.pivots.size());
        const chan::Pivot &x = a.snapshot.pivots[i], &y = all.snapshot.pivots[i];
        CHECK(x.index == y.index && x.kind == y.kind && x.high == y.high && x.low == y.low && x.fractalAt == y.fractalAt &&
              a.pivotFinalAt[i] == all.pivotFinalAt[i]);
        checked++;
      }
      for (std::size_t i = 0; i < a.centerFinalAt.size(); i++)
      {
        if (a.centerFinalAt[i] < 0) continue;
        REQUIRE(i < all.snapshot.centers.size());
        const chan::Center &x = a.snapshot.centers[i], &y = all.snapshot.centers[i];
        CHECK(x.start == y.start && x.end == y.end && x.zg == y.zg && x.zd == y.zd && x.gg == y.gg && x.dd == y.dd &&
              x.firstPivot == y.firstPivot && x.lastPivot == y.lastPivot && a.centerFinalAt[i] == all.centerFinalAt[i]);
      }
      for (std::size_t i = 0; i < a.movementFinalAt.size(); i++)
      {
        if (a.movementFinalAt[i] < 0) continue;
        REQUIRE(i < all.snapshot.movements.size());
        const chan::Movement &x = a.snapshot.movements[i], &y = all.snapshot.movements[i];
        CHECK(x.type == y.type && x.firstCenter == y.firstCenter && x.lastCenter == y.lastCenter && x.end == y.end &&
              a.movementFinalAt[i] == all.movementFinalAt[i]);
      }
      for (const chan::Breakout &b : a.snapshot.breakouts)
      {
        if (a.breakoutFinalAt[static_cast<std::size_t>(b.center)] < 0) continue;
        bool found = false;
        for (const chan::Breakout &o : all.snapshot.breakouts)
          if (o.center == b.center)
            found = o.leavePivot == b.leavePivot && o.retestPivot == b.retestPivot && o.third == b.third &&
                    o.divergence.holds == b.divergence.holds &&
                    all.breakoutFinalAt[static_cast<std::size_t>(o.center)] == a.breakoutFinalAt[static_cast<std::size_t>(b.center)];
        CHECK(found);
      }
    }
    // 全量中除末尾外应基本都已定型
    CHECK(all.pivotFinalAt.size() < 3 || all.pivotFinalAt[all.pivotFinalAt.size() - 3] >= 0 || c.analysis.segment.method == chan::SegmentMethod::Feature || c.analysis.stroke.endpoint == chan::StrokeEnd::Bounded);
  }
  CHECK(checked > 1000);
}

// 即时背驰预警逐根因果：前缀分析的预警 = 全量预警的对应前缀
TEST(InstantWarningIsCausal)
{
  chan::Series full = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW);
  for (auto c : AnalysisConfigs({0,1100}))
  {
    std::vector<int8_t> all = chan::Analyze(full, c).instantWarning;
    int up = 0, down = 0;
    for (int8_t w : all) { up += w > 0; down += w < 0; }
    CHECK(c.level != chan::CenterUnit::Stroke || (up > 0 && down > 0));
    for (int k = 100; k < full.Size(); k += 73)
    {
      chan::Series pre;
      pre.high.assign(full.high.begin(), full.high.begin() + k + 1);
      pre.low.assign(full.low.begin(), full.low.begin() + k + 1);
      std::vector<int8_t> p = chan::Analyze(pre, c).instantWarning;
      CHECK(std::equal(p.begin(), p.end(), all.begin()));
    }
  }
}

TEST(ParentCentersIncrementalReferenceAndCausality)
{
  auto full = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW, SSE_DAILY_CLOSE, SSE_DAILY_VOLUME);
  for (int method : {0, 1000})
  {
    for (int stroke = 0; stroke <= 4; ++stroke)
    {
      int code = 100000 + method + stroke;
      auto config = *migration::MapLegacyConfig(code);
      auto all = chan::Analyze(full, config);
      CHECK(Same(all.events, chan::AnalyzeReference(full, config).events));
      for (int n = 100; n < full.Size(); n += 89)
      {
        auto pre = chan::Series::FromRaw(n, SSE_DAILY_HIGH, SSE_DAILY_LOW, SSE_DAILY_CLOSE, SSE_DAILY_VOLUME);
        std::vector<chan::SignalEvent> expected;
        for (const auto &e : all.events) if (e.bar < n) expected.push_back(e);
        CHECK(Same(chan::Analyze(pre, config).events, expected));
      }
    }
  }
}

TEST(EarlyEngineMatchesReferenceAndEveryPrefix)
{
  auto full = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW, SSE_DAILY_CLOSE, SSE_DAILY_VOLUME);
  for (auto c : AnalysisConfigs({0,2,1100,100000,101000,1000000,1000004,1001100}))
  {
    c.analysis.signals.publication = chan::SignalPublication::Early;
    auto all = chan::Analyze(full, c);
    CHECK(Same(all.events, chan::AnalyzeReference(full, c).events));
    CHECK(Same(chan::Analyze(full, c, 700).events, chan::AnalyzeReference(full, c, 700).events));
    std::set<std::pair<int, int>> appeared;
    for (const auto &e : all.events)
      if (!e.revoked)
      {
        CHECK(appeared.insert({e.signal.index, static_cast<int>(e.signal.type)}).second);
        bool buy = static_cast<int>(e.signal.type) <= 3;
        CHECK(e.signal.stop == (buy ? full.low[e.signal.index] : full.high[e.signal.index]));
      }
    for (int t = 120; t < full.Size(); t += 37)
    {
      auto pre = chan::Series::FromRaw(t + 1, SSE_DAILY_HIGH, SSE_DAILY_LOW, SSE_DAILY_CLOSE, SSE_DAILY_VOLUME);
      std::vector<chan::SignalEvent> expected;
      for (const auto &e : all.events) if (e.bar <= t) expected.push_back(e);
      CHECK(Same(chan::Analyze(pre, c).events, expected));
    }
  }
}

TEST(BoundedEngineMatchesReferenceOnSse)
{
  auto full = chan::Series::FromRaw(SSE_DAILY_COUNT,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME);
  for (int unit : {0, 100, 1100})
    for (int stroke = 0; stroke <= 4; ++stroke)
    {
      auto c = *migration::MapLegacyConfig(1000000 + unit + stroke);
      CHECK(Same(chan::Analyze(full,c).events,chan::AnalyzeReference(full,c).events));
    }
}

TEST(SecondEngineRulesGapsPublicationAndBothLevels)
{
  auto source=chan::Series::FromRaw(600,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME);
  for (int rule=0;rule<=4;++rule)
    for (int gap=0;gap<=2;++gap)
    {
      if (rule==4 && gap!=0) continue;
      for (auto publication:{chan::SignalPublication::Standard,chan::SignalPublication::Early})
      {
        chan::LevelConfig c;
        c.analysis.stroke.endpoint=chan::StrokeEnd::Second;
        c.analysis.stroke.rule=static_cast<chan::StrokeRule>(rule);
        c.analysis.stroke.gap=static_cast<chan::GapRule>(gap);
        c.analysis.signals.publication=publication;
        auto family=chan::AnalyzeFamily(source,c.analysis);
        for (int level=0;level<2;++level)
        {
          c.level=static_cast<chan::CenterUnit>(level);
          const auto &actual=family.levels[level];
          auto reference=chan::AnalyzeReference(source,c);
          CHECK(Same(actual.events,reference.events));
          REQUIRE(actual.snapshot.pivots.size()==reference.snapshot.pivots.size());
          REQUIRE(actual.snapshot.centers.size()==reference.snapshot.centers.size());
          CHECK(actual.snapshot.movements.size()==reference.snapshot.movements.size());
          CHECK(actual.snapshot.signals.size()==reference.snapshot.signals.size());
          for (std::size_t i=0;i<actual.snapshot.pivots.size();++i)
          {
            const auto &a=actual.snapshot.pivots[i],&b=reference.snapshot.pivots[i];
            CHECK(a.index==b.index && a.kind==b.kind && a.high==b.high && a.low==b.low && a.fractalAt==b.fractalAt);
          }
          for (std::size_t i=0;i<actual.snapshot.centers.size();++i)
          {
            const auto &a=actual.snapshot.centers[i],&b=reference.snapshot.centers[i];
            CHECK(a.start==b.start && a.end==b.end && a.firstPivot==b.firstPivot && a.lastPivot==b.lastPivot);
            CHECK(a.zd==b.zd && a.zg==b.zg && a.dd==b.dd && a.gg==b.gg && a.direction==b.direction);
          }
        }
      }
    }
  // 不抽样：逐根检查真实样本上的两级事件与已定型端点，覆盖补选后的因果链。
  chan::AnalysisConfig config; config.stroke.endpoint=chan::StrokeEnd::Second;
  config.signals.publication=chan::SignalPublication::Early;
  auto full=chan::AnalyzeFamily(source,config);
  for (int n=1;n<=source.Size();++n)
  {
    auto prefix=chan::Series::FromRaw(n,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME);
    auto partial=chan::AnalyzeFamily(prefix,config);
    for (int level=0;level<2;++level)
    {
      const auto &a=partial.levels[level],&b=full.levels[level];
      std::vector<chan::SignalEvent> expected;
      for (const auto &event:b.events) if (event.bar<n) expected.push_back(event);
      CHECK(Same(a.events,expected));
      for (std::size_t i=0;i<a.pivotFinalAt.size();++i)
        if (a.pivotFinalAt[i]>=0)
        {
          REQUIRE(i<b.snapshot.pivots.size());
          CHECK(a.snapshot.pivots[i].index==b.snapshot.pivots[i].index);
          CHECK(a.pivotFinalAt[i]==b.pivotFinalAt[i]);
        }
    }
  }
  auto real=chan::Series::FromRaw(SSE_DAILY_COUNT,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME);
  int total=0;
  for (auto publication:{chan::SignalPublication::Standard,chan::SignalPublication::Early})
  {
    config.signals.publication=publication;
    auto family=chan::AnalyzeFamily(real,config);
    for (int level=0;level<2;++level)
    {
      const auto &a=family.levels[level];
      int revoked=0; for (const auto &e:a.events) revoked+=e.revoked;
      std::printf("  second SSE publication=%d level=%d pivots=%zu centers=%zu movements=%zu signals=%zu events=%zu revoked=%d\n",
        static_cast<int>(publication),level,a.snapshot.pivots.size(),a.snapshot.centers.size(),a.snapshot.movements.size(),a.snapshot.signals.size(),a.events.size(),revoked);
      total+=static_cast<int>(a.events.size());
    }
  }
  CHECK(total>0);
}


TEST(FamilySharesInputsAndMatchesIndependentReferenceAtBothLevels)
{
  auto source = chan::Series::FromRaw(SSE_DAILY_COUNT,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME);
  for (auto config : AnalysisConfigs({0,1,2,3,4,1000,101000,1000000,1000001,1000002,1000004,1101000}, {0,1,2,3,4}))
  {
    auto family = chan::AnalyzeFamily(source,config.analysis);
    CHECK(family.levels[0].inputs.get()==family.levels[1].inputs.get());
    for (int level=0; level<2; ++level)
    {
      config.level=static_cast<chan::CenterUnit>(level);
      auto reference=chan::AnalyzeReference(source,config);
      const auto &actual=family.levels[level];
      CHECK(Same(actual.events,reference.events));
      const auto &a=actual.snapshot,&b=reference.snapshot;
      REQUIRE(a.pivots.size()==b.pivots.size() && a.centers.size()==b.centers.size());
      CHECK(a.movements.size()==b.movements.size() && a.breakouts.size()==b.breakouts.size());
      for (std::size_t i=0; i<a.pivots.size(); ++i)
      {
        const auto &x=a.pivots[i],&y=b.pivots[i];
        CHECK(x.index==y.index && x.kind==y.kind && x.high==y.high && x.low==y.low && x.fractalAt==y.fractalAt);
        CHECK(x.firstFeatureIndex==y.firstFeatureIndex && x.lastFeatureIndex==y.lastFeatureIndex);
        CHECK(x.energy==y.energy && x.energyRed==y.energyRed && x.energyGreen==y.energyGreen && x.dif==y.dif && x.dea==y.dea);
      }
      for (std::size_t i=0; i<a.centers.size(); ++i)
      {
        const auto &x=a.centers[i],&y=b.centers[i];
        CHECK(x.firstPivot==y.firstPivot && x.lastPivot==y.lastPivot && x.start==y.start && x.end==y.end);
        CHECK(x.zg==y.zg && x.zd==y.zd && x.gg==y.gg && x.dd==y.dd && x.direction==y.direction);
      }
      std::vector<chan::SignalEvent> as,bs;
      for (const auto &s:a.signals) as.push_back({0,s,false});
      for (const auto &s:b.signals) bs.push_back({0,s,false});
      CHECK(Same(as,bs));
    }
  }
}

TEST(FamilyEventsAreCausalOnBothLevels)
{
  auto source = chan::Series::FromRaw(SSE_DAILY_COUNT,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME);
  for (auto config : AnalysisConfigs({0,101000,1000000,1101000}))
  {
    for (auto publication : {chan::SignalPublication::Standard,chan::SignalPublication::Early})
    {
      config.analysis.signals.publication=publication;
      auto full=chan::AnalyzeFamily(source,config.analysis);
      for (int n : {170,176,240,500,1000})
      {
        auto prefix=chan::Series::FromRaw(n,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME);
        auto partial=chan::AnalyzeFamily(prefix,config.analysis);
        for (int level=0; level<2; ++level)
        {
          std::vector<chan::SignalEvent> expected;
          for (const auto &event : full.levels[level].events) if (event.bar<n) expected.push_back(event);
          CHECK(Same(partial.levels[level].events,expected));
        }
      }
    }
  }
}
