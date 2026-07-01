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

static bool FindLastTrendStructure(const std::vector<TrendStructure> &Structures,
                                   int nIndex,
                                   int nDirection,
                                   int *pTrend)
{
  int nType = (nDirection > 0) ? CZSC_MOVEMENT_UP : CZSC_MOVEMENT_DOWN;
  int nTrend = -1;
  for (std::size_t i = 0; i < Structures.size(); i++)
  {
    if (Structures[i].nStart >= nIndex)
    {
      continue;
    }
    if (Structures[i].nType == nType)
    {
      nTrend = (int)i;
    }
  }

  if (nTrend < 0)
  {
    return false;
  }
  if (pTrend != 0)
  {
    *pTrend = nTrend;
  }
  return true;
}

static int FindLastCenterBeforeIndex(const std::vector<Center> &Centers, int nIndex);

static bool FindPreviousSameDirectionMove(const std::vector<SegmentPoint> &Points,
                                          std::size_t nPoint,
                                          int nDirection,
                                          std::size_t *pMove)
{
  if ((pMove == 0) || (nPoint < 2))
  {
    return false;
  }

  for (std::size_t i = nPoint - 2; ; i--)
  {
    if (GetMoveDirection(Points[i], Points[i + 1]) == nDirection)
    {
      *pMove = i;
      return true;
    }
    if (i == 0)
    {
      break;
    }
  }

  return false;
}

static bool FindPreviousSameDirectionMoveBeforeIndex(const std::vector<SegmentPoint> &Points,
                                                     std::size_t nPoint,
                                                     int nDirection,
                                                     int nBeforeIndex,
                                                     std::size_t *pMove)
{
  if ((pMove == 0) || (nPoint < 2))
  {
    return false;
  }

  for (std::size_t i = nPoint - 2; ; i--)
  {
    if ((Points[i + 1].nIndex < nBeforeIndex) &&
        (GetMoveDirection(Points[i], Points[i + 1]) == nDirection))
    {
      *pMove = i;
      return true;
    }
    if (i == 0)
    {
      break;
    }
  }

  return false;
}

static bool IsTrendDivergenceFirstBuy(const std::vector<SegmentPoint> &Points,
                                      const std::vector<Center> &Centers,
                                      const std::vector<TrendStructure> &Structures,
                                      std::size_t nPoint,
                                      DivergenceResult *pDivergence,
                                      int *pTrend)
{
  if (pDivergence != 0)
  {
    *pDivergence = MakeEmptyDivergence(-1);
  }
  if (pTrend != 0)
  {
    *pTrend = -1;
  }
  if ((nPoint < 4) || (Points[nPoint].nType != CZSC_POINT_BOTTOM))
  {
    return false;
  }

  int nTrend = -1;
  if (!FindLastTrendStructure(Structures, Points[nPoint].nIndex, -1, &nTrend))
  {
    return false;
  }
  if (Structures[(std::size_t)nTrend].nLastCenter <= Structures[(std::size_t)nTrend].nFirstCenter)
  {
    return false;  // 第一类买点只在至少两个同向中枢构成的下跌趋势背驰后出现
  }
  int nLastCenter = Structures[nTrend].nLastCenter;
  if (FindLastCenterBeforeIndex(Centers, Points[nPoint].nIndex) != nLastCenter)
  {
    return false;
  }

  const SegmentPoint &CurrentStart = Points[nPoint - 1];
  const SegmentPoint &CurrentEnd = Points[nPoint];
  if ((CurrentStart.nType != CZSC_POINT_TOP) ||
      (CurrentEnd.fLow >= Centers[nLastCenter].fLow))
  {
    return false;
  }

  std::size_t nPrevMove = 0;
  if (!FindPreviousSameDirectionMoveBeforeIndex(Points, nPoint, -1,
                                               Centers[nLastCenter].nStart,
                                               &nPrevMove))
  {
    return false;
  }

  const SegmentPoint &PrevStart = Points[nPrevMove];
  const SegmentPoint &PrevEnd = Points[nPrevMove + 1];
  if ((PrevStart.nType != CZSC_POINT_TOP) || (PrevEnd.nType != CZSC_POINT_BOTTOM))
  {
    return false;
  }

  DivergenceResult Divergence = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, -1);
  SetDivergencePointIds(&Divergence, (int)nPrevMove, (int)nPrevMove + 1, (int)nPoint - 1, (int)nPoint);
  if (pDivergence != 0)
  {
    *pDivergence = Divergence;
  }
  if (pTrend != 0)
  {
    *pTrend = nTrend;
  }
  return Divergence.bDivergence;
}

static bool IsTrendDivergenceFirstSell(const std::vector<SegmentPoint> &Points,
                                       const std::vector<Center> &Centers,
                                       const std::vector<TrendStructure> &Structures,
                                       std::size_t nPoint,
                                       DivergenceResult *pDivergence,
                                       int *pTrend)
{
  if (pDivergence != 0)
  {
    *pDivergence = MakeEmptyDivergence(1);
  }
  if (pTrend != 0)
  {
    *pTrend = -1;
  }
  if ((nPoint < 4) || (Points[nPoint].nType != CZSC_POINT_TOP))
  {
    return false;
  }

  int nTrend = -1;
  if (!FindLastTrendStructure(Structures, Points[nPoint].nIndex, 1, &nTrend))
  {
    return false;
  }
  if (Structures[(std::size_t)nTrend].nLastCenter <= Structures[(std::size_t)nTrend].nFirstCenter)
  {
    return false;  // 第一类卖点只在至少两个同向中枢构成的上涨趋势背驰后出现
  }
  int nLastCenter = Structures[nTrend].nLastCenter;
  if (FindLastCenterBeforeIndex(Centers, Points[nPoint].nIndex) != nLastCenter)
  {
    return false;
  }

  const SegmentPoint &CurrentStart = Points[nPoint - 1];
  const SegmentPoint &CurrentEnd = Points[nPoint];
  if ((CurrentStart.nType != CZSC_POINT_BOTTOM) ||
      (CurrentEnd.fHigh <= Centers[nLastCenter].fHigh))
  {
    return false;
  }

  std::size_t nPrevMove = 0;
  if (!FindPreviousSameDirectionMoveBeforeIndex(Points, nPoint, 1,
                                               Centers[nLastCenter].nStart,
                                               &nPrevMove))
  {
    return false;
  }

  const SegmentPoint &PrevStart = Points[nPrevMove];
  const SegmentPoint &PrevEnd = Points[nPrevMove + 1];
  if ((PrevStart.nType != CZSC_POINT_BOTTOM) || (PrevEnd.nType != CZSC_POINT_TOP))
  {
    return false;
  }

  DivergenceResult Divergence = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, 1);
  SetDivergencePointIds(&Divergence, (int)nPrevMove, (int)nPrevMove + 1, (int)nPoint - 1, (int)nPoint);
  if (pDivergence != 0)
  {
    *pDivergence = Divergence;
  }
  if (pTrend != 0)
  {
    *pTrend = nTrend;
  }
  return Divergence.bDivergence;
}

static DivergenceResult MeasureConsolidationDivergence(const std::vector<SegmentPoint> &Points,
                                                       std::size_t nLeavePoint,
                                                       int nDirection)
{
  DivergenceResult Empty = MakeEmptyDivergence(nDirection);
  if ((nLeavePoint == 0) || (nLeavePoint >= Points.size()))
  {
    return Empty;
  }

  std::size_t nPrevMove = 0;
  if (!FindPreviousSameDirectionMove(Points, nLeavePoint, nDirection, &nPrevMove))
  {
    return Empty;
  }

  const SegmentPoint &PrevStart = Points[nPrevMove];
  const SegmentPoint &PrevEnd = Points[nPrevMove + 1];
  const SegmentPoint &CurrentStart = Points[nLeavePoint - 1];
  const SegmentPoint &CurrentEnd = Points[nLeavePoint];

  if (nDirection > 0)
  {
    if ((PrevStart.nType != CZSC_POINT_BOTTOM) || (PrevEnd.nType != CZSC_POINT_TOP) ||
        (CurrentStart.nType != CZSC_POINT_BOTTOM) || (CurrentEnd.nType != CZSC_POINT_TOP) ||
        (CurrentEnd.fHigh <= PrevEnd.fHigh))
    {
      return Empty;
    }
  }
  else if (nDirection < 0)
  {
    if ((PrevStart.nType != CZSC_POINT_TOP) || (PrevEnd.nType != CZSC_POINT_BOTTOM) ||
        (CurrentStart.nType != CZSC_POINT_TOP) || (CurrentEnd.nType != CZSC_POINT_BOTTOM) ||
        (CurrentEnd.fLow >= PrevEnd.fLow))
    {
      return Empty;
    }
  }
  else
  {
    return Empty;
  }

  DivergenceResult Result = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, nDirection);
  SetDivergencePointIds(&Result, (int)nPrevMove, (int)nPrevMove + 1, (int)nLeavePoint - 1, (int)nLeavePoint);
  return Result;
}

