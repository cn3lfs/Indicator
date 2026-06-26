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

enum CzscSignalQuality
{
  CZSC_SIGNAL_QUALITY_WATCH     = 0,
  CZSC_SIGNAL_QUALITY_CONFIRMED = 1,
  CZSC_SIGNAL_QUALITY_STRONG    = 2,
};

enum CzscCenterPosition
{
  CZSC_CENTER_POSITION_BELOW   = -1,
  CZSC_CENTER_POSITION_INSIDE  = 0,
  CZSC_CENTER_POSITION_ABOVE   = 1,
  CZSC_CENTER_POSITION_UNKNOWN = 2,
};

enum CzscCenterRelation
{
  CZSC_CENTER_RELATION_DOWN      = -1,  // 后GG < 前DD：下跌及其延续
  CZSC_CENTER_RELATION_EXTENSION = 0,   // 全幅区间重叠：形成高级别中枢（扩展/扩张）
  CZSC_CENTER_RELATION_UP        = 1,   // 后DD > 前GG：上涨及其延续
};

enum CzscReversalStrength
{
  CZSC_REVERSAL_EXTENSION     = -1,  // 情况一：最后中枢级别扩展（最弱反弹）
  CZSC_REVERSAL_CONSOLIDATION = 0,   // 情况二：更大级别盘整
  CZSC_REVERSAL_TREND         = 1,   // 情况三：反趋势（最强）
  CZSC_REVERSAL_UNKNOWN       = 2,
};

enum CzscCenterAftermath
{
  CZSC_CENTER_AFTERMATH_UNKNOWN  = 0,  // 后续中枢尚未形成或反向异常
  CZSC_CENTER_AFTERMATH_EXTENDED = 1,  // 中枢扩张：与后续中枢形成更大级别中枢
  CZSC_CENTER_AFTERMATH_NEWBORN  = 2,  // 中枢新生：形成同向新中枢（趋势）
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
  float fEnergy;
};

struct Center
{
  int   nStart;
  int   nEnd;
  float fHigh;    // ZG = min(各段高点)，重叠区间上沿
  float fLow;     // ZD = max(各段低点)，重叠区间下沿
  float fTop;     // GG = max(各段高点)，全幅上沿，恒 >= fHigh
  float fBottom;  // DD = min(各段低点)，全幅下沿，恒 <= fLow
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
  bool            bWeakMacd;
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
  int   nQuality;
  int   nCenterPosition;
  int   nReversal;
  int   nAfterEffect;
  bool  bOverlapped;
  DivergenceResult Divergence;
};

std::vector<float> ComputeMacdHistogram(int nCount, const float *pPrice);
void AssignSegmentEnergy(std::vector<SegmentPoint> &Points, int nCount, const float *pHigh, const float *pLow);

std::vector<MergedBar> BuildMergedBars(int nCount, float *pHigh, float *pLow);
std::vector<Fractal> BuildFractals(const std::vector<MergedBar> &Bars);
std::vector<Stroke> BuildStrokes(const std::vector<Fractal> &Fractals);
std::vector<SegmentPoint> BuildSegmentPoints(const std::vector<Stroke> &Strokes);
std::vector<SegmentPoint> BuildLineSegmentPoints(const std::vector<Stroke> &Strokes);
std::vector<SegmentPoint> BuildSignalPoints(int nCount, float *pIn, float *pHigh, float *pLow);
std::vector<Center> BuildCenters(const std::vector<SegmentPoint> &Points);
std::vector<TrendStructure> BuildTrendStructures(const std::vector<Center> &Centers);
int ClassifyCenterRelation(const Center &Prev, const Center &Next);
int ClassifyCenterAftermath(const std::vector<Center> &Centers, int nCenter, float fSignal);
int ClassifyReversalStrength(const std::vector<SegmentPoint> &Points,
                             const std::vector<Center> &Centers,
                             int nPoint,
                             int nCenter,
                             float fSignal);
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
void ApplyTradingSignalQuality(int nCount,
                               float *pOut,
                               const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalReversal(int nCount,
                                float *pOut,
                                const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalAftermath(int nCount,
                                 float *pOut,
                                 const std::vector<TradingSignalCandidate> &Candidates);
void WriteSegmentSignal(int nCount, float *pOut, const std::vector<SegmentPoint> &Points);
void WriteCenterRelationSignal(int nCount, float *pOut, const std::vector<Center> &Centers);

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
void Func10(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func11(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func12(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func13(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func14(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);

#endif
