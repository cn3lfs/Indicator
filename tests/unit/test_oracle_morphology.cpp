// 迁移期：新形态层必须与旧实现（src/）逐字段一致。旧实现删除时本文件一并删除。
#include "check.h"
#include "oracle_data.h"
#include "core/morphology.h"
#include "CzscCore.h"

#include <cstdio>

TEST(OracleFractalsMatch)
{
  for (OracleSample &s : OracleSamples())
  {
    int n = static_cast<int>(s.high.size());
    std::vector<chan::Fractal> mine = chan::DetectFractals(chan::MergeBars(chan::Series::FromRaw(n, &s.high[0], &s.low[0])));
    std::vector<::Fractal> old = BuildFractals(BuildMergedBars(n, &s.high[0], &s.low[0]));
    REQUIRE(mine.size() == old.size());
    for (std::size_t i = 0; i < old.size(); i++)
    {
      CHECK(static_cast<int>(mine[i].kind) == old[i].nType && mine[i].index == old[i].nIndex &&
            mine[i].merged == old[i].nMergedIndex && mine[i].high == old[i].fHigh &&
            mine[i].low == old[i].fLow && mine[i].confirmedAt == old[i].nConfirmedAt);
    }
  }
}

TEST(OraclePivotsMatch)
{
  for (OracleSample &s : OracleSamples())
  {
    int n = static_cast<int>(s.high.size());
    std::vector<chan::Fractal> fractals =
      chan::DetectFractals(chan::MergeBars(chan::Series::FromRaw(n, &s.high[0], &s.low[0])));
    for (int code : kOracleConfigs)
    {
      std::vector<chan::Pivot> mine = chan::BuildPivots(fractals, *chan::Config::Decode(code));
      std::vector<SegmentPoint> old = BuildConfiguredPoints(n, &s.high[0], &s.low[0], DecodeConfig(static_cast<float>(code)));
      if (mine.size() != old.size())
      {
        std::printf("  %s cfg %d: %zu vs %zu pivots\n", s.name, code, mine.size(), old.size());
        CHECK(mine.size() == old.size());
        continue;
      }
      int bad = 0;
      for (std::size_t i = 0; i < old.size(); i++)
      {
        bool same = static_cast<int>(mine[i].kind) == old[i].nType && mine[i].index == old[i].nIndex &&
                    mine[i].high == old[i].fHigh && mine[i].low == old[i].fLow &&
                    mine[i].confirmedAt == old[i].nConfirmedAt;
        if (!same) bad++;
      }
      if (bad) std::printf("  %s cfg %d: %d pivots differ\n", s.name, code, bad);
      CHECK(bad == 0);
    }
  }
}
