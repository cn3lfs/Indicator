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

static MergedBar MakeMergedBar(int nIndex, float fHigh, float fLow)
{
  MergedBar Bar;
  Bar.nStart = nIndex;
  Bar.nEnd = nIndex;
  Bar.nHighIndex = nIndex;
  Bar.nLowIndex = nIndex;
  Bar.fHigh = fHigh;
  Bar.fLow = fLow;
  return Bar;
}

// 两K线是否存在包含关系（一根的高低点全在另一根范围内，第62课）
static bool IsIncluded(const MergedBar &Left, const MergedBar &Right)
{
  return ((Right.fHigh <= Left.fHigh) && (Right.fLow >= Left.fLow)) ||
         ((Right.fHigh >= Left.fHigh) && (Right.fLow <= Left.fLow));
}

// 由非包含的相邻两K线判定向上(+1)/向下(-1)/不确定(0)
static int DetectDirection(const MergedBar &Left, const MergedBar &Right)
{
  if ((Right.fHigh > Left.fHigh) && (Right.fLow > Left.fLow))
  {
    return 1;
  }
  if ((Right.fHigh < Left.fHigh) && (Right.fLow < Left.fLow))
  {
    return -1;
  }
  return 0;
}

// 选择包含合并的方向：已有趋势则沿用，否则按高/低点差值较大的一侧定向
static int ChooseMergeDirection(const MergedBar &Last, const MergedBar &Bar, int nDirection)
{
  if (nDirection != 0)
  {
    return nDirection;
  }

  float fHighDiff = Bar.fHigh - Last.fHigh;
  if (fHighDiff < 0)
  {
    fHighDiff = -fHighDiff;
  }

  float fLowDiff = Bar.fLow - Last.fLow;
  if (fLowDiff < 0)
  {
    fLowDiff = -fLowDiff;
  }

  return (fHighDiff >= fLowDiff) ? 1 : -1;
}

static void AssignHigh(MergedBar *pTarget, const MergedBar &Source)
{
  pTarget->fHigh = Source.fHigh;
  pTarget->nHighIndex = Source.nHighIndex;
}

static void AssignLow(MergedBar *pTarget, const MergedBar &Source)
{
  pTarget->fLow = Source.fLow;
  pTarget->nLowIndex = Source.nLowIndex;
}

// 把被包含的K线合并进前一根：向上取高高/低取高，向下取低低/高取低（第62课）
static void MergeIncludedBar(MergedBar *pLast, const MergedBar &Bar, int nDirection)
{
  if (nDirection >= 0)
  {
    if (Bar.fHigh >= pLast->fHigh)
    {
      AssignHigh(pLast, Bar);
    }
    if (Bar.fLow >= pLast->fLow)
    {
      AssignLow(pLast, Bar);
    }
  }
  else
  {
    if (Bar.fHigh <= pLast->fHigh)
    {
      AssignHigh(pLast, Bar);
    }
    if (Bar.fLow <= pLast->fLow)
    {
      AssignLow(pLast, Bar);
    }
  }

  pLast->nEnd = Bar.nEnd;
}

// 同类型分型 Right 是否比 Left 更极端（顶更高 / 底更低），用于合并相邻同型分型
static bool IsMoreExtreme(const Fractal &Left, const Fractal &Right)
{
  if (Left.nType == CZSC_POINT_TOP)
  {
    return Right.fHigh >= Left.fHigh;
  }
  if (Left.nType == CZSC_POINT_BOTTOM)
  {
    return Right.fLow <= Left.fLow;
  }
  return false;
}

// 由合并K线生成分型（顶取高点下标、底取低点下标；nMergedIndex 为分型中点的合并K线下标）
static Fractal MakeFractal(int nType, const MergedBar &Bar, int nMergedIndex)
{
  Fractal F;
  F.nType = nType;
  F.nIndex = (nType == CZSC_POINT_TOP) ? Bar.nHighIndex : Bar.nLowIndex;
  F.nMergedIndex = nMergedIndex;
  F.fHigh = Bar.fHigh;
  F.fLow = Bar.fLow;
  return F;
}