static CenterBreakout MakeCenterBreakout(const std::vector<SegmentPoint> &Points,
                                         const Center &C,
                                         std::size_t nCenter,
                                         std::size_t nLeavePoint,
                                         std::size_t nRetestPoint,
                                         int nDirection)
{
  CenterBreakout B;
  B.nCenter = (int)nCenter;
  B.nDirection = nDirection;
  B.nLeavePoint = (int)nLeavePoint;
  B.nRetestPoint = (int)nRetestPoint;
  B.bFirstRetest = true;
  B.bBackIntoCenter = false;
  B.bThirdSignal = false;
  B.Divergence = MeasureConsolidationDivergence(Points, nLeavePoint, nDirection);
  B.bConsolidationDivergence = B.Divergence.bDivergence;

  const SegmentPoint &Retest = Points[nRetestPoint];
  if (nDirection > 0)
  {
    B.bBackIntoCenter = Retest.fLow < C.fHigh;
    B.bThirdSignal = !B.bBackIntoCenter;
  }
  else
  {
    B.bBackIntoCenter = Retest.fHigh > C.fLow;
    B.bThirdSignal = !B.bBackIntoCenter;
  }

  return B;
}

std::vector<CenterBreakout> BuildCenterBreakouts(const std::vector<SegmentPoint> &Points,
                                                 const std::vector<Center> &Centers,
                                                 const std::vector<TrendStructure> &Structures)
{
  (void)Structures;
  std::vector<CenterBreakout> Breakouts;
  if (Points.size() < 2)
  {
    return Breakouts;
  }

  for (std::size_t i = 0; i < Centers.size(); i++)
  {
    const Center &C = Centers[i];
    // 离开点 = 中枢结束后第一个突破 ZG/ZD 的笔端点（次级别走势离开中枢，第20课）。
    // 注：中枢首尾相连，离开段同时是下一中枢的首段，故不以下一中枢起点为界。
    for (std::size_t j = 1; j < Points.size(); j++)
    {
      const SegmentPoint &Start = Points[j - 1];
      const SegmentPoint &End = Points[j];
      if (End.nIndex <= C.nEnd)
      {
        continue;  // 中枢内部，跳过
      }

      int nDirection = GetMoveDirection(Start, End);

      // 离开点可能是中枢末端点本身：若 Start 已突破 ZG/ZD，离开已发生，
      // 当前段即为离开后第一段。若其反向，则是回试（第20课「首次回试」）。
      if ((Start.nType == CZSC_POINT_BOTTOM) && (Start.fLow < C.fLow))
      {
        int nBreakDir = -1;
        if (nDirection < 0) { continue; }  // 继续同向离开，等反向段
        Breakouts.push_back(MakeCenterBreakout(Points, C, i, j - 1, j, nBreakDir));
        break;
      }
      if ((Start.nType == CZSC_POINT_TOP) && (Start.fHigh > C.fHigh))
      {
        int nBreakDir = 1;
        if (nDirection > 0) { continue; }  // 继续同向离开，等反向段
        Breakouts.push_back(MakeCenterBreakout(Points, C, i, j - 1, j, nBreakDir));
        break;
      }

      bool bLeavesUp = (nDirection > 0) && (End.nType == CZSC_POINT_TOP) && (End.fHigh > C.fHigh);
      bool bLeavesDown = (nDirection < 0) && (End.nType == CZSC_POINT_BOTTOM) && (End.fLow < C.fLow);
      if (!bLeavesUp && !bLeavesDown)
      {
        continue;  // 尚未突破中枢（回拉/震荡），继续找
      }

      int nBreakDirection = bLeavesUp ? 1 : -1;
      // 回试点 = 离开后第一个反向端点（第20课「必须是第一次」回试）
      for (std::size_t k = j + 1; k < Points.size(); k++)
      {
        if (((nBreakDirection > 0) && (Points[k].nType == CZSC_POINT_BOTTOM)) ||
            ((nBreakDirection < 0) && (Points[k].nType == CZSC_POINT_TOP)))
        {
          Breakouts.push_back(MakeCenterBreakout(Points, C, i, j, k, nBreakDirection));
          break;
        }
      }
      break;  // 每个中枢只取第一次离开+回试
    }
  }

  return Breakouts;
}

// 找到某下标处信号所属（最靠近的、起点不晚于它）的中枢，无则返回 -1
static int FindLastCenterBeforeIndex(const std::vector<Center> &Centers, int nIndex)
{
  int nCenter = -1;
  for (std::size_t i = 0; i < Centers.size(); i++)
  {
    if (Centers[i].nStart <= nIndex)
    {
      nCenter = (int)i;
    }
  }
  return nCenter;
}

static int FindCompletedTrendByCenter(const std::vector<TrendStructure> &Structures,
                                      int nCenter,
                                      int nIndex)
{
  if (nCenter < 0)
  {
    return -1;
  }

  for (std::size_t i = 0; i < Structures.size(); i++)
  {
    if ((Structures[i].nFirstCenter <= nCenter) &&
        (nCenter <= Structures[i].nLastCenter) &&
        (Structures[i].nEnd <= nIndex))
    {
      return (int)i;
    }
  }
  return -1;
}

// 信号质量分级（第24/27/61课）：一类看价差+速度或 MACD 背驰，二类看盘整背驰/重合，三类看背驰
static int ClassifyTradingSignalQuality(int nSource, const DivergenceResult &Divergence)
{
  if (nSource == SIGNAL_SOURCE_FIRST)
  {
    // 一类买卖点：价差与速度同时走弱，或 MACD 柱面积走弱，即为强质量信号。
    bool bStrong = Divergence.bDivergence &&
                   ((Divergence.bWeakSpace && Divergence.bWeakSpeed) || Divergence.bWeakMacd);
    return bStrong ? CZSC_SIGNAL_QUALITY_STRONG : CZSC_SIGNAL_QUALITY_CONFIRMED;
  }

  if (nSource == SIGNAL_SOURCE_SECOND)
  {
    // 二类买卖点：与三类重合（第61课）或回抽段构成盘整背驰（第27课）即为强信号。
    return Divergence.bDivergence ?
           CZSC_SIGNAL_QUALITY_STRONG :
           CZSC_SIGNAL_QUALITY_CONFIRMED;
  }

  if (nSource == SIGNAL_SOURCE_THIRD)
  {
    return Divergence.bDivergence ?
           CZSC_SIGNAL_QUALITY_STRONG :
           CZSC_SIGNAL_QUALITY_CONFIRMED;
  }

  return CZSC_SIGNAL_QUALITY_WATCH;
}

static int ClassifyCenterPosition(const std::vector<SegmentPoint> &Points,
                                  const std::vector<Center> &Centers,
                                  int nPoint,
                                  int nCenter)
{
  if ((nPoint < 0) || ((std::size_t)nPoint >= Points.size()) ||
      (nCenter < 0) || ((std::size_t)nCenter >= Centers.size()))
  {
    return CZSC_CENTER_POSITION_UNKNOWN;
  }

  float fPrice = GetPointPrice(Points[(std::size_t)nPoint]);
  const Center &C = Centers[(std::size_t)nCenter];
  if (fPrice < C.fLow)
  {
    return CZSC_CENTER_POSITION_BELOW;
  }
  if (fPrice > C.fHigh)
  {
    return CZSC_CENTER_POSITION_ABOVE;
  }
  return CZSC_CENTER_POSITION_INSIDE;
}

// 背驰-转折定理（第29课）：一买/一卖出现后，看其后第一段反弹相对最后中枢的高度，
// 区分 中枢扩展(最弱) / 更大级别盘整 / 反趋势(最强)。需要存在反弹点，否则未知。
int ClassifyReversalStrength(const std::vector<SegmentPoint> &Points,
                             const std::vector<Center> &Centers,
                             int nPoint,
                             int nCenter,
                             float fSignal)
{
  if ((nCenter < 0) || ((std::size_t)nCenter >= Centers.size()) || (nPoint < 0))
  {
    return CZSC_REVERSAL_UNKNOWN;
  }

  std::size_t nNext = (std::size_t)nPoint + 1;
  if (nNext >= Points.size())
  {
    return CZSC_REVERSAL_UNKNOWN;
  }

  const Center &C = Centers[(std::size_t)nCenter];
  const SegmentPoint &Rebound = Points[nNext];

  if (fSignal == SIGNAL_FIRST_BUY)
  {
    if (Rebound.nType != CZSC_POINT_TOP)
    {
      return CZSC_REVERSAL_UNKNOWN;
    }
    if (Rebound.fHigh > C.fHigh)     // 突破中枢上沿 → 反趋势
    {
      return CZSC_REVERSAL_TREND;
    }
    if (Rebound.fHigh >= C.fLow)     // 回到中枢区间 → 更大级别盘整
    {
      return CZSC_REVERSAL_CONSOLIDATION;
    }
    return CZSC_REVERSAL_EXTENSION;  // 仅触及下沿/DD → 最弱 → 最后中枢扩展
  }

  if (fSignal == SIGNAL_FIRST_SELL)
  {
    if (Rebound.nType != CZSC_POINT_BOTTOM)
    {
      return CZSC_REVERSAL_UNKNOWN;
    }
    if (Rebound.fLow < C.fLow)       // 跌破中枢下沿 → 反趋势
    {
      return CZSC_REVERSAL_TREND;
    }
    if (Rebound.fLow <= C.fHigh)     // 回到中枢区间 → 更大级别盘整
    {
      return CZSC_REVERSAL_CONSOLIDATION;
    }
    return CZSC_REVERSAL_EXTENSION;  // 仅触及上沿/GG → 最弱 → 最后中枢扩展
  }

  return CZSC_REVERSAL_UNKNOWN;
}

