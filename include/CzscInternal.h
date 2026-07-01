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
#ifndef __CZSC_INTERNAL_H__
#define __CZSC_INTERNAL_H__

#include "CzscTdxExports.h"

static const float SIGNAL_FIRST_BUY = 1.0f;
static const float SIGNAL_SECOND_BUY = 2.0f;
static const float SIGNAL_THIRD_BUY = 3.0f;
static const float SIGNAL_FIRST_SELL = 11.0f;
static const float SIGNAL_SECOND_SELL = 12.0f;
static const float SIGNAL_THIRD_SELL = 13.0f;

static const int SIGNAL_PRIORITY_SECOND = 10;
static const int SIGNAL_PRIORITY_THIRD = 20;
static const int SIGNAL_PRIORITY_FIRST = 30;

static const int SIGNAL_SOURCE_FIRST = 1;
static const int SIGNAL_SOURCE_SECOND = 2;
static const int SIGNAL_SOURCE_THIRD = 3;

inline bool IsFirstSignal(float fSignal)
{
  return (fSignal == SIGNAL_FIRST_BUY) || (fSignal == SIGNAL_FIRST_SELL);
}

inline bool IsSecondSignal(float fSignal)
{
  return (fSignal == SIGNAL_SECOND_BUY) || (fSignal == SIGNAL_SECOND_SELL);
}

inline bool IsThirdSignal(float fSignal)
{
  return (fSignal == SIGNAL_THIRD_BUY) || (fSignal == SIGNAL_THIRD_SELL);
}

inline int GetTradingSignalSide(float fSignal)
{
  if ((fSignal == SIGNAL_FIRST_BUY) || (fSignal == SIGNAL_SECOND_BUY) ||
      (fSignal == SIGNAL_THIRD_BUY))
  {
    return 1;
  }
  if ((fSignal == SIGNAL_FIRST_SELL) || (fSignal == SIGNAL_SECOND_SELL) ||
      (fSignal == SIGNAL_THIRD_SELL))
  {
    return -1;
  }
  return 0;
}

inline bool IsTradingSignal(float fSignal)
{
  return GetTradingSignalSide(fSignal) != 0;
}

inline bool HasTradingSignalOutput(const TradingSignalCandidate &C, int nCount)
{
  return (C.nIndex >= 0) && (C.nIndex < nCount) && IsTradingSignal(C.fSignal);
}

inline bool HasOutput(int nCount, float *pOut)
{
  return (nCount > 0) && (pOut != 0);
}

inline bool HasPriceInput(int nCount, float *pOut, float *pHigh, float *pLow)
{
  return HasOutput(nCount, pOut) && (pHigh != 0) && (pLow != 0);
}

inline void ClearOutput(int nCount, float *pOut)
{
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = 0;
  }
}

inline float AbsF(float fValue)
{
  return (fValue < 0) ? -fValue : fValue;
}

inline bool IsInvalidFloat(float fValue)
{
  unsigned int uBits;
  const unsigned char *pSrc = (const unsigned char *)&fValue;
  unsigned char *pDst = (unsigned char *)&uBits;
  for (int i = 0; i < (int)sizeof(float); i++)
  {
    pDst[i] = pSrc[i];
  }
  return uBits == 0xF8F8F8F8u;
}

inline std::vector<float> SanitizeSeries(int nCount, const float *pSrc)
{
  std::vector<float> Out;
  if ((nCount <= 0) || (pSrc == 0))
  {
    return Out;
  }
  Out.resize((std::size_t)nCount);

  float fLastValid = 0;
  bool bHaveValid = false;
  for (int i = 0; i < nCount; i++)
  {
    if (!IsInvalidFloat(pSrc[i]))
    {
      fLastValid = pSrc[i];
      bHaveValid = true;
      break;
    }
  }
  for (int i = 0; i < nCount; i++)
  {
    if (!IsInvalidFloat(pSrc[i]))
    {
      fLastValid = pSrc[i];
    }
    Out[(std::size_t)i] = bHaveValid ? fLastValid : 0;
  }
  return Out;
}

inline bool IsValidConfigCode(int nCode)
{
  if (nCode < 0)
  {
    return false;
  }

  for (int i = 0; i < 4; i++)
  {
    int nDigit = nCode % 10;
    if ((nDigit != 0) && (nDigit != 1))
    {
      return false;
    }
    nCode /= 10;
  }
  return nCode == 0;
}

