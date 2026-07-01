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

static bool TryBuildInitialCenter(const std::vector<SegmentPoint> &Points, std::size_t nStart, Center *pCenter)
{
  if ((pCenter == 0) || (nStart + 3 >= Points.size()))
  {
    return false;
  }

  SegmentInterval First = MakeSegmentInterval(Points[nStart], Points[nStart + 1]);
  SegmentInterval Second = MakeSegmentInterval(Points[nStart + 1], Points[nStart + 2]);
  SegmentInterval Third = MakeSegmentInterval(Points[nStart + 2], Points[nStart + 3]);

  float fLow = First.fLow;
  if (Second.fLow > fLow)
  {
    fLow = Second.fLow;
  }
  if (Third.fLow > fLow)
  {
    fLow = Third.fLow;
  }

  float fHigh = First.fHigh;
  if (Second.fHigh < fHigh)
  {
    fHigh = Second.fHigh;
  }
  if (Third.fHigh < fHigh)
  {
    fHigh = Third.fHigh;
  }

  if (fLow > fHigh)
  {
    return false;
  }

  // 全幅极值 GG/DD（union extent，与 ZG/ZD 的 intersection 相反方向聚合）
  float fTop = First.fHigh;
  if (Second.fHigh > fTop)
  {
    fTop = Second.fHigh;
  }
  if (Third.fHigh > fTop)
  {
    fTop = Third.fHigh;
  }

  float fBottom = First.fLow;
  if (Second.fLow < fBottom)
  {
    fBottom = Second.fLow;
  }
  if (Third.fLow < fBottom)
  {
    fBottom = Third.fLow;
  }

  pCenter->nStart = Points[nStart].nIndex;
  pCenter->nEnd = Points[nStart + 3].nIndex;
  pCenter->fHigh = fHigh;
  pCenter->fLow = fLow;
  pCenter->fTop = fTop;
  pCenter->fBottom = fBottom;
  pCenter->nDirection = 0;  // 方向由 BuildCenters 按进入段设定
  return true;
}

// 若新一段与中枢重叠则延伸：ZG/ZD 随重叠收缩、GG/DD 随全幅扩张、终点后移；否则中枢结束。
// 第20课中心定理一按 [dn, gn] 与 [ZD, ZG] 是否重叠判定延伸；穿越整个区间仍属于重叠。
static bool ExtendCenter(Center *pCenter, const SegmentInterval &Interval)
{
  if ((pCenter == 0) ||
      !IntervalsOverlap(pCenter->fLow, pCenter->fHigh, Interval.fLow, Interval.fHigh))
  {
    return false;
  }

  if (Interval.fLow > pCenter->fLow)
  {
    pCenter->fLow = Interval.fLow;
  }
  if (Interval.fHigh < pCenter->fHigh)
  {
    pCenter->fHigh = Interval.fHigh;
  }

  // ZG/ZD 随延伸收缩，GG/DD 随延伸扩张
  if (Interval.fHigh > pCenter->fTop)
  {
    pCenter->fTop = Interval.fHigh;
  }
  if (Interval.fLow < pCenter->fBottom)
  {
    pCenter->fBottom = Interval.fLow;
  }

  pCenter->nEnd = Interval.nEnd;
  return true;
}

static bool IsCenterLeaveAttempt(const Center &C,
                                 const SegmentPoint &Start,
                                 const SegmentPoint &End,
                                 int *pDirection)
{
  float fStart = GetPointPrice(Start);
  float fEnd = GetPointPrice(End);
  int nDirection = (fEnd > fStart) ? 1 : ((fEnd < fStart) ? -1 : 0);
  if (pDirection != 0)
  {
    *pDirection = nDirection;
  }

  if ((nDirection > 0) && (End.nType == CZSC_POINT_TOP) && (End.fHigh > C.fHigh))
  {
    return true;
  }
  if ((nDirection < 0) && (End.nType == CZSC_POINT_BOTTOM) && (End.fLow < C.fLow))
  {
    return true;
  }
  return false;
}