// 线段是否被反向的保护点破坏（向上线段被新低破坏 / 向下线段被新高破坏）
static bool IsBrokenByProtectPoint(int nDirection, const SegmentPoint &Protect, const SegmentPoint &Point)
{
  if ((nDirection > 0) && (Point.nType == CZSC_POINT_BOTTOM))
  {
    return Point.fLow < Protect.fLow;
  }
  if ((nDirection < 0) && (Point.nType == CZSC_POINT_TOP))
  {
    return Point.fHigh > Protect.fHigh;
  }
  return false;
}

// 取线段点的代表价：顶取高、底取低
// 形态学第一步：对原始K线做包含处理，合并出无包含关系的合并K线序列（第62/65课）
std::vector<MergedBar> BuildMergedBars(int nCount, float *pHigh, float *pLow)
{
  std::vector<MergedBar> Bars;
  if ((nCount <= 0) || (pHigh == 0) || (pLow == 0))
  {
    return Bars;
  }

  std::vector<float> High = SanitizeSeries(nCount, pHigh);
  std::vector<float> Low = SanitizeSeries(nCount, pLow);

  int nDirection = 0;
  Bars.push_back(MakeMergedBar(0, High[0], Low[0]));

  for (int i = 1; i < nCount; i++)
  {
    MergedBar Bar = MakeMergedBar(i, High[(std::size_t)i], Low[(std::size_t)i]);
    MergedBar &Last = Bars.back();

    if (IsIncluded(Last, Bar))
    {
      int nMergeDirection = ChooseMergeDirection(Last, Bar, nDirection);
      MergeIncludedBar(&Last, Bar, nMergeDirection);
      if (nDirection == 0)
      {
        nDirection = nMergeDirection;
      }
      continue;
    }

    int nNewDirection = DetectDirection(Last, Bar);
    if (nNewDirection != 0)
    {
      nDirection = nNewDirection;
    }
    Bars.push_back(Bar);
  }

  return Bars;
}

// 从合并K线识别顶/底分型，并合并相邻同型分型（保留更极端者，第62课）
std::vector<Fractal> BuildFractals(const std::vector<MergedBar> &Bars)
{
  std::vector<Fractal> Fractals;
  if (Bars.size() < 3)
  {
    return Fractals;
  }

  for (std::size_t i = 1; i + 1 < Bars.size(); i++)
  {
    const MergedBar &Left = Bars[i - 1];
    const MergedBar &Middle = Bars[i];
    const MergedBar &Right = Bars[i + 1];

    int nType = CZSC_POINT_NONE;
    if ((Middle.fHigh > Left.fHigh) && (Middle.fHigh > Right.fHigh) &&
        (Middle.fLow > Left.fLow) && (Middle.fLow > Right.fLow))
    {
      nType = CZSC_POINT_TOP;
    }
    else if ((Middle.fLow < Left.fLow) && (Middle.fLow < Right.fLow) &&
             (Middle.fHigh < Left.fHigh) && (Middle.fHigh < Right.fHigh))
    {
      nType = CZSC_POINT_BOTTOM;
    }

    if (nType == CZSC_POINT_NONE)
    {
      continue;
    }

    Fractal F = MakeFractal(nType, Middle, (int)i);
    if (!Fractals.empty() && (Fractals.back().nType == F.nType))
    {
      if (IsMoreExtreme(Fractals.back(), F))
      {
        Fractals.back() = F;
      }
      continue;
    }

    Fractals.push_back(F);
  }

  return Fractals;
}

