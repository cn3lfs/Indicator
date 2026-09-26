// 迁移期：引擎事件流与旧实现的逐步重放（src/CzscReplay.cpp）逐事件一致。旧实现删除时本文件一并删除。
#include "check.h"
#include "oracle_data.h"
#include "core/engine.h"
#include "CzscCore.h"

#include <cstdio>

namespace
{
bool SameEvents(const std::vector<chan::SignalEvent> &mine, const std::vector<ReplaySignalEvent> &old)
{
  if (mine.size() != old.size())
  {
    std::printf("    %zu vs %zu events\n", mine.size(), old.size());
    return false;
  }
  for (std::size_t i = 0; i < old.size(); i++)
  {
    const chan::SignalEvent &m = mine[i];
    const ReplaySignalEvent &o = old[i];
    if (m.bar != o.nBar || m.signal.index != o.nIndex || static_cast<int>(m.signal.type) != static_cast<int>(o.fSignal) ||
        m.revoked != o.bRevoke || m.signal.stop != o.fStop || m.signal.priority != o.nPriority)
    {
      std::printf("    event %zu differs\n", i);
      return false;
    }
  }
  return true;
}
}  // namespace

TEST(OracleEngineEventsMatchReplay)
{
  std::vector<OracleSample> samples = OracleSamples();
  samples[1].high.resize(8000);
  samples[1].low.resize(8000);
  int events = 0;
  for (OracleSample &s : samples)
  {
    int n = static_cast<int>(s.high.size());
    chan::Series series = chan::Series::FromRaw(n, &s.high[0], &s.low[0]);
    for (int code : {0, 1, 2, 10, 100, 1100, 1101})
    {
      chan::Analysis a = chan::Analyze(series, *chan::Config::Decode(code));
      std::vector<ReplaySignalEvent> old = BuildReplaySignalEvents(n, &s.high[0], &s.low[0], DecodeConfig(static_cast<float>(code)));
      bool same = SameEvents(a.events, old);
      if (!same) std::printf("  %s cfg %d\n", s.name, code);
      CHECK(same);
      events += static_cast<int>(a.events.size());

      CzscAnalyzer full;
      BuildAnalyzerFromPrice(full, n, &s.high[0], &s.low[0], DecodeConfig(static_cast<float>(code)));
      CHECK(a.snapshot.signals.size() == full.Candidates.size() && a.snapshot.pivots.size() == full.Points.size() &&
            a.snapshot.centers.size() == full.Centers.size());
    }
    // 窗口模式
    chan::Analysis w = chan::Analyze(series, chan::Config{}, 600);
    CHECK(SameEvents(w.events, BuildReplaySignalEvents(n, &s.high[0], &s.low[0], DecodeConfig(0.0f), 600)));
  }
  CHECK(events > 50);
}