static bool RetestBackIntoCenter(const Center &C, const SegmentPoint &Retest, int nDirection)
{
  if (nDirection > 0)
  {
    return Retest.fLow < C.fHigh;
  }
  if (nDirection < 0)
  {
    return Retest.fHigh > C.fLow;
  }
  return false;
}

// 走势中枢中心定理二（第20课）：用全幅极值 GG/DD 判定前后两个同级别中枢的关系。
// 后DD > 前GG → 上涨延续；后GG < 前DD → 下跌延续；其余（全幅重叠）→ 形成高级别中枢（扩展）。
int ClassifyCenterRelation(const Center &Prev, const Center &Next)
{
  if (Next.fBottom > Prev.fTop)
  {
    return CZSC_CENTER_RELATION_UP;
  }
  if (Next.fTop < Prev.fBottom)
  {
    return CZSC_CENTER_RELATION_DOWN;
  }
  return CZSC_CENTER_RELATION_EXTENSION;
}

// 中枢生命周期上下文（第18/20课）：
// [ZD,ZG] 重叠 → 同级延伸；核心区间不重叠但 GG/DD 全幅重叠 → 高级别扩展；
// GG/DD 全幅不重叠 → 同向趋势中新中枢新生。
int ClassifyCenterLifecycle(const Center &Prev, const Center &Next)
{
  if (IntervalsOverlap(Prev.fLow, Prev.fHigh, Next.fLow, Next.fHigh))
  {
    return CZSC_CENTER_LIFECYCLE_EXTENSION;
  }

  int nRelation = ClassifyCenterRelation(Prev, Next);
  if (nRelation == CZSC_CENTER_RELATION_EXTENSION)
  {
    return CZSC_CENTER_LIFECYCLE_EXPANSION;
  }
  if (nRelation == CZSC_CENTER_RELATION_UP)
  {
    return CZSC_CENTER_LIFECYCLE_NEWBORN_UP;
  }
  if (nRelation == CZSC_CENTER_RELATION_DOWN)
  {
    return CZSC_CENTER_LIFECYCLE_NEWBORN_DOWN;
  }
  return CZSC_CENTER_LIFECYCLE_UNKNOWN;
}

// 三类买卖点后续（第21课/第53课）：离开中枢后若与后一个中枢形成高级别中枢则为「中枢扩张」，
// 若形成同向新中枢则为「中枢新生（趋势）」。后续中枢尚未形成或方向相反则未知。
int ClassifyCenterAftermath(const std::vector<Center> &Centers, int nCenter, float fSignal)
{
  if ((nCenter < 0) || ((std::size_t)nCenter + 1 >= Centers.size()))
  {
    return CZSC_CENTER_AFTERMATH_UNKNOWN;
  }
  if (!IsThirdSignal(fSignal))
  {
    return CZSC_CENTER_AFTERMATH_UNKNOWN;
  }

  int nRelation = ClassifyCenterRelation(Centers[(std::size_t)nCenter],
                                         Centers[(std::size_t)nCenter + 1]);
  if (nRelation == CZSC_CENTER_RELATION_EXTENSION)
  {
    return CZSC_CENTER_AFTERMATH_EXTENDED;
  }
  if ((fSignal == SIGNAL_THIRD_BUY) && (nRelation == CZSC_CENTER_RELATION_UP))
  {
    return CZSC_CENTER_AFTERMATH_NEWBORN;
  }
  if ((fSignal == SIGNAL_THIRD_SELL) && (nRelation == CZSC_CENTER_RELATION_DOWN))
  {
    return CZSC_CENTER_AFTERMATH_NEWBORN;
  }
  return CZSC_CENTER_AFTERMATH_UNKNOWN;
}

static TrendStructure MakeTrendStructure(const std::vector<Center> &Centers,
                                         int nType,
                                         std::size_t nFirst,
                                         std::size_t nLast)
{
  TrendStructure T;
  T.nType = nType;
  T.nStart = Centers[nFirst].nStart;
  T.nEnd = Centers[nLast].nEnd;
  T.nFirstCenter = (int)nFirst;
  T.nLastCenter = (int)nLast;
  return T;
}

