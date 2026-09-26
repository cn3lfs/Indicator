// 迁移期：买卖点层与旧实现逐项一致（含顺序）。旧实现删除时本文件一并删除。
#include "check.h"
#include "oracle_data.h"
#include "core/dynamics.h"
#include "core/morphology.h"
#include "core/signals.h"
#include "core/structure.h"
#include "CzscCore.h"

#include <cstdio>

TEST(OracleSignalsMatch)
{
  int total = 0;
  for (OracleSample &s : OracleSamples())
  {
    int n = static_cast<int>(s.high.size());
    chan::Series series = chan::Series::FromRaw(n, &s.high[0], &s.low[0]);
    std::vector<chan::Fractal> fractals = chan::DetectFractals(chan::MergeBars(series));
    chan::EnergyTables tables = chan::BuildEnergyTables(series);
    for (int code : kOracleConfigs)
    {
      std::vector<chan::Pivot> p = chan::BuildPivots(fractals, *chan::Config::Decode(code));
      chan::AssignEnergy(p, tables);
      std::vector<chan::Center> c = chan::BuildCenters(p);
      std::vector<chan::Breakout> b = chan::BuildBreakouts(p, c);
      std::vector<chan::Signal> sig = chan::BuildSignals(p, c, chan::BuildMovements(c), b);

      CzscAnalyzer old;
      BuildAnalyzerFromPrice(old, n, &s.high[0], &s.low[0], DecodeConfig(static_cast<float>(code)));
      REQUIRE(b.size() == old.Breakouts.size());
      for (std::size_t i = 0; i < b.size(); i++)
      {
        const CenterBreakout &o = old.Breakouts[i];
        CHECK(b[i].center == o.nCenter && b[i].direction == o.nDirection && b[i].leavePivot == o.nLeavePoint &&
              b[i].retestPivot == o.nRetestPoint && b[i].third == o.bThirdSignal &&
              b[i].divergence.holds == o.Divergence.bDivergence);
      }
      if (sig.size() != old.Candidates.size())
      {
        std::printf("  %s cfg %d: %zu vs %zu signals\n", s.name, code, sig.size(), old.Candidates.size());
        CHECK(sig.size() == old.Candidates.size());
        continue;
      }
      int bad = 0;
      for (std::size_t i = 0; i < sig.size(); i++)
      {
        const TradingSignalCandidate &o = old.Candidates[i];
        const chan::Divergence &d = sig[i].divergence;
        bool same = sig[i].index == o.nIndex && static_cast<int>(sig[i].type) == static_cast<int>(o.fSignal) &&
                    sig[i].pivot == o.nPoint && sig[i].center == o.nCenter && sig[i].priority == o.nPriority &&
                    d.previousStart == o.Divergence.nPreviousStartPoint && d.previousEnd == o.Divergence.nPreviousEndPoint &&
                    d.currentStart == o.Divergence.nCurrentStartPoint && d.currentEnd == o.Divergence.nCurrentEndPoint &&
                    d.holds == o.Divergence.bDivergence && d.current.area == o.Divergence.Current.fMacdArea;
        if (!same) bad++;
      }
      if (bad) std::printf("  %s cfg %d: %d signals differ\n", s.name, code, bad);
      CHECK(bad == 0);
      total += static_cast<int>(sig.size());
    }
  }
  CHECK(total > 100);  // 放大样本上各配置应有足够多的信号参与对照
}
