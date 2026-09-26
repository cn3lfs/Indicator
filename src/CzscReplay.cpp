/*****************************************************************************
 * 禅论可视化分析系统
 * Copyright (C) 2016, Martin Tang

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#include "CzscInternal.h"
#include <algorithm>
#include <map>
#include <cstring>

namespace
{
struct SignalKey
{
  int   nIndex;
  float fSignal;
  bool operator<(const SignalKey &R) const
  {
    return (nIndex != R.nIndex) ? (nIndex < R.nIndex) : (fSignal < R.fSignal);
  }
};
}

// 合并K线无包含关系 → 相邻两根同升或同降，顶底分型严格交替，且分型在右侧首根K线出现时即定型；
// 故 t 时刻的前缀分型序列 = 全量分型中 nConfirmedAt <= t 的前缀，无需逐次重做包含处理。
// MACD 为因果 EMA，前缀取值与前缀重算一致，能量表只算一次。
std::vector<ReplaySignalEvent> BuildReplaySignalEvents(int nCount, float *pHigh, float *pLow,
                                                       const CzscConfig &Config, int nWindow)
{
  int nFromBar = (nWindow > 0) ? (nCount - nWindow) : 0;
  std::vector<ReplaySignalEvent> Events;
  if ((nCount <= 0) || (pHigh == 0) || (pLow == 0))
  {
    return Events;
  }

  std::vector<Fractal> All = BuildFractals(BuildMergedBars(nCount, pHigh, pLow));
  EnergyTables Tables = BuildEnergyTables(nCount, pHigh, pLow);

  std::map<SignalKey, int> Active;  // 当前有效信号 → 优先级
  std::vector<Fractal> Prefix;
  Prefix.reserve(All.size());
  for (std::size_t k = 0; k < All.size(); k++)
  {
    Prefix.push_back(All[k]);
    int nBar = All[k].nConfirmedAt;
    if ((k + 1 < All.size()) && (All[k + 1].nConfirmedAt == nBar))
    {
      continue;  // 同一时刻成立的分型一并处理
    }
    // 窗口外：只在窗口前的最后一步算基线，其余跳过
    bool bBaseline = (nBar < nFromBar);
    if (bBaseline && (k + 1 < All.size()) && (All[k + 1].nConfirmedAt < nFromBar))
    {
      continue;
    }

    std::vector<SegmentPoint> Points = BuildPointsFromFractals(Prefix, Config);
    AssignEnergyFromTables(Points, Tables);
    std::vector<Center> Centers = BuildCenters(Points);
    std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
    std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);
    std::vector<TradingSignalCandidate> Candidates =
      BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

    // 只认端点已被下一端点确认的候选（末端点仍可延伸，不输出）
    std::map<SignalKey, int> Now;
    for (std::size_t i = 0; i < Candidates.size(); i++)
    {
      const TradingSignalCandidate &C = Candidates[i];
      if ((C.nPoint < 0) || ((std::size_t)C.nPoint + 1 >= Points.size()))
      {
        continue;
      }
      SignalKey Key = {C.nIndex, C.fSignal};
      std::map<SignalKey, int>::iterator It = Now.find(Key);
      if ((It == Now.end()) || (It->second < C.nPriority))
      {
        Now[Key] = C.nPriority;
      }
    }

    if (bBaseline)
    {
      Active.swap(Now);  // 基线：窗口起点前已存在的信号，不作为事件
      continue;
    }
    for (std::map<SignalKey, int>::const_iterator It = Now.begin(); It != Now.end(); ++It)
    {
      if (Active.find(It->first) == Active.end())
      {
        ReplaySignalEvent E = {nBar, It->first.nIndex, It->first.fSignal, It->second, false};
        Events.push_back(E);
      }
    }
    for (std::map<SignalKey, int>::const_iterator It = Active.begin(); It != Active.end(); ++It)
    {
      if (Now.find(It->first) == Now.end())
      {
        ReplaySignalEvent E = {nBar, It->first.nIndex, It->first.fSignal, It->second, true};
        Events.push_back(E);
      }
    }
    Active.swap(Now);
  }
  return Events;
}

static unsigned int FnvFloats(unsigned int h, const float *p, int n)
{
  const unsigned char *b = (const unsigned char *)p;
  for (int i = 0; i < n * (int)sizeof(float); i++)
  {
    h ^= (unsigned int)b[i];
    h *= 16777619u;
  }
  return h;
}

const std::vector<ReplaySignalEvent> &GetOrBuildReplaySignalEvents(int nCount, float *pHigh, float *pLow,
                                                                   const CzscConfig &Config, int nWindow)
{
  static std::vector<ReplaySignalEvent> s_Events;
  static bool s_bValid = false;
  static int s_nCount = 0, s_nWindow = 0;
  static CzscConfig s_Config;
  static unsigned int s_hHash = 0;

  unsigned int h = 2166136261u;
  if ((nCount > 0) && pHigh && pLow)
  {
    h = FnvFloats(h, pHigh, nCount);
    h = FnvFloats(h, pLow, nCount);
    const std::vector<float> *pClose = GetValidatedClose(nCount, pHigh, pLow);
    if (pClose)
    {
      h = FnvFloats(h, &(*pClose)[0], nCount);
    }
  }
  if (s_bValid && (s_nCount == nCount) && (s_nWindow == nWindow) && (s_hHash == h) &&
      (memcmp(&s_Config, &Config, sizeof(CzscConfig)) == 0))
  {
    return s_Events;
  }
  s_Events = BuildReplaySignalEvents(nCount, pHigh, pLow, Config, nWindow);
  s_bValid = true;
  s_nCount = nCount;
  s_nWindow = nWindow;
  s_Config = Config;
  s_hHash = h;
  return s_Events;
}

void WriteReplaySignals(int nCount, float *pOut, const std::vector<ReplaySignalEvent> &Events, bool bRevoke)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }
  ClearOutput(nCount, pOut);
  std::vector<int> Priority((std::size_t)nCount, -1);
  for (std::size_t i = 0; i < Events.size(); i++)
  {
    const ReplaySignalEvent &E = Events[i];
    if ((E.bRevoke != bRevoke) || (E.nBar < 0) || (E.nBar >= nCount))
    {
      continue;
    }
    if (E.nPriority >= Priority[(std::size_t)E.nBar])
    {
      pOut[E.nBar] = E.fSignal;
      Priority[(std::size_t)E.nBar] = E.nPriority;
    }
  }
}