inline SegmentPoint MakeSegmentPoint(const Fractal &F)
{
  SegmentPoint Point;
  Point.nType = F.nType;
  Point.nIndex = F.nIndex;
  Point.fHigh = F.fHigh;
  Point.fLow = F.fLow;
  Point.fEnergy = 0;
  Point.fDif = 0;
  Point.fDea = 0;
  return Point;
}

inline bool IsMoreExtremePoint(const SegmentPoint &Left, const SegmentPoint &Right)
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

inline float GetPointPrice(const SegmentPoint &Point)
{
  return (Point.nType == CZSC_POINT_TOP) ? Point.fHigh : Point.fLow;
}

inline SegmentPoint MakeSignalPoint(int nIndex, int nType, float fHigh, float fLow)
{
  SegmentPoint Point;
  Point.nType = nType;
  Point.nIndex = nIndex;
  Point.fHigh = fHigh;
  Point.fLow = fLow;
  Point.fEnergy = 0;
  Point.fDif = 0;
  Point.fDea = 0;
  return Point;
}

struct SegmentInterval
{
  int   nStart;
  int   nEnd;
  float fHigh;
  float fLow;
};

inline SegmentInterval MakeSegmentInterval(const SegmentPoint &Start, const SegmentPoint &End)
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

inline bool IntervalsOverlap(float fLeftLow, float fLeftHigh, float fRightLow, float fRightHigh)
{
  return (fLeftLow <= fRightHigh) && (fRightLow <= fLeftHigh);
}

inline StrengthMetrics MakeEmptyStrength()
{
  StrengthMetrics Strength;
  Strength.fSpace = 0;
  Strength.fSpeed = 0;
  Strength.fDifHeight = 0;
  Strength.fDeaHeight = 0;
  Strength.fMacdArea = 0;
  Strength.bRsiDivergence = false;
  return Strength;
}

inline DivergenceResult MakeEmptyDivergence(int nDirection)
{
  DivergenceResult Result;
  Result.nDirection = nDirection;
  Result.nPreviousStartPoint = -1;
  Result.nPreviousEndPoint = -1;
  Result.nCurrentStartPoint = -1;
  Result.nCurrentEndPoint = -1;
  Result.bNewExtreme = false;
  Result.bWeakSpace = false;
  Result.bWeakSpeed = false;
  Result.bWeakMacd = false;
  Result.bDivergence = false;
  Result.Previous = MakeEmptyStrength();
  Result.Current = MakeEmptyStrength();
  return Result;
}

inline void SetDivergencePointIds(DivergenceResult *pResult,
                                  int nPreviousStartPoint,
                                  int nPreviousEndPoint,
                                  int nCurrentStartPoint,
                                  int nCurrentEndPoint)
{
  if (pResult == 0)
  {
    return;
  }
  pResult->nPreviousStartPoint = nPreviousStartPoint;
  pResult->nPreviousEndPoint = nPreviousEndPoint;
  pResult->nCurrentStartPoint = nCurrentStartPoint;
  pResult->nCurrentEndPoint = nCurrentEndPoint;
}

inline int GetMoveDirection(const SegmentPoint &Start, const SegmentPoint &End)
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

inline bool HasMatchingSmallTurn(const TradingSignalCandidate &C)
{
  if (!IsThirdSignal(C.fSignal))
  {
    return false;
  }
  if ((C.nSource != SIGNAL_SOURCE_THIRD) ||
      (C.nBreakout < 0) ||
      (C.nCenter < 0) ||
      (C.nPoint < 0))
  {
    return false;
  }
  int nSide = GetTradingSignalSide(C.fSignal);
  return (nSide != 0) && (C.nSmallTurn == nSide);
}

void WriteCenterHighSignal(int nCount, float *pOut, const std::vector<Center> &Centers);
void WriteCenterLowSignal(int nCount, float *pOut, const std::vector<Center> &Centers);
void WriteCenterMarkSignal(int nCount, float *pOut, const std::vector<Center> &Centers);
void WriteDivergenceSegmentSignal(int nCount,
                                  float *pOut,
                                  const std::vector<SegmentPoint> &Points,
                                  const std::vector<TradingSignalCandidate> &Candidates);

#endif
