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


#include "CzscCore.h"

#include "CCentroid.h"

struct SegmentInterval
{
  int   nStart;
  int   nEnd;
  float fHigh;
  float fLow;
};

struct MoveStrength
{
  float fSpace;
  float fSpeed;
};

static const float SIGNAL_FIRST_BUY = 1.0f;
static const float SIGNAL_SECOND_BUY = 2.0f;
static const float SIGNAL_THIRD_BUY = 3.0f;
static const float SIGNAL_FIRST_SELL = 11.0f;
static const float SIGNAL_SECOND_SELL = 12.0f;
static const float SIGNAL_THIRD_SELL = 13.0f;

static bool HasOutput(int nCount, float *pOut)
{
  return (nCount > 0) && (pOut != 0);
}

static bool HasPriceInput(int nCount, float *pOut, float *pHigh, float *pLow)
{
  return HasOutput(nCount, pOut) && (pHigh != 0) && (pLow != 0);
}

static void ClearOutput(int nCount, float *pOut)
{
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = 0;
  }
}

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

static bool IsIncluded(const MergedBar &Left, const MergedBar &Right)
{
  return ((Right.fHigh <= Left.fHigh) && (Right.fLow >= Left.fLow)) ||
         ((Right.fHigh >= Left.fHigh) && (Right.fLow <= Left.fLow));
}

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

static Fractal MakeFractal(int nType, const MergedBar &Bar)
{
  Fractal F;
  F.nType = nType;
  F.nIndex = (nType == CZSC_POINT_TOP) ? Bar.nHighIndex : Bar.nLowIndex;
  F.fHigh = Bar.fHigh;
  F.fLow = Bar.fLow;
  return F;
}

static SegmentPoint MakeSegmentPoint(const Fractal &F)
{
  SegmentPoint Point;
  Point.nType = F.nType;
  Point.nIndex = F.nIndex;
  Point.fHigh = F.fHigh;
  Point.fLow = F.fLow;
  return Point;
}

static bool IsMoreExtremePoint(const SegmentPoint &Left, const SegmentPoint &Right)
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

static float GetPointPrice(const SegmentPoint &Point)
{
  return (Point.nType == CZSC_POINT_TOP) ? Point.fHigh : Point.fLow;
}

static SegmentPoint MakeSignalPoint(int nIndex, int nType, float fHigh, float fLow)
{
  SegmentPoint Point;
  Point.nType = nType;
  Point.nIndex = nIndex;
  Point.fHigh = fHigh;
  Point.fLow = fLow;
  return Point;
}

static SegmentInterval MakeSegmentInterval(const SegmentPoint &Start, const SegmentPoint &End)
{
  SegmentInterval Interval;
  Interval.nStart = Start.nIndex;
  Interval.nEnd = End.nIndex;

  float fStart = GetPointPrice(Start);
  float fEnd = GetPointPrice(End);
  if (fStart > fEnd)
  {
    Interval.fHigh = fStart;
    Interval.fLow = fEnd;
  }
  else
  {
    Interval.fHigh = fEnd;
    Interval.fLow = fStart;
  }

  return Interval;
}

static bool IntervalsOverlap(float fLeftLow, float fLeftHigh, float fRightLow, float fRightHigh)
{
  return (fLeftLow <= fRightHigh) && (fRightLow <= fLeftHigh);
}

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

  pCenter->nStart = Points[nStart].nIndex;
  pCenter->nEnd = Points[nStart + 3].nIndex;
  pCenter->fHigh = fHigh;
  pCenter->fLow = fLow;
  return true;
}

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
  pCenter->nEnd = Interval.nEnd;
  return true;
}

static float GetMovePower(const SegmentPoint &Start, const SegmentPoint &End)
{
  int nSpan = End.nIndex - Start.nIndex;
  if (nSpan < 0)
  {
    nSpan = -nSpan;
  }
  if (nSpan == 0)
  {
    nSpan = 1;
  }

  float fDiff = GetPointPrice(End) - GetPointPrice(Start);
  if (fDiff < 0)
  {
    fDiff = -fDiff;
  }
  return fDiff / (float)nSpan;
}

static MoveStrength MeasureMoveStrength(const SegmentPoint &Start, const SegmentPoint &End)
{
  MoveStrength Strength;
  Strength.fSpace = GetPointPrice(End) - GetPointPrice(Start);
  if (Strength.fSpace < 0)
  {
    Strength.fSpace = -Strength.fSpace;
  }
  Strength.fSpeed = GetMovePower(Start, End);
  return Strength;
}