// 一笔的跨度是否达标（第62/65课，两端含顶底分型的极值K线）：
//  严格笔（旧笔）——处理包含关系后顶底间至少 1 根独立合并K线，即含顶底 ≥5 根合并K线
//                （nMergedIndex 跨度 ≥4，顶底分型不共用合并K线）；
//  新笔（宽笔）——放宽为处理后 ≥4 根合并K线（nMergedIndex 跨度 ≥3、仍不共用）且
//                处理前 ≥5 根原始K线（nIndex 跨度 ≥4）。
static bool StrokeSpanEnough(const Fractal &A, const Fractal &B, const CzscConfig &Config)
{
  int nMergedGap = B.nMergedIndex - A.nMergedIndex;  // 处理包含后的合并K线跨度
  int nRawGap = B.nIndex - A.nIndex;                 // 处理包含前的原始K线跨度
  if (Config.nStrokeType == CZSC_STROKE_NEW)
  {
    return (nMergedGap >= 3) && (nRawGap >= 4);
  }
  return (nMergedGap >= 4);
}

static void RefineStrictStrokeEnds(std::vector<Fractal> *pEnds,
                                   const std::vector<Fractal> &Fractals,
                                   const CzscConfig &Config)
{
  if ((pEnds == 0) || (Config.nStrokeEnd != CZSC_END_STRICT) || (pEnds->size() < 3))
  {
    return;
  }

  // 笔的最小合并K线跨度：严格笔≥4，新笔≥3
  int nMinSpan = (Config.nStrokeType == CZSC_STROKE_NEW) ? 3 : 4;

  std::vector<Fractal> &Ends = *pEnds;
  for (std::size_t i = 1; i + 1 < Ends.size(); i++)
  {
    const Fractal &Prev = Ends[i - 1];
    const Fractal &Next = Ends[i + 1];
    Fractal Best = Ends[i];

    // 候选只允许在 Prev 附近有限窗口内（≤ 2×最小跨度），防止跳过已验证的相反端点
    int nMaxMerged = Prev.nMergedIndex + 2 * nMinSpan;

    for (std::size_t j = 0; j < Fractals.size(); j++)
    {
      const Fractal &F = Fractals[j];
      if ((F.nType != Ends[i].nType) ||
          (F.nIndex <= Prev.nIndex) || (F.nIndex >= Next.nIndex) ||
          (F.nMergedIndex > nMaxMerged))
      {
        continue;
      }
      if (!StrokeSpanEnough(Prev, F, Config) || !StrokeSpanEnough(F, Next, Config))
      {
        continue;
      }
      if (IsMoreExtreme(Best, F))
      {
        Best = F;
      }
    }

    Ends[i] = Best;
  }
}

// 由相邻顶底连成笔（受配置驱动）：
//  笔类型见 StrokeSpanEnough（严格笔=合并K线≥5、新笔=合并K线≥4且原始K线≥5）；
//  笔结束 STRICT 取最严格极值收笔，SECOND 保留首个同型分型（允许次高/次低点）。
// 关键：先构建顶底交替的端点序列——同向更极端分型把端点向后「延伸（中继）」，反向分型仅当跨度达标
// 才作为新端点；跨度不足的反向分型直接忽略，不弹出已与前端点构成达标笔的端点（第65课）。再连成笔。
std::vector<Stroke> BuildStrokes(const std::vector<Fractal> &Fractals, const CzscConfig &Config)
{
  std::vector<Stroke> Strokes;
  if (Fractals.empty())
  {
    return Strokes;
  }

  // 第一阶段：构建顶底交替的笔端点序列
  std::vector<Fractal> Ends;
  Ends.push_back(Fractals[0]);

  for (std::size_t i = 1; i < Fractals.size(); i++)
  {
    const Fractal &F = Fractals[i];
    const Fractal &Last = Ends.back();
    if (F.nType == Last.nType)
    {
      // 同型：严格取极值则用更极端者延伸端点（中继顶/底）；允许次高/次低则保留首个
      if ((Config.nStrokeEnd == CZSC_END_STRICT) && IsMoreExtreme(Last, F))
      {
        Ends.back() = F;
      }
    }
    else if (StrokeSpanEnough(Last, F, Config))
    {
      Ends.push_back(F);  // 异型且跨度达标 → 新笔端点
    }
    // 异型但跨度不足 → 忽略该分型（不弹出已与前端点构成达标笔的端点，第65课）
  }

  RefineStrictStrokeEnds(&Ends, Fractals, Config);

  // 第二阶段：相邻交替端点连成笔
  for (std::size_t i = 1; i < Ends.size(); i++)
  {
    Stroke S;
    S.Start = Ends[i - 1];
    S.End = Ends[i];
    S.nDirection = (Ends[i - 1].nType == CZSC_POINT_BOTTOM) ? 1 : -1;
    Strokes.push_back(S);
  }

  return Strokes;
}