static TradingSignalCandidate MakeTradingSignalCandidate(int nIndex,
                                                         float fSignal,
                                                         int nPriority,
                                                         int nPoint,
                                                         int nCenter,
                                                         int nBreakout,
                                                         int nSource,
                                                         int nTrend,
                                                         int nMovementType,
                                                         int nCenterPosition,
                                                         bool bOverlapped,
                                                         const DivergenceResult &Divergence)
{
  TradingSignalCandidate C;
  C.nIndex = nIndex;
  C.fSignal = fSignal;
  C.nPriority = nPriority;
  C.nPoint = nPoint;
  C.nCenter = nCenter;
  C.nBreakout = nBreakout;
  C.nSource = nSource;
  C.nTrend = nTrend;
  C.nMovementType = nMovementType;
  C.nQuality = ClassifyTradingSignalQuality(nSource, Divergence);
  C.nCenterPosition = nCenterPosition;
  C.nReversal = CZSC_REVERSAL_UNKNOWN;
  C.nAfterEffect = CZSC_CENTER_AFTERMATH_UNKNOWN;
  C.nSecondBasePoint = -1;
  C.nSecondTurnPoint = -1;
  C.nSmallTurn = 0;
  C.nSmallTurnBasePoint = -1;
  C.nAbcStructure = 0;
  C.nAbcBreakout = -1;
  C.nMacdZeroPullback = 0;
  C.bOverlapped = bOverlapped;
  C.Divergence = Divergence;
  return C;
}

static int FindOverlappedBreakout(const std::vector<CenterBreakout> &Breakouts,
                                  int nPoint,
                                  int nCenter,
                                  int nDirection)
{
  for (std::size_t i = 0; i < Breakouts.size(); i++)
  {
    const CenterBreakout &B = Breakouts[i];
    if (B.bFirstRetest && B.bThirdSignal &&
        (B.nCenter == nCenter) &&
        (B.nDirection == nDirection) &&
        (B.nRetestPoint == nPoint))
    {
      return (int)i;
    }
  }
  return -1;
}

static void AppendFirstSignalCandidates(std::vector<TradingSignalCandidate> *pCandidates,
                                        const std::vector<SegmentPoint> &Points,
                                        const std::vector<Center> &Centers,
                                        const std::vector<TrendStructure> &Structures)
{
  if (pCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < Points.size(); i++)
  {
    DivergenceResult Divergence = MakeEmptyDivergence(0);
    int nTrend = -1;
    if (IsTrendDivergenceFirstBuy(Points, Centers, Structures, i, &Divergence, &nTrend))
    {
      int nCenter = Structures[(std::size_t)nTrend].nLastCenter;
      pCandidates->push_back(MakeTradingSignalCandidate(Points[i].nIndex,
                                                        SIGNAL_FIRST_BUY,
                                                        SIGNAL_PRIORITY_FIRST,
                                                        (int)i,
                                                        nCenter,
                                                        -1,
                                                        SIGNAL_SOURCE_FIRST,
                                                        nTrend,
                                                        Structures[(std::size_t)nTrend].nType,
                                                        ClassifyCenterPosition(Points, Centers, (int)i, nCenter),
                                                        false,
                                                        Divergence));
      pCandidates->back().nReversal =
        ClassifyReversalStrength(Points, Centers, (int)i, nCenter, SIGNAL_FIRST_BUY);
    }
    else if (IsTrendDivergenceFirstSell(Points, Centers, Structures, i, &Divergence, &nTrend))
    {
      int nCenter = Structures[(std::size_t)nTrend].nLastCenter;
      pCandidates->push_back(MakeTradingSignalCandidate(Points[i].nIndex,
                                                        SIGNAL_FIRST_SELL,
                                                        SIGNAL_PRIORITY_FIRST,
                                                        (int)i,
                                                        nCenter,
                                                        -1,
                                                        SIGNAL_SOURCE_FIRST,
                                                        nTrend,
                                                        Structures[(std::size_t)nTrend].nType,
                                                        ClassifyCenterPosition(Points, Centers, (int)i, nCenter),
                                                        false,
                                                        Divergence));
      pCandidates->back().nReversal =
        ClassifyReversalStrength(Points, Centers, (int)i, nCenter, SIGNAL_FIRST_SELL);
    }
  }

  // 去重：同一中枢区域内只保留价格最极端的一类买卖点（每中枢至多一个一买/一卖）。
  // 全趋势去重会丢失中间中枢的区域极值（如连续下跌趋势只保留最低点），改用中枢分组。
  std::vector<TradingSignalCandidate> Deduped;
  for (std::size_t a = 0; a < pCandidates->size(); a++)
  {
    const TradingSignalCandidate &Ca = (*pCandidates)[a];
    float fA = (Ca.fSignal == SIGNAL_FIRST_BUY) ? Points[Ca.nPoint].fLow : Points[Ca.nPoint].fHigh;

    bool bKeep = true;
    for (std::size_t b = 0; b < pCandidates->size(); b++)
    {
      if ((b == a) || ((*pCandidates)[b].fSignal != Ca.fSignal))
      {
        continue;
      }
      const TradingSignalCandidate &Cb = (*pCandidates)[b];
      if (Cb.nCenter != Ca.nCenter)
      {
        continue;  // 不同中枢，各保留
      }
      float fB = (Cb.fSignal == SIGNAL_FIRST_BUY) ? Points[Cb.nPoint].fLow : Points[Cb.nPoint].fHigh;
      bool bBmoreExtreme = (Ca.fSignal == SIGNAL_FIRST_BUY) ? (fB < fA) : (fB > fA);
      if (bBmoreExtreme || ((AbsF(fB - fA) < 0.0001f) && (b < a)))
      {
        bKeep = false;  // 同中枢内 b 更极端（或同值取靠前）→ 丢弃 a
        break;
      }
    }
    if (bKeep)
    {
      Deduped.push_back(Ca);
    }
  }
  *pCandidates = Deduped;
}

static bool IsAbcBreakoutAlignedWithCurrentSegment(const TradingSignalCandidate &C,
                                                   const CenterBreakout &B)
{
  if ((C.Divergence.nCurrentStartPoint < 0) ||
      (C.Divergence.nCurrentEndPoint < 0) ||
      (C.Divergence.nCurrentStartPoint >= C.Divergence.nCurrentEndPoint) ||
      (C.Divergence.nCurrentEndPoint != C.nPoint))
  {
    return false;
  }
  if ((B.nLeavePoint < 0) ||
      (B.nRetestPoint < 0) ||
      (B.nLeavePoint >= B.nRetestPoint))
  {
    return false;
  }
  return B.nRetestPoint == C.Divergence.nCurrentStartPoint;
}

static void AnnotateAbcDivergenceStructure(std::vector<TradingSignalCandidate> *pFirstCandidates,
                                           const std::vector<CenterBreakout> &Breakouts)
{
  if (pFirstCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < pFirstCandidates->size(); i++)
  {
    TradingSignalCandidate &C = (*pFirstCandidates)[i];
    if ((C.nSource != SIGNAL_SOURCE_FIRST) ||
        ((C.fSignal != SIGNAL_FIRST_BUY) && (C.fSignal != SIGNAL_FIRST_SELL)))
    {
      continue;
    }

    for (std::size_t j = 0; j < Breakouts.size(); j++)
    {
      const CenterBreakout &B = Breakouts[j];
      if (!B.bFirstRetest || !B.bThirdSignal ||
          (B.nCenter != C.nCenter) ||
          (B.nRetestPoint < 0) || (B.nRetestPoint >= C.nPoint))
      {
        continue;
      }
      if (!IsAbcBreakoutAlignedWithCurrentSegment(C, B))
      {
        continue;
      }
      if ((C.fSignal == SIGNAL_FIRST_BUY) && (B.nDirection < 0))
      {
        C.nAbcStructure = 1;
        C.nAbcBreakout = (int)j;
        break;
      }
      if ((C.fSignal == SIGNAL_FIRST_SELL) && (B.nDirection > 0))
      {
        C.nAbcStructure = -1;
        C.nAbcBreakout = (int)j;
        break;
      }
    }
  }
}

static bool CenterHasMacdZeroPullback(const std::vector<SegmentPoint> &Points,
                                      const Center &C,
                                      const DivergenceResult &D)
{
  float fBase = D.Previous.fDifHeight;
  if (D.Previous.fDeaHeight > fBase)
  {
    fBase = D.Previous.fDeaHeight;
  }
  if (D.Current.fDifHeight > fBase)
  {
    fBase = D.Current.fDifHeight;
  }
  if (D.Current.fDeaHeight > fBase)
  {
    fBase = D.Current.fDeaHeight;
  }
  if (fBase <= 0)
  {
    return false;
  }

  float fThreshold = fBase * 0.25f;
  if (fThreshold < 0.0001f)
  {
    fThreshold = 0.0001f;
  }

  for (std::size_t i = 0; i < Points.size(); i++)
  {
    const SegmentPoint &P = Points[i];
    if ((P.nIndex < C.nStart) || (P.nIndex > C.nEnd))
    {
      continue;
    }
    if ((AbsF(P.fDif) <= fThreshold) && (AbsF(P.fDea) <= fThreshold))
    {
      return true;
    }
  }
  return false;
}

static void AnnotateMacdZeroPullback(std::vector<TradingSignalCandidate> *pFirstCandidates,
                                     const std::vector<SegmentPoint> &Points,
                                     const std::vector<Center> &Centers)
{
  if (pFirstCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < pFirstCandidates->size(); i++)
  {
    TradingSignalCandidate &C = (*pFirstCandidates)[i];
    if ((C.nSource != SIGNAL_SOURCE_FIRST) ||
        (C.nCenter < 0) || ((std::size_t)C.nCenter >= Centers.size()))
    {
      continue;
    }
    if (!CenterHasMacdZeroPullback(Points, Centers[(std::size_t)C.nCenter], C.Divergence))
    {
      continue;
    }
    if (C.fSignal == SIGNAL_FIRST_BUY)
    {
      C.nMacdZeroPullback = 1;
    }
    else if (C.fSignal == SIGNAL_FIRST_SELL)
    {
      C.nMacdZeroPullback = -1;
    }
  }
}