static bool IsWeakerMove(const MoveStrength &Current, const MoveStrength &Previous)
{
  return (Current.fSpace < Previous.fSpace) || (Current.fSpeed < Previous.fSpeed);
}

static int GetMoveDirection(const SegmentPoint &Start, const SegmentPoint &End)
{
  float fStart = GetPointPrice(Start);
  float fEnd = GetPointPrice(End);
  if (fEnd > fStart)
  {
    return 1;
  }
  if (fEnd < fStart)
  {
    return -1;
  }
  return 0;
}

static bool IsCenterAbove(const Center &Left, const Center &Right)
{
  return Right.fLow > Left.fHigh;
}

static bool IsCenterBelow(const Center &Left, const Center &Right)
{
  return Right.fHigh < Left.fLow;
}

static bool FindLastTwoTrendCenters(const std::vector<Center> &Centers,
                                    int nIndex,
                                    int nDirection,
                                    int *pPrev,
                                    int *pLast)
{
  int nPrev = -1;
  int nLast = -1;

  for (std::size_t i = 0; i < Centers.size(); i++)
  {
    if (Centers[i].nStart >= nIndex)
    {
      continue;
    }
    nPrev = nLast;
    nLast = (int)i;
  }

  if ((nPrev < 0) || (nLast < 0))
  {
    return false;
  }

  bool bTrend = false;
  if (nDirection > 0)
  {
    bTrend = IsCenterAbove(Centers[nPrev], Centers[nLast]);
  }
  else
  {
    bTrend = IsCenterBelow(Centers[nPrev], Centers[nLast]);
  }

  if (!bTrend)
  {
    return false;
  }

  if (pPrev != 0)
  {
    *pPrev = nPrev;
  }
  if (pLast != 0)
  {
    *pLast = nLast;
  }
  return true;
}

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

static bool IsTrendDivergenceFirstBuy(const std::vector<SegmentPoint> &Points,
                                      const std::vector<Center> &Centers,
                                      std::size_t nPoint)
{
  if ((nPoint < 4) || (Points[nPoint].nType != CZSC_POINT_BOTTOM))
  {
    return false;
  }

  int nPrevCenter = -1;
  int nLastCenter = -1;
  if (!FindLastTwoTrendCenters(Centers, Points[nPoint].nIndex, -1, &nPrevCenter, &nLastCenter))
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
  if (!FindPreviousSameDirectionMove(Points, nPoint, -1, &nPrevMove))
  {
    return false;
  }

  const SegmentPoint &PrevStart = Points[nPrevMove];
  const SegmentPoint &PrevEnd = Points[nPrevMove + 1];
  if ((PrevStart.nType != CZSC_POINT_TOP) || (PrevEnd.nType != CZSC_POINT_BOTTOM))
  {
    return false;
  }

  MoveStrength Previous = MeasureMoveStrength(PrevStart, PrevEnd);
  MoveStrength Current = MeasureMoveStrength(CurrentStart, CurrentEnd);
  return (CurrentEnd.fLow < PrevEnd.fLow) &&
         IsWeakerMove(Current, Previous);
}

static bool IsTrendDivergenceFirstSell(const std::vector<SegmentPoint> &Points,
                                       const std::vector<Center> &Centers,
                                       std::size_t nPoint)
{
  if ((nPoint < 4) || (Points[nPoint].nType != CZSC_POINT_TOP))
  {
    return false;
  }

  int nPrevCenter = -1;
  int nLastCenter = -1;
  if (!FindLastTwoTrendCenters(Centers, Points[nPoint].nIndex, 1, &nPrevCenter, &nLastCenter))
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
  if (!FindPreviousSameDirectionMove(Points, nPoint, 1, &nPrevMove))
  {
    return false;
  }

  const SegmentPoint &PrevStart = Points[nPrevMove];
  const SegmentPoint &PrevEnd = Points[nPrevMove + 1];
  if ((PrevStart.nType != CZSC_POINT_BOTTOM) || (PrevEnd.nType != CZSC_POINT_TOP))
  {
    return false;
  }

  MoveStrength Previous = MeasureMoveStrength(PrevStart, PrevEnd);
  MoveStrength Current = MeasureMoveStrength(CurrentStart, CurrentEnd);
  return (CurrentEnd.fHigh > PrevEnd.fHigh) &&
         IsWeakerMove(Current, Previous);
}

