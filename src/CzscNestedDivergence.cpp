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

void WriteDivergenceSegmentSignal(int nCount,
                                         float *pOut,
                                         const std::vector<SegmentPoint> &Points,
                                         const std::vector<TradingSignalCandidate> &Candidates)
{
  ClearOutput(nCount, pOut);
  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nSource != SIGNAL_SOURCE_FIRST) ||
        !IsFirstSignal(C.fSignal) ||
        (C.nPoint < 1) || ((std::size_t)C.nPoint >= Points.size()))
    {
      continue;
    }

    int nStartBar = Points[(std::size_t)(C.nPoint - 1)].nIndex;
    int nEndBar = C.nIndex;
    int nSign = (C.fSignal == SIGNAL_FIRST_BUY) ? 1 : -1;
    if ((nStartBar >= 0) && (nStartBar < nCount))
    {
      pOut[nStartBar] = (float)(nSign * 1);
    }
    if ((nEndBar >= 0) && (nEndBar < nCount))
    {
      pOut[nEndBar] = (float)(nSign * 2);
    }
  }
}

static bool GetDivergenceSegmentBars(const std::vector<SegmentPoint> &Points,
                                     const TradingSignalCandidate &C,
                                     int *pStart,
                                     int *pEnd)
{
  if ((pStart == 0) || (pEnd == 0) ||
      (C.nSource != SIGNAL_SOURCE_FIRST) ||
      !IsFirstSignal(C.fSignal) ||
      (C.nPoint < 1) || ((std::size_t)C.nPoint >= Points.size()))
  {
    return false;
  }

  *pStart = Points[(std::size_t)(C.nPoint - 1)].nIndex;
  *pEnd = C.nIndex;
  if (*pStart > *pEnd)
  {
    int nTmp = *pStart;
    *pStart = *pEnd;
    *pEnd = nTmp;
  }
  return true;
}

static bool IsTrendDivergenceFirstCandidate(const TradingSignalCandidate &C)
{
  return (C.nSource == SIGNAL_SOURCE_FIRST) &&
         IsFirstSignal(C.fSignal) &&
         C.Divergence.bDivergence &&
         C.Divergence.bNewExtreme;
}

static bool IsConfirmedFirstDivergenceCandidate(const TradingSignalCandidate &C)
{
  return (C.nSource == SIGNAL_SOURCE_FIRST) &&
         IsFirstSignal(C.fSignal) &&
         C.Divergence.bDivergence;
}

static bool HasNestedSmallTurnConfirmation(const std::vector<TradingSignalCandidate> &Candidates,
                                           const TradingSignalCandidate &Low,
                                           int nHighStart,
                                           int nHighEnd)
{
  int nSide = GetTradingSignalSide(Low.fSignal);
  if ((nSide == 0) || (Low.nPoint < 0))
  {
    return false;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasMatchingSmallTurn(C) ||
        (C.nSmallTurnBasePoint != Low.nPoint) ||
        (GetTradingSignalSide(C.fSignal) != nSide) ||
        (C.nIndex < nHighStart) ||
        (C.nIndex > nHighEnd))
    {
      continue;
    }
    return true;
  }

  return false;
}