static int CenterRelationToMovement(int nRelation)
{
  if (nRelation == CZSC_CENTER_RELATION_UP)
  {
    return CZSC_MOVEMENT_UP;
  }
  if (nRelation == CZSC_CENTER_RELATION_DOWN)
  {
    return CZSC_MOVEMENT_DOWN;
  }
  return CZSC_MOVEMENT_CONSOLIDATION;
}

// 把中枢序列归并为走势类型：连续同向（同级中枢全幅不重叠）的中枢合并为一段趋势，
// 单个或不同向的归为盘整（第17/18课：趋势含两个以上依次同向中枢）
std::vector<TrendStructure> BuildTrendStructures(const std::vector<Center> &Centers)
{
  std::vector<TrendStructure> Structures;
  std::size_t i = 0;
  while (i < Centers.size())
  {
    if (i + 1 >= Centers.size())
    {
      Structures.push_back(MakeTrendStructure(Centers, CZSC_MOVEMENT_CONSOLIDATION, i, i));
      break;
    }

    int nType = CenterRelationToMovement(ClassifyCenterRelation(Centers[i], Centers[i + 1]));

    if (nType == CZSC_MOVEMENT_CONSOLIDATION)
    {
      Structures.push_back(MakeTrendStructure(Centers, CZSC_MOVEMENT_CONSOLIDATION, i, i));
      i++;
      continue;
    }

    std::size_t nLast = i + 1;
    while (nLast + 1 < Centers.size())
    {
      int nNextType = CenterRelationToMovement(ClassifyCenterRelation(Centers[nLast], Centers[nLast + 1]));
      if (nNextType == nType)
      {
        nLast++;
        continue;
      }
      break;
    }

    Structures.push_back(MakeTrendStructure(Centers, nType, i, nLast));
    i = nLast + 1;
  }

  return Structures;
}
// 扫描端点序列构造同级中枢（第17/20课）：连续三段走势有重叠即形成一个候选中枢。
// 中枢成形后以 ExtendCenter 吸收后续与 ZG/ZD 重叠的段来延伸（第20课中枢延伸）。
// 每个完成中枢都输出；相邻中枢的上涨/下跌/扩展关系由 ClassifyCenterRelation 单独标注。
// 方向由进入段定：前一点为底则进入段向上，前一点为顶则进入段向下。
std::vector<Center> BuildCenters(const std::vector<SegmentPoint> &Points)
{
  std::vector<Center> Centers;
  if (Points.size() < 4)
  {
    return Centers;
  }

  std::size_t i = 1;
  while (i + 3 < Points.size())
  {
    Center C;
    if (!TryBuildInitialCenter(Points, i, &C))
    {
      i++;
      continue;
    }
    C.nDirection = (Points[i - 1].nType == CZSC_POINT_BOTTOM) ? 1 : -1;

    // 中枢延伸：后续段与 ZG/ZD 重叠则吸收；若离开段+回试段构成三买卖则立即封死（第20课）
    std::size_t nExtend = i + 3;
    while (nExtend + 1 < Points.size())
    {
      SegmentInterval Interval = MakeSegmentInterval(Points[nExtend], Points[nExtend + 1]);

      // 先检查是否与 ZG/ZD 有重叠 → 正常延伸；穿越整个区间也按重叠吸收。
      if (IntervalsOverlap(C.fLow, C.fHigh, Interval.fLow, Interval.fHigh))
      {
        if (!ExtendCenter(&C, Interval))
        {
          break;
        }
        nExtend++;
        continue;
      }

      // 无重叠 → 检测是否为离开段，前探回试是否构成三买卖
      int nLeaveDir = 0;
      if (IsCenterLeaveAttempt(C, Points[nExtend], Points[nExtend + 1], &nLeaveDir))
      {
        if (nExtend + 2 < Points.size())
        {
          int nRetestMove = GetMoveDirection(Points[nExtend + 1], Points[nExtend + 2]);
          if ((nRetestMove != 0) && (nRetestMove != nLeaveDir))
          {
            if (!RetestBackIntoCenter(C, Points[nExtend + 2], nLeaveDir))
            {
              break;  // 离开+回试不回 → 三买卖点，封死中枢
            }
            // 离开+回试回中枢 → 中枢延伸：吸收离开段与回试段
            ExtendCenter(&C, Interval);
            nExtend++;
            SegmentInterval RetestInterval = MakeSegmentInterval(Points[nExtend], Points[nExtend + 1]);
            ExtendCenter(&C, RetestInterval);
            nExtend++;
            continue;
          }
        }
      }

      break;  // 无重叠且非延伸型离开 → 中枢结束
    }

    Centers.push_back(C);
    i = nExtend + 1;
  }

  return Centers;
}