// 第二类买卖点的盘整背驰（第27课）：比较进入一类买卖点的那一段(A段)与转折后回抽到
// 二类买卖点的那一段(C段)的力度。C段更弱（价差与速度同时走弱，或 MACD 柱面积走弱）即构成
// 盘整背驰。盘整背驰不要求创新高/新低。
static DivergenceResult MeasureSecondDivergence(const std::vector<SegmentPoint> &Points,
                                                std::size_t nFirstPoint,
                                                int nDirection)
{
  DivergenceResult Result = MakeEmptyDivergence(nDirection);
  if ((nFirstPoint < 1) || (nFirstPoint + 2 >= Points.size()))
  {
    return Result;
  }

  Result.Previous = MeasureStrength(Points[nFirstPoint - 1], Points[nFirstPoint]);
  Result.Current = MeasureStrength(Points[nFirstPoint + 1], Points[nFirstPoint + 2]);
  SetDivergencePointIds(&Result,
                        (int)nFirstPoint - 1,
                        (int)nFirstPoint,
                        (int)nFirstPoint + 1,
                        (int)nFirstPoint + 2);
  Result.bWeakSpace = Result.Current.fSpace < Result.Previous.fSpace;
  Result.bWeakSpeed = Result.Current.fSpeed < Result.Previous.fSpeed;
  Result.bWeakMacd = (Result.Current.fMacdArea > 0) &&
                     (Result.Previous.fMacdArea > 0) &&
                     (Result.Current.fMacdArea < Result.Previous.fMacdArea);
  Result.bNewExtreme = false;
  Result.bDivergence = (Result.bWeakSpace && Result.bWeakSpeed) || Result.bWeakMacd;
  return Result;
}

static void AppendSecondSignalCandidates(std::vector<TradingSignalCandidate> *pCandidates,
                                         const std::vector<SegmentPoint> &Points,
                                         const std::vector<Center> &Centers,
                                         const std::vector<CenterBreakout> &Breakouts,
                                         const std::vector<TradingSignalCandidate> &FirstCandidates)
{
  if (pCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < FirstCandidates.size(); i++)
  {
    const TradingSignalCandidate &FirstSignal = FirstCandidates[i];
    if ((FirstSignal.nPoint < 0) || ((std::size_t)FirstSignal.nPoint + 2 >= Points.size()))
    {
      continue;
    }

    std::size_t nPoint = (std::size_t)FirstSignal.nPoint;
    const SegmentPoint &First = Points[nPoint];
    const SegmentPoint &Turn = Points[nPoint + 1];
    const SegmentPoint &Second = Points[nPoint + 2];
    if ((FirstSignal.fSignal == SIGNAL_FIRST_BUY) &&
        (Turn.nType == CZSC_POINT_TOP) &&
        (Second.nType == CZSC_POINT_BOTTOM) &&
        (Second.fLow >= First.fLow))
    {
      int nBreakout = FindOverlappedBreakout(Breakouts, (int)nPoint + 2, FirstSignal.nCenter, 1);
      DivergenceResult Divergence = (nBreakout >= 0) ?
                                    Breakouts[(std::size_t)nBreakout].Divergence :
                                    MeasureSecondDivergence(Points, nPoint, 1);
      int nSecondPoint = (int)nPoint + 2;
      int nCenter = FindLastCenterBeforeIndex(Centers, Second.nIndex);
      pCandidates->push_back(MakeTradingSignalCandidate(Second.nIndex,
                                                        SIGNAL_SECOND_BUY,
                                                        SIGNAL_PRIORITY_SECOND,
                                                        nSecondPoint,
                                                        nCenter,
                                                        nBreakout,
                                                        SIGNAL_SOURCE_SECOND,
                                                        FirstSignal.nTrend,
                                                        FirstSignal.nMovementType,
                                                        ClassifyCenterPosition(Points, Centers, nSecondPoint, nCenter),
                                                        nBreakout >= 0,
                                                        Divergence));
      pCandidates->back().nSecondBasePoint = FirstSignal.nPoint;
      pCandidates->back().nSecondTurnPoint = (int)nPoint + 1;
    }
    else if ((FirstSignal.fSignal == SIGNAL_FIRST_SELL) &&
             (Turn.nType == CZSC_POINT_BOTTOM) &&
             (Second.nType == CZSC_POINT_TOP) &&
             (Second.fHigh <= First.fHigh))
    {
      int nBreakout = FindOverlappedBreakout(Breakouts, (int)nPoint + 2, FirstSignal.nCenter, -1);
      DivergenceResult Divergence = (nBreakout >= 0) ?
                                    Breakouts[(std::size_t)nBreakout].Divergence :
                                    MeasureSecondDivergence(Points, nPoint, -1);
      int nSecondPoint = (int)nPoint + 2;
      int nCenter = FindLastCenterBeforeIndex(Centers, Second.nIndex);
      pCandidates->push_back(MakeTradingSignalCandidate(Second.nIndex,
                                                        SIGNAL_SECOND_SELL,
                                                        SIGNAL_PRIORITY_SECOND,
                                                        nSecondPoint,
                                                        nCenter,
                                                        nBreakout,
                                                        SIGNAL_SOURCE_SECOND,
                                                        FirstSignal.nTrend,
                                                        FirstSignal.nMovementType,
                                                        ClassifyCenterPosition(Points, Centers, nSecondPoint, nCenter),
                                                        nBreakout >= 0,
                                                        Divergence));
      pCandidates->back().nSecondBasePoint = FirstSignal.nPoint;
      pCandidates->back().nSecondTurnPoint = (int)nPoint + 1;
    }
  }
}

static void AppendThirdSignalCandidates(std::vector<TradingSignalCandidate> *pCandidates,
                                        const std::vector<SegmentPoint> &Points,
                                        const std::vector<Center> &Centers,
                                        const std::vector<TrendStructure> &Structures,
                                        const std::vector<CenterBreakout> &Breakouts)
{
  if (pCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < Breakouts.size(); i++)
  {
    const CenterBreakout &B = Breakouts[i];
    if (!B.bFirstRetest || !B.bThirdSignal ||
        (B.nDirection == 0) ||
        (B.nCenter < 0) || ((std::size_t)B.nCenter >= Centers.size()) ||
        (B.nRetestPoint < 0) || ((std::size_t)B.nRetestPoint >= Points.size()))
    {
      continue;
    }

    float fSignal = (B.nDirection > 0) ? SIGNAL_THIRD_BUY : SIGNAL_THIRD_SELL;
    bool bOverlapped = false;
    for (std::size_t j = 0; j < pCandidates->size(); j++)
    {
      const TradingSignalCandidate &Second = (*pCandidates)[j];
      if ((Second.nSource == SIGNAL_SOURCE_SECOND) &&
          (Second.nBreakout == (int)i) &&
          (Second.nIndex == Points[B.nRetestPoint].nIndex) &&
          (GetTradingSignalSide(Second.fSignal) == GetTradingSignalSide(fSignal)))
      {
        bOverlapped = true;
        break;
      }
    }
    int nTrend = FindCompletedTrendByCenter(Structures,
                                            B.nCenter,
                                            Points[B.nRetestPoint].nIndex);
    int nMovementType = (nTrend >= 0) ? Structures[(std::size_t)nTrend].nType : CZSC_MOVEMENT_CONSOLIDATION;
    pCandidates->push_back(MakeTradingSignalCandidate(Points[B.nRetestPoint].nIndex,
                                                      fSignal,
                                                      SIGNAL_PRIORITY_THIRD,
                                                      B.nRetestPoint,
                                                      B.nCenter,
                                                      (int)i,
                                                      SIGNAL_SOURCE_THIRD,
                                                      nTrend,
                                                      nMovementType,
                                                      ClassifyCenterPosition(Points, Centers, B.nRetestPoint, B.nCenter),
                                                      bOverlapped,
                                                      B.Divergence));
    pCandidates->back().nAfterEffect = ClassifyCenterAftermath(Centers, B.nCenter, fSignal);
  }
}

static void AnnotateSmallTurnConditions(std::vector<TradingSignalCandidate> *pCandidates,
                                        const std::vector<TradingSignalCandidate> &FirstCandidates)
{
  if (pCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < pCandidates->size(); i++)
  {
    TradingSignalCandidate &C = (*pCandidates)[i];
    if ((C.fSignal != SIGNAL_THIRD_BUY) && (C.fSignal != SIGNAL_THIRD_SELL))
    {
      continue;
    }

    for (std::size_t j = 0; j < FirstCandidates.size(); j++)
    {
      const TradingSignalCandidate &First = FirstCandidates[j];
      if ((First.nCenter != C.nCenter) || (First.nPoint >= C.nPoint))
      {
        continue;
      }
      if ((First.fSignal == SIGNAL_FIRST_BUY) && (C.fSignal == SIGNAL_THIRD_BUY))
      {
        C.nSmallTurn = 1;
        C.nSmallTurnBasePoint = First.nPoint;
        break;
      }
      if ((First.fSignal == SIGNAL_FIRST_SELL) && (C.fSignal == SIGNAL_THIRD_SELL))
      {
        C.nSmallTurn = -1;
        C.nSmallTurnBasePoint = First.nPoint;
        break;
      }
    }
  }
}