std::vector<NestedDivergenceContext> BuildNestedDivergenceContexts(
  const std::vector<SegmentPoint> &HighPoints,
  const std::vector<TradingSignalCandidate> &HighCandidates,
  const std::vector<SegmentPoint> &LowPoints,
  const std::vector<TradingSignalCandidate> &LowCandidates)
{
  std::vector<NestedDivergenceContext> Contexts;
  for (std::size_t i = 0; i < HighCandidates.size(); i++)
  {
    const TradingSignalCandidate &High = HighCandidates[i];
    if (!IsTrendDivergenceFirstCandidate(High))
    {
      continue;
    }

    int nHighStart = 0;
    int nHighEnd = 0;
    if (!GetDivergenceSegmentBars(HighPoints, High, &nHighStart, &nHighEnd))
    {
      continue;
    }

    for (std::size_t j = 0; j < LowCandidates.size(); j++)
    {
      const TradingSignalCandidate &Low = LowCandidates[j];
      if ((Low.fSignal != High.fSignal) || !IsConfirmedFirstDivergenceCandidate(Low))
      {
        continue;
      }

      int nLowStart = 0;
      int nLowEnd = 0;
      if (!GetDivergenceSegmentBars(LowPoints, Low, &nLowStart, &nLowEnd))
      {
        continue;
      }
      if ((nLowStart < nHighStart) || (nLowEnd > nHighEnd))
      {
        continue;
      }
      if ((nLowStart == nHighStart) && (nLowEnd == nHighEnd))
      {
        continue;
      }

      NestedDivergenceContext Ctx;
      Ctx.nIndex = Low.nIndex;
      Ctx.nLevel = Low.Divergence.bNewExtreme ? 1 : 0;
      Ctx.nSemantic = Low.Divergence.bNewExtreme ?
                      CZSC_DIVERGENCE_SEM_TREND :
                      CZSC_DIVERGENCE_SEM_CONSOLIDATION;
      Ctx.nConfirmFlags = CZSC_NESTED_INSIDE_HIGH_SEGMENT |
                          CZSC_NESTED_CONFIRMED_DIVERGENCE;
      if (Low.Divergence.bNewExtreme)
      {
        Ctx.nConfirmFlags |= CZSC_NESTED_NEW_EXTREME;
      }
      Ctx.nSourceDivergence = (int)i;
      Ctx.nLowStartPoint = Low.nPoint - 1;
      Ctx.nLowEndPoint = Low.nPoint;
      Ctx.nDirection = GetTradingSignalSide(Low.fSignal);
      Ctx.bSmallTurnSatisfied =
        Low.Divergence.bNewExtreme &&
        HasNestedSmallTurnConfirmation(LowCandidates, Low, nHighStart, nHighEnd);
      if (Ctx.bSmallTurnSatisfied && Low.Divergence.bNewExtreme)
      {
        Ctx.nLevel = 2;
        Ctx.nSemantic = CZSC_DIVERGENCE_SEM_SMALL_TURN;
        Ctx.nConfirmFlags |= CZSC_NESTED_SMALL_TURN;
      }
      Contexts.push_back(Ctx);
    }
  }

  return Contexts;
}

// 第27/61课区间套：在高级别背驰段中继续找低级别一类背驰段。输出编码沿用背驰段：
// 买 起点1/终点2，卖 起点-1/终点-2。该函数只做几何包含标记，不改变买卖点判定。
void WriteNestedDivergenceSignal(int nCount,
                                 float *pOut,
                                 const std::vector<SegmentPoint> &HighPoints,
                                 const std::vector<TradingSignalCandidate> &HighCandidates,
                                 const std::vector<SegmentPoint> &LowPoints,
                                 const std::vector<TradingSignalCandidate> &LowCandidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<NestedDivergenceContext> Contexts =
    BuildNestedDivergenceContexts(HighPoints, HighCandidates, LowPoints, LowCandidates);
  for (std::size_t i = 0; i < Contexts.size(); i++)
  {
    const NestedDivergenceContext &Ctx = Contexts[i];
    if ((Ctx.nLevel <= 0) ||
        (Ctx.nLowStartPoint < 0) || ((std::size_t)Ctx.nLowStartPoint >= LowPoints.size()) ||
        (Ctx.nLowEndPoint < 0) || ((std::size_t)Ctx.nLowEndPoint >= LowPoints.size()) ||
        (Ctx.nDirection == 0))
    {
      continue;
    }

    int nLowStart = LowPoints[(std::size_t)Ctx.nLowStartPoint].nIndex;
    int nLowEnd = LowPoints[(std::size_t)Ctx.nLowEndPoint].nIndex;
    int nSign = (Ctx.nDirection > 0) ? 1 : -1;
    if ((nLowStart >= 0) && (nLowStart < nCount))
    {
      pOut[nLowStart] = (float)(nSign * 1);
    }
    if ((nLowEnd >= 0) && (nLowEnd < nCount))
    {
      pOut[nLowEnd] = (float)(nSign * 2);
    }
  }
}