// 在每个中枢的时间跨度内写出其上沿 ZG（fHigh），通达信据此画中枢上边
void WriteCenterHighSignal(int nCount, float *pOut, const std::vector<Center> &Centers)
{
  ClearOutput(nCount, pOut);
  for (std::size_t i = 0; i < Centers.size(); i++)
  {
    int nStart = (Centers[i].nStart < 0) ? 0 : Centers[i].nStart;
    int nEnd = (Centers[i].nEnd >= nCount) ? (nCount - 1) : Centers[i].nEnd;
    for (int j = nStart; j <= nEnd; j++)
    {
      pOut[j] = Centers[i].fHigh;
    }
  }
}

// 在每个中枢的时间跨度内写出其下沿 ZD（fLow），通达信据此画中枢下边
void WriteCenterLowSignal(int nCount, float *pOut, const std::vector<Center> &Centers)
{
  ClearOutput(nCount, pOut);
  for (std::size_t i = 0; i < Centers.size(); i++)
  {
    int nStart = (Centers[i].nStart < 0) ? 0 : Centers[i].nStart;
    int nEnd = (Centers[i].nEnd >= nCount) ? (nCount - 1) : Centers[i].nEnd;
    for (int j = nStart; j <= nEnd; j++)
    {
      pOut[j] = Centers[i].fLow;
    }
  }
}

// 在每个中枢的起点标 1、终点标 2，通达信据此标注中枢起止
void WriteCenterMarkSignal(int nCount, float *pOut, const std::vector<Center> &Centers)
{
  ClearOutput(nCount, pOut);
  for (std::size_t i = 0; i < Centers.size(); i++)
  {
    int nStart = Centers[i].nStart;
    int nEnd = Centers[i].nEnd;
    if ((nStart >= 0) && (nStart < nCount))
    {
      pOut[nStart] = 1;
    }
    if ((nEnd >= 0) && (nEnd < nCount))
    {
      pOut[nEnd] = 2;
    }
  }
}

// 相邻中枢关系（第20课中心定理二）：在后中枢起点处标记
// 2=中枢扩展（形成高级别中枢）、1=上涨延续、-1=下跌延续。
void WriteCenterRelationSignal(int nCount, float *pOut, const std::vector<Center> &Centers)
{
  ClearOutput(nCount, pOut);
  for (std::size_t i = 1; i < Centers.size(); i++)
  {
    int nMark = Centers[i].nStart;
    if ((nMark < 0) || (nMark >= nCount))
    {
      continue;
    }

    int nRelation = ClassifyCenterRelation(Centers[i - 1], Centers[i]);
    if (nRelation == CZSC_CENTER_RELATION_EXTENSION)
    {
      pOut[nMark] = 2;
    }
    else if (nRelation == CZSC_CENTER_RELATION_UP)
    {
      pOut[nMark] = 1;
    }
    else
    {
      pOut[nMark] = -1;
    }
  }
}

// 相邻中枢生命周期（第18/20课）：在后中枢起点处标记。
void WriteCenterLifecycleSignal(int nCount, float *pOut, const std::vector<Center> &Centers)
{
  ClearOutput(nCount, pOut);
  for (std::size_t i = 1; i < Centers.size(); i++)
  {
    int nMark = Centers[i].nStart;
    if ((nMark < 0) || (nMark >= nCount))
    {
      continue;
    }

    pOut[nMark] = (float)ClassifyCenterLifecycle(Centers[i - 1], Centers[i]);
  }
}