// 把笔的端点串成线段点序列（笔级别的转折点）
std::vector<SegmentPoint> BuildSegmentPoints(const std::vector<Stroke> &Strokes)
{
  std::vector<SegmentPoint> Points;
  if (Strokes.empty())
  {
    return Points;
  }

  Points.push_back(MakeSegmentPoint(Strokes[0].Start));

  for (std::size_t i = 0; i < Strokes.size(); i++)
  {
    SegmentPoint Point = MakeSegmentPoint(Strokes[i].End);
    if (Points.empty() || (Points.back().nIndex != Point.nIndex))
    {
      Points.push_back(Point);
    }
  }

  return Points;
}

// 由笔进一步划分线段（更高级别）：至少三笔起步，用保护点是否被破坏确认线段转折
std::vector<SegmentPoint> BuildLineSegmentPoints(const std::vector<Stroke> &Strokes)
{
  std::vector<SegmentPoint> Points;
  if (Strokes.size() < 3)
  {
    return Points;
  }

  std::vector<SegmentPoint> StrokePoints = BuildSegmentPoints(Strokes);
  if (StrokePoints.size() < 4)
  {
    return Points;
  }

  Points.push_back(StrokePoints[0]);

  std::size_t nStart = 0;
  std::size_t i = nStart + 1;
  bool bHasCandidate = false;
  std::size_t nCandidate = nStart;
  std::size_t nProtect = nStart;

  while (i < StrokePoints.size())
  {
    const SegmentPoint &Start = StrokePoints[nStart];
    const SegmentPoint &Point = StrokePoints[i];
    int nDirection = (Start.nType == CZSC_POINT_BOTTOM) ? 1 : -1;
    int nStrokeSpan = (int)(i - nStart);

    if (!bHasCandidate)
    {
      if ((nStrokeSpan >= 3) && (Point.nType != Start.nType))
      {
        nCandidate = i;
        nProtect = i - 1;
        bHasCandidate = true;
      }
      i++;
      continue;
    }

    if ((Point.nType == StrokePoints[nCandidate].nType) &&
        IsMoreExtremePoint(StrokePoints[nCandidate], Point))
    {
      nCandidate = i;
      nProtect = i - 1;
      i++;
      continue;
    }

    if (IsBrokenByProtectPoint(nDirection, StrokePoints[nProtect], Point))
    {
      if (Points.back().nIndex != StrokePoints[nCandidate].nIndex)
      {
        Points.push_back(StrokePoints[nCandidate]);
      }
      nStart = nCandidate;
      bHasCandidate = false;
      i = nStart + 1;
      continue;
    }

    i++;
  }

  if (bHasCandidate && (Points.back().nIndex != StrokePoints[nCandidate].nIndex))
  {
    Points.push_back(StrokePoints[nCandidate]);
  }

  return Points;
}

//=============================================================================
// 线段划分·特征序列法（第67课）：与上面的保护点启发式并存的可选实现
//=============================================================================

struct FeatureElement
{
  std::size_t nInnerPoint;
  std::size_t nOuterPoint;
  std::size_t nHighPoint;
  std::size_t nLowPoint;
  float       fHigh;
  float       fLow;
};

