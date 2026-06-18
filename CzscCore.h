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

#ifndef __CZSC_CORE_H__
#define __CZSC_CORE_H__

#include <vector>

enum CzscPointType
{
  CZSC_POINT_BOTTOM = -1,
  CZSC_POINT_NONE   = 0,
  CZSC_POINT_TOP    = 1,
};

enum CzscMovementType
{
  CZSC_MOVEMENT_DOWN          = -1,
  CZSC_MOVEMENT_CONSOLIDATION = 0,
  CZSC_MOVEMENT_UP            = 1,
};

struct KBar
{
  int   nIndex;
  float fHigh;
  float fLow;
};

struct MergedBar
{
  int   nStart;
  int   nEnd;
  int   nHighIndex;
  int   nLowIndex;
  float fHigh;
  float fLow;
};

struct Fractal
{
  int   nType;
  int   nIndex;
  float fHigh;
  float fLow;
};

struct Stroke
{
  Fractal Start;
  Fractal End;
  int     nDirection;
};

struct SegmentPoint
{
  int   nType;
  int   nIndex;
  float fHigh;
  float fLow;
};

struct Center
{
  int   nStart;
  int   nEnd;
  float fHigh;
  float fLow;
};

struct StrengthMetrics
{
  float fSpace;
  float fSpeed;
  float fDifHeight;
  float fDeaHeight;
  float fMacdArea;
  bool  bRsiDivergence;
};

struct DivergenceResult
{
  int             nDirection;
  bool            bNewExtreme;
  bool            bWeakSpace;
  bool            bWeakSpeed;
  bool            bDivergence;
  StrengthMetrics Previous;
  StrengthMetrics Current;
};

struct TrendStructure
{
  int nType;
  int nStart;
  int nEnd;
  int nFirstCenter;
  int nLastCenter;
};

struct CenterBreakout
{
  int  nCenter;
  int  nDirection;
  int  nLeavePoint;
  int  nRetestPoint;
  bool bFirstRetest;
  bool bBackIntoCenter;
  bool bThirdSignal;
  bool bConsolidationDivergence;
  DivergenceResult Divergence;
};

struct TradingSignalCandidate
{
  int   nIndex;
  float fSignal;
  int   nPriority;
  int   nPoint;
  int   nCenter;
  int   nBreakout;
  int   nSource;
  bool  bOverlapped;
  DivergenceResult Divergence;
};

std::vector<MergedBar> BuildMergedBars(int nCount, float *pHigh, float *pLow);
std::vector<Fractal> BuildFractals(const std::vector<MergedBar> &Bars);
std::vector<Stroke> BuildStrokes(const std::vector<Fractal> &Fractals);
std::vector<SegmentPoint> BuildSegmentPoints(const std::vector<Stroke> &Strokes);
std::vector<SegmentPoint> BuildLineSegmentPoints(const std::vector<Stroke> &Strokes);
std::vector<SegmentPoint> BuildSignalPoints(int nCount, float *pIn, float *pHigh, float *pLow);
std::vector<Center> BuildCenters(const std::vector<SegmentPoint> &Points);
std::vector<TrendStructure> BuildTrendStructures(const std::vector<Center> &Centers);
std::vector<CenterBreakout> BuildCenterBreakouts(const std::vector<SegmentPoint> &Points,
                                                 const std::vector<Center> &Centers,
                                                 const std::vector<TrendStructure> &Structures);
std::vector<TradingSignalCandidate> BuildTradingSignalCandidates(const std::vector<SegmentPoint> &Points,
                                                                  const std::vector<Center> &Centers,
                                                                  const std::vector<TrendStructure> &Structures,
                                                                  const std::vector<CenterBreakout> &Breakouts);
StrengthMetrics MeasureStrength(const SegmentPoint &Start, const SegmentPoint &End);
DivergenceResult MeasureDivergence(const SegmentPoint &PrevStart,
                                   const SegmentPoint &PrevEnd,
                                   const SegmentPoint &CurrentStart,
                                   const SegmentPoint &CurrentEnd,
                                   int nDirection);
bool IsWeakerStrength(const StrengthMetrics &Current, const StrengthMetrics &Previous);
void ApplyTradingSignalCandidates(int nCount,
                                  float *pOut,
                                  const std::vector<TradingSignalCandidate> &Candidates);
void WriteSegmentSignal(int nCount, float *pOut, const std::vector<SegmentPoint> &Points);

void Parse1(int nCount, float *pOut, float *pHigh, float *pLow);
void Parse2(int nCount, float *pOut, float *pHigh, float *pLow);

void Func1(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime);
void Func2(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func3(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func4(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func5(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func6(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func7(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func8(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func9(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime);

#endif
