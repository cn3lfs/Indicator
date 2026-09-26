// 增量引擎 = 参照实现（每步整条重算后差分），逐事件一致；这是增量逻辑的长期规格。
#include "check.h"
#include "sse_data.h"
#include "core/engine.h"

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
        x.stop != y.stop || x.priority != y.priority || x.pivot != y.pivot || x.center != y.center)
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
    for (int code : kConfigs)
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
  for (int code : {0, 2, 1100})
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
