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
#include <cmath>

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

// 若新一段与中枢重叠则延伸：GG/DD 随全幅扩张、终点后移；否则中枢结束。
// 第20课：ZG=min(g1,g2)、ZD=max(d1,d2) 只由成枢的前两个 Zn 决定，延伸不改变 [ZD,ZG]；
// 中心定理一按 [dn, gn] 与 [ZD, ZG] 是否重叠判定延伸；穿越整个区间仍属于重叠。
static bool ExtendCenter(Center *pCenter, const SegmentInterval &Interval)
{
  if ((pCenter == 0) ||
      !IntervalsOverlap(pCenter->fLow, pCenter->fHigh, Interval.fLow, Interval.fHigh))
  {
    return false;
  }

  // [ZD,ZG] 固定，GG/DD 随延伸扩张
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
    bool bLeftByPrevious = false;
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

      bLeftByPrevious = true;  // 无重叠且非延伸型离开 → 中枢结束
      break;
    }

    // 第18课中枢定理三：前一段离开后，本段回抽不回 [ZD,ZG] → 中枢破坏。离开段是连接中枢的次级别走势
    // （中枢定理一），不属于本中枢：退回中枢终点并重算 GG/DD，离开段作为下一中枢的进入段。
    if (bLeftByPrevious && (nExtend > i + 3))
    {
      C.nEnd = Points[nExtend - 1].nIndex;
      C.fTop = GetPointPrice(Points[i]);
      C.fBottom = C.fTop;
      for (std::size_t k = i + 1; k < nExtend; k++)
      {
        float fPrice = GetPointPrice(Points[k]);
        if (fPrice > C.fTop) C.fTop = fPrice;
        if (fPrice < C.fBottom) C.fBottom = fPrice;
      }
      Centers.push_back(C);
      i = nExtend;
      continue;
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


// C3：第17课末端定义/递归定义；第33课结合律的具名分解之一，不声称唯一。
// leftmost-core-first-departure-v1：从左至右取首个三单位交集；交集固定，
// 重叠单位延伸末端。离开后至少再有一个同侧、不触及闭区间的完整单位，
// 才有当前分解的“不再返回”证据。首次离开端点连接下一走势，确认端点不回填。
// 完成后的返回属于后继；这不是对未来永不返回的预言。
namespace
{
struct RecursiveUnit
{
  int nStart;
  int nEnd;
  int nAvailable;
  int nChild;
  float fHigh;
  float fLow;
};

bool ValidAnchorDate(int nDate)
{
  int y = nDate / 10000, m = (nDate / 100) % 100, d = nDate % 100;
  if (y < 1900 || y > 2199 || m < 1 || m > 12) return false;
  const int Days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int nDays = Days[m - 1];
  if (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) nDays++;
  return d >= 1 && d <= nDays;
}

int OutsideSide(float fLow, float fHigh, float fCenterLow, float fCenterHigh)
{
  return fLow > fCenterHigh ? 1 : (fHigh < fCenterLow ? -1 : 0);
}

std::vector<int> BuildSubTrendLevel(const std::vector<RecursiveUnit> &Units,
                                    int nLevel, SubTrendHierarchy *pHierarchy,
                                    const float *pHigh, const float *pLow)
{
  std::vector<int> Completed;
  int nCursor = 0, nPrevious = -1, nConnection = -1;
  const int nSize = static_cast<int>(Units.size());
  while (nCursor + 2 < nSize)
  {
    int nCore = nCursor;
    float fLow = 0, fHigh = 0;
    for (; nCore + 2 < nSize; nCore++)
    {
      fLow = std::max(Units[nCore].fLow, std::max(Units[nCore+1].fLow, Units[nCore+2].fLow));
      fHigh = std::min(Units[nCore].fHigh, std::min(Units[nCore+1].fHigh, Units[nCore+2].fHigh));
      if (fLow <= fHigh) break; // 闭区间，接触也算重叠
    }
    if (nCore + 2 >= nSize) break;
    SubTrendNode Node = {};
    Node.nLevel = nLevel;
    Node.nStart = nConnection >= 0 ? nConnection : Units[nCursor].nStart;
    Node.nCenterStart = Units[nCore].nStart;
    Node.nCenterEnd = Units[nCore+2].nEnd;
    Node.nEstablishedAt = Units[nCore+2].nAvailable;
    Node.nConnection = Node.nCompletedAt = Node.nSuccessor = -1;
    Node.fCenterHigh = fHigh;
    Node.fCenterLow = fLow;
    int nLeave = -1, nSide = 0, nLast = nSize - 1;
    for (int j = nCore + 3; j < nSize; j++)
    {
      int nNow = OutsideSide(Units[j].fLow, Units[j].fHigh, fLow, fHigh);
      if (nNow == 0)
      {
        Node.nCenterEnd = Units[j].nEnd;
        nLeave = -1;
        nSide = 0;
      }
      else if (nLeave >= 0 && nSide == nNow)
      {
        Node.nConnection = Units[nLeave].nEnd;
        Node.nCompletedAt = std::max(Node.nEstablishedAt, Units[j].nAvailable);
        nLast = nLeave;
        break;
      }
      else
      {
        // 跨越到另一侧意味着价格段穿越中枢，不能当作同侧不返回。
        nLeave = j;
        nSide = nNow;
      }
    }
    Node.nEnd = Units[nLast].nEnd;
    Node.fHigh = Units[nCursor].fHigh;
    Node.fLow = Units[nCursor].fLow;
    if (nCursor > 0 && nConnection >= 0)
    {
      // 共享连接K线的价格包络也属于下一走势，不能丢掉时间起点的价格。
      Node.fHigh = std::max(Node.fHigh, pHigh[nConnection]);
      Node.fLow = std::min(Node.fLow, pLow[nConnection]);
    }
    for (int j = nCursor; j <= nLast; j++)
    {
      Node.fHigh = std::max(Node.fHigh, Units[j].fHigh);
      Node.fLow = std::min(Node.fLow, Units[j].fLow);
      if (Units[j].nChild >= 0) Node.Children.push_back(Units[j].nChild);
    }
    int nNode = static_cast<int>(pHierarchy->Nodes.size());
    pHierarchy->Nodes.push_back(Node);
    if (nPrevious >= 0) pHierarchy->Nodes[nPrevious].nSuccessor = nNode;
    if (Node.nCompletedAt < 0) break;
    Completed.push_back(nNode);
    nPrevious = nNode;
    nConnection = Node.nConnection;
    nCursor = nLast + 1; // 子走势不重复归属；相邻父走势只共享连接端点
  }
  return Completed;
}
}

SubTrendHierarchy BuildSubTrendHierarchy(int nCount, const float *pHigh, const float *pLow,
                                         const TrendAnchorContract &Contract)
{
  SubTrendHierarchy Result;
  Result.nAnchorVersion = Contract.nVersion;
  if (nCount < 0 || (Contract.nVersion != 1 && Contract.nVersion != 2 && Contract.nVersion != 3) ||
      Contract.Dates.size() != static_cast<std::size_t>(nCount) ||
      (nCount > 0 && (!pHigh || !pLow))) return Result;
  std::vector<RecursiveUnit> Units;
  for (int i = 0; i < nCount; i++)
  {
    int nDate = Contract.Dates[i];
    if (!ValidAnchorDate(nDate) ||
        (i > 0 && (nDate < Contract.Dates[i-1] ||
                    (Contract.nVersion == 1 && nDate == Contract.Dates[i-1]) ||
                    (Contract.nVersion == 3 && nDate/100 <= Contract.Dates[i-1]/100))) ||
        (Contract.nVersion == 2 && (nDate < 20000104 || nDate > 20221130)) ||
        !std::isfinite(pHigh[i]) || !std::isfinite(pLow[i]) ||
        pLow[i] <= 0 || pHigh[i] < pLow[i]) return Result;
    RecursiveUnit U = {i, i, i, -1, pHigh[i], pLow[i]};
    Units.push_back(U);
  }
  Result.bAvailable = true;
  // 每个父走势至少消耗三子走势；单位数严格减少，无任意级别上限。
  for (int nLevel = 0; Units.size() >= 3; nLevel++)
  {
    std::vector<int> Completed = BuildSubTrendLevel(Units, nLevel, &Result, pHigh, pLow);
    std::vector<RecursiveUnit> Next;
    for (std::size_t i = 0; i < Completed.size(); i++)
    {
      const SubTrendNode &N = Result.Nodes[Completed[i]];
      RecursiveUnit U = {N.nStart, N.nEnd, N.nCompletedAt, Completed[i], N.fHigh, N.fLow};
      Next.push_back(U);
    }
    Units.swap(Next);
  }
  return Result;
}

// 第17/18/20课：先按完整波动区间严格分离判同向，接触/重叠不是趋势。
// 第33课 maximal-same-direction-centers-v1：当前前缀从左向右最大同向分组。
// 各层新中枢仅消费上一层完成的分组走势；不能把三颗旧单枢节点直接当三趋势。
RecursiveMovementHierarchy BuildRecursiveMovements(int nCount, const float *pHigh,
  const float *pLow, const TrendAnchorContract &Contract)
{
  RecursiveMovementHierarchy R;
  R.nAnchorVersion = Contract.nVersion;
  // 复用输入契约检查；不修改 C3 的 Nodes 或完成表。
  if (!BuildSubTrendHierarchy(nCount, pHigh, pLow, Contract).bAvailable) return R;
  R.bAvailable = true;
  std::vector<RecursiveUnit> Units;
  for (int i = 0; i < nCount; i++)
    Units.push_back(RecursiveUnit{i,i,i,-1,pHigh[i],pLow[i]});
  for (int level = 0; Units.size() >= 3; level++)
  {
    SubTrendHierarchy Layer;
    BuildSubTrendLevel(Units, level, &Layer, pHigh, pLow);
    if (Layer.Nodes.empty()) break;
    const int base = static_cast<int>(R.Centers.size());
    for (std::size_t i = 0; i < Layer.Nodes.size(); i++)
    {
      SubTrendNode N = Layer.Nodes[i];
      if (N.nSuccessor >= 0) N.nSuccessor += base;
      R.Centers.push_back(N);
    }
    std::vector<int> Groups;
    for (std::size_t i = 0; i < Layer.Nodes.size(); i++)
    {
      const SubTrendNode &N = Layer.Nodes[i];
      int direction = 0;
      RecursiveConnection Link = {};
      bool connected = false;
      if (i > 0)
      {
        const SubTrendNode &P = Layer.Nodes[i-1];
        float ph = pHigh[P.nCenterStart], pl = pLow[P.nCenterStart];
        float nh = pHigh[N.nCenterStart], nl = pLow[N.nCenterStart];
        for (int j = P.nCenterStart; j <= P.nCenterEnd; j++)
        { ph = std::max(ph,pHigh[j]); pl = std::min(pl,pLow[j]); }
        for (int j = N.nCenterStart; j <= N.nCenterEnd; j++)
        { nh = std::max(nh,pHigh[j]); nl = std::min(nl,pLow[j]); }
        direction = nl > ph ? 1 : (nh < pl ? -1 : 0);
        Link.nLeftCenter = base + static_cast<int>(i)-1;
        Link.nRightCenter = base + static_cast<int>(i);
        Link.nLevel = level-1;
        Link.nStart = P.nCenterEnd;
        Link.nEnd = N.nCenterStart;
        Link.nRequiredAt = 0;
        Link.nMemberSpace = level == 0 ? 0 : 1;
        int covered = Link.nStart;
        bool began = false;
        for (std::size_t j = 0; j < Units.size(); j++)
        {
          const RecursiveUnit &U = Units[j];
          if (U.nEnd < Link.nStart || U.nStart > Link.nEnd) continue;
          if ((!began && U.nStart > Link.nStart) ||
              (began && U.nStart > covered + (level == 0 ? 1 : 0))) break;
          began = true;
          Link.Members.push_back(level == 0 ? U.nStart : U.nChild);
          covered = std::max(covered,U.nEnd);
          Link.nRequiredAt = std::max(Link.nRequiredAt,U.nAvailable);
          if (covered >= Link.nEnd) break;
        }
        connected = began && Link.nEnd > Link.nStart && covered >= Link.nEnd;
      }
      bool append = !Groups.empty() && direction != 0 && connected;
      if (append)
      {
        const RecursiveMovement &P = R.Movements[Groups.back()];
        append = P.nType == 0 || P.nType == direction;
      }
      if (!append)
      {
        RecursiveMovement M = {};
        M.nLevel = level; M.nType = 0; M.nStart = N.nStart;
        M.nEstablishedAt = N.nEstablishedAt; M.nSuccessor = -1;
        M.fHigh = N.fHigh; M.fLow = N.fLow;
        int id = static_cast<int>(R.Movements.size());
        if (!Groups.empty()) R.Movements[Groups.back()].nSuccessor = id;
        R.Movements.push_back(M); Groups.push_back(id);
      }
      RecursiveMovement &M = R.Movements[Groups.back()];
      if (append)
      {
        M.nType = direction;
        M.Connections.push_back(static_cast<int>(R.Connections.size()));
        R.Connections.push_back(Link);
        M.nEstablishedAt = std::max(M.nEstablishedAt,Link.nRequiredAt);
      }
      M.Centers.push_back(base + static_cast<int>(i));
      M.nEnd = N.nEnd;
      M.nEstablishedAt = std::max(M.nEstablishedAt,N.nEstablishedAt);
      M.nCompletedAt = N.nCompletedAt < 0 ? -1 : std::max(M.nEstablishedAt,N.nCompletedAt);
      M.fHigh = std::max(M.fHigh,N.fHigh); M.fLow = std::min(M.fLow,N.fLow);
    }
    std::vector<RecursiveUnit> Next;
    for (std::size_t i = 0; i < Groups.size(); i++)
    {
      const RecursiveMovement &M = R.Movements[Groups[i]];
      if (M.nCompletedAt >= 0)
        Next.push_back(RecursiveUnit{M.nStart,M.nEnd,M.nCompletedAt,Groups[i],M.fHigh,M.fLow});
    }
    Units.swap(Next); // 每枢至少三低级单位；规模严格缩小
  }
  return R;
}

std::vector<StructureAssociation> BuildStructureAssociations(
  const std::vector<Center> &Centers, const std::vector<TrendStructure> &Structures,
  const std::vector<TrendCompletionEvidence> &Evidence,
  const RecursiveMovementHierarchy &H, const float *pHigh, const float *pLow)
{
  std::vector<StructureAssociation> Result;
  if (!H.bAvailable || !pHigh || !pLow) return Result;
  for (std::size_t t = 0; t < Structures.size(); t++)
  {
    StructureAssociation A = {};
    A.nStructure = static_cast<int>(t);
    A.nMovement = A.nCompletion = A.nLevel = -1;
    for (std::size_t e = 0; e < Evidence.size(); e++)
      if (Evidence[e].nTrendSpace == 0 && Evidence[e].nTrend == A.nStructure)
        A.nCompletion = static_cast<int>(e);
    const TrendStructure &T = Structures[t];
    if (T.nFirstCenter >= 0 && T.nLastCenter >= T.nFirstCenter &&
        static_cast<std::size_t>(T.nLastCenter) < Centers.size() &&
        T.nStart == Centers[T.nFirstCenter].nStart && T.nEnd == Centers[T.nLastCenter].nEnd)
    for (std::size_t m = 0; m < H.Movements.size(); m++)
    {
      const RecursiveMovement &M = H.Movements[m];
      if (M.nType != T.nType || M.Centers.size() !=
          static_cast<std::size_t>(T.nLastCenter-T.nFirstCenter+1)) continue;
      bool exact = true;
      for (std::size_t c = 0; exact && c < M.Centers.size(); c++)
      {
        const Center &Old = Centers[T.nFirstCenter+c];
        const SubTrendNode &New = H.Centers[M.Centers[c]];
        float high = pHigh[New.nCenterStart], low = pLow[New.nCenterStart];
        for (int k = New.nCenterStart; k <= New.nCenterEnd; k++)
        { high = std::max(high,pHigh[k]); low = std::min(low,pLow[k]); }
        exact = Old.nStart == New.nCenterStart && Old.nEnd == New.nCenterEnd &&
                Old.fHigh == New.fCenterHigh && Old.fLow == New.fCenterLow &&
                Old.fTop == high && Old.fBottom == low;
      }
      // 旧完成证据存在时还需端点、证据时点相等，不能借映射升级另一段走势。
      if (exact && A.nCompletion >= 0)
      {
        const TrendCompletionEvidence &E = Evidence[A.nCompletion];
        exact = E.nConnectionBar == M.nEnd && E.nLatestBar == M.nCompletedAt;
      }
      if (!exact) continue;
      if (A.nStatus == 1)
      { A.nStatus = 2; A.nMovement = A.nLevel = -1; A.Centers.clear(); break; }
      A.nStatus = 1; A.nMovement = static_cast<int>(m); A.nLevel = M.nLevel;
      A.Centers = M.Centers;
    }
    Result.push_back(A);
  }
  return Result;
}

std::vector<TrendCompletionEvidence> BuildTrendCompletionEvidence(
  const std::vector<SegmentPoint> &Points, const std::vector<Center> &Centers,
  const std::vector<TrendStructure> &Structures, const SubTrendHierarchy &Hierarchy,
  int nCount)
{
  std::vector<TrendCompletionEvidence> Result;
  // 旧构件走势仅给构件完成证据，理论级别保持未知；不强行关联递归树。
  for (std::size_t i = 0; i < Structures.size(); i++)
  {
    const TrendStructure &T = Structures[i];
    if (T.nFirstCenter < 0 || T.nLastCenter < T.nFirstCenter ||
        static_cast<std::size_t>(T.nLastCenter) >= Centers.size()) continue;
    const Center &C = Centers[T.nLastCenter];
    int nBoundary = nCount;
    if (i + 1 < Structures.size()) nBoundary = Structures[i+1].nStart;
    for (std::size_t j = 1; j + 1 < Points.size(); j++)
    {
      const SegmentPoint &P = Points[j], &Q = Points[j+1];
      if (P.nIndex <= C.nEnd || P.nIndex > nBoundary || Q.nIndex >= nCount ||
          Q.nIndex <= P.nIndex || Points[j-1].nIndex > C.nEnd) continue;
      int nSide = OutsideSide(P.fLow, P.fHigh, C.fLow, C.fHigh);
      if (nSide == 0 || OutsideSide(Q.fLow, Q.fHigh, C.fLow, C.fHigh) != nSide) continue;
      TrendCompletionEvidence E = {};
      E.nTrend = static_cast<int>(i);
      E.nTrendSpace = 0;
      E.nConnectionPoint = static_cast<int>(j);
      E.nConnectionBar = P.nIndex;
      E.nReason = 1;
      E.nLatestPoint = static_cast<int>(j+1);
      E.nLatestBar = Q.nIndex;
      E.nObservedAt = nCount - 1;
      E.nSuccessor = E.nSuccessorEstablishedAt = -1;
      E.nTheoreticalLevel = -1;
      E.nAnchorVersion = 0;
      E.nDecompositionRule = 1;
      if (i + 1 < Structures.size())
      {
        int nFirst = Structures[i+1].nFirstCenter;
        if (nFirst >= 0 && static_cast<std::size_t>(nFirst) < Centers.size() &&
            Centers[nFirst].nEnd > E.nLatestBar && Centers[nFirst].nEnd < nCount)
        {
          E.nSuccessor = static_cast<int>(i+1);
          E.nSuccessorEstablishedAt = Centers[nFirst].nEnd;
        }
      }
      Result.push_back(E);
      break;
    }
  }
  if (!Hierarchy.bAvailable) return Result;
  for (std::size_t i = 0; i < Hierarchy.Nodes.size(); i++)
  {
    const SubTrendNode &N = Hierarchy.Nodes[i];
    if (N.nCompletedAt < 0) continue;
    TrendCompletionEvidence E = {};
    E.nTrend = static_cast<int>(i);
    E.nTrendSpace = 1;
    E.nConnectionPoint = E.nLatestPoint = -1;
    E.nConnectionBar = N.nConnection;
    E.nReason = 1;
    E.nLatestBar = N.nCompletedAt;
    E.nObservedAt = nCount - 1;
    E.nSuccessor = N.nSuccessor;
    E.nSuccessorEstablishedAt = N.nSuccessor < 0 ? -1 : Hierarchy.Nodes[N.nSuccessor].nEstablishedAt;
    E.nTheoreticalLevel = N.nLevel;
    E.nAnchorVersion = Hierarchy.nAnchorVersion;
    E.nDecompositionRule = 1;
    Result.push_back(E);
  }
  return Result;
}

std::vector<ZhongYinEvidence> BuildZhongYinEvidence(
  const std::vector<TrendCompletionEvidence> &Evidence, int nCount,
  const std::vector<float> *pClose)
{
  bool bHasRecursiveEvidence = false;
  for (std::size_t i = 0; i < Evidence.size(); i++)
    if (Evidence[i].nTrendSpace == 1) bHasRecursiveEvidence = true;
  if (!bHasRecursiveEvidence) return std::vector<ZhongYinEvidence>();
  // 第90课 BOLL 是“辅助判断”；只消费真实C，20根总体标准差，宽度=4*std。
  std::vector<double> Width(static_cast<std::size_t>(std::max(nCount, 0)), -1.0);
  if (pClose && pClose->size() == Width.size())
  {
    for (int i = 19; i < nCount; i++)
    {
      double fMean = 0, fVariance = 0;
      bool bValid = true;
      for (int j = i-19; j <= i; j++)
      {
        bValid = bValid && std::isfinite((*pClose)[j]) && (*pClose)[j] > 0;
        fMean += (*pClose)[j] / 20.0;
      }
      if (!bValid) continue;
      for (int j = i-19; j <= i; j++)
      {
        double d = (*pClose)[j] - fMean;
        fVariance += d*d / 20.0;
      }
      Width[i] = 4.0 * std::sqrt(fVariance);
    }
  }
  std::vector<ZhongYinEvidence> Result;
  for (std::size_t i = 0; i < Evidence.size(); i++)
  {
    const TrendCompletionEvidence &E = Evidence[i];
    if (E.nTrendSpace != 1) continue; // 主结构只收严格递归证据
    ZhongYinEvidence Z = {static_cast<int>(i), 1, E.nLatestBar,
                         E.nSuccessorEstablishedAt, E.nObservedAt, -1, true};
    Result.push_back(Z);
    Z.nVersion = 2;
    Z.nEnd = -1;
    Z.bAvailable = false;
    for (int j = std::max(20, Z.nEnter); j < nCount; j++)
    {
      if (Width[j] < 0 || Width[j-1] < 0) continue;
      Z.bAvailable = true;
      if (Z.nContraction < 0 && Width[j] < Width[j-1]) Z.nContraction = j;
      if (Z.nContraction >= 0 && j > Z.nContraction &&
          E.nSuccessorEstablishedAt >= 0 && j >= E.nSuccessorEstablishedAt && Width[j] > Width[j-1])
      {
        Z.nEnd = j;
        break;
      }
    }
    Result.push_back(Z);
  }
  return Result;
}