std::vector<TradingSignalCandidate> BuildTradingSignalCandidates(const std::vector<SegmentPoint> &Points,
                                                                  const std::vector<Center> &Centers,
                                                                  const std::vector<TrendStructure> &Structures,
                                                                  const std::vector<CenterBreakout> &Breakouts)
{
  std::vector<TradingSignalCandidate> Candidates;
  std::vector<TradingSignalCandidate> FirstCandidates;

  AppendFirstSignalCandidates(&FirstCandidates, Points, Centers, Structures);
  AnnotateAbcDivergenceStructure(&FirstCandidates, Breakouts);
  AnnotateMacdZeroPullback(&FirstCandidates, Points, Centers);
  AppendSecondSignalCandidates(&Candidates, Points, Centers, Breakouts, FirstCandidates);
  AppendThirdSignalCandidates(&Candidates, Points, Centers, Structures, Breakouts);
  AnnotateSmallTurnConditions(&Candidates, FirstCandidates);
  Candidates.insert(Candidates.end(), FirstCandidates.begin(), FirstCandidates.end());

  return Candidates;
}

static void SetFilterReason(std::vector<int> *pReasons, const std::vector<SegmentPoint> &Points, int nPoint, int nReason)
{
  if ((pReasons == 0) || (nReason == CZSC_FILTER_NONE) ||
      (nPoint < 0) || ((std::size_t)nPoint >= Points.size()))
  {
    return;
  }

  int nIndex = Points[(std::size_t)nPoint].nIndex;
  if ((nIndex < 0) || ((std::size_t)nIndex >= pReasons->size()))
  {
    return;
  }
  if ((*pReasons)[(std::size_t)nIndex] == CZSC_FILTER_NONE)
  {
    (*pReasons)[(std::size_t)nIndex] = nReason;
  }
}

static bool HasCandidateAtPoint(const std::vector<TradingSignalCandidate> &Candidates, int nPoint, float fSignal)
{
  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    if ((Candidates[i].nPoint == nPoint) && (Candidates[i].fSignal == fSignal))
    {
      return true;
    }
  }
  return false;
}

static int ClassifyFirstFilterReason(const std::vector<SegmentPoint> &Points,
                                     const std::vector<Center> &Centers,
                                     const std::vector<TrendStructure> &Structures,
                                     std::size_t nPoint,
                                     int nDirection)
{
  int nRequiredType = (nDirection > 0) ? CZSC_POINT_TOP : CZSC_POINT_BOTTOM;
  if ((nPoint < 4) || (Points[nPoint].nType != nRequiredType))
  {
    return CZSC_FILTER_NONE;
  }

  int nTrend = -1;
  if (!FindLastTrendStructure(Structures, Points[nPoint].nIndex, nDirection, &nTrend))
  {
    return CZSC_FILTER_NO_TREND;
  }
  if (Structures[(std::size_t)nTrend].nLastCenter <= Structures[(std::size_t)nTrend].nFirstCenter)
  {
    return CZSC_FILTER_NO_TREND;
  }

  int nLastCenter = Structures[(std::size_t)nTrend].nLastCenter;
  if ((nLastCenter < 0) || ((std::size_t)nLastCenter >= Centers.size()))
  {
    return CZSC_FILTER_MISSING_CENTER;
  }
  int nNearCenter = FindLastCenterBeforeIndex(Centers, Points[nPoint].nIndex);
  if (nNearCenter < 0)
  {
    return CZSC_FILTER_MISSING_CENTER;
  }
  if (nNearCenter != nLastCenter)
  {
    return CZSC_FILTER_NON_TREND_DIVERGENCE;
  }

  const SegmentPoint &CurrentStart = Points[nPoint - 1];
  const SegmentPoint &CurrentEnd = Points[nPoint];
  if (nDirection < 0)
  {
    if ((CurrentStart.nType != CZSC_POINT_TOP) || (CurrentEnd.fLow >= Centers[(std::size_t)nLastCenter].fLow))
    {
      return CZSC_FILTER_NON_TREND_DIVERGENCE;
    }
  }
  else
  {
    if ((CurrentStart.nType != CZSC_POINT_BOTTOM) || (CurrentEnd.fHigh <= Centers[(std::size_t)nLastCenter].fHigh))
    {
      return CZSC_FILTER_NON_TREND_DIVERGENCE;
    }
  }

  std::size_t nPrevMove = 0;
  if (!FindPreviousSameDirectionMoveBeforeIndex(Points,
                                                nPoint,
                                                nDirection,
                                                Centers[(std::size_t)nLastCenter].nStart,
                                                &nPrevMove))
  {
    return CZSC_FILTER_NON_TREND_DIVERGENCE;
  }

  const SegmentPoint &PrevStart = Points[nPrevMove];
  const SegmentPoint &PrevEnd = Points[nPrevMove + 1];
  if (nDirection < 0)
  {
    if ((PrevStart.nType != CZSC_POINT_TOP) || (PrevEnd.nType != CZSC_POINT_BOTTOM))
    {
      return CZSC_FILTER_DIRECTION_MISMATCH;
    }
  }
  else
  {
    if ((PrevStart.nType != CZSC_POINT_BOTTOM) || (PrevEnd.nType != CZSC_POINT_TOP))
    {
      return CZSC_FILTER_DIRECTION_MISMATCH;
    }
  }

  DivergenceResult Divergence = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, nDirection);
  return Divergence.bDivergence ? CZSC_FILTER_NONE : CZSC_FILTER_NON_TREND_DIVERGENCE;
}

static void MarkFirstFilterReasons(std::vector<int> *pReasons,
                                   const std::vector<SegmentPoint> &Points,
                                   const std::vector<Center> &Centers,
                                   const std::vector<TrendStructure> &Structures,
                                   const std::vector<TradingSignalCandidate> &Candidates)
{
  for (std::size_t i = 0; i < Points.size(); i++)
  {
    if ((Points[i].nType == CZSC_POINT_BOTTOM) && !HasCandidateAtPoint(Candidates, (int)i, SIGNAL_FIRST_BUY))
    {
      SetFilterReason(pReasons,
                      Points,
                      (int)i,
                      ClassifyFirstFilterReason(Points, Centers, Structures, i, -1));
    }
    else if ((Points[i].nType == CZSC_POINT_TOP) && !HasCandidateAtPoint(Candidates, (int)i, SIGNAL_FIRST_SELL))
    {
      SetFilterReason(pReasons,
                      Points,
                      (int)i,
                      ClassifyFirstFilterReason(Points, Centers, Structures, i, 1));
    }
  }
}

static void MarkSecondFilterReasons(std::vector<int> *pReasons,
                                    const std::vector<SegmentPoint> &Points,
                                    const std::vector<TradingSignalCandidate> &Candidates)
{
  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &First = Candidates[i];
    if (First.nSource != SIGNAL_SOURCE_FIRST)
    {
      continue;
    }
    if ((First.nPoint < 0) || ((std::size_t)First.nPoint + 2 >= Points.size()))
    {
      continue;
    }

    std::size_t nPoint = (std::size_t)First.nPoint;
    const SegmentPoint &FirstPoint = Points[nPoint];
    const SegmentPoint &Turn = Points[nPoint + 1];
    const SegmentPoint &Second = Points[nPoint + 2];
    bool bValid = false;
    if (First.fSignal == SIGNAL_FIRST_BUY)
    {
      bValid = (Turn.nType == CZSC_POINT_TOP) &&
               (Second.nType == CZSC_POINT_BOTTOM) &&
               (Second.fLow >= FirstPoint.fLow);
    }
    else if (First.fSignal == SIGNAL_FIRST_SELL)
    {
      bValid = (Turn.nType == CZSC_POINT_BOTTOM) &&
               (Second.nType == CZSC_POINT_TOP) &&
               (Second.fHigh <= FirstPoint.fHigh);
    }
    if (!bValid)
    {
      SetFilterReason(pReasons, Points, (int)nPoint + 2, CZSC_FILTER_SECOND_ORDER);
    }
  }
}

static void MarkThirdFilterReasons(std::vector<int> *pReasons,
                                   const std::vector<SegmentPoint> &Points,
                                   const std::vector<CenterBreakout> &Breakouts,
                                   const std::vector<TradingSignalCandidate> &Candidates)
{
  for (std::size_t i = 0; i < Breakouts.size(); i++)
  {
    const CenterBreakout &B = Breakouts[i];
    if ((B.nRetestPoint < 0) || ((std::size_t)B.nRetestPoint >= Points.size()))
    {
      continue;
    }

    float fSignal = (B.nDirection > 0) ? SIGNAL_THIRD_BUY :
                    ((B.nDirection < 0) ? SIGNAL_THIRD_SELL : 0.0f);
    if ((fSignal != 0.0f) && HasCandidateAtPoint(Candidates, B.nRetestPoint, fSignal))
    {
      continue;
    }
    if (!B.bFirstRetest)
    {
      SetFilterReason(pReasons, Points, B.nRetestPoint, CZSC_FILTER_NOT_FIRST_RETEST);
    }
    else if (B.bBackIntoCenter)
    {
      SetFilterReason(pReasons, Points, B.nRetestPoint, CZSC_FILTER_RETEST_BACK_CENTER);
    }
    else if ((B.nDirection == 0) || (B.nCenter < 0))
    {
      SetFilterReason(pReasons, Points, B.nRetestPoint, CZSC_FILTER_DIRECTION_MISMATCH);
    }
  }
}