static bool MakeFeatureElement(const std::vector<SegmentPoint> &P,
                               std::size_t nStart,
                               std::size_t nElement,
                               FeatureElement *pElement)
{
  if ((pElement == 0) || (nStart + 2 + 2 * nElement >= P.size()))
  {
    return false;
  }

  pElement->nInnerPoint = nStart + 1 + 2 * nElement;
  pElement->nOuterPoint = nStart + 2 + 2 * nElement;
  SegmentInterval Interval = MakeSegmentInterval(P[pElement->nInnerPoint], P[pElement->nOuterPoint]);
  pElement->fHigh = Interval.fHigh;
  pElement->fLow = Interval.fLow;
  if (GetPointPrice(P[pElement->nInnerPoint]) >= GetPointPrice(P[pElement->nOuterPoint]))
  {
    pElement->nHighPoint = pElement->nInnerPoint;
    pElement->nLowPoint = pElement->nOuterPoint;
  }
  else
  {
    pElement->nHighPoint = pElement->nOuterPoint;
    pElement->nLowPoint = pElement->nInnerPoint;
  }
  return true;
}

static bool FeatureElementsIncluded(const FeatureElement &Left, const FeatureElement &Right)
{
  return ((Right.fHigh <= Left.fHigh) && (Right.fLow >= Left.fLow)) ||
         ((Right.fHigh >= Left.fHigh) && (Right.fLow <= Left.fLow));
}

static int DetectFeatureDirection(const FeatureElement &Left, const FeatureElement &Right)
{
  if ((Right.fHigh > Left.fHigh) && (Right.fLow > Left.fLow))
  {
    return 1;
  }
  if ((Right.fHigh < Left.fHigh) && (Right.fLow < Left.fLow))
  {
    return -1;
  }
  return 0;
}

static int ChooseFeatureMergeDirection(const FeatureElement &Last, const FeatureElement &Current, int nDirection)
{
  if (nDirection != 0)
  {
    return nDirection;
  }

  float fHighDiff = Current.fHigh - Last.fHigh;
  if (fHighDiff < 0)
  {
    fHighDiff = -fHighDiff;
  }

  float fLowDiff = Current.fLow - Last.fLow;
  if (fLowDiff < 0)
  {
    fLowDiff = -fLowDiff;
  }

  return (fHighDiff >= fLowDiff) ? 1 : -1;
}

static void MergeFeatureElement(FeatureElement *pLast, const FeatureElement &Current, int nDirection)
{
  if (pLast == 0)
  {
    return;
  }

  if (nDirection >= 0)
  {
    if (Current.fHigh >= pLast->fHigh)
    {
      pLast->fHigh = Current.fHigh;
      pLast->nHighPoint = Current.nHighPoint;
    }
    if (Current.fLow >= pLast->fLow)
    {
      pLast->fLow = Current.fLow;
      pLast->nLowPoint = Current.nLowPoint;
    }
  }
  else
  {
    if (Current.fHigh <= pLast->fHigh)
    {
      pLast->fHigh = Current.fHigh;
      pLast->nHighPoint = Current.nHighPoint;
    }
    if (Current.fLow <= pLast->fLow)
    {
      pLast->fLow = Current.fLow;
      pLast->nLowPoint = Current.nLowPoint;
    }
  }

  pLast->nOuterPoint = Current.nOuterPoint;
}

static std::vector<FeatureElement> BuildStandardFeatureSequence(const std::vector<SegmentPoint> &P,
                                                                std::size_t nStart)
{
  std::vector<FeatureElement> Standard;
  int nDirection = 0;

  for (std::size_t i = 0; ; i++)
  {
    FeatureElement Current;
    if (!MakeFeatureElement(P, nStart, i, &Current))
    {
      break;
    }
    if (Standard.empty())
    {
      Standard.push_back(Current);
      continue;
    }

    FeatureElement &Last = Standard.back();
    if (FeatureElementsIncluded(Last, Current))
    {
      int nMergeDirection = ChooseFeatureMergeDirection(Last, Current, nDirection);
      MergeFeatureElement(&Last, Current, nMergeDirection);
      if (nDirection == 0)
      {
        nDirection = nMergeDirection;
      }
      continue;
    }

    int nNewDirection = DetectFeatureDirection(Last, Current);
    if (nNewDirection != 0)
    {
      nDirection = nNewDirection;
    }
    Standard.push_back(Current);
  }

  return Standard;
}