static void WriteTrendDivergenceSignals(int nCount,
                                        float *pOut,
                                        const std::vector<SegmentPoint> &Points,
                                        const std::vector<Center> &Centers)
{
  for (std::size_t i = 0; i < Points.size(); i++)
  {
    int nIndex = Points[i].nIndex;
    if ((nIndex < 0) || (nIndex >= nCount))
    {
      continue;
    }

    if (IsTrendDivergenceFirstBuy(Points, Centers, i))
    {
      pOut[nIndex] = SIGNAL_FIRST_BUY;
    }
    else if (IsTrendDivergenceFirstSell(Points, Centers, i))
    {
      pOut[nIndex] = SIGNAL_FIRST_SELL;
    }
  }
}

std::vector<MergedBar> BuildMergedBars(int nCount, float *pHigh, float *pLow)
{
  std::vector<MergedBar> Bars;
  if ((nCount <= 0) || (pHigh == 0) || (pLow == 0))
  {
    return Bars;
  }

  int nDirection = 0;
  Bars.push_back(MakeMergedBar(0, pHigh[0], pLow[0]));

  for (int i = 1; i < nCount; i++)
  {
    MergedBar Bar = MakeMergedBar(i, pHigh[i], pLow[i]);
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

    Fractal F = MakeFractal(nType, Middle);
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

std::vector<Stroke> BuildStrokes(const std::vector<Fractal> &Fractals)
{
  std::vector<Stroke> Strokes;
  if (Fractals.empty())
  {
    return Strokes;
  }

  Fractal Candidate = Fractals[0];
  for (std::size_t i = 1; i < Fractals.size(); i++)
  {
    const Fractal &Current = Fractals[i];
    if (Current.nType == Candidate.nType)
    {
      if (IsMoreExtreme(Candidate, Current))
      {
        Candidate = Current;
      }
      continue;
    }

    if (Current.nIndex - Candidate.nIndex < 4)
    {
      continue;
    }

    Stroke S;
    S.Start = Candidate;
    S.End = Current;
    S.nDirection = (Candidate.nType == CZSC_POINT_BOTTOM) ? 1 : -1;
    Strokes.push_back(S);
    Candidate = Current;
  }

  return Strokes;
}

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

std::vector<SegmentPoint> BuildSignalPoints(int nCount, float *pIn, float *pHigh, float *pLow)
{
  std::vector<SegmentPoint> Points;
  if ((nCount <= 0) || (pIn == 0) || (pHigh == 0) || (pLow == 0))
  {
    return Points;
  }

  for (int i = 0; i < nCount; i++)
  {
    int nType = CZSC_POINT_NONE;
    if (pIn[i] > 0)
    {
      nType = CZSC_POINT_TOP;
    }
    else if (pIn[i] < 0)
    {
      nType = CZSC_POINT_BOTTOM;
    }

    if (nType == CZSC_POINT_NONE)
    {
      continue;
    }

    SegmentPoint Point = MakeSignalPoint(i, nType, pHigh[i], pLow[i]);
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

std::vector<Center> BuildCenters(const std::vector<SegmentPoint> &Points)
{
  std::vector<Center> Centers;
  if (Points.size() < 4)
  {
    return Centers;
  }

  std::size_t i = 0;
  while (i + 3 < Points.size())
  {
    Center C;
    if (!TryBuildInitialCenter(Points, i, &C))
    {
      i++;
      continue;
    }

    std::size_t nInterval = i + 3;
    while (nInterval + 1 < Points.size())
    {
      SegmentInterval Interval = MakeSegmentInterval(Points[nInterval], Points[nInterval + 1]);
      if (!ExtendCenter(&C, Interval))
      {
        break;
      }
      nInterval++;
    }

    Centers.push_back(C);
    if (nInterval + 1 >= Points.size())
    {
      break;
    }

    i = (nInterval > i) ? nInterval : (i + 1);
  }

  return Centers;
}

static void WriteCenterHighSignal(int nCount, float *pOut, const std::vector<Center> &Centers)
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

static void WriteCenterLowSignal(int nCount, float *pOut, const std::vector<Center> &Centers)
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

static void WriteCenterMarkSignal(int nCount, float *pOut, const std::vector<Center> &Centers)
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

//=============================================================================
// 数学函数部分
//=============================================================================

// 顶底扫描定位函数
void Parse1(int nCount, float *pOut, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow))
  {
    return;
  }

  ClearOutput(nCount, pOut);

  int nState = -1;
  int nHigh  = 0;
  int nLow   = 0;

  for (int i = 1; i < nCount; i++)
  {
    // 寻找高点模式
    if (nState == 1)
    {
      // 如果当前最高大于之前最高，更新位置信息
      if (pHigh[i] >= pHigh[nHigh])
      {
        pOut[nHigh] = 0;
        nHigh = i;
        pOut[nHigh] = 1;
      }

      // 确认转向（原文：当前最高小于高点最低，当前最低小于高点最低）
      if ((pHigh[i] < pHigh[nHigh]) && (pLow[i]  < pLow[nHigh]))
      {
        pOut[nHigh] = 1;

        nState = -1;
        nLow   = i;
      }
    }

    // 寻找低点模式
    else if (nState == -1)
    {
      // 如果当前最低小于之前最低，更新位置信息
      if (pLow[i] <= pLow[nLow])
      {
        pOut[nLow] = 0;
        nLow = i;
        pOut[nLow] = -1;
      }

      // 确认转向（原文：当前最高大于高点最低，当前最低大于高点最低）
      if ((pLow[i]  > pLow[nLow]) && (pHigh[i] > pHigh[nLow]))
      {
        pOut[nLow] = -1;

        nState = 1;
        nHigh  = i;
      }
    }
  }
}

// 化简函数（至少5根K线完成一笔）
void Parse2(int nCount, float *pOut, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow))
  {
    return;
  }

  int nSpan = 0;
  int nCurrTop = 0, nPrevTop = 0;
  int nCurrBot = 0, nPrevBot = 0;

  for (int i = 0; i < nCount; i++)
  {
    // 遇到高点，合并化简上升段（上下上）
    if (pOut[i] == 1)
    {
      // 更新位置信息
      nPrevTop = nCurrTop;
      nCurrTop = i;

      // 存在小于五根的线段，去除中间一段
      if ((pHigh[nCurrTop] >= pHigh[nPrevTop]) &&
          (pLow [nCurrBot] >  pLow [nPrevBot]))
      {
        // 检查合法性（严格按照连续五根形成一笔）
        if (((nCurrTop - nCurrBot < 4) && (nCount   - nCurrTop > 4)) ||
             (nCurrBot - nPrevTop < 4) || (nPrevTop - nPrevBot < 4))
        {
          pOut[nCurrBot] = 0;
          pOut[nPrevTop] = 0;
        }
        else if (nCount - nCurrTop > 4)
        {
          // 检查第三段（上）K线合并
          nSpan = nCurrTop - nCurrBot;
          for (int j = nCurrBot; j < nCurrTop; j++)
          {
            if ((pHigh[j] >= pHigh[j+1]) && (pLow[j] <= pLow[j+1]))
            {
              nSpan--;
            }
          }
          if (nSpan < 4)
          {
            pOut[nCurrBot] = 0;
            pOut[nPrevTop] = 0;
          }

          // 检查第二段（下）K线合并
          nSpan = nCurrBot - nPrevTop;
          for (int j = nPrevTop; j < nCurrBot; j++)
          {
            if ((pHigh[j] >= pHigh[j+1]) && (pLow[j] <= pLow[j+1]))
            {
              nSpan--;
            }
          }
          if (nSpan < 4)
          {
            pOut[nCurrBot] = 0;
            pOut[nPrevTop] = 0;
          }

          // 检查第一段（上）K线合并
          nSpan = nPrevTop - nPrevBot;
          for (int j = nPrevBot; j < nPrevTop; j++)
          {
            if ((pHigh[j] >= pHigh[j+1]) && (pLow[j] <= pLow[j+1]))
            {
              nSpan--;
            }
          }
          if (nSpan < 4)
          {
            pOut[nCurrBot] = 0;
            pOut[nPrevTop] = 0;
          }
        }
      }
    }

    // 遇到低点，合并化简下降段（下上下）
    if (pOut[i] == -1)
    {
      // 更新位置信息
      nPrevBot = nCurrBot;
      nCurrBot = i;

      // 存在小于五根的线段，去除中间一段
      if ((pLow [nCurrBot] <= pLow [nPrevBot]) &&
          (pHigh[nCurrTop] <  pHigh[nPrevTop]))
      {
        // 检查合法性（严格按照连续五根形成一笔）
        if (((nCurrBot - nCurrTop < 4) && (nCount   - nCurrBot > 4)) ||
             (nCurrTop - nPrevBot < 4) || (nPrevBot - nPrevTop < 4))
        {
          pOut[nCurrTop] = 0;
          pOut[nPrevBot] = 0;
        }
        else if (nCount - nCurrBot > 4)
        {
          // 检查第三段（下）K线合并
          nSpan = nCurrBot - nCurrTop;
          for (int j = nCurrTop; j < nCurrBot; j++)
          {
            if ((pHigh[j] >= pHigh[j+1]) && (pLow[j] <= pLow[j+1]))
            {
              nSpan--;
            }
          }
          if (nSpan < 4)
          {
            pOut[nCurrTop] = 0;
            pOut[nPrevBot] = 0;
          }

          // 检查第二段（上）K线合并
          nSpan = nCurrTop - nPrevBot;
          for (int j = nPrevBot; j < nCurrTop; j++)
          {
            if ((pHigh[j] >= pHigh[j+1]) && (pLow[j] <= pLow[j+1]))
            {
              nSpan--;
            }
          }
          if (nSpan < 4)
          {
            pOut[nCurrTop] = 0;
            pOut[nPrevBot] = 0;
          }

          // 检查第一段（下）K线合并
          nSpan = nPrevBot - nPrevTop;
          for (int j = nPrevTop; j < nPrevBot; j++)
          {
            if ((pHigh[j] >= pHigh[j+1]) && (pLow[j] <= pLow[j+1]))
            {
              nSpan--;
            }
          }
          if (nSpan < 4)
          {
            pOut[nCurrTop] = 0;
            pOut[nPrevBot] = 0;
          }
        }
      }
    }
  }
}