static void MarkAbcFilterReasons(std::vector<int> *pReasons,
                                 const std::vector<SegmentPoint> &Points,
                                 const std::vector<CenterBreakout> &Breakouts,
                                 const std::vector<TradingSignalCandidate> &Candidates)
{
  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nSource != SIGNAL_SOURCE_FIRST) || (C.nAbcBreakout >= 0))
    {
      continue;
    }

    for (std::size_t j = 0; j < Breakouts.size(); j++)
    {
      const CenterBreakout &B = Breakouts[j];
      if (!B.bFirstRetest || !B.bThirdSignal ||
          (B.nCenter != C.nCenter) ||
          (B.nRetestPoint < 0) || (B.nRetestPoint >= C.nPoint))
      {
        continue;
      }
      if (((C.fSignal == SIGNAL_FIRST_BUY) && (B.nDirection < 0)) ||
          ((C.fSignal == SIGNAL_FIRST_SELL) && (B.nDirection > 0)))
      {
        SetFilterReason(pReasons, Points, C.nPoint, CZSC_FILTER_ABC_NOT_ALIGNED);
        break;
      }
    }
  }
}

std::vector<int> BuildTradingFilterReasons(const std::vector<SegmentPoint> &Points,
                                           const std::vector<Center> &Centers,
                                           const std::vector<TrendStructure> &Structures,
                                           const std::vector<CenterBreakout> &Breakouts,
                                           const std::vector<TradingSignalCandidate> &Candidates)
{
  int nCount = 0;
  for (std::size_t i = 0; i < Points.size(); i++)
  {
    if (Points[i].nIndex + 1 > nCount)
    {
      nCount = Points[i].nIndex + 1;
    }
  }

  std::vector<int> Reasons((std::size_t)nCount, CZSC_FILTER_NONE);
  MarkFirstFilterReasons(&Reasons, Points, Centers, Structures, Candidates);
  MarkSecondFilterReasons(&Reasons, Points, Candidates);
  MarkThirdFilterReasons(&Reasons, Points, Breakouts, Candidates);
  MarkAbcFilterReasons(&Reasons, Points, Breakouts, Candidates);
  return Reasons;
}

void ApplyTradingSignalCandidates(int nCount,
                                  float *pOut,
                                  const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = C.fSignal;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 与 ApplyTradingSignalCandidates 同样按优先级取胜，但导出胜出信号的质量等级
// （0=观察，1=确认，2=强质量）。两者并用即可在图上区分 MACD 动力学确认的强信号。
void ApplyTradingSignalQuality(int nCount,
                               float *pOut,
                               const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)C.nQuality;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

void ApplyTradingFilterReasons(int nCount, float *pOut, const std::vector<int> &Reasons)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  int nSize = (int)Reasons.size();
  if (nSize > nCount)
  {
    nSize = nCount;
  }
  for (int i = 0; i < nSize; i++)
  {
    pOut[i] = (float)Reasons[(std::size_t)i];
  }
}