static bool FeatureElementsOverlap(const FeatureElement &Left, const FeatureElement &Right)
{
  return IntervalsOverlap(Left.fLow, Left.fHigh, Right.fLow, Right.fHigh);
}

static bool IsFeatureFractal(const FeatureElement &Left,
                             const FeatureElement &Middle,
                             const FeatureElement &Right,
                             int nDir)
{
  if (nDir > 0)
  {
    // 以向上笔开始的线段只考察特征序列顶分型（第67课）
    return (Middle.fHigh > Left.fHigh) && (Middle.fHigh > Right.fHigh) &&
           (Middle.fLow > Left.fLow) && (Middle.fLow > Right.fLow);
  }
  if (nDir < 0)
  {
    // 以向下笔开始的线段只考察特征序列底分型（第67课）
    return (Middle.fLow < Left.fLow) && (Middle.fLow < Right.fLow) &&
           (Middle.fHigh < Left.fHigh) && (Middle.fHigh < Right.fHigh);
  }
  return false;
}

static bool HasAnyFeatureFractal(const std::vector<SegmentPoint> &P, std::size_t nStart, int nDir)
{
  std::vector<FeatureElement> Seq = BuildStandardFeatureSequence(P, nStart);
  for (std::size_t i = 1; i + 1 < Seq.size(); i++)
  {
    if (IsFeatureFractal(Seq[i - 1], Seq[i], Seq[i + 1], nDir))
    {
      return true;
    }
  }
  return false;
}

static std::size_t FeatureFractalPoint(const FeatureElement &Element, int nDir)
{
  return (nDir > 0) ? Element.nHighPoint : Element.nLowPoint;
}

// 定位线段终点（第67课）：
//  第一种情况：标准特征序列分型的第一、第二元素无缺口，线段在该分型高/低点结束；
//  第二种情况：第一、第二元素有缺口，必须从该分型高/低点开始的反向特征序列出现分型确认。
static int FindFeatureSegmentEnd(const std::vector<SegmentPoint> &P, std::size_t nStart, int nDir)
{
  std::vector<FeatureElement> Seq = BuildStandardFeatureSequence(P, nStart);
  for (std::size_t i = 1; i + 1 < Seq.size(); i++)
  {
    if (!IsFeatureFractal(Seq[i - 1], Seq[i], Seq[i + 1], nDir))
    {
      continue;
    }

    std::size_t nEndPoint = FeatureFractalPoint(Seq[i], nDir);
    if (FeatureElementsOverlap(Seq[i - 1], Seq[i]))
    {
      return (int)nEndPoint;
    }

    if (HasAnyFeatureFractal(P, nEndPoint, -nDir))
    {
      return (int)nEndPoint;
    }
  }
  return -1;
}

static bool FirstThreeStrokesOverlap(const std::vector<SegmentPoint> &P, std::size_t nStart)
{
  if (nStart + 3 >= P.size())
  {
    return false;
  }

  SegmentInterval A = MakeSegmentInterval(P[nStart], P[nStart + 1]);
  SegmentInterval B = MakeSegmentInterval(P[nStart + 1], P[nStart + 2]);
  SegmentInterval C = MakeSegmentInterval(P[nStart + 2], P[nStart + 3]);

  float fLow = A.fLow;
  if (B.fLow > fLow) fLow = B.fLow;
  if (C.fLow > fLow) fLow = C.fLow;

  float fHigh = A.fHigh;
  if (B.fHigh < fHigh) fHigh = B.fHigh;
  if (C.fHigh < fHigh) fHigh = C.fHigh;

  return fLow <= fHigh;
}