//=============================================================================
// 输出函数1号：线段高低点标记信号
//=============================================================================

void Func1(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow))
  {
    return;
  }

  (void)pTime;

  std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  std::vector<SegmentPoint> Points = BuildSegmentPoints(Strokes);
  WriteSegmentSignal(nCount, pOut, Points);
}

//=============================================================================
// 输出函数2号：中枢高点数据
//=============================================================================

void Func2(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers = BuildCenters(Points);
  WriteCenterHighSignal(nCount, pOut, Centers);
}

//=============================================================================
// 输出函数3号：中枢低点数据
//=============================================================================

void Func3(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers = BuildCenters(Points);
  WriteCenterLowSignal(nCount, pOut, Centers);
}

//=============================================================================
// 输出函数4号：中枢起点、终点信号
//=============================================================================

void Func4(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers = BuildCenters(Points);
  WriteCenterMarkSignal(nCount, pOut, Centers);
}

//=============================================================================
// 输出函数5号：三类买卖点信号
//=============================================================================

void Func5(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  ClearOutput(nCount, pOut);

  CCentroid Centroid;

  for (int i = 0; i < nCount; i++)
  {
    if (pIn[i] == 1)
    {
      if (Centroid.PushHigh(i, pHigh[i]))
      {
        // 第三类卖点信号
        pOut[i] = SIGNAL_THIRD_SELL;
      }
      else if (Centroid.fTop1 < Centroid.fTop2)
      {
        // 第二类卖点信号
        pOut[i] = SIGNAL_SECOND_SELL;
      }
      else
      {
        pOut[i] = 0;
      }
    }
    else if (pIn[i] == -1)
    {
      if (Centroid.PushLow(i, pLow[i]))
      {
        // 第三类买点信号
        pOut[i] = SIGNAL_THIRD_BUY;
      }
      else if (Centroid.fBot1 > Centroid.fBot2)
      {
        // 第二类买点信号
        pOut[i] = SIGNAL_SECOND_BUY;
      }
      else
      {
        pOut[i] = 0;
      }
    }
  }

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers = BuildCenters(Points);
  WriteTrendDivergenceSignals(nCount, pOut, Points, Centers);
}