// 与 ApplyTradingSignalCandidates 同样按优先级取胜，导出胜出信号的背驰-转折分类（第29课）。
// 输出编码：1=最后中枢扩展、2=更大级别盘整、3=反趋势，0=非一类买卖点/无反弹（未知）。
void ApplyTradingSignalReversal(int nCount,
                                float *pOut,
                                const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0;
      bool bFirstSignal = IsFirstSignal(C.fSignal);
      if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_EXTENSION))
      {
        fCode = 1;
      }
      else if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_CONSOLIDATION))
      {
        fCode = 2;
      }
      else if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_TREND))
      {
        fCode = 3;
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 第29课背驰-转折核对端点：输出一类买卖点后首段反弹/回落端点编号，1基。
void ApplyTradingSignalReversalPointId(int nCount,
                                       float *pOut,
                                       const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0.0f;
      if (IsFirstSignal(C.fSignal) &&
          (C.nReversal != CZSC_REVERSAL_UNKNOWN) &&
          (C.nPoint >= 0))
      {
        fCode = (float)(C.nPoint + 2);
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号的三类买卖点后续（第21课）：1=中枢扩张、2=中枢新生，0=其它。
void ApplyTradingSignalAftermath(int nCount,
                                 float *pOut,
                                 const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0;
      bool bThirdSignal = IsThirdSignal(C.fSignal);
      if (bThirdSignal && (C.nAfterEffect == CZSC_CENTER_AFTERMATH_EXTENDED))
      {
        fCode = 1;
      }
      else if (bThirdSignal && (C.nAfterEffect == CZSC_CENTER_AFTERMATH_NEWBORN))
      {
        fCode = 2;
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号相对所属中枢的位置：-1=下方、0=内部、1=上方、2=未知。
void ApplyTradingSignalCenterPosition(int nCount,
                                      float *pOut,
                                      const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)C.nCenterPosition;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号所属走势类型：-1=下跌、0=盘整、1=上涨。
void ApplyTradingSignalMovementType(int nCount,
                                    float *pOut,
                                    const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)C.nMovementType;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号本身的候选优先级：二类10、三类20、一类30。
void ApplyTradingSignalPriority(int nCount,
                                float *pOut,
                                const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)C.nPriority;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号所属中枢的一基编号；0 表示无信号或未知中枢。
void ApplyTradingSignalCenterId(int nCount,
                                float *pOut,
                                const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (C.nCenter >= 0) ? (float)(C.nCenter + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号所属中枢与后续中枢的生命周期关系。
void ApplyTradingSignalCenterLifecycle(int nCount,
                                       float *pOut,
                                       const std::vector<TradingSignalCandidate> &Candidates,
                                       const std::vector<Center> &Centers)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority < Priorities[(std::size_t)C.nIndex])
    {
      continue;
    }

    int nLifecycle = CZSC_CENTER_LIFECYCLE_UNKNOWN;
    if ((C.nCenter >= 0) && ((std::size_t)C.nCenter + 1 < Centers.size()))
    {
      nLifecycle = ClassifyCenterLifecycle(Centers[(std::size_t)C.nCenter],
                                           Centers[(std::size_t)C.nCenter + 1]);
    }
    pOut[C.nIndex] = (float)nLifecycle;
    Priorities[(std::size_t)C.nIndex] = C.nPriority;
  }
}

// 按优先级取胜，导出胜出信号关联中枢突破的一基编号；0 表示无关联突破。
void ApplyTradingSignalBreakoutId(int nCount,
                                  float *pOut,
                                  const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (C.nBreakout >= 0) ? (float)(C.nBreakout + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

static void ApplyTradingSignalBreakoutPointId(int nCount,
                                              float *pOut,
                                              const std::vector<TradingSignalCandidate> &Candidates,
                                              const std::vector<CenterBreakout> &Breakouts,
                                              bool bLeavePoint)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      int nPoint = -1;
      if ((C.nBreakout >= 0) && ((std::size_t)C.nBreakout < Breakouts.size()))
      {
        const CenterBreakout &B = Breakouts[(std::size_t)C.nBreakout];
        nPoint = bLeavePoint ? B.nLeavePoint : B.nRetestPoint;
      }
      pOut[C.nIndex] = (nPoint >= 0) ? (float)(nPoint + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号关联突破的离开端点一基编号；0 表示无关联突破。
void ApplyTradingSignalBreakoutLeavePointId(int nCount,
                                            float *pOut,
                                            const std::vector<TradingSignalCandidate> &Candidates,
                                            const std::vector<CenterBreakout> &Breakouts)
{
  ApplyTradingSignalBreakoutPointId(nCount, pOut, Candidates, Breakouts, true);
}

// 按优先级取胜，导出胜出信号关联突破的回试端点一基编号；0 表示无关联突破。
void ApplyTradingSignalBreakoutRetestPointId(int nCount,
                                             float *pOut,
                                             const std::vector<TradingSignalCandidate> &Candidates,
                                             const std::vector<CenterBreakout> &Breakouts)
{
  ApplyTradingSignalBreakoutPointId(nCount, pOut, Candidates, Breakouts, false);
}

// 按优先级取胜，导出胜出信号对应端点的一基编号；0 表示无信号或未知端点。
void ApplyTradingSignalPointId(int nCount,
                               float *pOut,
                               const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (C.nPoint >= 0) ? (float)(C.nPoint + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号所属走势结构的一基编号；0 表示无走势结构。
void ApplyTradingSignalTrendId(int nCount,
                               float *pOut,
                               const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (C.nTrend >= 0) ? (float)(C.nTrend + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

static bool HasSecondSignalContext(const TradingSignalCandidate &C)
{
  return IsSecondSignal(C.fSignal) &&
         (C.nSource == SIGNAL_SOURCE_SECOND) &&
         (C.nSecondBasePoint >= 0) &&
         (C.nSecondTurnPoint > C.nSecondBasePoint) &&
         (C.nPoint > C.nSecondTurnPoint);
}

static void ApplyTradingSignalSecondContextPointId(int nCount,
                                                   float *pOut,
                                                   const std::vector<TradingSignalCandidate> &Candidates,
                                                   bool bBasePoint)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      int nPoint = -1;
      if (HasSecondSignalContext(C))
      {
        nPoint = bBasePoint ? C.nSecondBasePoint : C.nSecondTurnPoint;
      }
      pOut[C.nIndex] = (nPoint >= 0) ? (float)(nPoint + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 输出第二类买卖点关联的一类端点编号，1基；无二类确认输出0。
void ApplyTradingSignalSecondBasePointId(int nCount,
                                         float *pOut,
                                         const std::vector<TradingSignalCandidate> &Candidates)
{
  ApplyTradingSignalSecondContextPointId(nCount, pOut, Candidates, true);
}

// 输出第二类买卖点中间反向端点编号，1基；无二类确认输出0。
void ApplyTradingSignalSecondTurnPointId(int nCount,
                                         float *pOut,
                                         const std::vector<TradingSignalCandidate> &Candidates)
{
  ApplyTradingSignalSecondContextPointId(nCount, pOut, Candidates, false);
}

static bool HasValidOverlapContext(const TradingSignalCandidate &C)
{
  if (!C.bOverlapped || (C.nBreakout < 0) || (C.nPoint < 0) || (C.nCenter < 0))
  {
    return false;
  }
  if ((C.nSource != SIGNAL_SOURCE_SECOND) && (C.nSource != SIGNAL_SOURCE_THIRD))
  {
    return false;
  }
  return (IsSecondSignal(C.fSignal) || IsThirdSignal(C.fSignal)) &&
         (GetTradingSignalSide(C.fSignal) != 0);
}

// 第44课小转大必要条件：小级别底/顶背驰后，最后中枢出现三买/三卖才可能引发大级别转折。
// 输出 1=底背驰后的三买必要条件成立，-1=顶背驰后的三卖必要条件成立，0=无。
void ApplyTradingSignalSmallTurn(int nCount,
                                 float *pOut,
                                 const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = HasMatchingSmallTurn(C) ? (float)C.nSmallTurn : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 输出第44课小转大必要条件关联的小级别一类买卖点端点编号，1基；无确认输出0。
void ApplyTradingSignalSmallTurnBasePointId(int nCount,
                                            float *pOut,
                                            const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (HasMatchingSmallTurn(C) && (C.nSmallTurnBasePoint >= 0)) ?
                       (float)(C.nSmallTurnBasePoint + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

static void ApplyTradingSignalSmallTurnPointId(int nCount,
                                               float *pOut,
                                               const std::vector<TradingSignalCandidate> &Candidates,
                                               const std::vector<CenterBreakout> &Breakouts,
                                               bool bLeavePoint)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      int nPoint = -1;
      if (HasMatchingSmallTurn(C) && ((std::size_t)C.nBreakout < Breakouts.size()))
      {
        const CenterBreakout &B = Breakouts[(std::size_t)C.nBreakout];
        nPoint = bLeavePoint ? B.nLeavePoint : B.nRetestPoint;
      }
      pOut[C.nIndex] = (nPoint >= 0) ? (float)(nPoint + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 输出第44课小转大必要条件关联三买/三卖突破的离开端点编号，1基；无确认输出0。
void ApplyTradingSignalSmallTurnLeavePointId(int nCount,
                                             float *pOut,
                                             const std::vector<TradingSignalCandidate> &Candidates,
                                             const std::vector<CenterBreakout> &Breakouts)
{
  ApplyTradingSignalSmallTurnPointId(nCount, pOut, Candidates, Breakouts, true);
}

// 输出第44课小转大必要条件关联三买/三卖突破的回试端点编号，1基；无确认输出0。
void ApplyTradingSignalSmallTurnRetestPointId(int nCount,
                                              float *pOut,
                                              const std::vector<TradingSignalCandidate> &Candidates,
                                              const std::vector<CenterBreakout> &Breakouts)
{
  ApplyTradingSignalSmallTurnPointId(nCount, pOut, Candidates, Breakouts, false);
}

// 第37课a+A+b+B+c：一类背驰的c段至少包含对最后中枢B的三买/三卖。
static int GetFirstSignalDirection(const TradingSignalCandidate &C)
{
  if (C.fSignal == SIGNAL_FIRST_BUY)
  {
    return 1;
  }
  if (C.fSignal == SIGNAL_FIRST_SELL)
  {
    return -1;
  }
  return 0;
}

static bool HasMatchingAbcStructure(const TradingSignalCandidate &C)
{
  int nSign = GetFirstSignalDirection(C);
  return (nSign != 0) && (C.nAbcStructure == nSign) && (C.nAbcBreakout >= 0);
}

static bool HasMatchingMacdZeroPullback(const TradingSignalCandidate &C)
{
  int nSign = GetFirstSignalDirection(C);
  return (nSign != 0) && (C.nMacdZeroPullback == nSign);
}

// 输出 1=一买前具备三卖结构，-1=一卖前具备三买结构，0=无。
void ApplyTradingSignalAbcStructure(int nCount,
                                    float *pOut,
                                    const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = HasMatchingAbcStructure(C) ? (float)C.nAbcStructure : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 输出第37课ABC结构里c段所包含的三买/三卖突破编号，1基；无ABC确认输出0。
void ApplyTradingSignalAbcBreakoutId(int nCount,
                                     float *pOut,
                                     const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] =
        (HasMatchingAbcStructure(C) && (C.nAbcBreakout >= 0)) ? (float)(C.nAbcBreakout + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

static void ApplyTradingSignalAbcBreakoutPointId(int nCount,
                                                 float *pOut,
                                                 const std::vector<TradingSignalCandidate> &Candidates,
                                                 const std::vector<CenterBreakout> &Breakouts,
                                                 bool bLeavePoint)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      int nPoint = -1;
      if (HasMatchingAbcStructure(C) && ((std::size_t)C.nAbcBreakout < Breakouts.size()))
      {
        const CenterBreakout &B = Breakouts[(std::size_t)C.nAbcBreakout];
        nPoint = bLeavePoint ? B.nLeavePoint : B.nRetestPoint;
      }
      pOut[C.nIndex] = (nPoint >= 0) ? (float)(nPoint + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 输出第37课ABC结构里c段所含三买/三卖突破的离开端点编号，1基；无ABC确认输出0。
void ApplyTradingSignalAbcBreakoutLeavePointId(int nCount,
                                               float *pOut,
                                               const std::vector<TradingSignalCandidate> &Candidates,
                                               const std::vector<CenterBreakout> &Breakouts)
{
  ApplyTradingSignalAbcBreakoutPointId(nCount, pOut, Candidates, Breakouts, true);
}

// 输出第37课ABC结构里c段所含三买/三卖突破的回试端点编号，1基；无ABC确认输出0。
void ApplyTradingSignalAbcBreakoutRetestPointId(int nCount,
                                                float *pOut,
                                                const std::vector<TradingSignalCandidate> &Candidates,
                                                const std::vector<CenterBreakout> &Breakouts)
{
  ApplyTradingSignalAbcBreakoutPointId(nCount, pOut, Candidates, Breakouts, false);
}

// 与常规买卖点输出相同，但一类买卖点必须满足第37课A-B-C结构标记。
// 二、三类买卖点不按此条件过滤，保持第三类“首次离开+首次回试”的独立判定。
void ApplyTradingSignalStrictAbcCandidates(int nCount,
                                           float *pOut,
                                           const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (IsFirstSignal(C.fSignal) && !HasMatchingAbcStructure(C))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = C.fSignal;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

static bool IsMacdLineWeak(const DivergenceResult &D)
{
  return (D.Previous.fDifHeight > 0) &&
         (D.Previous.fDeaHeight > 0) &&
         (D.Current.fDifHeight > 0) &&
         (D.Current.fDeaHeight > 0) &&
         (D.Current.fDifHeight < D.Previous.fDifHeight) &&
         (D.Current.fDeaHeight <= D.Previous.fDeaHeight);
}

// 第25课 MACD 黄白线辅助：候选信号对应的C段黄白线高度弱于A段时标记。
// 输出 1=买点黄白线走弱，-1=卖点黄白线走弱，0=无。
void ApplyTradingSignalMacdLineWeakness(int nCount,
                                        float *pOut,
                                        const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0;
      int nSide = GetTradingSignalSide(C.fSignal);
      if ((nSide != 0) && IsMacdLineWeak(C.Divergence))
      {
        fCode = (float)nSide;
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 第24/25课 MACD 辅助：B中枢一般会把黄白线回拉到0轴附近。
// 输出 1=一买对应B中枢回拉0轴，-1=一卖对应B中枢回拉0轴，0=无。
void ApplyTradingSignalMacdZeroPullback(int nCount,
                                        float *pOut,
                                        const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = HasMatchingMacdZeroPullback(C) ? (float)C.nMacdZeroPullback : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

static bool IsStandardMacdDivergence(const TradingSignalCandidate &C)
{
  if (C.nSource != SIGNAL_SOURCE_FIRST)
  {
    return false;
  }
  int nSignalDirection = GetFirstSignalDirection(C);
  if ((nSignalDirection == 0) || (C.Divergence.nDirection != -nSignalDirection))
  {
    return false;
  }
  if ((C.nTrend < 0) || (C.nMovementType != -nSignalDirection))
  {
    return false;
  }
  if (!C.Divergence.bDivergence)
  {
    return false;
  }
  if (!HasMatchingAbcStructure(C) || !HasMatchingMacdZeroPullback(C))
  {
    return false;
  }
  if (!C.Divergence.bNewExtreme || !C.Divergence.bWeakMacd)
  {
    return false;
  }
  return IsMacdLineWeak(C.Divergence);
}

// 第24/25/37课标准趋势背驰组合诊断：
// 一类趋势背驰 + C段创新极值 + 柱面积走弱 + 黄白线高度走弱 + B中枢回零 + ABC关联突破。
// 输出 1=标准一买背驰，-1=标准一卖背驰，0=未满足完整组合。
void ApplyTradingSignalStandardDivergence(int nCount,
                                          float *pOut,
                                          const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0;
      if (IsStandardMacdDivergence(C))
      {
        fCode = (C.fSignal == SIGNAL_FIRST_BUY) ? 1.0f : -1.0f;
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 第24课 MACD 柱面积量化：按优先级导出胜出候选 C段/A段 的面积百分比。
// 小于 100 表示 C段柱面积弱于 A段；无有效 A段面积时输出 0。
void ApplyTradingSignalMacdAreaRatio(int nCount,
                                     float *pOut,
                                     const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0.0f;
      if (C.Divergence.Previous.fMacdArea > 0)
      {
        fCode = C.Divergence.Current.fMacdArea / C.Divergence.Previous.fMacdArea * 100.0f;
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 第15课力度量化：按优先级导出胜出候选 C段/A段 的价差力度百分比。
// 小于 100 表示 C段价差力度弱于 A段；无有效 A段价差时输出 0。
void ApplyTradingSignalSpaceRatio(int nCount,
                                  float *pOut,
                                  const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0.0f;
      if (C.Divergence.Previous.fSpace > 0)
      {
        fCode = C.Divergence.Current.fSpace / C.Divergence.Previous.fSpace * 100.0f;
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 第15课趋势平均力度量化：按优先级导出胜出候选 C段/A段 的平均速度百分比。
// 小于 100 表示 C段平均力度弱于 A段；无有效 A段速度时输出 0。
void ApplyTradingSignalSpeedRatio(int nCount,
                                  float *pOut,
                                  const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0.0f;
      if (C.Divergence.Previous.fSpeed > 0)
      {
        fCode = C.Divergence.Current.fSpeed / C.Divergence.Previous.fSpeed * 100.0f;
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

int BuildDivergenceFlags(const DivergenceResult &D)
{
  int nFlags = 0;
  if (D.bNewExtreme)
  {
    nFlags |= CZSC_DIVERGENCE_NEW_EXTREME;
  }
  if (D.bWeakSpace)
  {
    nFlags |= CZSC_DIVERGENCE_WEAK_SPACE;
  }
  if (D.bWeakSpeed)
  {
    nFlags |= CZSC_DIVERGENCE_WEAK_SPEED;
  }
  if (D.bWeakMacd)
  {
    nFlags |= CZSC_DIVERGENCE_WEAK_MACD;
  }
  if (D.bDivergence)
  {
    nFlags |= CZSC_DIVERGENCE_CONFIRMED;
  }
  return nFlags;
}

// 背驰要素诊断位图：创新极值、空间走弱、速度走弱、MACD柱面积走弱、综合成立。
void ApplyTradingSignalDivergenceFlags(int nCount,
                                       float *pOut,
                                       const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)BuildDivergenceFlags(C.Divergence);
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

static int GetDivergencePointId(const DivergenceResult &D, int nWhich)
{
  switch (nWhich)
  {
    case 0: return D.nPreviousStartPoint;
    case 1: return D.nPreviousEndPoint;
    case 2: return D.nCurrentStartPoint;
    case 3: return D.nCurrentEndPoint;
    default: return -1;
  }
}

static void ApplyTradingSignalDivergencePointId(int nCount,
                                                float *pOut,
                                                const std::vector<TradingSignalCandidate> &Candidates,
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      int nPoint = GetDivergencePointId(C.Divergence, nWhich);
      pOut[C.nIndex] = (nPoint >= 0) ? (float)(nPoint + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 输出胜出候选背驰 A 段起点端点编号，1基；无可复核端点输出0。
void ApplyTradingSignalDivergencePreviousStartPointId(int nCount,
                                                      float *pOut,
                                                      const std::vector<TradingSignalCandidate> &Candidates)
{
  ApplyTradingSignalDivergencePointId(nCount, pOut, Candidates, 0);
}

// 输出胜出候选背驰 A 段终点端点编号，1基；无可复核端点输出0。
void ApplyTradingSignalDivergencePreviousEndPointId(int nCount,
                                                    float *pOut,
                                                    const std::vector<TradingSignalCandidate> &Candidates)
{
  ApplyTradingSignalDivergencePointId(nCount, pOut, Candidates, 1);
}

// 输出胜出候选背驰 C 段起点端点编号，1基；无可复核端点输出0。
void ApplyTradingSignalDivergenceCurrentStartPointId(int nCount,
                                                     float *pOut,
                                                     const std::vector<TradingSignalCandidate> &Candidates)
{
  ApplyTradingSignalDivergencePointId(nCount, pOut, Candidates, 2);
}

// 输出胜出候选背驰 C 段终点端点编号，1基；无可复核端点输出0。
void ApplyTradingSignalDivergenceCurrentEndPointId(int nCount,
                                                   float *pOut,
                                                   const std::vector<TradingSignalCandidate> &Candidates)
{
  ApplyTradingSignalDivergencePointId(nCount, pOut, Candidates, 3);
}

int BuildTradingSignalContextFlags(const TradingSignalCandidate &C)
{
  if (!IsTradingSignal(C.fSignal))
  {
    return 0;
  }

  int nFlags = 0;
  if (C.nQuality == CZSC_SIGNAL_QUALITY_STRONG)
  {
    nFlags |= CZSC_SIGNAL_CTX_STRONG_QUALITY;
  }
  if (HasMatchingAbcStructure(C))
  {
    nFlags |= CZSC_SIGNAL_CTX_ABC_STRUCTURE;
  }
  if (HasMatchingMacdZeroPullback(C))
  {
    nFlags |= CZSC_SIGNAL_CTX_MACD_ZERO_PULL;
  }
  if ((GetTradingSignalSide(C.fSignal) != 0) && IsMacdLineWeak(C.Divergence))
  {
    nFlags |= CZSC_SIGNAL_CTX_MACD_LINE_WEAK;
  }
  if (HasMatchingSmallTurn(C))
  {
    nFlags |= CZSC_SIGNAL_CTX_SMALL_TURN;
  }
  if (IsStandardMacdDivergence(C))
  {
    nFlags |= CZSC_SIGNAL_CTX_STANDARD_DIV;
  }
  bool bThirdSignal = IsThirdSignal(C.fSignal);
  if (bThirdSignal && (C.nAfterEffect == CZSC_CENTER_AFTERMATH_NEWBORN))
  {
    nFlags |= CZSC_SIGNAL_CTX_AFTERMATH_NEWBORN;
  }
  else if (bThirdSignal && (C.nAfterEffect == CZSC_CENTER_AFTERMATH_EXTENDED))
  {
    nFlags |= CZSC_SIGNAL_CTX_AFTERMATH_EXTEND;
  }
  bool bFirstSignal = IsFirstSignal(C.fSignal);
  if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_TREND))
  {
    nFlags |= CZSC_SIGNAL_CTX_REVERSAL_TREND;
  }
  else if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_CONSOLIDATION))
  {
    nFlags |= CZSC_SIGNAL_CTX_REVERSAL_CONS;
  }
  else if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_EXTENSION))
  {
    nFlags |= CZSC_SIGNAL_CTX_REVERSAL_EXTEND;
  }
  if (HasValidOverlapContext(C))
  {
    nFlags |= CZSC_SIGNAL_CTX_OVERLAPPED;
  }
  if (C.nBreakout >= 0)
  {
    nFlags |= CZSC_SIGNAL_CTX_CENTER_BREAKOUT;
  }
  return nFlags;
}

int BuildTradingSignalDivergenceSemantic(const TradingSignalCandidate &C)
{
  if (!IsTradingSignal(C.fSignal))
  {
    return CZSC_DIVERGENCE_SEM_NONE;
  }

  if (HasMatchingSmallTurn(C))
  {
    return CZSC_DIVERGENCE_SEM_SMALL_TURN;
  }

  if ((C.nSource == SIGNAL_SOURCE_FIRST) && IsFirstSignal(C.fSignal))
  {
    int nSignalDirection = GetFirstSignalDirection(C);
    if ((nSignalDirection != 0) &&
        (C.nTrend >= 0) &&
        (C.nMovementType == -nSignalDirection) &&
        (C.Divergence.nDirection == -nSignalDirection) &&
        C.Divergence.bNewExtreme &&
        C.Divergence.bDivergence)
    {
      return CZSC_DIVERGENCE_SEM_TREND;
    }
    return CZSC_DIVERGENCE_SEM_NONE;
  }

  if (((C.nSource == SIGNAL_SOURCE_SECOND) || (C.nSource == SIGNAL_SOURCE_THIRD)) &&
      (IsSecondSignal(C.fSignal) || IsThirdSignal(C.fSignal)) &&
      C.Divergence.bDivergence)
  {
    return CZSC_DIVERGENCE_SEM_CONSOLIDATION;
  }

  return CZSC_DIVERGENCE_SEM_NONE;
}

// 胜出买卖点候选上下文位图：把质量、ABC、MACD、小转大、转折/后续等离散诊断压成一列，
// 方便通达信公式用一个 DLL 输出做组合筛选；同柱仍按候选优先级取胜。
void ApplyTradingSignalContextFlags(int nCount,
                                    float *pOut,
                                    const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)BuildTradingSignalContextFlags(C);
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 胜出买卖点候选背驰语义：1=趋势背驰，2=盘整背驰，3=小转大必要条件。
void ApplyTradingSignalDivergenceSemantic(int nCount,
                                          float *pOut,
                                          const std::vector<TradingSignalCandidate> &Candidates)
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

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (!HasTradingSignalOutput(C, nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)BuildTradingSignalDivergenceSemantic(C);
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 形态学第一步：对原始K线做包含处理，合并出无包含关系的合并K线序列（第62/65课）