// 特征序列法划分线段（第67课），与 BuildLineSegmentPoints 的启发式并存
std::vector<SegmentPoint> BuildLineSegmentPointsByFeature(const std::vector<Stroke> &Strokes)
{
  std::vector<SegmentPoint> Points;
  std::vector<SegmentPoint> StrokePoints = BuildSegmentPoints(Strokes);
  if (StrokePoints.size() < 4)
  {
    return Points;
  }

  std::size_t nStart = 0;
  while ((nStart + 3 < StrokePoints.size()) && !FirstThreeStrokesOverlap(StrokePoints, nStart))
  {
    nStart++;
  }
  if (nStart + 3 >= StrokePoints.size())
  {
    return Points;
  }

  Points.push_back(StrokePoints[nStart]);
  while (nStart + 3 < StrokePoints.size())
  {
    if (!FirstThreeStrokesOverlap(StrokePoints, nStart))
    {
      break;
    }
    int nDir = (StrokePoints[nStart].nType == CZSC_POINT_BOTTOM) ? 1 : -1;
    int nEnd = FindFeatureSegmentEnd(StrokePoints, nStart, nDir);
    if ((nEnd < 0) || ((std::size_t)nEnd <= nStart))
    {
      break;
    }
    if (Points.back().nIndex != StrokePoints[(std::size_t)nEnd].nIndex)
    {
      Points.push_back(StrokePoints[(std::size_t)nEnd]);
    }
    nStart = (std::size_t)nEnd;
  }
  return Points;
}

// 在各线段点处写出其类型（±1），通达信据此画线段
void WriteSegmentSignal(int nCount, float *pOut, const std::vector<SegmentPoint> &Points)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  for (std::size_t i = 0; i < Points.size(); i++)
  {
    int nIndex = Points[i].nIndex;
    if ((nIndex >= 0) && (nIndex < nCount))
    {
      pOut[nIndex] = (float)(Points[i].nType);
    }
  }
}

// 从通达信传入的线段点信号（±1）还原线段点序列，供中枢/买卖点计算复用
std::vector<SegmentPoint> BuildSignalPoints(int nCount, float *pIn, float *pHigh, float *pLow)
{
  std::vector<SegmentPoint> Points;
  if ((nCount <= 0) || (pIn == 0) || (pHigh == 0) || (pLow == 0))
  {
    return Points;
  }

  std::vector<float> High = SanitizeSeries(nCount, pHigh);
  std::vector<float> Low = SanitizeSeries(nCount, pLow);

  for (int i = 0; i < nCount; i++)
  {
    float fSignal = IsInvalidFloat(pIn[i]) ? 0.0f : pIn[i];  // 无效信号视作无端点
    int nType = CZSC_POINT_NONE;
    if (fSignal > 0)
    {
      nType = CZSC_POINT_TOP;
    }
    else if (fSignal < 0)
    {
      nType = CZSC_POINT_BOTTOM;
    }

    if (nType == CZSC_POINT_NONE)
    {
      continue;
    }

    SegmentPoint Point = MakeSignalPoint(i, nType, High[(std::size_t)i], Low[(std::size_t)i]);
    if (!Points.empty() && (Points.back().nType == Point.nType))
    {
      if (IsMoreExtremePoint(Points.back(), Point))
      {
        Points.back() = Point;
      }
      continue;
    }

    Points.push_back(Point);
  }

  return Points;
}

std::vector<SegmentPoint> BuildConfiguredPoints(int nCount, float *pHigh, float *pLow, const CzscConfig &Config)
{
  std::vector<SegmentPoint> Points;
  if ((nCount <= 0) || (pHigh == 0) || (pLow == 0))
  {
    return Points;
  }

  std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals, Config);
  if (Config.nCenterUnit == CZSC_UNIT_SEGMENT)
  {
    Points = (Config.nSegmentMethod == CZSC_SEG_FEATURE)
             ? BuildLineSegmentPointsByFeature(Strokes)
             : BuildLineSegmentPoints(Strokes);
  }
  else
  {
    Points = BuildSegmentPoints(Strokes);
  }
  return Points;
}

