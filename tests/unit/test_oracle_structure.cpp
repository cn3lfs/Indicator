// 迁移期：结构层/动力层与旧实现逐字段一致。旧实现删除时本文件一并删除。
#include "check.h"
#include "oracle_data.h"
#include "core/dynamics.h"
#include "core/morphology.h"
#include "core/structure.h"
#include "CzscCore.h"

#include <cstdio>

TEST(OracleCentersMovementsEnergyMatch)
{
  for (OracleSample &s : OracleSamples())
  {
    int n = static_cast<int>(s.high.size());
    chan::Series series = chan::Series::FromRaw(n, &s.high[0], &s.low[0]);
    std::vector<chan::Fractal> fractals = chan::DetectFractals(chan::MergeBars(series));
    chan::EnergyTables tables = chan::BuildEnergyTables(series);
    for (int code : kOracleConfigs)
    {
      std::vector<chan::Pivot> pivots = chan::BuildPivots(fractals, *chan::Config::Decode(code));
      chan::AssignEnergy(pivots, tables);
      std::vector<chan::Center> centers = chan::BuildCenters(pivots);
      std::vector<chan::Movement> moves = chan::BuildMovements(centers);

      CzscAnalyzer old;
      BuildAnalyzerFromPrice(old, n, &s.high[0], &s.low[0], DecodeConfig(static_cast<float>(code)));
      REQUIRE(pivots.size() == old.Points.size());
      int badEnergy = 0;
      for (std::size_t i = 0; i < pivots.size(); i++)
      {
        const SegmentPoint &o = old.Points[i];
        if (!(pivots[i].energy == o.fEnergy && pivots[i].energyRed == o.fEnergyRed &&
              pivots[i].energyGreen == o.fEnergyGreen && pivots[i].dif == o.fDif && pivots[i].dea == o.fDea))
          badEnergy++;
      }
      CHECK(badEnergy == 0);

      if (centers.size() != old.Centers.size())
      {
        std::printf("  %s cfg %d: %zu vs %zu centers\n", s.name, code, centers.size(), old.Centers.size());
        CHECK(centers.size() == old.Centers.size());
        continue;
      }
      int badCenter = 0;
      for (std::size_t i = 0; i < centers.size(); i++)
      {
        const ::Center &o = old.Centers[i];
        const chan::Center &c = centers[i];
        bool same = c.start == o.nStart && c.end == o.nEnd && c.zg == o.fHigh && c.zd == o.fLow &&
                    c.gg == o.fTop && c.dd == o.fBottom && c.direction == o.nDirection &&
                    pivots[static_cast<std::size_t>(c.lastPivot)].index == c.end &&
                    pivots[static_cast<std::size_t>(c.firstPivot)].index == c.start;
        if (!same) badCenter++;
      }
      if (badCenter) std::printf("  %s cfg %d: %d centers differ\n", s.name, code, badCenter);
      CHECK(badCenter == 0);

      REQUIRE(moves.size() == old.Structures.size());
      for (std::size_t i = 0; i < moves.size(); i++)
      {
        const TrendStructure &o = old.Structures[i];
        CHECK(static_cast<int>(moves[i].type) == o.nType && moves[i].firstCenter == o.nFirstCenter &&
              moves[i].lastCenter == o.nLastCenter && moves[i].start == o.nStart && moves[i].end == o.nEnd);
      }
    }
  }
}

TEST(OracleMovingAveragesMatch)
{
  for (OracleSample &s : OracleSamples())
  {
    int n = static_cast<int>(s.high.size());
    chan::MovingAverages mine = chan::BuildMovingAverages(chan::Series::FromRaw(n, &s.high[0], &s.low[0]));
    std::vector<float> shortMa, longMa;
    ComputeShortLongMa(n, &s.high[0], &s.low[0], &shortMa, &longMa);
    std::vector<int> kiss = ClassifyMaKisses(shortMa, longMa);
    REQUIRE(mine.shortMa == shortMa && mine.longMa == longMa);
    int bad = 0;
    for (int i = 0; i < n; i++)
      if (static_cast<int>(mine.kisses[static_cast<std::size_t>(i)]) != kiss[static_cast<std::size_t>(i)]) bad++;
    CHECK(bad == 0);
  }
}
