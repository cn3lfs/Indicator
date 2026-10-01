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
    for (int code : {0, 1, 2, 10, 101, 1100, 1101, 3, 4, 13, 14, 1103, 1104, 11100, 11101, 11102, 11103, 11104, 21100, 21101, 21102, 21103, 21104})
    {
      chan::Config c = *chan::Config::Decode(code);
      chan::Analysis inc = chan::Analyze(series, c);
      chan::Analysis ref = chan::AnalyzeReference(series, c);
      bool same = Same(inc.events, ref.events);
      if (!same) std::printf("  %s cfg %d\n", s.name, code);
      CHECK(same);
      total += static_cast<int>(inc.events.size());
    }
    CHECK(Same(chan::Analyze(series, chan::Config{}, 700).events, chan::AnalyzeReference(series, chan::Config{}, 700).events));
  }
  CHECK(total > 100);
}

// 无未来函数的定义：只拿 [0,t] 数据分析得到的事件 = 全量分析中发生在 t 及以前的事件
TEST(EngineIsCausalOnEveryPrefix)
{
  chan::Series full = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW);
  for (int code : {0, 2, 1100, 3, 4, 13, 14, 1103, 1104, 11100, 11101, 11102, 11103, 11104, 21100, 21101, 21102, 21103, 21104})
  {
    chan::Config c = *chan::Config::Decode(code);
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
  for (int code : {0, 1, 2, 10, 101, 1100, 1101, 3, 4, 13, 14, 1103, 1104, 11100, 11101, 11102, 11103, 11104, 21100, 21101, 21102, 21103, 21104})
  {
    chan::Config c = *chan::Config::Decode(code);
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
    CHECK(all.pivotFinalAt.size() < 3 || all.pivotFinalAt[all.pivotFinalAt.size() - 3] >= 0 || code >= 1000);
  }
  CHECK(checked > 1000);
}

// 即时背驰预警逐根因果：前缀分析的预警 = 全量预警的对应前缀
TEST(InstantWarningIsCausal)
{
  chan::Series full = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW);
  for (int code : {0, 1100})
  {
    chan::Config c = *chan::Config::Decode(code);
    std::vector<int8_t> all = chan::Analyze(full, c).instantWarning;
    int up = 0, down = 0;
    for (int8_t w : all) { up += w > 0; down += w < 0; }
    CHECK(code != 0 || (up > 0 && down > 0));
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
      auto config = *chan::Config::Decode(code);
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
  for (int code : {0, 2, 1100, 100000})
  {
    auto c = *chan::Config::Decode(code);
    c.earlySignals = true;
    auto all = chan::Analyze(full, c);
    CHECK(Same(all.events, chan::AnalyzeReference(full, c).events));
    CHECK(Same(chan::Analyze(full, c, 700).events, chan::AnalyzeReference(full, c, 700).events));
    std::set<std::pair<int, int>> appeared;
    for (const auto &e : all.events)
      if (!e.revoked) CHECK(appeared.insert({e.signal.index, static_cast<int>(e.signal.type)}).second);
    for (int t = 120; t < full.Size(); t += 37)
    {
      auto pre = chan::Series::FromRaw(t + 1, SSE_DAILY_HIGH, SSE_DAILY_LOW, SSE_DAILY_CLOSE, SSE_DAILY_VOLUME);
      std::vector<chan::SignalEvent> expected;
      for (const auto &e : all.events) if (e.bar <= t) expected.push_back(e);
      CHECK(Same(chan::Analyze(pre, c).events, expected));
    }
  }
}