//=============================================================================
// 输出函数6号：形态买卖点信号
//=============================================================================

void Func6(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  ClearOutput(nCount, pOut);

  float fTop1 = 0, fTop2 = 0, fTop3 = 0, fTop4 = 0;
  float fBot1 = 0, fBot2 = 0, fBot3 = 0, fBot4 = 0;

  for (int i = 0; i < nCount; i++)
  {
    if (pIn[i] == 1)
    {
      fTop4 = fTop3;
      fTop3 = fTop2;
      fTop2 = fTop1;
      fTop1 = pHigh[i];

      if (((fBot1 - fTop2)/fTop2 > (fBot2 - fTop3)/fTop3) &&
          ((fBot2 - fTop3)/fTop3 > (fBot3 - fTop4)/fTop4))
      {
        if ((fBot1 < fBot2) && (fTop2 < fTop3) &&
            (fBot2 < fBot3) && (fTop3 < fTop4))
        {
          pOut[i] = 1;
          continue;
        }
        if ((fBot1 > fBot2) && (fTop2 > fTop3) && (fBot2 < fBot3) &&
            (fTop3 < fTop4) && (fBot1 < fTop3))
        {
          pOut[i] = 2;
          continue;
        }
        if ((fBot1 > fTop3) && (fBot2 > fBot3) && (fTop3 > fTop4))
        {
          pOut[i] = 3;
          continue;
        }
      }
    }
    else if (pIn[i] == -1)
    {
      fBot4 = fBot3;
      fBot3 = fBot2;
      fBot2 = fBot1;
      fBot1 = pLow[i];

      if (((fBot1 - fTop1)/fTop1 > (fBot2 - fTop2)/fTop2) &&
          ((fBot2 - fTop2)/fTop2 > (fBot3 - fTop3)/fTop3))
      {
        if ((fBot1 < fBot2) && (fTop1 < fTop2) &&
            (fBot2 < fBot3) && (fTop2 < fTop3))
        {
          pOut[i] = 1;
          continue;
        }
        if ((fBot1 > fBot2) && (fTop1 > fTop2) && (fBot2 < fBot3) &&
            (fTop2 < fTop3) && (fBot1 < fTop2))
        {
          pOut[i] = 2;
          continue;
        }
        if ((fBot1 > fTop2) && (fBot2 > fBot3) && (fTop2 > fTop3))
        {
          pOut[i] = 3;
          continue;
        }
      }
    }
  }
}