static void ApplyNestedDivergenceContextValue(int nCount,
                                              float *pOut,
                                              const std::vector<NestedDivergenceContext> &Contexts,
                                              int nWhich)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Contexts.size(); i++)
  {
    const NestedDivergenceContext &Ctx = Contexts[i];
    if ((Ctx.nLevel <= 0) ||
        (Ctx.nIndex < 0) || (Ctx.nIndex >= nCount) ||
        (Ctx.nLevel < Priorities[(std::size_t)Ctx.nIndex]))
    {
      continue;
    }

    float fCode = 0.0f;
    if (nWhich == 0)
    {
      fCode = (float)Ctx.nLevel;
    }
    else if (nWhich == 1)
    {
      fCode = (Ctx.nSourceDivergence >= 0) ? (float)(Ctx.nSourceDivergence + 1) : 0.0f;
    }
    else if (nWhich == 2)
    {
      fCode = (Ctx.nLowStartPoint >= 0) ? (float)(Ctx.nLowStartPoint + 1) : 0.0f;
    }
    else if (nWhich == 3)
    {
      fCode = (Ctx.nLowEndPoint >= 0) ? (float)(Ctx.nLowEndPoint + 1) : 0.0f;
    }

    pOut[Ctx.nIndex] = fCode;
    Priorities[(std::size_t)Ctx.nIndex] = Ctx.nLevel;
  }
}

void ApplyNestedDivergenceLevel(int nCount,
                                float *pOut,
                                const std::vector<NestedDivergenceContext> &Contexts)
{
  ApplyNestedDivergenceContextValue(nCount, pOut, Contexts, 0);
}

void ApplyNestedDivergenceSourceId(int nCount,
                                   float *pOut,
                                   const std::vector<NestedDivergenceContext> &Contexts)
{
  ApplyNestedDivergenceContextValue(nCount, pOut, Contexts, 1);
}

void ApplyNestedDivergenceStartPointId(int nCount,
                                       float *pOut,
                                       const std::vector<NestedDivergenceContext> &Contexts)
{
  ApplyNestedDivergenceContextValue(nCount, pOut, Contexts, 2);
}

void ApplyNestedDivergenceEndPointId(int nCount,
                                     float *pOut,
                                     const std::vector<NestedDivergenceContext> &Contexts)
{
  ApplyNestedDivergenceContextValue(nCount, pOut, Contexts, 3);
}

static void ApplyNestedDivergenceRecursiveValue(int nCount,
                                                float *pOut,
                                                const std::vector<NestedDivergenceContext> &Contexts,
                                                int nWhich)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Contexts.size(); i++)
  {
    const NestedDivergenceContext &Ctx = Contexts[i];
    if ((Ctx.nIndex < 0) || (Ctx.nIndex >= nCount))
    {
      continue;
    }

    int nPriority = Ctx.nLevel;
    if (nPriority <= 0)
    {
      nPriority = 0;
    }
    if (nPriority < Priorities[(std::size_t)Ctx.nIndex])
    {
      continue;
    }

    float fCode = 0.0f;
    if (nWhich == 0)
    {
      fCode = (float)Ctx.nSemantic;
    }
    else if (nWhich == 1)
    {
      fCode = (float)Ctx.nConfirmFlags;
    }
    else if (nWhich == 2)
    {
      fCode = (float)Ctx.nDirection;
    }

    pOut[Ctx.nIndex] = fCode;
    Priorities[(std::size_t)Ctx.nIndex] = nPriority;
  }
}

void ApplyNestedDivergenceSemantic(int nCount,
                                   float *pOut,
                                   const std::vector<NestedDivergenceContext> &Contexts)
{
  ApplyNestedDivergenceRecursiveValue(nCount, pOut, Contexts, 0);
}

void ApplyNestedDivergenceConfirmFlags(int nCount,
                                       float *pOut,
                                       const std::vector<NestedDivergenceContext> &Contexts)
{
  ApplyNestedDivergenceRecursiveValue(nCount, pOut, Contexts, 1);
}

void ApplyNestedDivergenceDirection(int nCount,
                                    float *pOut,
                                    const std::vector<NestedDivergenceContext> &Contexts)
{
  ApplyNestedDivergenceRecursiveValue(nCount, pOut, Contexts, 2);
}