//=============================================================================
// 输出函数7号：线段强度分析指标
//=============================================================================

void Func7(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  ClearOutput(nCount, pOut);

  int nPrevTop = 0, nPrevBot = 0;

  for (int i = 1; i < nCount; i++)
  {
    // 遇到线段高点
    if (pIn[i-1] == 1)
    {
      // 标记高点位置
      nPrevTop = i - 1;
    }
    // 遇到线段低点
    else if (pIn[i-1] == -1)
    {
      // 标记低点位置
      nPrevBot = i - 1;
    }

    // 上升线段计算模式
    if (pIn[i] == 1)
    {
      // 计算上升线段斜率
      if (pLow[nPrevBot] != 0)
      {
        pOut[i] = (pHigh[i] - pLow[nPrevBot]) / pLow[nPrevBot] * 100;
      }
    }
    // 下降线段计算模式
    else if (pIn[i] == -1)
    {
      // 计算上升线段斜率
      if (pHigh[nPrevTop] != 0)
      {
        pOut[i] = (pLow[i] - pHigh[nPrevTop]) / pHigh[nPrevTop] * 100;
      }
    }
  }
}

//=============================================================================
// 输出函数8号：线段斜率分析指标
//=============================================================================

void Func8(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  ClearOutput(nCount, pOut);

  int nPrevTop = 0, nPrevBot = 0;

  for (int i = 1; i < nCount; i++)
  {
    // 遇到线段高点
    if (pIn[i-1] == 1)
    {
      // 标记高点位置
      nPrevTop = i - 1;
    }
    // 遇到线段低点
    else if (pIn[i-1] == -1)
    {
      // 标记低点位置
      nPrevBot = i - 1;
    }

    // 上升线段计算模式
    if (pIn[i] == 1)
    {
      // 计算上升线段斜率
      pOut[i] = (pHigh[i] - pLow[nPrevBot]) / (i - nPrevBot);
    }
    // 下降线段计算模式
    else if (pIn[i] == -1)
    {
      // 计算上升线段斜率
      pOut[i] = (pLow[i] - pHigh[nPrevTop]) / (i - nPrevTop);
    }
  }
}

//=============================================================================
// 输出函数9号：线段高低点标记信号
//=============================================================================

void Func9(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow))
  {
    return;
  }

  (void)pTime;

  std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  std::vector<SegmentPoint> Points = BuildLineSegmentPoints(Strokes);
  WriteSegmentSignal(nCount, pOut, Points);
}
