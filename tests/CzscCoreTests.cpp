#include "../CzscCore.h"
#include "SseIndexDaily.h"
#include <cstring>

static bool NearlyEqual(float a, float b)
{
  float fDiff = a - b;
  if (fDiff < 0)
  {
    fDiff = -fDiff;
  }
  return fDiff < 0.0001f;
}

static bool TestOutputIsCleared()
{
  const int nCount = 4;
  float pIn[nCount] = {0, 0, 0, 0};
  float pHigh[nCount] = {10, 11, 12, 13};
  float pLow[nCount] = {5, 6, 7, 8};
  float pOut[nCount] = {-1, -1, -1, -1};

  Func5(nCount, pOut, pIn, pHigh, pLow);

  for (int i = 0; i < nCount; i++)
  {
    if (pOut[i] != 0)
    {
      return false;
    }
  }

  return true;
}

static bool TestMergedBarsHandleInclusion()
{
  const int nCount = 4;
  float pHigh[nCount] = {10, 9, 11, 12};
  float pLow[nCount] = {5, 6, 7, 8};

  std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);

  if (Bars.size() != 3)
  {
    return false;
  }
  if ((Bars[0].nStart != 0) || (Bars[0].nEnd != 1))
  {
    return false;
  }
  if (!NearlyEqual(Bars[0].fHigh, 10.0f) || !NearlyEqual(Bars[0].fLow, 6.0f))
  {
    return false;
  }

  return true;
}

static bool TestMergedBarsTrackExtremeIndexes()
{
  {
    const int nCount = 3;
    float pHigh[nCount] = {10, 12, 11};
    float pLow[nCount] = {5, 7, 8};
    std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);

    if (Bars.size() != 2) return false;
    if ((Bars[1].nStart != 1) || (Bars[1].nEnd != 2)) return false;
    if (!NearlyEqual(Bars[1].fHigh, 12) || !NearlyEqual(Bars[1].fLow, 8)) return false;
    if ((Bars[1].nHighIndex != 1) || (Bars[1].nLowIndex != 2)) return false;
  }

  {
    const int nCount = 3;
    float pHigh[nCount] = {12, 10, 11};
    float pLow[nCount] = {8, 6, 5};
    std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);

    if (Bars.size() != 2) return false;
    if ((Bars[1].nStart != 1) || (Bars[1].nEnd != 2)) return false;
    if (!NearlyEqual(Bars[1].fHigh, 10) || !NearlyEqual(Bars[1].fLow, 5)) return false;
    if ((Bars[1].nHighIndex != 1) || (Bars[1].nLowIndex != 2)) return false;
  }

  return true;
}

static bool TestMergedBarsApplySequentialInclusionDirection()
{
  {
    const int nCount = 4;
    float pHigh[nCount] = {10, 12, 11, 11.5f};
    float pLow[nCount] = {5, 7, 8, 9};
    std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);

    if (Bars.size() != 2) return false;
    if ((Bars[1].nStart != 1) || (Bars[1].nEnd != 3)) return false;
    if (!NearlyEqual(Bars[1].fHigh, 12) || !NearlyEqual(Bars[1].fLow, 9)) return false;
    if ((Bars[1].nHighIndex != 1) || (Bars[1].nLowIndex != 3)) return false;
  }

  {
    const int nCount = 4;
    float pHigh[nCount] = {12, 10, 11, 9};
    float pLow[nCount] = {8, 6, 5, 5.5f};
    std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);

    if (Bars.size() != 2) return false;
    if ((Bars[1].nStart != 1) || (Bars[1].nEnd != 3)) return false;
    if (!NearlyEqual(Bars[1].fHigh, 9) || !NearlyEqual(Bars[1].fLow, 5)) return false;
    if ((Bars[1].nHighIndex != 3) || (Bars[1].nLowIndex != 2)) return false;
  }

  return true;
}

static MergedBar MakeTestMergedBar(int nStart, int nEnd,
                                   int nHighIndex, int nLowIndex,
                                   float fHigh, float fLow)
{
  MergedBar Bar;
  Bar.nStart = nStart;
  Bar.nEnd = nEnd;
  Bar.nHighIndex = nHighIndex;
  Bar.nLowIndex = nLowIndex;
  Bar.fHigh = fHigh;
  Bar.fLow = fLow;
  return Bar;
}

static bool TestFractalsUseMergedExtremeIndexes()
{
  {
    std::vector<MergedBar> Bars;
    Bars.push_back(MakeTestMergedBar(0, 0, 0, 0, 9, 4));
    Bars.push_back(MakeTestMergedBar(1, 2, 1, 2, 12, 8));
    Bars.push_back(MakeTestMergedBar(3, 3, 3, 3, 10, 7));

    std::vector<Fractal> Fractals = BuildFractals(Bars);
    if ((Fractals.size() != 1) ||
        (Fractals[0].nType != CZSC_POINT_TOP) ||
        (Fractals[0].nIndex != 1) ||
        (Fractals[0].nMergedIndex != 1))
    {
      return false;
    }
  }

  {
    std::vector<MergedBar> Bars;
    Bars.push_back(MakeTestMergedBar(0, 0, 0, 0, 12, 10));
    Bars.push_back(MakeTestMergedBar(1, 2, 1, 2, 9, 4));
    Bars.push_back(MakeTestMergedBar(3, 3, 3, 3, 10, 7));

    std::vector<Fractal> Fractals = BuildFractals(Bars);
    if ((Fractals.size() != 1) ||
        (Fractals[0].nType != CZSC_POINT_BOTTOM) ||
        (Fractals[0].nIndex != 2) ||
        (Fractals[0].nMergedIndex != 1))
    {
      return false;
    }
  }

  return true;
}

static bool TestFractalsAndStrokes()
{
  const int nCount = 7;
  float pHigh[nCount] = {10, 12, 11, 10, 9, 8, 10};
  float pLow[nCount] = {5, 7, 6, 5, 4, 2, 4};

  std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  std::vector<SegmentPoint> Points = BuildSegmentPoints(Strokes);

  if (Fractals.size() != 2)
  {
    return false;
  }
  if ((Fractals[0].nType != CZSC_POINT_TOP) || (Fractals[0].nIndex != 1))
  {
    return false;
  }
  if ((Fractals[1].nType != CZSC_POINT_BOTTOM) || (Fractals[1].nIndex != 5))
  {
    return false;
  }
  if (Strokes.size() != 1)
  {
    return false;
  }
  if ((Strokes[0].Start.nIndex != 1) || (Strokes[0].End.nIndex != 5))
  {
    return false;
  }
  if (Points.size() != 2)
  {
    return false;
  }

  return true;
}

static bool TestStrokeRequiresFiveBars()
{
  Fractal Top;
  Top.nType = CZSC_POINT_TOP;
  Top.nIndex = 1;
  Top.nMergedIndex = 1;
  Top.fHigh = 12;
  Top.fLow = 7;

  Fractal Bottom;
  Bottom.nType = CZSC_POINT_BOTTOM;
  Bottom.nIndex = 3;
  Bottom.nMergedIndex = 3;
  Bottom.fHigh = 10;
  Bottom.fLow = 4;

  std::vector<Fractal> Fractals;
  Fractals.push_back(Top);
  Fractals.push_back(Bottom);

  std::vector<Stroke> Strokes = BuildStrokes(Fractals);

  return Strokes.empty();
}

//=============================================================================
// 真实数据测试：上证指数(000001.SH)日线，验证笔/线段算法的结构性质（见 SseIndexDaily.h）
//=============================================================================

static int FindSseDateIndex(const char *pDate)
{
  for (int i = 0; i < SSE_DAILY_COUNT; i++)
  {
    if (std::strcmp(SSE_DAILY_DATE[i], pDate) == 0)
    {
      return i;
    }
  }
  return -1;
}

static float TestPointPrice(const SegmentPoint &Point)
{
  return (Point.nType == CZSC_POINT_TOP) ? Point.fHigh : Point.fLow;
}

static bool TestIntervalsOverlap(float fLeftLow, float fLeftHigh, float fRightLow, float fRightHigh)
{
  return (fLeftLow <= fRightHigh) && (fRightLow <= fLeftHigh);
}

static bool TestFirstThreeStrokePointsOverlap(const std::vector<SegmentPoint> &Points, std::size_t nStart)
{
  if (nStart + 3 >= Points.size())
  {
    return false;
  }

  float fLow = TestPointPrice(Points[nStart]);
  float fHigh = TestPointPrice(Points[nStart + 1]);
  if (fLow > fHigh)
  {
    float fSwap = fLow;
    fLow = fHigh;
    fHigh = fSwap;
  }

  for (std::size_t i = nStart + 1; i < nStart + 3; i++)
  {
    float fA = TestPointPrice(Points[i]);
    float fB = TestPointPrice(Points[i + 1]);
    float fSegLow = (fA < fB) ? fA : fB;
    float fSegHigh = (fA > fB) ? fA : fB;
    if (!TestIntervalsOverlap(fLow, fHigh, fSegLow, fSegHigh))
    {
      return false;
    }
    if (fSegLow > fLow)
    {
      fLow = fSegLow;
    }
    if (fSegHigh < fHigh)
    {
      fHigh = fSegHigh;
    }
  }

  return fLow <= fHigh;
}

static bool TestRealSseMergedBarsAreWellFormed()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  std::vector<MergedBar> Bars = BuildMergedBars(SSE_DAILY_COUNT, pH, pL);

  if (Bars.size() < 10)
  {
    return false;
  }
  for (std::size_t i = 0; i < Bars.size(); i++)
  {
    const MergedBar &B = Bars[i];
    if ((B.nStart > B.nEnd) ||
        (B.nHighIndex < B.nStart) || (B.nHighIndex > B.nEnd) ||
        (B.nLowIndex < B.nStart) || (B.nLowIndex > B.nEnd))
    {
      return false;
    }
    if (!NearlyEqual(B.fHigh, SSE_DAILY_HIGH[B.nHighIndex]) ||
        !NearlyEqual(B.fLow, SSE_DAILY_LOW[B.nLowIndex]))
    {
      return false;
    }
    if (i > 0)
    {
      const MergedBar &P = Bars[i - 1];
      bool bIncluded = ((B.fHigh <= P.fHigh) && (B.fLow >= P.fLow)) ||
                       ((B.fHigh >= P.fHigh) && (B.fLow <= P.fLow));
      if (bIncluded)
      {
        return false;
      }
    }
  }
  return true;
}

// 笔结构良好：顶底异型、方向正确、严格笔合并K线跨度≥4（含顶底≥5根合并K线）、首尾相接且方向交替
static bool TestRealSseStrokesWellFormed()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  std::vector<MergedBar> Bars = BuildMergedBars(SSE_DAILY_COUNT, pH, pL);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);  // 默认严格笔

  if (Strokes.size() < 10)
  {
    return false;  // 随仓日线样本应有足够多笔
  }
  for (std::size_t i = 0; i < Strokes.size(); i++)
  {
    const Stroke &S = Strokes[i];
    if (S.Start.nType == S.End.nType)
    {
      return false;  // 顶底异型
    }
    int nExpDir = (S.Start.nType == CZSC_POINT_BOTTOM) ? 1 : -1;
    if (S.nDirection != nExpDir)
    {
      return false;  // 方向：底→顶=+1、顶→底=-1
    }
    if ((S.End.nMergedIndex - S.Start.nMergedIndex) < 4)
    {
      return false;  // 严格笔：处理后≥5根合并K线
    }
    if (i > 0)
    {
      if (Strokes[i - 1].End.nIndex != S.Start.nIndex)
      {
        return false;  // 相邻笔首尾相接
      }
      if (Strokes[i - 1].nDirection == S.nDirection)
      {
        return false;  // 相邻笔方向相反
      }
    }
  }
  return true;
}

// 线段是更高级别：线段端点必落在某个笔端点上、顶底交替、且数量不多于笔端点
static bool TestRealSseSegmentsSubsetOfStrokes()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  std::vector<MergedBar> Bars = BuildMergedBars(SSE_DAILY_COUNT, pH, pL);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  std::vector<SegmentPoint> StrokePts = BuildSegmentPoints(Strokes);
  std::vector<SegmentPoint> SegPts = BuildLineSegmentPointsByFeature(Strokes);

  if (SegPts.size() < 2)
  {
    return false;  // 随仓日线样本应有若干线段
  }
  if (SegPts.size() > StrokePts.size())
  {
    return false;  // 线段不多于笔
  }
  std::size_t nStrokePrev = 0;
  for (std::size_t i = 0; i < SegPts.size(); i++)
  {
    bool bOnStroke = false;
    std::size_t nStrokeIndex = 0;
    for (std::size_t j = 0; j < StrokePts.size(); j++)
    {
      if (StrokePts[j].nIndex == SegPts[i].nIndex)
      {
        bOnStroke = true;
        nStrokeIndex = j;
        break;
      }
    }
    if (!bOnStroke)
    {
      return false;  // 线段端点必落在笔端点上
    }
    if ((StrokePts[nStrokeIndex].nType != SegPts[i].nType) ||
        !NearlyEqual(StrokePts[nStrokeIndex].fHigh, SegPts[i].fHigh) ||
        !NearlyEqual(StrokePts[nStrokeIndex].fLow, SegPts[i].fLow))
    {
      return false;  // 线段端点必须复用同一个笔端点，而不只是同一天
    }
    if ((i > 0) && (SegPts[i].nType == SegPts[i - 1].nType))
    {
      return false;  // 线段端点顶底交替
    }
    if ((i > 0) && (nStrokeIndex <= nStrokePrev))
    {
      return false;  // 线段端点顺序必须沿笔端点推进
    }
    if ((i > 0) && ((nStrokeIndex - nStrokePrev) < 3))
    {
      return false;  // 相邻线段端点之间至少三笔
    }
    if ((i > 0) && !TestFirstThreeStrokePointsOverlap(StrokePts, nStrokePrev))
    {
      return false;  // 每个线段前三笔必须有重叠
    }
    nStrokePrev = nStrokeIndex;
  }
  return true;
}

// 新笔（合并≥4 且 原始≥5，放宽）的笔数不少于严格笔（合并≥5）
static bool TestRealSseNewBiNotFewerThanStrict()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  std::vector<MergedBar> Bars = BuildMergedBars(SSE_DAILY_COUNT, pH, pL);
  std::vector<Fractal> Fractals = BuildFractals(Bars);

  std::vector<Stroke> Strict = BuildStrokes(Fractals);     // 默认严格笔
  CzscConfig NewBi = DefaultConfig();
  NewBi.nStrokeType = CZSC_STROKE_NEW;
  std::vector<Stroke> New = BuildStrokes(Fractals, NewBi);  // 新笔

  return (New.size() >= Strict.size()) && !Strict.empty();
}

// 真实上证：买卖点结构良好——编码合法；三买/三卖都存在（曾因中枢首尾相连而全无）；
// 一类经「每中枢区域一个」去重后不泛滥（8 年日线远少于笔数）
static bool TestRealSseSignalsWellFormed()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  std::vector<MergedBar> Bars = BuildMergedBars(SSE_DAILY_COUNT, pH, pL);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  std::vector<SegmentPoint> SP = BuildSegmentPoints(Strokes);
  std::vector<Center> Ce = BuildCenters(SP);
  std::vector<TrendStructure> Tr = BuildTrendStructures(Ce);
  std::vector<CenterBreakout> Bk = BuildCenterBreakouts(SP, Ce, Tr);
  std::vector<TradingSignalCandidate> Ca = BuildTradingSignalCandidates(SP, Ce, Tr, Bk);

  int nFirst = 0;
  int nThirdBuy = 0;
  int nThirdSell = 0;
  for (std::size_t i = 0; i < Ca.size(); i++)
  {
    float s = Ca[i].fSignal;
    bool bValid = NearlyEqual(s, 1) || NearlyEqual(s, 2) || NearlyEqual(s, 3) ||
                  NearlyEqual(s, 11) || NearlyEqual(s, 12) || NearlyEqual(s, 13);
    if (!bValid)
    {
      return false;  // 信号编码须合法
    }
    if (NearlyEqual(s, 1) || NearlyEqual(s, 11)) nFirst++;
    if (NearlyEqual(s, 3)) nThirdBuy++;
    if (NearlyEqual(s, 13)) nThirdSell++;
  }
  if ((nThirdBuy == 0) || (nThirdSell == 0))
  {
    return false;  // 三买/三卖须都能检出
  }
  if (nFirst > 15)
  {
    return false;  // 一类去重后不泛滥
  }
  return true;
}

struct SseCandidateSummary
{
  int nFirstBuy;
  int nSecondBuy;
  int nThirdBuy;
  int nFirstSell;
  int nSecondSell;
  int nThirdSell;
  int nStrong;
  int nAbc;
  int nZeroPull;
  int nLineWeak;
  int nStandard;
  int nSmallTurn;
  int nOverlapped;
  int nBreakout;
};

static SseCandidateSummary CountSseCandidateSummary(const std::vector<TradingSignalCandidate> &Candidates)
{
  SseCandidateSummary S;
  S.nFirstBuy = 0;
  S.nSecondBuy = 0;
  S.nThirdBuy = 0;
  S.nFirstSell = 0;
  S.nSecondSell = 0;
  S.nThirdSell = 0;
  S.nStrong = 0;
  S.nAbc = 0;
  S.nZeroPull = 0;
  S.nLineWeak = 0;
  S.nStandard = 0;
  S.nSmallTurn = 0;
  S.nOverlapped = 0;
  S.nBreakout = 0;

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (NearlyEqual(C.fSignal, 1.0f))
    {
      S.nFirstBuy++;
    }
    else if (NearlyEqual(C.fSignal, 2.0f))
    {
      S.nSecondBuy++;
    }
    else if (NearlyEqual(C.fSignal, 3.0f))
    {
      S.nThirdBuy++;
    }
    else if (NearlyEqual(C.fSignal, 11.0f))
    {
      S.nFirstSell++;
    }
    else if (NearlyEqual(C.fSignal, 12.0f))
    {
      S.nSecondSell++;
    }
    else if (NearlyEqual(C.fSignal, 13.0f))
    {
      S.nThirdSell++;
    }

    int nCtx = BuildTradingSignalContextFlags(C);
    if ((nCtx & CZSC_SIGNAL_CTX_STRONG_QUALITY) != 0)
    {
      S.nStrong++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_ABC_STRUCTURE) != 0)
    {
      S.nAbc++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_MACD_ZERO_PULL) != 0)
    {
      S.nZeroPull++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_MACD_LINE_WEAK) != 0)
    {
      S.nLineWeak++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_STANDARD_DIV) != 0)
    {
      S.nStandard++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_SMALL_TURN) != 0)
    {
      S.nSmallTurn++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_OVERLAPPED) != 0)
    {
      S.nOverlapped++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_CENTER_BREAKOUT) != 0)
    {
      S.nBreakout++;
    }
  }

  return S;
}

static bool CheckSseCandidateSummary(const SseCandidateSummary &S,
                                     int nFirstBuy,
                                     int nSecondBuy,
                                     int nThirdBuy,
                                     int nFirstSell,
                                     int nSecondSell,
                                     int nThirdSell,
                                     int nStrong,
                                     int nAbc,
                                     int nZeroPull,
                                     int nLineWeak,
                                     int nStandard,
                                     int nSmallTurn,
                                     int nOverlapped,
                                     int nBreakout)
{
  return (S.nFirstBuy == nFirstBuy) &&
         (S.nSecondBuy == nSecondBuy) &&
         (S.nThirdBuy == nThirdBuy) &&
         (S.nFirstSell == nFirstSell) &&
         (S.nSecondSell == nSecondSell) &&
         (S.nThirdSell == nThirdSell) &&
         (S.nStrong == nStrong) &&
         (S.nAbc == nAbc) &&
         (S.nZeroPull == nZeroPull) &&
         (S.nLineWeak == nLineWeak) &&
         (S.nStandard == nStandard) &&
         (S.nSmallTurn == nSmallTurn) &&
         (S.nOverlapped == nOverlapped) &&
         (S.nBreakout == nBreakout);
}

static bool TestRealSseDiagnosticCounts()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  std::vector<MergedBar> Bars = BuildMergedBars(SSE_DAILY_COUNT, pH, pL);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);

  CzscAnalyzer StrokeAn;
  BuildAnalyzerFromPrice(StrokeAn, SSE_DAILY_COUNT, pH, pL, DefaultConfig());

  CzscConfig SegmentConfig = DefaultConfig();
  SegmentConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
  SegmentConfig.nSegmentMethod = CZSC_SEG_FEATURE;
  CzscAnalyzer SegmentAn;
  BuildAnalyzerFromPrice(SegmentAn, SSE_DAILY_COUNT, pH, pL, SegmentConfig);

  SseCandidateSummary StrokeSummary = CountSseCandidateSummary(StrokeAn.Candidates);
  SseCandidateSummary SegmentSummary = CountSseCandidateSummary(SegmentAn.Candidates);

  return (Strokes.size() == 157) &&
         (StrokeAn.Points.size() == 158) &&
         (SegmentAn.Points.size() == 15) &&
         (StrokeAn.Centers.size() == 18) &&
         (SegmentAn.Centers.size() == 2) &&
         (StrokeAn.Candidates.size() == 17) &&
         (SegmentAn.Candidates.size() == 2) &&
         CheckSseCandidateSummary(StrokeSummary, 0, 0, 9, 0, 0, 8,
                                  5, 0, 0, 3, 0, 0, 0, 17) &&
         CheckSseCandidateSummary(SegmentSummary, 0, 0, 1, 0, 0, 1,
                                  1, 0, 0, 0, 0, 0, 0, 2);
}

static bool TestRealSsePricePointsStayOnStrictStrokeEndpoints()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  std::vector<MergedBar> Bars = BuildMergedBars(SSE_DAILY_COUNT, pH, pL);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  std::vector<SegmentPoint> StrokePoints = BuildSegmentPoints(Strokes);
  std::vector<SegmentPoint> ConfigPoints = BuildConfiguredPoints(SSE_DAILY_COUNT, pH, pL, DefaultConfig());

  if (StrokePoints.size() != ConfigPoints.size())
  {
    return false;
  }

  for (std::size_t i = 0; i < StrokePoints.size(); i++)
  {
    if ((StrokePoints[i].nIndex != ConfigPoints[i].nIndex) ||
        (StrokePoints[i].nType != ConfigPoints[i].nType) ||
        !NearlyEqual(StrokePoints[i].fHigh, ConfigPoints[i].fHigh) ||
        !NearlyEqual(StrokePoints[i].fLow, ConfigPoints[i].fLow))
    {
      return false;
    }
  }

  return true;
}

static bool TestRealSseFirstCenterStopsBeforeLeave()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  CzscConfig Config = DefaultConfig();
  std::vector<SegmentPoint> Points = BuildConfiguredPoints(SSE_DAILY_COUNT, pH, pL, Config);
  std::vector<Center> Centers = BuildCenters(Points);

  int nZ00Start = FindSseDateIndex("2018-02-26");
  int nZ00End = FindSseDateIndex("2018-07-06");
  if ((Centers.empty()) || (nZ00Start < 0) || (nZ00End < 0))
  {
    return false;
  }

  if ((Centers[0].nStart != nZ00Start) || (Centers[0].nEnd != nZ00End))
  {
    return false;
  }
  if (!NearlyEqual(Centers[0].fHigh, 3128.72f) || !NearlyEqual(Centers[0].fLow, 3091.46f))
  {
    return false;
  }

  return true;
}

static bool CheckSseCenter(const Center &C,
                           int nDirection,
                           const char *pStart,
                           const char *pEnd,
                           float fHigh,
                           float fLow)
{
  int nStart = FindSseDateIndex(pStart);
  int nEnd = FindSseDateIndex(pEnd);
  if ((nStart < 0) || (nEnd < 0))
  {
    return false;
  }
  return (C.nDirection == nDirection) &&
         (C.nStart == nStart) &&
         (C.nEnd == nEnd) &&
         ((C.fHigh - fHigh < 0.011f) && (fHigh - C.fHigh < 0.011f)) &&
         ((C.fLow - fLow < 0.011f) && (fLow - C.fLow < 0.011f));
}

static bool ContainsSseCenter(const std::vector<Center> &Centers,
                              int nDirection,
                              const char *pStart,
                              const char *pEnd,
                              float fHigh,
                              float fLow)
{
  for (std::size_t i = 0; i < Centers.size(); i++)
  {
    if (CheckSseCenter(Centers[i], nDirection, pStart, pEnd, fHigh, fLow))
    {
      return true;
    }
  }
  return false;
}

struct SseCandidateExpectation
{
  const char *pDate;
  float fSignal;
  int nQuality;
  int nCenter;
  int nTrend;
  int nMovementType;
  int nPoint;
  int nBreakout;
  int nCenterPosition;
  int nAfterEffect;
  int nContextFlags;
};

struct SseBreakoutExpectation
{
  int nBreakout;
  int nCenter;
  int nDirection;
  int nLeavePoint;
  const char *pLeaveDate;
  int nRetestPoint;
  const char *pRetestDate;
  bool bFirstRetest;
  bool bBackIntoCenter;
  bool bThirdSignal;
};

static bool ContainsSseCandidate(const std::vector<TradingSignalCandidate> &Candidates,
                                 const SseCandidateExpectation &E)
{
  int nIndex = FindSseDateIndex(E.pDate);
  if (nIndex < 0)
  {
    return false;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex == nIndex) && NearlyEqual(C.fSignal, E.fSignal))
    {
      return (C.nQuality == E.nQuality) &&
             (C.nCenter == E.nCenter) &&
             (C.nTrend == E.nTrend) &&
             (C.nMovementType == E.nMovementType) &&
             (C.nPoint == E.nPoint) &&
             (C.nBreakout == E.nBreakout) &&
             (C.nCenterPosition == E.nCenterPosition) &&
             (C.nAfterEffect == E.nAfterEffect) &&
             (BuildTradingSignalContextFlags(C) == E.nContextFlags);
    }
  }
  return false;
}

static bool ContainsAllSseCandidates(const std::vector<TradingSignalCandidate> &Candidates,
                                     const SseCandidateExpectation *pExpected,
                                     std::size_t nExpected)
{
  if (Candidates.size() != nExpected)
  {
    return false;
  }

  for (std::size_t i = 0; i < nExpected; i++)
  {
    if (!ContainsSseCandidate(Candidates, pExpected[i]))
    {
      return false;
    }
  }
  return true;
}

static bool CheckSseBreakout(const std::vector<SegmentPoint> &Points,
                             const std::vector<CenterBreakout> &Breakouts,
                             const SseBreakoutExpectation &E)
{
  if ((E.nBreakout < 0) || ((std::size_t)E.nBreakout >= Breakouts.size()) ||
      (E.nLeavePoint < 0) || ((std::size_t)E.nLeavePoint >= Points.size()) ||
      (E.nRetestPoint < 0) || ((std::size_t)E.nRetestPoint >= Points.size()))
  {
    return false;
  }

  int nLeaveIndex = FindSseDateIndex(E.pLeaveDate);
  int nRetestIndex = FindSseDateIndex(E.pRetestDate);
  if ((nLeaveIndex < 0) || (nRetestIndex < 0))
  {
    return false;
  }

  const CenterBreakout &B = Breakouts[(std::size_t)E.nBreakout];
  return (B.nCenter == E.nCenter) &&
         (B.nDirection == E.nDirection) &&
         (B.nLeavePoint == E.nLeavePoint) &&
         (B.nRetestPoint == E.nRetestPoint) &&
         (B.bFirstRetest == E.bFirstRetest) &&
         (B.bBackIntoCenter == E.bBackIntoCenter) &&
         (B.bThirdSignal == E.bThirdSignal) &&
         (Points[(std::size_t)E.nLeavePoint].nIndex == nLeaveIndex) &&
         (Points[(std::size_t)E.nRetestPoint].nIndex == nRetestIndex);
}

static bool CheckAllSseBreakouts(const std::vector<SegmentPoint> &Points,
                                 const std::vector<CenterBreakout> &Breakouts,
                                 const SseBreakoutExpectation *pExpected,
                                 std::size_t nExpected)
{
  for (std::size_t i = 0; i < nExpected; i++)
  {
    if (!CheckSseBreakout(Points, Breakouts, pExpected[i]))
    {
      return false;
    }
  }
  return true;
}

static bool CentersAreStrictlySeparated(const std::vector<Center> &Centers)
{
  for (std::size_t i = 1; i < Centers.size(); i++)
  {
    if (Centers[i].nStart <= Centers[i - 1].nEnd)
    {
      return false;
    }
  }
  return true;
}

static bool TestRealSseGoldenCentersPresent()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  std::vector<SegmentPoint> Points = BuildConfiguredPoints(SSE_DAILY_COUNT, pH, pL, DefaultConfig());
  std::vector<Center> Centers = BuildCenters(Points);

  if (Centers.size() < 6)
  {
    return false;
  }

  return ContainsSseCenter(Centers, 1, "2018-02-26", "2018-07-06", 3128.72f, 3091.46f) &&
         ContainsSseCenter(Centers, 1, "2018-07-12", "2018-11-30", 2676.48f, 2653.11f) &&
         ContainsSseCenter(Centers, -1, "2019-01-04", "2019-05-10", 3125.02f, 2987.77f) &&
         ContainsSseCenter(Centers, 1, "2019-05-17", "2020-03-19", 2922.91f, 2891.54f) &&
         ContainsSseCenter(Centers, 1, "2020-04-10", "2020-07-09", 2833.02f, 2802.47f) &&
         ContainsSseCenter(Centers, -1, "2020-07-27", "2021-01-25", 3350.59f, 3325.17f);
}

static bool TestRealSseCentersDoNotShareEndpoints()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  std::vector<SegmentPoint> StrokePoints = BuildConfiguredPoints(SSE_DAILY_COUNT, pH, pL, DefaultConfig());
  std::vector<Center> StrokeCenters = BuildCenters(StrokePoints);

  CzscConfig SegmentConfig = DefaultConfig();
  SegmentConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
  SegmentConfig.nSegmentMethod = CZSC_SEG_FEATURE;
  std::vector<SegmentPoint> SegmentPoints = BuildConfiguredPoints(SSE_DAILY_COUNT, pH, pL, SegmentConfig);
  std::vector<Center> SegmentCenters = BuildCenters(SegmentPoints);

  return CentersAreStrictlySeparated(StrokeCenters) &&
         CentersAreStrictlySeparated(SegmentCenters);
}

static bool TestRealSseGoldenSegmentCentersPresent()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  CzscConfig Config = DefaultConfig();
  Config.nCenterUnit = CZSC_UNIT_SEGMENT;
  Config.nSegmentMethod = CZSC_SEG_FEATURE;
  std::vector<SegmentPoint> Points = BuildConfiguredPoints(SSE_DAILY_COUNT, pH, pL, Config);
  std::vector<Center> Centers = BuildCenters(Points);

  if (Centers.size() != 2)
  {
    return false;
  }

  return ContainsSseCenter(Centers, 1, "2018-11-19", "2020-07-09", 2822.19f, 2822.19f) &&
         ContainsSseCenter(Centers, -1, "2020-09-25", "2023-06-26", 3418.95f, 3312.72f);
}

struct CenterLifecycleCounts
{
  int nExtension;
  int nExpansion;
  int nNewbornUp;
  int nNewbornDown;
  int nUnknown;
};

static CenterLifecycleCounts CountCenterLifecycles(const std::vector<Center> &Centers)
{
  CenterLifecycleCounts Counts = {0, 0, 0, 0, 0};
  for (std::size_t i = 1; i < Centers.size(); i++)
  {
    int nLifecycle = ClassifyCenterLifecycle(Centers[i - 1], Centers[i]);
    if (nLifecycle == CZSC_CENTER_LIFECYCLE_EXTENSION)
    {
      Counts.nExtension++;
    }
    else if (nLifecycle == CZSC_CENTER_LIFECYCLE_EXPANSION)
    {
      Counts.nExpansion++;
    }
    else if (nLifecycle == CZSC_CENTER_LIFECYCLE_NEWBORN_UP)
    {
      Counts.nNewbornUp++;
    }
    else if (nLifecycle == CZSC_CENTER_LIFECYCLE_NEWBORN_DOWN)
    {
      Counts.nNewbornDown++;
    }
    else
    {
      Counts.nUnknown++;
    }
  }
  return Counts;
}

static bool TestRealSseRecursiveCenterLifecycleCounts()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);

  CzscAnalyzer StrokeAn;
  BuildAnalyzerFromPrice(StrokeAn, SSE_DAILY_COUNT, pH, pL, DefaultConfig());

  CzscConfig SegmentConfig = DefaultConfig();
  SegmentConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
  SegmentConfig.nSegmentMethod = CZSC_SEG_FEATURE;
  CzscAnalyzer SegmentAn;
  BuildAnalyzerFromPrice(SegmentAn, SSE_DAILY_COUNT, pH, pL, SegmentConfig);

  CenterLifecycleCounts StrokeCounts = CountCenterLifecycles(StrokeAn.Centers);
  CenterLifecycleCounts SegmentCounts = CountCenterLifecycles(SegmentAn.Centers);

  return (StrokeAn.Centers.size() == 18) &&
         (StrokeCounts.nExtension == 1) &&
         (StrokeCounts.nExpansion == 16) &&
         (StrokeCounts.nNewbornUp == 0) &&
         (StrokeCounts.nNewbornDown == 0) &&
         (StrokeCounts.nUnknown == 0) &&
         (SegmentAn.Centers.size() == 2) &&
         (SegmentCounts.nExtension == 0) &&
         (SegmentCounts.nExpansion == 1) &&
         (SegmentCounts.nNewbornUp == 0) &&
         (SegmentCounts.nNewbornDown == 0) &&
         (SegmentCounts.nUnknown == 0);
}

static bool TestRecentSseRecursiveCenterLifecycleCounts()
{
  int nStart = FindSseDateIndex("2024-01-02");
  int nEnd = FindSseDateIndex("2026-06-26");
  if ((nStart < 0) || (nEnd < nStart))
  {
    return false;
  }

  int nCount = nEnd - nStart + 1;
  float *pH = const_cast<float *>(SSE_DAILY_HIGH + nStart);
  float *pL = const_cast<float *>(SSE_DAILY_LOW + nStart);

  CzscAnalyzer StrokeAn;
  BuildAnalyzerFromPrice(StrokeAn, nCount, pH, pL, DefaultConfig());

  CzscConfig SegmentConfig = DefaultConfig();
  SegmentConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
  SegmentConfig.nSegmentMethod = CZSC_SEG_FEATURE;
  CzscAnalyzer SegmentAn;
  BuildAnalyzerFromPrice(SegmentAn, nCount, pH, pL, SegmentConfig);

  CenterLifecycleCounts StrokeCounts = CountCenterLifecycles(StrokeAn.Centers);
  CenterLifecycleCounts SegmentCounts = CountCenterLifecycles(SegmentAn.Centers);

  return (StrokeAn.Centers.size() == 6) &&
         (StrokeCounts.nExtension == 1) &&
         (StrokeCounts.nExpansion == 4) &&
         (StrokeCounts.nNewbornUp == 0) &&
         (StrokeCounts.nNewbornDown == 0) &&
         (StrokeCounts.nUnknown == 0) &&
         SegmentAn.Centers.empty() &&
         (SegmentCounts.nExtension == 0) &&
         (SegmentCounts.nExpansion == 0) &&
         (SegmentCounts.nNewbornUp == 0) &&
         (SegmentCounts.nNewbornDown == 0) &&
         (SegmentCounts.nUnknown == 0);
}

static bool TestRealSseGoldenCandidatesPresent()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);

  CzscAnalyzer StrokeAn;
  BuildAnalyzerFromPrice(StrokeAn, SSE_DAILY_COUNT, pH, pL, DefaultConfig());

  CzscConfig SegmentConfig = DefaultConfig();
  SegmentConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
  SegmentConfig.nSegmentMethod = CZSC_SEG_FEATURE;
  CzscAnalyzer SegmentAn;
  BuildAnalyzerFromPrice(SegmentAn, SSE_DAILY_COUNT, pH, pL, SegmentConfig);

  static const SseCandidateExpectation StrokeExpected[] = {
    {"2018-07-12", 13.0f, 1, 0, 0, CZSC_MOVEMENT_CONSOLIDATION, 9, 0, CZSC_CENTER_POSITION_BELOW, CZSC_CENTER_AFTERMATH_EXTENDED, 4224},
    {"2018-12-13", 13.0f, 2, 1, 1, CZSC_MOVEMENT_CONSOLIDATION, 17, 1, CZSC_CENTER_POSITION_BELOW, CZSC_CENTER_AFTERMATH_EXTENDED, 4225},
    {"2019-05-17", 13.0f, 1, 2, 2, CZSC_MOVEMENT_CONSOLIDATION, 25, 2, CZSC_CENTER_POSITION_BELOW, CZSC_CENTER_AFTERMATH_EXTENDED, 4224},
    {"2020-04-10", 13.0f, 1, 3, 3, CZSC_MOVEMENT_CONSOLIDATION, 43, 3, CZSC_CENTER_POSITION_BELOW, CZSC_CENTER_AFTERMATH_EXTENDED, 4224},
    {"2020-07-27", 3.0f, 1, 4, 4, CZSC_MOVEMENT_CONSOLIDATION, 48, 4, CZSC_CENTER_POSITION_ABOVE, CZSC_CENTER_AFTERMATH_EXTENDED, 4224},
    {"2021-01-29", 3.0f, 2, 5, 5, CZSC_MOVEMENT_CONSOLIDATION, 58, 5, CZSC_CENTER_POSITION_ABOVE, CZSC_CENTER_AFTERMATH_EXTENDED, 4233},
    {"2021-06-18", 3.0f, 1, 6, 6, CZSC_MOVEMENT_CONSOLIDATION, 68, 6, CZSC_CENTER_POSITION_ABOVE, CZSC_CENTER_AFTERMATH_EXTENDED, 4224},
    {"2022-03-03", 13.0f, 1, 7, 7, CZSC_MOVEMENT_CONSOLIDATION, 81, 7, CZSC_CENTER_POSITION_BELOW, CZSC_CENTER_AFTERMATH_EXTENDED, 4224},
    {"2022-10-18", 13.0f, 1, 8, 8, CZSC_MOVEMENT_CONSOLIDATION, 93, 8, CZSC_CENTER_POSITION_BELOW, CZSC_CENTER_AFTERMATH_EXTENDED, 4224},
    {"2023-02-17", 3.0f, 2, 9, 9, CZSC_MOVEMENT_CONSOLIDATION, 100, 9, CZSC_CENTER_POSITION_ABOVE, CZSC_CENTER_AFTERMATH_EXTENDED, 4225},
    {"2023-09-04", 13.0f, 1, 10, 10, CZSC_MOVEMENT_CONSOLIDATION, 113, 10, CZSC_CENTER_POSITION_BELOW, CZSC_CENTER_AFTERMATH_EXTENDED, 4224},
    {"2024-03-28", 3.0f, 1, 11, 11, CZSC_MOVEMENT_CONSOLIDATION, 120, 11, CZSC_CENTER_POSITION_ABOVE, CZSC_CENTER_AFTERMATH_EXTENDED, 4224},
    {"2024-10-16", 3.0f, 1, 12, 12, CZSC_MOVEMENT_CONSOLIDATION, 126, 12, CZSC_CENTER_POSITION_ABOVE, CZSC_CENTER_AFTERMATH_EXTENDED, 4224},
    {"2025-04-24", 13.0f, 2, 13, 13, CZSC_MOVEMENT_CONSOLIDATION, 135, 13, CZSC_CENTER_POSITION_BELOW, CZSC_CENTER_AFTERMATH_EXTENDED, 4233},
    {"2025-09-04", 3.0f, 1, 14, 14, CZSC_MOVEMENT_CONSOLIDATION, 142, 14, CZSC_CENTER_POSITION_ABOVE, CZSC_CENTER_AFTERMATH_EXTENDED, 4224},
    {"2025-11-05", 3.0f, 2, 15, 15, CZSC_MOVEMENT_CONSOLIDATION, 146, 15, CZSC_CENTER_POSITION_ABOVE, CZSC_CENTER_AFTERMATH_EXTENDED, 4233},
    {"2026-02-03", 3.0f, 1, 16, 16, CZSC_MOVEMENT_CONSOLIDATION, 152, 16, CZSC_CENTER_POSITION_ABOVE, CZSC_CENTER_AFTERMATH_EXTENDED, 4224}
  };

  static const SseCandidateExpectation SegmentExpected[] = {
    {"2020-09-25", 3.0f, 2, 0, 0, CZSC_MOVEMENT_CONSOLIDATION, 6, 0, CZSC_CENTER_POSITION_ABOVE, CZSC_CENTER_AFTERMATH_EXTENDED, 4225},
    {"2024-05-20", 13.0f, 1, 1, 1, CZSC_MOVEMENT_CONSOLIDATION, 13, 1, CZSC_CENTER_POSITION_BELOW, CZSC_CENTER_AFTERMATH_UNKNOWN, 4096}
  };

  return ContainsAllSseCandidates(StrokeAn.Candidates,
                                  StrokeExpected,
                                  sizeof(StrokeExpected) / sizeof(StrokeExpected[0])) &&
         ContainsAllSseCandidates(SegmentAn.Candidates,
                                  SegmentExpected,
                                  sizeof(SegmentExpected) / sizeof(SegmentExpected[0]));
}

static bool TestRealSseGoldenBreakoutsPresent()
{
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);

  CzscAnalyzer StrokeAn;
  BuildAnalyzerFromPrice(StrokeAn, SSE_DAILY_COUNT, pH, pL, DefaultConfig());

  CzscConfig SegmentConfig = DefaultConfig();
  SegmentConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
  SegmentConfig.nSegmentMethod = CZSC_SEG_FEATURE;
  CzscAnalyzer SegmentAn;
  BuildAnalyzerFromPrice(SegmentAn, SSE_DAILY_COUNT, pH, pL, SegmentConfig);

  static const SseBreakoutExpectation StrokeExpected[] = {
    {0, 0, -1, 8, "2018-07-06", 9, "2018-07-12", true, false, true},
    {5, 5, 1, 57, "2021-01-25", 58, "2021-01-29", true, false, true},
    {13, 13, -1, 134, "2025-04-07", 135, "2025-04-24", true, false, true},
    {15, 15, 1, 145, "2025-10-30", 146, "2025-11-05", true, false, true}
  };

  static const SseBreakoutExpectation SegmentExpected[] = {
    {0, 0, 1, 5, "2020-07-09", 6, "2020-09-25", true, false, true},
    {1, 1, -1, 12, "2023-06-26", 13, "2024-05-20", true, false, true}
  };

  return CheckAllSseBreakouts(StrokeAn.Points,
                              StrokeAn.Breakouts,
                              StrokeExpected,
                              sizeof(StrokeExpected) / sizeof(StrokeExpected[0])) &&
         CheckAllSseBreakouts(SegmentAn.Points,
                              SegmentAn.Breakouts,
                              SegmentExpected,
                              sizeof(SegmentExpected) / sizeof(SegmentExpected[0]));
}

static bool TestFunc1WritesCompatibleSignal()
{
  const int nCount = 7;
  float pHigh[nCount] = {10, 12, 11, 10, 9, 8, 10};
  float pLow[nCount] = {5, 7, 6, 5, 4, 2, 4};
  float pOut[nCount] = {-1, -1, -1, -1, -1, -1, -1};
  float fTime = 5;

  Func1(nCount, pOut, pHigh, pLow, &fTime);

  for (int i = 0; i < nCount; i++)
  {
    float fExpected = 0;
    if (i == 1)
    {
      fExpected = 1;
    }
    else if (i == 5)
    {
      fExpected = -1;
    }

    if (!NearlyEqual(pOut[i], fExpected))
    {
      return false;
    }
  }

  return true;
}

static Fractal MakeTestFractal(int nType, int nIndex, float fHigh, float fLow)
{
  Fractal F;
  F.nType = nType;
  F.nIndex = nIndex;
  F.nMergedIndex = nIndex;
  F.fHigh = fHigh;
  F.fLow = fLow;
  return F;
}

static Fractal MakeTestFractalFull(int nType, int nIndex, int nMergedIndex, float fHigh, float fLow)
{
  Fractal F = MakeTestFractal(nType, nIndex, fHigh, fLow);
  F.nMergedIndex = nMergedIndex;
  return F;
}

static SegmentPoint MakeTestPoint(int nType, int nIndex, float fPrice)
{
  SegmentPoint Point;
  Point.nType = nType;
  Point.nIndex = nIndex;
  Point.fHigh = fPrice;
  Point.fLow = fPrice;
  Point.fEnergy = 0;
  Point.fDif = 0;
  Point.fDea = 0;
  return Point;
}

static SegmentPoint MakeTestEnergyPoint(int nType, int nIndex, float fPrice, float fEnergy)
{
  SegmentPoint Point = MakeTestPoint(nType, nIndex, fPrice);
  Point.fEnergy = fEnergy;
  return Point;
}

static Center MakeTestCenter(int nStart, int nEnd, float fHigh, float fLow)
{
  Center C;
  C.nStart = nStart;
  C.nEnd = nEnd;
  C.fHigh = fHigh;
  C.fLow = fLow;
  C.fTop = fHigh;
  C.fBottom = fLow;
  C.nDirection = 0;
  return C;
}

static Center MakeTestCenterFull(int nStart, int nEnd, float fHigh, float fLow, float fTop, float fBottom)
{
  Center C = MakeTestCenter(nStart, nEnd, fHigh, fLow);
  C.fTop = fTop;
  C.fBottom = fBottom;
  return C;
}

static TradingSignalCandidate MakeTestCandidate(int nIndex, float fSignal, int nPriority)
{
  TradingSignalCandidate C;
  C.nIndex = nIndex;
  C.fSignal = fSignal;
  C.nPriority = nPriority;
  C.nPoint = -1;
  C.nCenter = -1;
  C.nBreakout = -1;
  C.nSource = 0;
  C.nTrend = -1;
  C.nMovementType = CZSC_MOVEMENT_CONSOLIDATION;
  C.nQuality = CZSC_SIGNAL_QUALITY_WATCH;
  C.nCenterPosition = CZSC_CENTER_POSITION_UNKNOWN;
  C.nReversal = CZSC_REVERSAL_UNKNOWN;
  C.nAfterEffect = CZSC_CENTER_AFTERMATH_UNKNOWN;
  C.nSecondBasePoint = -1;
  C.nSecondTurnPoint = -1;
  C.nSmallTurn = 0;
  C.nSmallTurnBasePoint = -1;
  C.nAbcStructure = 0;
  C.nAbcBreakout = -1;
  C.nMacdZeroPullback = 0;
  C.bOverlapped = false;
  C.Divergence.nDirection = 0;
  C.Divergence.nPreviousStartPoint = -1;
  C.Divergence.nPreviousEndPoint = -1;
  C.Divergence.nCurrentStartPoint = -1;
  C.Divergence.nCurrentEndPoint = -1;
  C.Divergence.bNewExtreme = false;
  C.Divergence.bWeakSpace = false;
  C.Divergence.bWeakSpeed = false;
  C.Divergence.bWeakMacd = false;
  C.Divergence.bDivergence = false;
  C.Divergence.Previous = MeasureStrength(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1),
                                          MakeTestPoint(CZSC_POINT_TOP, 4, 2));
  C.Divergence.Current = C.Divergence.Previous;
  return C;
}

static CenterBreakout MakeTestBreakout(int nDirection, int nRetestPoint)
{
  CenterBreakout B;
  B.nCenter = -1;
  B.nDirection = nDirection;
  B.nLeavePoint = -1;
  B.nRetestPoint = nRetestPoint;
  B.bFirstRetest = true;
  B.bBackIntoCenter = false;
  B.bThirdSignal = true;
  B.bConsolidationDivergence = false;
  B.Divergence.nDirection = nDirection;
  B.Divergence.nPreviousStartPoint = -1;
  B.Divergence.nPreviousEndPoint = -1;
  B.Divergence.nCurrentStartPoint = -1;
  B.Divergence.nCurrentEndPoint = -1;
  B.Divergence.bNewExtreme = false;
  B.Divergence.bWeakSpace = false;
  B.Divergence.bWeakSpeed = false;
  B.Divergence.bWeakMacd = false;
  B.Divergence.bDivergence = false;
  B.Divergence.Previous = MeasureStrength(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1),
                                          MakeTestPoint(CZSC_POINT_TOP, 4, 2));
  B.Divergence.Current = B.Divergence.Previous;
  return B;
}

static bool HasSignalCandidate(const std::vector<TradingSignalCandidate> &Candidates,
                               int nIndex,
                               float fSignal)
{
  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    if ((Candidates[i].nIndex == nIndex) && NearlyEqual(Candidates[i].fSignal, fSignal))
    {
      return true;
    }
  }
  return false;
}

static const TradingSignalCandidate *FindSignalCandidate(const std::vector<TradingSignalCandidate> &Candidates,
                                                         int nIndex,
                                                         float fSignal)
{
  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    if ((Candidates[i].nIndex == nIndex) && NearlyEqual(Candidates[i].fSignal, fSignal))
    {
      return &Candidates[i];
    }
  }
  return 0;
}

static bool TestLineSegmentsAreHigherLevelThanStrokes()
{
  std::vector<Fractal> Fractals;
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 5, 1));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 10, 6));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 8, 7, 3));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 12, 12, 8));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 16, 9, 5));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 20, 14, 10));

  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  std::vector<SegmentPoint> StrokePoints = BuildSegmentPoints(Strokes);
  std::vector<SegmentPoint> LinePoints = BuildLineSegmentPoints(Strokes);

  if (Strokes.size() != 5)
  {
    return false;
  }
  if (StrokePoints.size() != 6)
  {
    return false;
  }
  if (LinePoints.size() != 2)
  {
    return false;
  }
  if ((LinePoints[0].nType != CZSC_POINT_BOTTOM) || (LinePoints[0].nIndex != 0))
  {
    return false;
  }
  if ((LinePoints[1].nType != CZSC_POINT_TOP) || (LinePoints[1].nIndex != 20))
  {
    return false;
  }

  return true;
}

static bool TestLineSegmentBreakConfirmsTurn()
{
  std::vector<Fractal> Fractals;
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 5, 1));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 10, 6));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 8, 7, 3));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 12, 12, 8));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 16, 9, 5));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 20, 14, 10));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 24, 8, 4));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 28, 11, 7));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 32, 6, 2));

  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  std::vector<SegmentPoint> LinePoints = BuildLineSegmentPoints(Strokes);

  if (Strokes.size() != 8)
  {
    return false;
  }
  if (LinePoints.size() != 3)
  {
    return false;
  }
  if ((LinePoints[0].nType != CZSC_POINT_BOTTOM) || (LinePoints[0].nIndex != 0))
  {
    return false;
  }
  if ((LinePoints[1].nType != CZSC_POINT_TOP) || (LinePoints[1].nIndex != 20))
  {
    return false;
  }
  if ((LinePoints[2].nType != CZSC_POINT_BOTTOM) || (LinePoints[2].nIndex != 32))
  {
    return false;
  }

  return true;
}

static bool TestStrengthMetricsMeasureSpaceAndSpeed()
{
  SegmentPoint Start = MakeTestPoint(CZSC_POINT_TOP, 2, 10);
  SegmentPoint End = MakeTestPoint(CZSC_POINT_BOTTOM, 7, 4);

  StrengthMetrics Strength = MeasureStrength(Start, End);

  if (!NearlyEqual(Strength.fSpace, 6.0f) || !NearlyEqual(Strength.fSpeed, 1.2f))
  {
    return false;
  }
  if ((Strength.fDifHeight != 0) || (Strength.fDeaHeight != 0) ||
      (Strength.fMacdArea != 0) || Strength.bRsiDivergence)
  {
    return false;
  }

  return true;
}

static bool TestStrengthMetricsDetectWeakening()
{
  StrengthMetrics Previous;
  Previous.fSpace = 6;
  Previous.fSpeed = 2;
  Previous.fDifHeight = 0;
  Previous.fDeaHeight = 0;
  Previous.fMacdArea = 0;
  Previous.bRsiDivergence = false;

  StrengthMetrics WeakSpace = Previous;
  WeakSpace.fSpace = 5;
  if (!IsWeakerStrength(WeakSpace, Previous))
  {
    return false;
  }

  StrengthMetrics WeakSpeed = Previous;
  WeakSpeed.fSpeed = 1;
  if (!IsWeakerStrength(WeakSpeed, Previous))
  {
    return false;
  }

  StrengthMetrics Stronger = Previous;
  Stronger.fSpace = 7;
  Stronger.fSpeed = 3;
  return !IsWeakerStrength(Stronger, Previous);
}

static bool TestDivergenceResultDetectsUpWeakening()
{
  SegmentPoint PrevStart = MakeTestPoint(CZSC_POINT_BOTTOM, 0, 2);
  SegmentPoint PrevEnd = MakeTestPoint(CZSC_POINT_TOP, 4, 12);
  SegmentPoint CurrentStart = MakeTestPoint(CZSC_POINT_BOTTOM, 8, 9.5f);
  SegmentPoint CurrentEnd = MakeTestPoint(CZSC_POINT_TOP, 12, 13);

  DivergenceResult Result = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, 1);

  return (Result.nDirection == 1) &&
         Result.bNewExtreme &&
         Result.bWeakSpace &&
         Result.bWeakSpeed &&
         Result.bDivergence;
}

static bool TestDivergenceResultDetectsDownWeakening()
{
  SegmentPoint PrevStart = MakeTestPoint(CZSC_POINT_TOP, 0, 12);
  SegmentPoint PrevEnd = MakeTestPoint(CZSC_POINT_BOTTOM, 4, 2);
  SegmentPoint CurrentStart = MakeTestPoint(CZSC_POINT_TOP, 8, 4.5f);
  SegmentPoint CurrentEnd = MakeTestPoint(CZSC_POINT_BOTTOM, 12, 1);

  DivergenceResult Result = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, -1);

  return (Result.nDirection == -1) &&
         Result.bNewExtreme &&
         Result.bWeakSpace &&
         Result.bWeakSpeed &&
         Result.bDivergence;
}

static bool TestDivergenceResultSkipsStrongNewHigh()
{
  SegmentPoint PrevStart = MakeTestPoint(CZSC_POINT_BOTTOM, 0, 2);
  SegmentPoint PrevEnd = MakeTestPoint(CZSC_POINT_TOP, 4, 12);
  SegmentPoint CurrentStart = MakeTestPoint(CZSC_POINT_BOTTOM, 8, 1);
  SegmentPoint CurrentEnd = MakeTestPoint(CZSC_POINT_TOP, 12, 14);

  DivergenceResult Result = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, 1);

  return Result.bNewExtreme &&
         !Result.bWeakSpace &&
         !Result.bWeakSpeed &&
         !Result.bDivergence;
}

static bool TestTrendStructuresDetectConsolidation()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 10, 4));

  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);

  if (Structures.size() != 1)
  {
    return false;
  }
  return (Structures[0].nType == CZSC_MOVEMENT_CONSOLIDATION) &&
         (Structures[0].nFirstCenter == 0) &&
         (Structures[0].nLastCenter == 0);
}

static bool TestTrendStructuresDetectUpTrend()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 10, 4));
  Centers.push_back(MakeTestCenter(16, 28, 15, 11));

  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);

  if (Structures.size() != 1)
  {
    return false;
  }
  return (Structures[0].nType == CZSC_MOVEMENT_UP) &&
         (Structures[0].nFirstCenter == 0) &&
         (Structures[0].nLastCenter == 1);
}

static bool TestTrendStructuresDetectDownTrend()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 15, 11));
  Centers.push_back(MakeTestCenter(16, 28, 10, 4));

  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);

  if (Structures.size() != 1)
  {
    return false;
  }
  return (Structures[0].nType == CZSC_MOVEMENT_DOWN) &&
         (Structures[0].nFirstCenter == 0) &&
         (Structures[0].nLastCenter == 1);
}

static bool TestTrendStructuresSkipOverlappingTrend()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 10, 4));
  Centers.push_back(MakeTestCenter(16, 28, 12, 8));

  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);

  if (Structures.size() != 2)
  {
    return false;
  }
  return (Structures[0].nType == CZSC_MOVEMENT_CONSOLIDATION) &&
         (Structures[1].nType == CZSC_MOVEMENT_CONSOLIDATION);
}

static bool TestTrendStructuresUseFullCenterExtent()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenterFull(0, 12, 10, 4, 12, 2));
  Centers.push_back(MakeTestCenterFull(16, 28, 16, 11, 18, 8));

  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);

  if (Structures.size() != 2)
  {
    return false;
  }
  return (Structures[0].nType == CZSC_MOVEMENT_CONSOLIDATION) &&
         (Structures[1].nType == CZSC_MOVEMENT_CONSOLIDATION);
}

static bool TestTrendStructuresTreatTouchingFullExtentsAsConsolidation()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenterFull(0, 12, 10, 4, 12, 2));
  Centers.push_back(MakeTestCenterFull(16, 28, 16, 11, 18, 12));

  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);

  if (Structures.size() != 2)
  {
    return false;
  }
  return (Structures[0].nType == CZSC_MOVEMENT_CONSOLIDATION) &&
         (Structures[1].nType == CZSC_MOVEMENT_CONSOLIDATION);
}

static bool TestCenterBreakoutsDetectThirdBuy()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 9.5f));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 9, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

  if (Breakouts.size() != 1)
  {
    return false;
  }
  return (Breakouts[0].nDirection > 0) &&
         (Breakouts[0].nLeavePoint == 5) &&
         (Breakouts[0].nRetestPoint == 6) &&
         Breakouts[0].bFirstRetest &&
         !Breakouts[0].bBackIntoCenter &&
         Breakouts[0].bThirdSignal;
}

static bool TestCenterBreakoutsDetectThirdSell()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 2));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 7));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 20, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 24, 3.5f));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 10, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

  if (Breakouts.size() != 1)
  {
    return false;
  }
  return (Breakouts[0].nDirection < 0) &&
         (Breakouts[0].nLeavePoint == 5) &&
         (Breakouts[0].nRetestPoint == 6) &&
         Breakouts[0].bFirstRetest &&
         !Breakouts[0].bBackIntoCenter &&
         Breakouts[0].bThirdSignal;
}

static bool TestCenterBreakoutsAllowBoundaryRetest()
{
  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 6));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 12));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 9));

    std::vector<Center> Centers;
    Centers.push_back(MakeTestCenter(0, 12, 9, 4));
    std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
    std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

    if ((Breakouts.size() != 1) ||
        (Breakouts[0].nDirection <= 0) ||
        (Breakouts[0].nRetestPoint != 6) ||
        !Breakouts[0].bFirstRetest ||
        Breakouts[0].bBackIntoCenter ||
        !Breakouts[0].bThirdSignal)
    {
      return false;
    }
  }

  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 12));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 2));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 4));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 7));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 20, 1));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 24, 4));

    std::vector<Center> Centers;
    Centers.push_back(MakeTestCenter(0, 12, 10, 4));
    std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
    std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

    if ((Breakouts.size() != 1) ||
        (Breakouts[0].nDirection >= 0) ||
        (Breakouts[0].nRetestPoint != 6) ||
        !Breakouts[0].bFirstRetest ||
        Breakouts[0].bBackIntoCenter ||
        !Breakouts[0].bThirdSignal)
    {
      return false;
    }
  }

  return true;
}

static bool TestCenterBreakoutsUseFirstRetestOnly()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 9.5f));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 28, 13));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 32, 10));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 9, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

  if (Breakouts.size() != 1)
  {
    return false;
  }
  return Breakouts[0].bThirdSignal &&
         (Breakouts[0].nRetestPoint == 6);
}

static bool TestCenterBreakoutsUseCenterEndAsLeavePoint()
{
  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 12));     // 中枢末端已向上离开
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 9.5f)); // 下一段反向即首次回试

    std::vector<Center> Centers;
    Centers.push_back(MakeTestCenter(0, 12, 9, 4));
    std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
    std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

    if ((Breakouts.size() != 1) ||
        (Breakouts[0].nDirection != 1) ||
        (Breakouts[0].nLeavePoint != 3) ||
        (Breakouts[0].nRetestPoint != 4) ||
        !Breakouts[0].bThirdSignal)
    {
      return false;
    }
  }

  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 12));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 2));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 1));   // 中枢末端已向下离开
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 3.5f));   // 下一段反向即首次回试

    std::vector<Center> Centers;
    Centers.push_back(MakeTestCenter(0, 12, 10, 4));
    std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
    std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

    if ((Breakouts.size() != 1) ||
        (Breakouts[0].nDirection != -1) ||
        (Breakouts[0].nLeavePoint != 3) ||
        (Breakouts[0].nRetestPoint != 4) ||
        !Breakouts[0].bThirdSignal)
    {
      return false;
    }
  }

  return true;
}

static bool TestCenterBreakoutsSkipBackIntoCenter()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 8.5f));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 9, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

  if (Breakouts.size() != 1)
  {
    return false;
  }
  return Breakouts[0].bBackIntoCenter && !Breakouts[0].bThirdSignal;
}

static bool TestCenterBreakoutsDoNotUseLaterRetestAfterBackIntoCenter()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 12));      // 首次离开
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 8.5f)); // 首次回试回中枢，不能算三买
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 28, 13));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 32, 9.5f)); // 后续回试不回，也不能补算三买

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 9, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

  if (Breakouts.size() != 1)
  {
    return false;
  }
  return (Breakouts[0].nLeavePoint == 5) &&
         (Breakouts[0].nRetestPoint == 6) &&
         Breakouts[0].bBackIntoCenter &&
         !Breakouts[0].bThirdSignal;
}

static bool TestCenterBreakoutsSkipWithoutLeave()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 8.5f));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 7));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 9, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

  return Breakouts.empty();
}

static bool TestConsolidationDivergenceDetectsUpWeakening()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 2));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 9.5f));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 11));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 10.5f));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 10, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

  return (Breakouts.size() == 1) && Breakouts[0].bConsolidationDivergence;
}

static bool TestConsolidationDivergenceDetectsDownWeakening()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 2));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 4.5f));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 20, 3));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 24, 3.5f));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 10, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

  return (Breakouts.size() == 1) && Breakouts[0].bConsolidationDivergence;
}

static bool TestConsolidationDivergenceSkipsStrongBreakout()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 2));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 13));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 10.5f));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 10, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);

  return (Breakouts.size() == 1) && !Breakouts[0].bConsolidationDivergence;
}

static bool TestTradingCandidatesGenerateFirstAndSecondBuy()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = -1;
  pHigh[0] = 7;
  pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = 12;
  pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = 8;
  pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = 10;
  pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = 7.5f;
  pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = 7;
  pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = 4;
  pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = 4.2f;
  pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = 3.8f;
  pLow[32] = 3.8f;
  pIn[36] = 1;
  pHigh[36] = 6;
  pLow[36] = 6;
  pIn[40] = -1;
  pHigh[40] = 3.9f;
  pLow[40] = 3.9f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 8));
  Centers.push_back(MakeTestCenter(20, 32, 6, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

  return HasSignalCandidate(Candidates, 32, 1.0f) &&
         HasSignalCandidate(Candidates, 40, 2.0f);
}

static bool TestTradingCandidatesAllowEqualSecondExtremes()
{
  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 7));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 12));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 8));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 7.5f));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 7));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 4));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 28, 4.2f));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 32, 3.8f));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 36, 6));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 40, 3.8f));

    std::vector<Center> Centers;
    Centers.push_back(MakeTestCenter(4, 16, 10, 8));
    Centers.push_back(MakeTestCenter(20, 32, 6, 4));
    std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
    std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);
    std::vector<TradingSignalCandidate> Candidates =
      BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

    if (!HasSignalCandidate(Candidates, 32, 1.0f) ||
        !HasSignalCandidate(Candidates, 40, 2.0f))
    {
      return false;
    }
  }

  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 5));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 9));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 7));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 12));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 20, 12.1f));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 24, 13));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 28, 12.8f));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 32, 13.2f));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 36, 12.6f));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 40, 13.2f));

    std::vector<Center> Centers;
    Centers.push_back(MakeTestCenter(4, 16, 9, 7));
    Centers.push_back(MakeTestCenter(20, 32, 13, 12.8f));
    std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
    std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);
    std::vector<TradingSignalCandidate> Candidates =
      BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

    if (!HasSignalCandidate(Candidates, 32, 11.0f) ||
        !HasSignalCandidate(Candidates, 40, 12.0f))
    {
      return false;
    }
  }

  return true;
}

static bool TestTradingCandidatesGenerateThirdBuy()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 9.5f));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 9, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

  return HasSignalCandidate(Candidates, 24, 3.0f);
}

static bool TestTradingCandidatesSkipSecondWithoutFirst()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 8));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 5));

  std::vector<Center> Centers;
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

  return Candidates.empty();
}

static bool TestApplyTradingCandidatesKeepsPriority()
{
  const int nCount = 4;
  float pOut[nCount] = {-1, -1, -1, -1};
  std::vector<TradingSignalCandidate> Candidates;
  Candidates.push_back(MakeTestCandidate(2, 2.0f, 10));
  Candidates.push_back(MakeTestCandidate(2, 3.0f, 20));
  Candidates.push_back(MakeTestCandidate(2, 1.0f, 30));

  ApplyTradingSignalCandidates(nCount, pOut, Candidates);

  return (pOut[0] == 0) &&
         (pOut[1] == 0) &&
         NearlyEqual(pOut[2], 1.0f) &&
         (pOut[3] == 0);
}

static bool TestApplyTradingCandidatesThirdOverridesSecond()
{
  const int nCount = 4;
  float pOut[nCount] = {-1, -1, -1, -1};
  std::vector<TradingSignalCandidate> Candidates;
  Candidates.push_back(MakeTestCandidate(2, 2.0f, 10));
  Candidates.push_back(MakeTestCandidate(2, 3.0f, 20));

  ApplyTradingSignalCandidates(nCount, pOut, Candidates);

  return NearlyEqual(pOut[2], 3.0f);
}

static bool TestApplyTradingOutputsSkipInvalidSignals()
{
  const int nCount = 4;
  float pSignal[nCount] = {-1, -1, -1, -1};
  float pQuality[nCount] = {-1, -1, -1, -1};
  float pPriority[nCount] = {-1, -1, -1, -1};
  float pCenter[nCount] = {-1, -1, -1, -1};
  float pContext[nCount] = {-1, -1, -1, -1};

  TradingSignalCandidate Valid = MakeTestCandidate(2, 3.0f, 20);
  Valid.nSource = 3;
  Valid.nPoint = 2;
  Valid.nQuality = CZSC_SIGNAL_QUALITY_CONFIRMED;
  Valid.nCenter = 4;
  Valid.nBreakout = 2;
  Valid.bOverlapped = true;

  TradingSignalCandidate Invalid = MakeTestCandidate(2, 99.0f, 30);
  Invalid.nQuality = CZSC_SIGNAL_QUALITY_STRONG;
  Invalid.nCenter = 8;
  Invalid.nBreakout = 8;
  Invalid.bOverlapped = false;

  std::vector<TradingSignalCandidate> Candidates;
  Candidates.push_back(Valid);
  Candidates.push_back(Invalid);

  ApplyTradingSignalCandidates(nCount, pSignal, Candidates);
  ApplyTradingSignalQuality(nCount, pQuality, Candidates);
  ApplyTradingSignalPriority(nCount, pPriority, Candidates);
  ApplyTradingSignalCenterId(nCount, pCenter, Candidates);
  ApplyTradingSignalContextFlags(nCount, pContext, Candidates);

  float fContext = (float)(CZSC_SIGNAL_CTX_OVERLAPPED |
                           CZSC_SIGNAL_CTX_CENTER_BREAKOUT);
  return NearlyEqual(pSignal[2], 3.0f) &&
         NearlyEqual(pQuality[2], (float)CZSC_SIGNAL_QUALITY_CONFIRMED) &&
         NearlyEqual(pPriority[2], 20.0f) &&
         NearlyEqual(pCenter[2], 5.0f) &&
         NearlyEqual(pContext[2], fContext) &&
         NearlyEqual(pSignal[0], 0.0f) &&
         NearlyEqual(pQuality[0], 0.0f) &&
         NearlyEqual(pPriority[0], 0.0f) &&
         NearlyEqual(pCenter[0], 0.0f) &&
         NearlyEqual(pContext[0], 0.0f);
}

static bool TestFirstCandidateKeepsTrendDivergence()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = -1;
  pHigh[0] = 7;
  pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = 12;
  pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = 8;
  pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = 10;
  pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = 7.5f;
  pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = 7;
  pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = 4;
  pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = 4.2f;
  pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = 3.8f;
  pLow[32] = 3.8f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 8));
  Centers.push_back(MakeTestCenter(20, 32, 4.2f, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pFirst = FindSignalCandidate(Candidates, 32, 1.0f);

  return (pFirst != 0) &&
         (pFirst->Divergence.nDirection == -1) &&
         pFirst->Divergence.bNewExtreme &&
         pFirst->Divergence.bDivergence &&
         (pFirst->nQuality == CZSC_SIGNAL_QUALITY_STRONG) &&
         (pFirst->nCenter == 1) &&
         (pFirst->nTrend == 0) &&
         (pFirst->nMovementType == CZSC_MOVEMENT_DOWN) &&
         (pFirst->nCenterPosition == CZSC_CENTER_POSITION_BELOW);
}

static bool TestFirstCandidateRequiresTrendStructure()
{
  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 7));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 12));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 8));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 7.5f));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 7));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 4));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 28, 4.2f));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 32, 3.8f));

    std::vector<Center> OneCenter;
    OneCenter.push_back(MakeTestCenter(4, 16, 10, 8));
    std::vector<TrendStructure> OneStruct = BuildTrendStructures(OneCenter);
    std::vector<CenterBreakout> Breakouts;
    std::vector<TradingSignalCandidate> Single =
      BuildTradingSignalCandidates(Points, OneCenter, OneStruct, Breakouts);
    if (HasSignalCandidate(Single, 32, 1.0f))
    {
      return false;
    }

    std::vector<Center> TrendCenters = OneCenter;
    TrendCenters.push_back(MakeTestCenter(20, 32, 4.2f, 4));
    std::vector<TrendStructure> TrendStruct = BuildTrendStructures(TrendCenters);
    std::vector<TradingSignalCandidate> Trend =
      BuildTradingSignalCandidates(Points, TrendCenters, TrendStruct, Breakouts);
    if (!HasSignalCandidate(Trend, 32, 1.0f))
    {
      return false;
    }
  }

  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 5));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 9));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 7));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 12));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 20, 12.1f));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 24, 13));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 28, 12.8f));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 32, 13.2f));

    std::vector<Center> OneCenter;
    OneCenter.push_back(MakeTestCenter(4, 16, 9, 7));
    std::vector<TrendStructure> OneStruct = BuildTrendStructures(OneCenter);
    std::vector<CenterBreakout> Breakouts;
    std::vector<TradingSignalCandidate> Single =
      BuildTradingSignalCandidates(Points, OneCenter, OneStruct, Breakouts);
    if (HasSignalCandidate(Single, 32, 11.0f))
    {
      return false;
    }

    std::vector<Center> TrendCenters = OneCenter;
    TrendCenters.push_back(MakeTestCenter(20, 32, 13, 12.8f));
    std::vector<TrendStructure> TrendStruct = BuildTrendStructures(TrendCenters);
    std::vector<TradingSignalCandidate> Trend =
      BuildTradingSignalCandidates(Points, TrendCenters, TrendStruct, Breakouts);
    if (!HasSignalCandidate(Trend, 32, 11.0f))
    {
      return false;
    }
  }

  return true;
}

static bool TestFirstCandidateRequiresOutsideLastCenter()
{
  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 7));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 12));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 8));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 7.5f));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 7));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 4));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 28, 4.2f));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 32, 3.8f));

    std::vector<Center> Centers;
    Centers.push_back(MakeTestCenter(4, 16, 10, 8));
    Centers.push_back(MakeTestCenter(20, 32, 4.2f, 3.7f));
    std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
    std::vector<CenterBreakout> Breakouts;
    std::vector<TradingSignalCandidate> Candidates =
      BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
    if (HasSignalCandidate(Candidates, 32, 1.0f))
    {
      return false;
    }
  }

  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 5));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 9));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 7));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 12));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 20, 12.1f));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 24, 13));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 28, 12.8f));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 32, 13.2f));

    std::vector<Center> Centers;
    Centers.push_back(MakeTestCenter(4, 16, 9, 7));
    Centers.push_back(MakeTestCenter(20, 32, 13.3f, 12.8f));
    std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
    std::vector<CenterBreakout> Breakouts;
    std::vector<TradingSignalCandidate> Candidates =
      BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
    if (HasSignalCandidate(Candidates, 32, 11.0f))
    {
      return false;
    }
  }

  return true;
}

static bool TestFirstCandidateMarksAbcStructure()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = -1;
  pHigh[0] = pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = pLow[32] = 3.8f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 8));
  Centers.push_back(MakeTestCenter(20, 32, 4.2f, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(-1, 7));
  Breakouts.back().nCenter = 1;
  Breakouts.back().nLeavePoint = 6;

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pFirst = FindSignalCandidate(Candidates, 32, 1.0f);

  return (pFirst != 0) &&
         (pFirst->nCenter == 1) &&
         (pFirst->nAbcStructure == 1) &&
         (pFirst->nAbcBreakout == 0);
}

static bool TestFirstSellCandidateMarksAbcStructure()
{
  const int nCount = 33;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = 1;
  pHigh[0] = pLow[0] = 10;
  pIn[4] = -1;
  pHigh[4] = pLow[4] = 5;
  pIn[8] = 1;
  pHigh[8] = pLow[8] = 9;
  pIn[12] = -1;
  pHigh[12] = pLow[12] = 7;
  pIn[16] = 1;
  pHigh[16] = pLow[16] = 12;
  pIn[20] = -1;
  pHigh[20] = pLow[20] = 12.1f;
  pIn[24] = 1;
  pHigh[24] = pLow[24] = 13;
  pIn[28] = -1;
  pHigh[28] = pLow[28] = 12.8f;
  pIn[32] = 1;
  pHigh[32] = pLow[32] = 13.2f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 9, 7));
  Centers.push_back(MakeTestCenter(20, 32, 13, 12.8f));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(1, 7));
  Breakouts.back().nCenter = 1;
  Breakouts.back().nLeavePoint = 6;

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pFirst = FindSignalCandidate(Candidates, 32, 11.0f);

  return (pFirst != 0) &&
         (pFirst->nCenter == 1) &&
         (pFirst->nAbcStructure == -1) &&
         (pFirst->nAbcBreakout == 0);
}

static bool TestFirstCandidateRequiresValidAbcBreakout()
{
  {
    const int nCount = 41;
    float pIn[nCount];
    float pHigh[nCount];
    float pLow[nCount];

    for (int i = 0; i < nCount; i++)
    {
      pIn[i] = 0;
      pHigh[i] = 0;
      pLow[i] = 0;
    }

    pIn[0] = -1;
    pHigh[0] = pLow[0] = 7;
    pIn[4] = 1;
    pHigh[4] = pLow[4] = 12;
    pIn[8] = -1;
    pHigh[8] = pLow[8] = 8;
    pIn[12] = 1;
    pHigh[12] = pLow[12] = 10;
    pIn[16] = -1;
    pHigh[16] = pLow[16] = 7.5f;
    pIn[20] = 1;
    pHigh[20] = pLow[20] = 7;
    pIn[24] = -1;
    pHigh[24] = pLow[24] = 4;
    pIn[28] = 1;
    pHigh[28] = pLow[28] = 4.2f;
    pIn[32] = -1;
    pHigh[32] = pLow[32] = 3.8f;

    std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
    std::vector<Center> Centers;
    Centers.push_back(MakeTestCenter(4, 16, 10, 8));
    Centers.push_back(MakeTestCenter(20, 32, 4.2f, 4));
    std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
    std::vector<CenterBreakout> Breakouts;
    Breakouts.push_back(MakeTestBreakout(1, 7));   // 一买前必须是三卖，不是三买
    Breakouts.back().nCenter = 1;
    Breakouts.push_back(MakeTestBreakout(-1, 7));
    Breakouts.back().nCenter = 1;
    Breakouts.back().bThirdSignal = false;
    Breakouts.push_back(MakeTestBreakout(-1, 7));
    Breakouts.back().nCenter = 0;                  // 不是一买所属最后中枢
    Breakouts.push_back(MakeTestBreakout(-1, 6));  // 回试点早于C段起点
    Breakouts.back().nCenter = 1;
    Breakouts.back().nLeavePoint = 5;
    Breakouts.push_back(MakeTestBreakout(-1, 8));  // 回试不早于一买点
    Breakouts.back().nCenter = 1;

    std::vector<TradingSignalCandidate> Candidates =
      BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
    const TradingSignalCandidate *pFirst = FindSignalCandidate(Candidates, 32, 1.0f);
    if ((pFirst == 0) || (pFirst->nAbcStructure != 0) || (pFirst->nAbcBreakout != -1))
    {
      return false;
    }
  }

  {
    const int nCount = 33;
    float pIn[nCount];
    float pHigh[nCount];
    float pLow[nCount];

    for (int i = 0; i < nCount; i++)
    {
      pIn[i] = 0;
      pHigh[i] = 0;
      pLow[i] = 0;
    }

    pIn[0] = 1;
    pHigh[0] = pLow[0] = 10;
    pIn[4] = -1;
    pHigh[4] = pLow[4] = 5;
    pIn[8] = 1;
    pHigh[8] = pLow[8] = 9;
    pIn[12] = -1;
    pHigh[12] = pLow[12] = 7;
    pIn[16] = 1;
    pHigh[16] = pLow[16] = 12;
    pIn[20] = -1;
    pHigh[20] = pLow[20] = 12.1f;
    pIn[24] = 1;
    pHigh[24] = pLow[24] = 13;
    pIn[28] = -1;
    pHigh[28] = pLow[28] = 12.8f;
    pIn[32] = 1;
    pHigh[32] = pLow[32] = 13.2f;

    std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
    std::vector<Center> Centers;
    Centers.push_back(MakeTestCenter(4, 16, 9, 7));
    Centers.push_back(MakeTestCenter(20, 32, 13, 12.8f));
    std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
    std::vector<CenterBreakout> Breakouts;
    Breakouts.push_back(MakeTestBreakout(-1, 7));  // 一卖前必须是三买，不是三卖
    Breakouts.back().nCenter = 1;
    Breakouts.push_back(MakeTestBreakout(1, 7));
    Breakouts.back().nCenter = 1;
    Breakouts.back().bThirdSignal = false;
    Breakouts.push_back(MakeTestBreakout(1, 7));
    Breakouts.back().nCenter = 0;
    Breakouts.push_back(MakeTestBreakout(1, 6));   // 回试点早于C段起点
    Breakouts.back().nCenter = 1;
    Breakouts.back().nLeavePoint = 5;
    Breakouts.push_back(MakeTestBreakout(1, 8));
    Breakouts.back().nCenter = 1;

    std::vector<TradingSignalCandidate> Candidates =
      BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
    const TradingSignalCandidate *pFirst = FindSignalCandidate(Candidates, 32, 11.0f);
    if ((pFirst == 0) || (pFirst->nAbcStructure != 0) || (pFirst->nAbcBreakout != -1))
    {
      return false;
    }
  }

  return true;
}

static bool TestFirstCandidateMarksMacdZeroPullback()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = -1;
  pHigh[0] = pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = pLow[32] = 3.8f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  Points[3].fDif = 6.0f;  Points[3].fDea = 5.0f;
  Points[4].fDif = 2.0f;  Points[4].fDea = 1.0f;
  Points[5].fDif = 4.0f;  Points[5].fDea = 3.0f;
  Points[6].fDif = 0.2f;  Points[6].fDea = -0.1f;
  Points[7].fDif = 2.0f;  Points[7].fDea = 2.0f;
  Points[8].fDif = 1.0f;  Points[8].fDea = 1.2f;

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 8));
  Centers.push_back(MakeTestCenter(20, 32, 4.2f, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pFirst = FindSignalCandidate(Candidates, 32, 1.0f);

  return (pFirst != 0) &&
         (pFirst->nCenter == 1) &&
         (pFirst->nMacdZeroPullback == 1);
}

static bool TestFirstCandidateBuildsStandardMacdDivergence()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = -1;
  pHigh[0] = pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = pLow[32] = 3.8f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  Points[3].fEnergy = 100; Points[3].fDif = 8;   Points[3].fDea = 6;
  Points[4].fEnergy = 70;  Points[4].fDif = 2;   Points[4].fDea = 1;
  Points[6].fDif = 0.2f;   Points[6].fDea = -0.1f;
  Points[7].fEnergy = 50;  Points[7].fDif = 3;   Points[7].fDea = 2;
  Points[8].fEnergy = 45;  Points[8].fDif = 1;   Points[8].fDea = 1;

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 8));
  Centers.push_back(MakeTestCenter(20, 32, 4.2f, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(-1, 7));
  Breakouts.back().nCenter = 1;
  Breakouts.back().nLeavePoint = 6;

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pFirst = FindSignalCandidate(Candidates, 32, 1.0f);
  ApplyTradingSignalStandardDivergence(nCount, pOut, Candidates);

  return (pFirst != 0) &&
         (pFirst->nAbcStructure == 1) &&
         (pFirst->nMacdZeroPullback == 1) &&
         pFirst->Divergence.bNewExtreme &&
         pFirst->Divergence.bWeakMacd &&
         (pFirst->Divergence.Current.fDifHeight < pFirst->Divergence.Previous.fDifHeight) &&
         (pFirst->Divergence.Current.fDeaHeight <= pFirst->Divergence.Previous.fDeaHeight) &&
         NearlyEqual(pOut[32], 1.0f);
}

static bool TestFirstSellCandidateBuildsStandardMacdDivergence()
{
  const int nCount = 33;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = 1;
  pHigh[0] = pLow[0] = 10;
  pIn[4] = -1;
  pHigh[4] = pLow[4] = 5;
  pIn[8] = 1;
  pHigh[8] = pLow[8] = 9;
  pIn[12] = -1;
  pHigh[12] = pLow[12] = 7;
  pIn[16] = 1;
  pHigh[16] = pLow[16] = 12;
  pIn[20] = -1;
  pHigh[20] = pLow[20] = 12.1f;
  pIn[24] = 1;
  pHigh[24] = pLow[24] = 13;
  pIn[28] = -1;
  pHigh[28] = pLow[28] = 12.8f;
  pIn[32] = 1;
  pHigh[32] = pLow[32] = 13.2f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  Points[3].fEnergy = 100; Points[3].fDif = 1;   Points[3].fDea = 1;
  Points[4].fEnergy = 70;  Points[4].fDif = 8;   Points[4].fDea = 6;
  Points[6].fDif = 0.2f;   Points[6].fDea = -0.1f;
  Points[7].fEnergy = 50;  Points[7].fDif = 2;   Points[7].fDea = 1;
  Points[8].fEnergy = 45;  Points[8].fDif = 3;   Points[8].fDea = 2;

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 9, 7));
  Centers.push_back(MakeTestCenter(20, 32, 13, 12.8f));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(1, 7));
  Breakouts.back().nCenter = 1;
  Breakouts.back().nLeavePoint = 6;

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pFirst = FindSignalCandidate(Candidates, 32, 11.0f);
  ApplyTradingSignalStandardDivergence(nCount, pOut, Candidates);

  return (pFirst != 0) &&
         (pFirst->nAbcStructure == -1) &&
         (pFirst->nMacdZeroPullback == -1) &&
         pFirst->Divergence.bNewExtreme &&
         pFirst->Divergence.bWeakMacd &&
         (pFirst->Divergence.Current.fDifHeight < pFirst->Divergence.Previous.fDifHeight) &&
         (pFirst->Divergence.Current.fDeaHeight <= pFirst->Divergence.Previous.fDeaHeight) &&
         NearlyEqual(pOut[32], -1.0f);
}

static bool TestFirstCandidateSkipsAfterLaterCenter()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = -1;
  pHigh[0] = pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = pLow[16] = 3;
  pIn[20] = 1;
  pHigh[20] = pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = pLow[32] = 3.8f;
  pIn[36] = 1;
  pHigh[36] = pLow[36] = 4.0f;
  pIn[40] = -1;
  pHigh[40] = pLow[40] = 3.7f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 8));
  Centers.push_back(MakeTestCenter(20, 32, 4.2f, 4));
  Centers.push_back(MakeTestCenter(36, 44, 4.1f, 3.5f));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

  return !HasSignalCandidate(Candidates, 40, 1.0f);
}

static bool TestFirstCandidateUsesPreLastCenterMove()
{
  const int nCount = 33;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = -1;
  pHigh[0] = pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = pLow[12] = 8;
  pIn[16] = -1;
  pHigh[16] = pLow[16] = 7.5f;  // A段，弱于后续 C 段
  pIn[20] = 1;
  pHigh[20] = pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = pLow[24] = 4;     // 最后中枢内部下行段，不能作为 A 段
  pIn[28] = 1;
  pHigh[28] = pLow[28] = 4.6f;
  pIn[32] = -1;
  pHigh[32] = pLow[32] = 3.8f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 8));
  Centers.push_back(MakeTestCenter(20, 32, 4.2f, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

  return !HasSignalCandidate(Candidates, 32, 1.0f);
}

static bool TestThirdCandidateKeepsBreakoutDivergence()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 11));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 8));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 10.5f));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 6));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pThird = FindSignalCandidate(Candidates, 24, 3.0f);

  return (pThird != 0) &&
         (pThird->nBreakout == 0) &&
         (pThird->Divergence.nDirection == 1) &&
         pThird->Divergence.bNewExtreme &&
         pThird->Divergence.bWeakSpace &&
         pThird->Divergence.bDivergence &&
         (pThird->nQuality == CZSC_SIGNAL_QUALITY_STRONG) &&
         (pThird->nCenterPosition == CZSC_CENTER_POSITION_ABOVE) &&
         (pThird->nSmallTurn == 0);
}

static bool TestThirdCandidateRequiresBreakoutDirection()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 11));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 8));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 10.5f));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 6));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(0, 6));
  Breakouts.back().nCenter = 0;

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

  return !HasSignalCandidate(Candidates, 24, 3.0f) &&
         !HasSignalCandidate(Candidates, 24, 13.0f);
}

static bool TestThirdCandidateRequiresValidCenter()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 11));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 8));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 10.5f));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 6));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);

  std::vector<CenterBreakout> MissingCenter;
  MissingCenter.push_back(MakeTestBreakout(1, 6));
  std::vector<TradingSignalCandidate> MissingCandidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, MissingCenter);
  if (HasSignalCandidate(MissingCandidates, 24, 3.0f))
  {
    return false;
  }

  std::vector<CenterBreakout> OutOfRangeCenter;
  OutOfRangeCenter.push_back(MakeTestBreakout(1, 6));
  OutOfRangeCenter.back().nCenter = 1;
  std::vector<TradingSignalCandidate> OutOfRangeCandidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, OutOfRangeCenter);
  return !HasSignalCandidate(OutOfRangeCandidates, 24, 3.0f);
}

static bool TestThirdCandidateUsesOnlyCompletedTrendStructure()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 9.5f));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 28, 18));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 32, 14));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 36, 17));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 40, 15));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 44, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 48, 17.5f));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenterFull(0, 12, 9, 4, 10, 1));
  Centers.push_back(MakeTestCenterFull(28, 40, 17, 14, 18, 13));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  if ((Structures.size() != 1) || (Structures[0].nType != CZSC_MOVEMENT_UP))
  {
    return false;
  }

  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(1, 6));
  Breakouts.back().nCenter = 0;
  Breakouts.push_back(MakeTestBreakout(1, 12));
  Breakouts.back().nCenter = 1;

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

  const TradingSignalCandidate *pEarlyThird = FindSignalCandidate(Candidates, 24, 3.0f);
  const TradingSignalCandidate *pCompletedThird = FindSignalCandidate(Candidates, 48, 3.0f);
  return (pEarlyThird != 0) &&
         (pEarlyThird->nCenter == 0) &&
         (pEarlyThird->nTrend == -1) &&
         (pEarlyThird->nMovementType == CZSC_MOVEMENT_CONSOLIDATION) &&
         (pEarlyThird->nAfterEffect == CZSC_CENTER_AFTERMATH_NEWBORN) &&
         (pCompletedThird != 0) &&
         (pCompletedThird->nCenter == 1) &&
         (pCompletedThird->nTrend == 0) &&
         (pCompletedThird->nMovementType == CZSC_MOVEMENT_UP);
}

static bool TestTradingCandidatesMarkSecondThirdBuyOverlap()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];
  float pCtx[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
    pCtx[i] = -1;
  }

  pIn[0] = -1;
  pHigh[0] = 7;
  pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = 12;
  pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = 8;
  pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = 10;
  pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = 7.5f;
  pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = 7;
  pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = 4;
  pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = 4.2f;
  pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = 3.8f;
  pLow[32] = 3.8f;
  pIn[36] = 1;
  pHigh[36] = 6;
  pLow[36] = 6;
  pIn[40] = -1;
  pHigh[40] = 4.5f;
  pLow[40] = 4.5f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 8));
  Centers.push_back(MakeTestCenter(20, 32, 4.2f, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(1, 10));
  Breakouts.back().nCenter = 1;

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pSecond = FindSignalCandidate(Candidates, 40, 2.0f);
  const TradingSignalCandidate *pThird = FindSignalCandidate(Candidates, 40, 3.0f);
  ApplyTradingSignalCandidates(nCount, pOut, Candidates);
  ApplyTradingSignalContextFlags(nCount, pCtx, Candidates);

  return (pSecond != 0) &&
         (pThird != 0) &&
         pSecond->bOverlapped &&
         pThird->bOverlapped &&
         (pThird->nSmallTurn == 1) &&
         (pThird->nSmallTurnBasePoint == 8) &&
         (pSecond->nBreakout == 0) &&
         (pSecond->nSecondBasePoint == 8) &&
         (pSecond->nSecondTurnPoint == 9) &&
         (pSecond->nPoint == 10) &&
         (pSecond->Divergence.nDirection == 1) &&
         (pSecond->nQuality == CZSC_SIGNAL_QUALITY_CONFIRMED) &&
         (pSecond->nCenterPosition == CZSC_CENTER_POSITION_ABOVE) &&
         NearlyEqual(pOut[40], 3.0f) &&
         (((int)pCtx[40] & CZSC_SIGNAL_CTX_OVERLAPPED) != 0);
}

static bool TestTradingCandidatesMarkSecondThirdSellOverlap()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];
  float pCtx[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
    pCtx[i] = -1;
  }

  pIn[0] = 1;
  pHigh[0] = 10;
  pLow[0] = 10;
  pIn[4] = -1;
  pHigh[4] = 5;
  pLow[4] = 5;
  pIn[8] = 1;
  pHigh[8] = 9;
  pLow[8] = 9;
  pIn[12] = -1;
  pHigh[12] = 7;
  pLow[12] = 7;
  pIn[16] = 1;
  pHigh[16] = 12;
  pLow[16] = 12;
  pIn[20] = -1;
  pHigh[20] = 12.1f;
  pLow[20] = 12.1f;
  pIn[24] = 1;
  pHigh[24] = 13;
  pLow[24] = 13;
  pIn[28] = -1;
  pHigh[28] = 12.8f;
  pLow[28] = 12.8f;
  pIn[32] = 1;
  pHigh[32] = 13.2f;
  pLow[32] = 13.2f;
  pIn[36] = -1;
  pHigh[36] = 11;
  pLow[36] = 11;
  pIn[40] = 1;
  pHigh[40] = 12.5f;
  pLow[40] = 12.5f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 9, 7));
  Centers.push_back(MakeTestCenter(20, 32, 13, 12.8f));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(-1, 10));
  Breakouts.back().nCenter = 1;

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pSecond = FindSignalCandidate(Candidates, 40, 12.0f);
  const TradingSignalCandidate *pThird = FindSignalCandidate(Candidates, 40, 13.0f);
  ApplyTradingSignalCandidates(nCount, pOut, Candidates);
  ApplyTradingSignalContextFlags(nCount, pCtx, Candidates);

  return (pSecond != 0) &&
         (pThird != 0) &&
         pSecond->bOverlapped &&
         pThird->bOverlapped &&
         (pThird->nSmallTurn == -1) &&
         (pThird->nSmallTurnBasePoint == 8) &&
         (pSecond->nBreakout == 0) &&
         (pSecond->nSecondBasePoint == 8) &&
         (pSecond->nSecondTurnPoint == 9) &&
         (pSecond->nPoint == 10) &&
         (pSecond->Divergence.nDirection == -1) &&
         (pSecond->nQuality == CZSC_SIGNAL_QUALITY_CONFIRMED) &&
         (pSecond->nCenterPosition == CZSC_CENTER_POSITION_BELOW) &&
         NearlyEqual(pOut[40], 13.0f) &&
         (((int)pCtx[40] & CZSC_SIGNAL_CTX_OVERLAPPED) != 0);
}

static bool TestSecondThirdOverlapRequiresFirstCenter()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = -1;
  pHigh[0] = pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = pLow[32] = 3.8f;
  pIn[36] = 1;
  pHigh[36] = pLow[36] = 6;
  pIn[40] = -1;
  pHigh[40] = pLow[40] = 4.5f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 8));
  Centers.push_back(MakeTestCenter(20, 32, 4.2f, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(1, 10));
  Breakouts.back().nCenter = 0;  // 同端点三买，但不属于一买的最后中枢

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pSecond = FindSignalCandidate(Candidates, 40, 2.0f);
  const TradingSignalCandidate *pThird = FindSignalCandidate(Candidates, 40, 3.0f);

  return (pSecond != 0) &&
         (pThird != 0) &&
         !pSecond->bOverlapped &&
         (pSecond->nBreakout == -1);
}

static bool TestSecondThirdSellOverlapRequiresFirstCenter()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = 1;
  pHigh[0] = pLow[0] = 10;
  pIn[4] = -1;
  pHigh[4] = pLow[4] = 5;
  pIn[8] = 1;
  pHigh[8] = pLow[8] = 9;
  pIn[12] = -1;
  pHigh[12] = pLow[12] = 7;
  pIn[16] = 1;
  pHigh[16] = pLow[16] = 12;
  pIn[20] = -1;
  pHigh[20] = pLow[20] = 12.1f;
  pIn[24] = 1;
  pHigh[24] = pLow[24] = 13;
  pIn[28] = -1;
  pHigh[28] = pLow[28] = 12.8f;
  pIn[32] = 1;
  pHigh[32] = pLow[32] = 13.2f;
  pIn[36] = -1;
  pHigh[36] = pLow[36] = 11;
  pIn[40] = 1;
  pHigh[40] = pLow[40] = 12.5f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 9, 7));
  Centers.push_back(MakeTestCenter(20, 32, 13, 12.8f));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(-1, 10));
  Breakouts.back().nCenter = 0;  // 同端点三卖，但不属于一卖的最后中枢

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pSecond = FindSignalCandidate(Candidates, 40, 12.0f);
  const TradingSignalCandidate *pThird = FindSignalCandidate(Candidates, 40, 13.0f);

  return (pSecond != 0) &&
         (pThird != 0) &&
         !pSecond->bOverlapped &&
         (pSecond->nBreakout == -1);
}

static bool TestSmallTurnRequiresSameCenterAndLaterThird()
{
  const int nCount = 45;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = -1;
  pHigh[0] = pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = pLow[32] = 3.8f;
  pIn[36] = 1;
  pHigh[36] = pLow[36] = 6;
  pIn[40] = -1;
  pHigh[40] = pLow[40] = 4.5f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 8));
  Centers.push_back(MakeTestCenter(20, 32, 4.2f, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(1, 7));
  Breakouts.back().nCenter = 1;  // 同一最后中枢，但三买早于一买
  Breakouts.push_back(MakeTestBreakout(1, 10));
  Breakouts.back().nCenter = 0;  // 一买之后，但不是最后中枢

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pFirst = FindSignalCandidate(Candidates, 32, 1.0f);
  const TradingSignalCandidate *pEarlyThird = FindSignalCandidate(Candidates, 28, 3.0f);
  const TradingSignalCandidate *pOtherCenterThird = FindSignalCandidate(Candidates, 40, 3.0f);

  return (pFirst != 0) &&
         (pEarlyThird != 0) &&
         (pOtherCenterThird != 0) &&
         (pFirst->nCenter == 1) &&
         (pEarlyThird->nSmallTurn == 0) &&
         (pEarlyThird->nSmallTurnBasePoint == -1) &&
         (pOtherCenterThird->nSmallTurn == 0) &&
         (pOtherCenterThird->nSmallTurnBasePoint == -1);
}

static bool TestSmallTurnSellRequiresSameCenterAndLaterThird()
{
  const int nCount = 45;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = 1;
  pHigh[0] = pLow[0] = 10;
  pIn[4] = -1;
  pHigh[4] = pLow[4] = 5;
  pIn[8] = 1;
  pHigh[8] = pLow[8] = 9;
  pIn[12] = -1;
  pHigh[12] = pLow[12] = 7;
  pIn[16] = 1;
  pHigh[16] = pLow[16] = 12;
  pIn[20] = -1;
  pHigh[20] = pLow[20] = 12.1f;
  pIn[24] = 1;
  pHigh[24] = pLow[24] = 13;
  pIn[28] = -1;
  pHigh[28] = pLow[28] = 12.8f;
  pIn[32] = 1;
  pHigh[32] = pLow[32] = 13.2f;
  pIn[36] = -1;
  pHigh[36] = pLow[36] = 11;
  pIn[40] = 1;
  pHigh[40] = pLow[40] = 12.5f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 9, 7));
  Centers.push_back(MakeTestCenter(20, 32, 13, 12.8f));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(-1, 7));
  Breakouts.back().nCenter = 1;  // 同一最后中枢，但三卖早于一卖
  Breakouts.push_back(MakeTestBreakout(-1, 10));
  Breakouts.back().nCenter = 0;  // 一卖之后，但不是最后中枢

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pFirst = FindSignalCandidate(Candidates, 32, 11.0f);
  const TradingSignalCandidate *pEarlyThird = FindSignalCandidate(Candidates, 28, 13.0f);
  const TradingSignalCandidate *pOtherCenterThird = FindSignalCandidate(Candidates, 40, 13.0f);

  return (pFirst != 0) &&
         (pEarlyThird != 0) &&
         (pOtherCenterThird != 0) &&
         (pFirst->nCenter == 1) &&
         (pEarlyThird->nSmallTurn == 0) &&
         (pEarlyThird->nSmallTurnBasePoint == -1) &&
         (pOtherCenterThird->nSmallTurn == 0) &&
         (pOtherCenterThird->nSmallTurnBasePoint == -1);
}

static bool TestTradingCandidatesMarkSecondBuyInsideCenter()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = -1;
  pHigh[0] = 7;
  pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = 12;
  pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = 8;
  pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = 10;
  pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = 7.5f;
  pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = 7;
  pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = 4;
  pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = 4.2f;
  pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = 3.8f;
  pLow[32] = 3.8f;
  pIn[36] = 1;
  pHigh[36] = 6;
  pLow[36] = 6;
  pIn[40] = -1;
  pHigh[40] = 4.1f;
  pLow[40] = 4.1f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 10, 8));
  Centers.push_back(MakeTestCenter(20, 32, 4.2f, 4));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pSecond = FindSignalCandidate(Candidates, 40, 2.0f);
  ApplyTradingSignalCandidates(nCount, pOut, Candidates);

  return (pSecond != 0) &&
         (pSecond->nCenterPosition == CZSC_CENTER_POSITION_INSIDE) &&
         NearlyEqual(pOut[40], 2.0f);
}

static bool TestTradingCandidatesMarkSecondSellInsideCenter()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = 1;
  pHigh[0] = pLow[0] = 10;
  pIn[4] = -1;
  pHigh[4] = pLow[4] = 5;
  pIn[8] = 1;
  pHigh[8] = pLow[8] = 9;
  pIn[12] = -1;
  pHigh[12] = pLow[12] = 7;
  pIn[16] = 1;
  pHigh[16] = pLow[16] = 12;
  pIn[20] = -1;
  pHigh[20] = pLow[20] = 12.1f;
  pIn[24] = 1;
  pHigh[24] = pLow[24] = 13;
  pIn[28] = -1;
  pHigh[28] = pLow[28] = 12.8f;
  pIn[32] = 1;
  pHigh[32] = pLow[32] = 13.2f;
  pIn[36] = -1;
  pHigh[36] = pLow[36] = 11;
  pIn[40] = 1;
  pHigh[40] = pLow[40] = 12.9f;

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(4, 16, 9, 7));
  Centers.push_back(MakeTestCenter(20, 32, 13, 12.8f));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pSecond = FindSignalCandidate(Candidates, 40, 12.0f);
  ApplyTradingSignalCandidates(nCount, pOut, Candidates);

  return (pSecond != 0) &&
         (pSecond->nCenterPosition == CZSC_CENTER_POSITION_INSIDE) &&
         NearlyEqual(pOut[40], 12.0f);
}

// 真实上证日线：Func9 输出有效的线段端点信号（每个非零值为 ±1、顶底交替、至少两个端点）
static bool TestFunc9WritesLineSegmentSignal()
{
  const int n = SSE_DAILY_COUNT;
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);
  std::vector<float> Out((std::size_t)n, -99.0f);
  float fTime = 5;

  Func9(n, &Out[0], pH, pL, &fTime);

  int nPrev = 0;
  int nPoints = 0;
  for (int i = 0; i < n; i++)
  {
    if (NearlyEqual(Out[i], 0))
    {
      continue;  // 非端点
    }
    if (!NearlyEqual(Out[i], 1) && !NearlyEqual(Out[i], -1))
    {
      return false;  // 端点只能是 ±1
    }
    int nType = (Out[i] > 0) ? 1 : -1;
    if ((nPrev != 0) && (nType == nPrev))
    {
      return false;  // 线段端点顶底交替
    }
    nPrev = nType;
    nPoints++;
  }
  return nPoints >= 2;
}

static bool TestCentersUseThreeOverlappingSegments()
{
  // 第一笔(0->4)是进入段不算中枢；中枢由其后的第 2/3/4 笔（4->8、8->12、12->16）重叠构成。
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 5));

  std::vector<Center> Centers = BuildCenters(Points);

  if (Centers.size() != 1)
  {
    return false;
  }
  // 中枢从进入段终点(index4)起算，跨到第 4 笔终点(index16)
  if ((Centers[0].nStart != 4) || (Centers[0].nEnd != 16))
  {
    return false;
  }
  // ZG=min(10,9,9)=9，ZD=max(4,4,5)=5（三段重叠区间）
  if (!NearlyEqual(Centers[0].fHigh, 9.0f) || !NearlyEqual(Centers[0].fLow, 5.0f))
  {
    return false;
  }
  if (Centers[0].nDirection != 1)
  {
    return false;
  }

  return true;
}

static bool TestCentersTrackEntryDirection()
{
  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 5));

    std::vector<Center> Centers = BuildCenters(Points);
    if ((Centers.size() != 1) || (Centers[0].nDirection != 1))
    {
      return false;
    }
  }

  {
    std::vector<SegmentPoint> Points;
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 10));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 1));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 7));
    Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 2));
    Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 6));

    std::vector<Center> Centers = BuildCenters(Points);
    if ((Centers.size() != 1) || (Centers[0].nDirection != -1))
    {
      return false;
    }
  }

  return true;
}

static bool TestCenterExtendsWithOverlappingSegment()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 5));

  std::vector<Center> Centers = BuildCenters(Points);

  if (Centers.size() != 1)
  {
    return false;
  }
  if (Centers[0].nEnd != 16)
  {
    return false;
  }
  if (!NearlyEqual(Centers[0].fHigh, 9.0f) || !NearlyEqual(Centers[0].fLow, 5.0f))
  {
    return false;
  }

  return true;
}

static bool TestCenterExtendsWithCrossingSegment()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 5));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 4));

  std::vector<Center> Centers = BuildCenters(Points);

  if (Centers.size() != 1)
  {
    return false;
  }
  return (Centers[0].nEnd == 24) &&
         NearlyEqual(Centers[0].fHigh, 9.0f) &&
         NearlyEqual(Centers[0].fLow, 5.0f) &&
         NearlyEqual(Centers[0].fTop, 20.0f) &&
         NearlyEqual(Centers[0].fBottom, 4.0f);
}

static bool TestCentersSplitWhenOverlapBreaks()
{
  // 第一个中枢在低位带[5,9]成形后，向上离开且首次回试不进中枢 → 中枢结束；
  // 下一段走势从其终点(index20)起，在高位带[17,18]另成一枢，故分裂为两个中枢。
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 5));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 15));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 28, 19));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 32, 16));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 36, 18));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 40, 17));

  std::vector<Center> Centers = BuildCenters(Points);

  if (Centers.size() != 2)
  {
    return false;
  }
  // 中枢1：进入段终点(index4)到离开点(index20)，P4→P5 [5,20] 触及 ZD=5 故中枢延伸至20。
  if ((Centers[0].nStart != 4) || (Centers[0].nEnd != 20))
  {
    return false;
  }
  // 中枢2：从 index24 起在高位成枢，并延伸至末段 index40，ZG=18、ZD=17
  if ((Centers[1].nStart != 24) || (Centers[1].nEnd != 40))
  {
    return false;
  }
  if (!NearlyEqual(Centers[0].fHigh, 9.0f) || !NearlyEqual(Centers[0].fLow, 5.0f))
  {
    return false;
  }
  if (!NearlyEqual(Centers[1].fHigh, 18.0f) || !NearlyEqual(Centers[1].fLow, 17.0f))
  {
    return false;
  }

  return true;
}

static bool TestCenterFunctionsWriteSignals()
{
  const int nCount = 17;
  float pIn[nCount] = {-1, 0, 0, 0, 1, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0, 0, -1};
  float pHigh[nCount] = {1, 0, 0, 0, 10, 0, 0, 0, 4, 0, 0, 0, 9, 0, 0, 0, 5};
  float pLow[nCount] = {1, 0, 0, 0, 10, 0, 0, 0, 4, 0, 0, 0, 9, 0, 0, 0, 5};
  float pOut[nCount];

  // 进入段(0->4)不算中枢；中枢从其终点(index4)起算，跨到第 4 笔终点(index16)。
  // 故 ZG/ZD 仅在中枢跨度 index4..16 内写出，进入段(index0..3)留 0。
  Func2(nCount, pOut, pIn, pHigh, pLow);
  for (int i = 0; i < nCount; i++)
  {
    float fExpected = ((i >= 4) && (i <= 16)) ? 9.0f : 0.0f;  // ZG=min(10,9,9)=9
    if (!NearlyEqual(pOut[i], fExpected))
    {
      return false;
    }
  }

  Func3(nCount, pOut, pIn, pHigh, pLow);
  for (int i = 0; i < nCount; i++)
  {
    float fExpected = ((i >= 4) && (i <= 16)) ? 5.0f : 0.0f;  // ZD=max(4,4,5)=5
    if (!NearlyEqual(pOut[i], fExpected))
    {
      return false;
    }
  }

  Func4(nCount, pOut, pIn, pHigh, pLow);
  for (int i = 0; i < nCount; i++)
  {
    float fExpected = 0;
    if (i == 4)  // 中枢起点（进入段终点）
    {
      fExpected = 1;
    }
    else if (i == 16)  // 中枢终点
    {
      fExpected = 2;
    }

    if (!NearlyEqual(pOut[i], fExpected))
    {
      return false;
    }
  }

  return true;
}

static bool TestFunc5WritesTrendDivergenceFirstBuy()
{
  const int nCount = 33;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = -1;
  pHigh[0] = 7;
  pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = 12;
  pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = 8;
  pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = 10;
  pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = 7.5f;
  pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = 7;
  pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = 4;
  pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = 4.2f;
  pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = 3.8f;
  pLow[32] = 3.8f;

  Func5(nCount, pOut, pIn, pHigh, pLow);

  // 两个中枢全幅不重叠(GG/DD: [12,7.5]→[7,3.8])构成下跌趋势；
  // 中枢0离开后的首次回试不回中枢 → 三卖在28；一买在趋势末32。
  return NearlyEqual(pOut[28], 13.0f) && NearlyEqual(pOut[32], 1.0f);
}

static bool TestFunc5WritesCenterThirdBuy()
{
  // 上升中枢在低位带[4,5.5]成形后向上离开，回试不回中枢(底 6 > ZG 5.5) → 第三类买点。
  // 进入段(0->4) + 中枢三笔(4->8、8->12、12->16) + 离开(16->20创新高) + 回试(底 index24)。
  const int nCount = 33;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = -1;
  pHigh[0] = 1;
  pLow[0] = 1;
  pIn[4] = 1;
  pHigh[4] = 6;
  pLow[4] = 6;
  pIn[8] = -1;
  pHigh[8] = 4;
  pLow[8] = 4;
  pIn[12] = 1;
  pHigh[12] = 5.5f;
  pLow[12] = 5.5f;
  pIn[16] = -1;
  pHigh[16] = 5.6f;   // 中枢结束后第一个底，高于 ZG=5.5 → 延伸终止
  pLow[16] = 5.6f;
  pIn[20] = 1;
  pHigh[20] = 7;      // 向上离开中枢
  pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = 6;      // 回试底，在 ZG 上方 → 第三类买点
  pLow[24] = 6;
  pIn[28] = 1;
  pHigh[28] = 8;
  pLow[28] = 8;
  pIn[32] = -1;
  pHigh[32] = 7;
  pLow[32] = 7;

  Func5(nCount, pOut, pIn, pHigh, pLow);

  return NearlyEqual(pOut[24], 3.0f);
}

static bool TestFunc5WritesCenterThirdSell()
{
  // 下降中枢在高位带[9.5,11]成形后向下离开，回试不回中枢(顶 8 < ZD 9.5) → 第三类卖点。
  // 进入段(0->4) + 中枢三笔(4->8、8->12、12->16) + 离开(16->20创新低) + 回试(顶 index24)。
  const int nCount = 33;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = 1;
  pHigh[0] = 12;
  pLow[0] = 12;
  pIn[4] = -1;
  pHigh[4] = 9;
  pLow[4] = 9;
  pIn[8] = 1;
  pHigh[8] = 11;
  pLow[8] = 11;
  pIn[12] = -1;
  pHigh[12] = 9.5f;
  pLow[12] = 9.5f;
  pIn[16] = 1;
  pHigh[16] = 9.3f;   // 中枢结束后第一个顶，低于 ZD=9.5 → 延伸终止
  pLow[16] = 9.3f;
  pIn[20] = -1;
  pHigh[20] = 7;      // 向下离开中枢
  pLow[20] = 7;
  pIn[24] = 1;
  pHigh[24] = 8;      // 回试顶，在 ZD 下方 → 第三类卖点
  pLow[24] = 8;
  pIn[28] = -1;
  pHigh[28] = 6;
  pLow[28] = 6;
  pIn[32] = 1;
  pHigh[32] = 7.5f;
  pLow[32] = 7.5f;

  Func5(nCount, pOut, pIn, pHigh, pLow);

  return NearlyEqual(pOut[24], 13.0f);
}

static bool TestFunc5WritesThirdSignalsAtCenterBoundary()
{
  {
    const int nCount = 33;
    float pIn[nCount];
    float pHigh[nCount];
    float pLow[nCount];
    float pOut[nCount];

    for (int i = 0; i < nCount; i++)
    {
      pIn[i] = 0;
      pHigh[i] = 0;
      pLow[i] = 0;
      pOut[i] = -1;
    }

    pIn[0] = -1;  pHigh[0] = pLow[0] = 1;
    pIn[4] = 1;   pHigh[4] = pLow[4] = 6;
    pIn[8] = -1;  pHigh[8] = pLow[8] = 4;
    pIn[12] = 1;  pHigh[12] = pLow[12] = 5.5f;
    pIn[16] = -1; pHigh[16] = pLow[16] = 5.6f;
    pIn[20] = 1;  pHigh[20] = pLow[20] = 7;
    pIn[24] = -1; pHigh[24] = pLow[24] = 5.5f;  // 回试低点等于 ZG，不跌破
    pIn[28] = 1;  pHigh[28] = pLow[28] = 8;
    pIn[32] = -1; pHigh[32] = pLow[32] = 7;

    Func5(nCount, pOut, pIn, pHigh, pLow);
    if (!NearlyEqual(pOut[24], 3.0f))
    {
      return false;
    }
  }

  {
    const int nCount = 33;
    float pIn[nCount];
    float pHigh[nCount];
    float pLow[nCount];
    float pOut[nCount];

    for (int i = 0; i < nCount; i++)
    {
      pIn[i] = 0;
      pHigh[i] = 0;
      pLow[i] = 0;
      pOut[i] = -1;
    }

    pIn[0] = 1;   pHigh[0] = pLow[0] = 12;
    pIn[4] = -1;  pHigh[4] = pLow[4] = 9;
    pIn[8] = 1;   pHigh[8] = pLow[8] = 11;
    pIn[12] = -1; pHigh[12] = pLow[12] = 9.5f;
    pIn[16] = 1;  pHigh[16] = pLow[16] = 9.3f;
    pIn[20] = -1; pHigh[20] = pLow[20] = 7;
    pIn[24] = 1;  pHigh[24] = pLow[24] = 9.5f;  // 回抽高点等于 ZD，不升破
    pIn[28] = -1; pHigh[28] = pLow[28] = 6;
    pIn[32] = 1;  pHigh[32] = pLow[32] = 7;

    Func5(nCount, pOut, pIn, pHigh, pLow);
    if (!NearlyEqual(pOut[24], 13.0f))
    {
      return false;
    }
  }

  return true;
}

static bool TestFunc5WritesSecondBuyAfterFirstBuy()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = -1;
  pHigh[0] = 7;
  pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = 12;
  pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = 8;
  pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = 10;
  pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = 7.5f;
  pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = 7;
  pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = 4;
  pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = 4.2f;
  pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = 3.8f;
  pLow[32] = 3.8f;
  pIn[36] = 1;
  pHigh[36] = 6;
  pLow[36] = 6;
  pIn[40] = -1;
  pHigh[40] = 3.9f;
  pLow[40] = 3.9f;

  Func5(nCount, pOut, pIn, pHigh, pLow);

  return NearlyEqual(pOut[32], 1.0f) && NearlyEqual(pOut[40], 2.0f);
}

static bool TestFunc5WritesSecondSellAfterFirstSell()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = 1;
  pHigh[0] = 10;
  pLow[0] = 10;
  pIn[4] = -1;
  pHigh[4] = 5;
  pLow[4] = 5;
  pIn[8] = 1;
  pHigh[8] = 9;
  pLow[8] = 9;
  pIn[12] = -1;
  pHigh[12] = 7;
  pLow[12] = 7;
  pIn[16] = 1;
  pHigh[16] = 12;
  pLow[16] = 12;
  pIn[20] = -1;
  pHigh[20] = 12.1f;
  pLow[20] = 12.1f;
  pIn[24] = 1;
  pHigh[24] = 13;
  pLow[24] = 13;
  pIn[28] = -1;
  pHigh[28] = 12.8f;
  pLow[28] = 12.8f;
  pIn[32] = 1;
  pHigh[32] = 13.2f;
  pLow[32] = 13.2f;
  pIn[36] = -1;
  pHigh[36] = 12.6f;
  pLow[36] = 12.6f;
  pIn[40] = 1;
  pHigh[40] = 13.1f;
  pLow[40] = 13.1f;

  Func5(nCount, pOut, pIn, pHigh, pLow);

  return NearlyEqual(pOut[32], 11.0f) && NearlyEqual(pOut[40], 12.0f);
}

static bool TestFunc5WritesTrendDivergenceFirstSell()
{
  const int nCount = 33;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = 1;
  pHigh[0] = 10;
  pLow[0] = 10;
  pIn[4] = -1;
  pHigh[4] = 5;
  pLow[4] = 5;
  pIn[8] = 1;
  pHigh[8] = 9;
  pLow[8] = 9;
  pIn[12] = -1;
  pHigh[12] = 7;
  pLow[12] = 7;
  pIn[16] = 1;
  pHigh[16] = 12;
  pLow[16] = 12;
  pIn[20] = -1;
  pHigh[20] = 12.1f;
  pLow[20] = 12.1f;
  pIn[24] = 1;
  pHigh[24] = 13;
  pLow[24] = 13;
  pIn[28] = -1;
  pHigh[28] = 12.8f;
  pLow[28] = 12.8f;
  pIn[32] = 1;
  pHigh[32] = 13.2f;
  pLow[32] = 13.2f;

  Func5(nCount, pOut, pIn, pHigh, pLow);

  for (int i = 0; i < nCount; i++)
  {
    if ((i == 32) && !NearlyEqual(pOut[i], 11.0f))
    {
      return false;
    }
    if ((i != 32) && NearlyEqual(pOut[i], 11.0f))
    {
      return false;
    }
  }

  return true;
}

static bool TestFunc5SkipsStrongNewLow()
{
  const int nCount = 33;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = -1;
  pHigh[0] = 7;
  pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = 12;
  pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = 8;
  pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = 10;
  pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = 7.5f;
  pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = 7;
  pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = 4;
  pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = 7;
  pLow[28] = 7;
  pIn[32] = -1;
  pHigh[32] = 3.5f;
  pLow[32] = 3.5f;

  Func5(nCount, pOut, pIn, pHigh, pLow);

  return !NearlyEqual(pOut[32], 1.0f);
}

static bool TestStrengthAndSlopeUsePreviousExtremes()
{
  const int nCount = 5;
  float pIn[nCount] = {0, 0, 1, 0, -1};
  float pHigh[nCount] = {10, 11, 12, 13, 14};
  float pLow[nCount] = {5, 6, 7, 8, 9};
  float pOut[nCount] = {-1, -1, -1, -1, -1};

  Func7(nCount, pOut, pIn, pHigh, pLow);
  if ((pOut[0] != 0) || (pOut[1] != 0) ||
      !NearlyEqual(pOut[2], 140.0f) || (pOut[3] != 0) ||
      !NearlyEqual(pOut[4], -25.0f))
  {
    return false;
  }

  Func8(nCount, pOut, pIn, pHigh, pLow);
  if ((pOut[0] != 0) || (pOut[1] != 0) ||
      !NearlyEqual(pOut[2], 3.5f) || (pOut[3] != 0) ||
      !NearlyEqual(pOut[4], -1.5f))
  {
    return false;
  }

  return true;
}

static bool TestEmptyInputReturns()
{
  Func1(0, 0, 0, 0, 0);
  Func2(0, 0, 0, 0, 0);
  Func7(0, 0, 0, 0, 0);

  return true;
}

static bool TestMacdHistogramFlatSeriesIsZero()
{
  const int nCount = 40;
  float pPrice[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pPrice[i] = 10;
  }

  std::vector<float> Histogram = ComputeMacdHistogram(nCount, pPrice);
  if ((int)Histogram.size() != nCount)
  {
    return false;
  }
  for (int i = 0; i < nCount; i++)
  {
    if (!NearlyEqual(Histogram[(std::size_t)i], 0.0f))
    {
      return false;
    }
  }

  return true;
}

static bool TestMacdHistogramRisingSeriesIsPositive()
{
  const int nCount = 60;
  float pPrice[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pPrice[i] = 10.0f + (float)i;
  }

  std::vector<float> Histogram = ComputeMacdHistogram(nCount, pPrice);
  if ((int)Histogram.size() != nCount)
  {
    return false;
  }

  // 单调上升序列：快线 EMA 始终高于慢线，DIF 单增、DEA 滞后，柱子非负。
  float fSum = 0;
  for (int i = 0; i < nCount; i++)
  {
    if (Histogram[(std::size_t)i] < -0.0001f)
    {
      return false;
    }
    fSum += Histogram[(std::size_t)i];
  }

  return fSum > 0;
}

static bool TestAssignSegmentEnergySetsCumulativeArea()
{
  const int nCount = 60;
  float pHigh[nCount];
  float pLow[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pHigh[i] = 10.5f + (float)i;
    pLow[i] = 9.5f + (float)i;
  }

  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 30, 40));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 59, 69));

  AssignSegmentEnergy(Points, nCount, pHigh, pLow);

  // 上升序列柱子非负，累积能量随序号单调不减且终点为正。
  return (Points[0].fEnergy <= Points[1].fEnergy + 0.0001f) &&
         (Points[1].fEnergy <= Points[2].fEnergy + 0.0001f) &&
         (Points[2].fEnergy > 0);
}

static bool TestAssignSegmentEnergySetsMacdLines()
{
  const int nCount = 60;
  float pHigh[nCount];
  float pLow[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pHigh[i] = 10.5f + (float)i;
    pLow[i] = 9.5f + (float)i;
  }

  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 30, 40));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 59, 69));

  AssignSegmentEnergy(Points, nCount, pHigh, pLow);

  return NearlyEqual(Points[0].fDif, 0.0f) &&
         NearlyEqual(Points[0].fDea, 0.0f) &&
         (Points[1].fDif > 0.0f) &&
         (Points[1].fDea > 0.0f) &&
         (Points[2].fDif > Points[1].fDif) &&
         (Points[2].fDea > Points[1].fDea);
}

static bool TestStrengthMetricsUseMacdEnergy()
{
  SegmentPoint Start = MakeTestEnergyPoint(CZSC_POINT_TOP, 2, 10, 100);
  SegmentPoint End = MakeTestEnergyPoint(CZSC_POINT_BOTTOM, 7, 4, 70);

  StrengthMetrics Strength = MeasureStrength(Start, End);

  return NearlyEqual(Strength.fMacdArea, 30.0f) &&
         NearlyEqual(Strength.fSpace, 6.0f) &&
         NearlyEqual(Strength.fSpeed, 1.2f);
}

static bool TestStrengthMetricsUseMacdLineHeight()
{
  SegmentPoint Start = MakeTestEnergyPoint(CZSC_POINT_TOP, 2, 10, 100);
  SegmentPoint End = MakeTestEnergyPoint(CZSC_POINT_BOTTOM, 7, 4, 70);
  Start.fDif = 1.5f;
  Start.fDea = -0.5f;
  End.fDif = -2.0f;
  End.fDea = 1.0f;

  StrengthMetrics Strength = MeasureStrength(Start, End);

  return NearlyEqual(Strength.fDifHeight, 3.5f) &&
         NearlyEqual(Strength.fDeaHeight, 1.5f) &&
         NearlyEqual(Strength.fMacdArea, 30.0f);
}

static bool TestDivergenceDetectsMacdWeakening()
{
  SegmentPoint PrevStart = MakeTestEnergyPoint(CZSC_POINT_TOP, 0, 12, 100);
  SegmentPoint PrevEnd = MakeTestEnergyPoint(CZSC_POINT_BOTTOM, 4, 2, 60);
  SegmentPoint CurrentStart = MakeTestEnergyPoint(CZSC_POINT_TOP, 8, 4.5f, 55);
  SegmentPoint CurrentEnd = MakeTestEnergyPoint(CZSC_POINT_BOTTOM, 12, 1, 40);

  DivergenceResult Weak = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, -1);
  if (!Weak.bWeakMacd || !Weak.bDivergence)
  {
    return false;
  }

  // 末段 MACD 面积不缩小时不应判定 MACD 背驰。
  SegmentPoint StrongEnd = MakeTestEnergyPoint(CZSC_POINT_BOTTOM, 12, 1, -10);
  DivergenceResult Strong = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, StrongEnd, -1);

  return !Strong.bWeakMacd;
}

static bool TestMacdHistogramHandlesTinyInput()
{
  std::vector<float> Empty = ComputeMacdHistogram(0, 0);
  if (!Empty.empty())
  {
    return false;
  }

  float pOne[1] = {42.0f};
  std::vector<float> One = ComputeMacdHistogram(1, pOne);
  return (One.size() == 1) && NearlyEqual(One[0], 0.0f);
}

static bool TestAssignSegmentEnergyIgnoresDegenerateInput()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 5));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 1, 9));

  float pHigh[2] = {9, 10};
  float pLow[2] = {5, 6};

  AssignSegmentEnergy(Points, 0, 0, 0);     // 无数据
  AssignSegmentEnergy(Points, 2, 0, pLow);  // 缺最高价
  AssignSegmentEnergy(Points, 2, pHigh, 0); // 缺最低价

  return NearlyEqual(Points[0].fEnergy, 0.0f) && NearlyEqual(Points[1].fEnergy, 0.0f);
}

static bool TestDivergenceMacdRequiresEnergyData()
{
  // 无能量数据（fEnergy 全 0）时两段面积均为 0，不应判定 MACD 背驰，但几何背驰仍成立。
  SegmentPoint PrevStart = MakeTestPoint(CZSC_POINT_TOP, 0, 12);
  SegmentPoint PrevEnd = MakeTestPoint(CZSC_POINT_BOTTOM, 4, 2);
  SegmentPoint CurrentStart = MakeTestPoint(CZSC_POINT_TOP, 8, 4.5f);
  SegmentPoint CurrentEnd = MakeTestPoint(CZSC_POINT_BOTTOM, 12, 1);

  DivergenceResult Result = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, -1);
  return !Result.bWeakMacd && Result.bDivergence;
}

static bool TestApplyTradingQualityKeepsWinnerQuality()
{
  const int nCount = 4;
  float pOut[nCount] = {-1, -1, -1, -1};
  std::vector<TradingSignalCandidate> Candidates;

  // 同一根K线：低优先级二买(STRONG) 与 高优先级三买(CONFIRMED)，质量须随胜出信号。
  TradingSignalCandidate Second = MakeTestCandidate(2, 2.0f, 10);
  Second.nQuality = CZSC_SIGNAL_QUALITY_STRONG;
  TradingSignalCandidate Third = MakeTestCandidate(2, 3.0f, 20);
  Third.nQuality = CZSC_SIGNAL_QUALITY_CONFIRMED;
  Candidates.push_back(Second);
  Candidates.push_back(Third);

  ApplyTradingSignalQuality(nCount, pOut, Candidates);

  return NearlyEqual(pOut[2], (float)CZSC_SIGNAL_QUALITY_CONFIRMED) &&
         (pOut[0] == 0) && (pOut[1] == 0) && (pOut[3] == 0);
}

static bool TestFunc10HandlesEmptyInput()
{
  Func10(0, 0, 0, 0, 0);

  const int nCount = 3;
  float pHigh[nCount] = {1, 2, 3};
  float pLow[nCount] = {1, 2, 3};
  float pOut[nCount] = {9, 9, 9};

  Func10(nCount, pOut, 0, pHigh, pLow); // 缺线段信号 → 提前返回，不改写输出

  return (pOut[0] == 9) && (pOut[1] == 9) && (pOut[2] == 9);
}

static bool TestFunc10MatchesFunc5SignalBars()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pSignal[nCount];
  float pQuality[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }

  pIn[0] = -1;
  pHigh[0] = 7;
  pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = 12;
  pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = 8;
  pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = 10;
  pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = 7.5f;
  pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = 7;
  pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = 4;
  pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = 4.2f;
  pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = 3.8f;
  pLow[32] = 3.8f;
  pIn[36] = 1;
  pHigh[36] = 6;
  pLow[36] = 6;
  pIn[40] = -1;
  pHigh[40] = 4.5f;
  pLow[40] = 4.5f;

  Func5(nCount, pSignal, pIn, pHigh, pLow);
  Func10(nCount, pQuality, pIn, pHigh, pLow);

  // 每个买卖点都应带有质量等级(1 或 2)，无信号处两者皆为 0，位置完全对应。
  for (int i = 0; i < nCount; i++)
  {
    bool bHasSignal = !NearlyEqual(pSignal[i], 0.0f);
    bool bHasQuality = !NearlyEqual(pQuality[i], 0.0f);
    if (bHasSignal != bHasQuality)
    {
      return false;
    }
    if (bHasQuality && ((pQuality[i] < 0.9f) || (pQuality[i] > 2.1f)))
    {
      return false;
    }
  }

  return true;
}

static bool TestFunc10WritesSignalQuality()
{
  // 复用一类买点背驰场景：index 32 出现一类买点，价差与速度同时走弱 → 标准强信号。
  const int nCount = 33;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = -1;
  pHigh[0] = 7;
  pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = 12;
  pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = 8;
  pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = 10;
  pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = 7.5f;
  pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = 7;
  pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = 4;
  pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = 4.2f;
  pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = 3.8f;
  pLow[32] = 3.8f;

  Func10(nCount, pOut, pIn, pHigh, pLow);

  // 一类买点(index32)价差与速度同时走弱 → 标准强信号(2)。
  // 中枢0离开后的首次回试不回中枢 → 三卖在28(确认级 1)。
  for (int i = 0; i < nCount; i++)
  {
    float fExpected = 0.0f;
    if (i == 32)
    {
      fExpected = (float)CZSC_SIGNAL_QUALITY_STRONG;
    }
    else if (i == 28)
    {
      fExpected = (float)CZSC_SIGNAL_QUALITY_CONFIRMED;
    }
    if (!NearlyEqual(pOut[i], fExpected))
    {
      return false;
    }
  }

  return true;
}

static bool TestFirstCandidateMacdUpgradesQuality()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 14));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 17));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 15));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 30, 8));

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 18, 13));
  Centers.push_back(MakeTestCenter(16, 24, 12, 9));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;

  // 末段（点4→点5）价差更大但速度更慢：仅速度走弱，不构成严格趋势背驰。
  std::vector<TradingSignalCandidate> Plain =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pPlain = FindSignalCandidate(Plain, 30, 1.0f);
  if (pPlain != 0)
  {
    return false;
  }

  // 注入末段 MACD 面积小于前段：动力学走弱后，才生成强一类信号。
  Points[2].fEnergy = 100;
  Points[3].fEnergy = 70;
  Points[4].fEnergy = 50;
  Points[5].fEnergy = 40;

  std::vector<TradingSignalCandidate> Energized =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pEnergized = FindSignalCandidate(Energized, 30, 1.0f);

  return (pEnergized != 0) &&
         pEnergized->Divergence.bWeakMacd &&
         !pEnergized->Divergence.bWeakSpace &&
         pEnergized->Divergence.bWeakSpeed &&
         (pEnergized->nQuality == CZSC_SIGNAL_QUALITY_STRONG);
}

static bool TestCenterAccumulatesGGDD()
{
  // 进入段(0->4)不算中枢；中枢由其后三笔(4->8、8->12、12->16)重叠构成。
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 5));

  std::vector<Center> Centers = BuildCenters(Points);
  if (Centers.size() != 1)
  {
    return false;
  }

  // ZG/ZD 为三段重叠区间[5,9]，GG/DD 为三段全幅极值[4,10]
  return NearlyEqual(Centers[0].fHigh, 9.0f) && NearlyEqual(Centers[0].fLow, 5.0f) &&
         NearlyEqual(Centers[0].fTop, 10.0f) && NearlyEqual(Centers[0].fBottom, 4.0f);
}

static bool TestCenterExtendUpdatesGGDD()
{
  // 进入段(0->4)不算中枢；中枢三笔(4->8、8->12、12->16)初成，GG=9；
  // 16->20 留在中枢内，20->24 向上离开，24->28 首次回试进中枢，故原中枢延伸；
  // 离开段创新高 11 把 GG 从 9 扩张到 11，ZD 收缩到 7。
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 8));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 5));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 8));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 20, 7));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 24, 11));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 28, 7));

  std::vector<Center> Centers = BuildCenters(Points);
  if (Centers.size() != 1)
  {
    return false;
  }

  // 延伸把 GG 从初始 9 扩张到 11、ZD 从 6 收缩到 7；ZG=8（min 8,9,8），DD=5（min 5,6,6）
  return (Centers[0].nEnd == 28) &&
         NearlyEqual(Centers[0].fHigh, 8.0f) && NearlyEqual(Centers[0].fLow, 7.0f) &&
         NearlyEqual(Centers[0].fTop, 11.0f) && NearlyEqual(Centers[0].fBottom, 5.0f);
}

static bool TestClassifyCenterRelationUp()
{
  Center Prev = MakeTestCenterFull(0, 12, 9, 5, 10, 4);     // GG=10
  Center Next = MakeTestCenterFull(16, 28, 14, 12, 15, 11); // DD=11 > 10
  return ClassifyCenterRelation(Prev, Next) == CZSC_CENTER_RELATION_UP;
}

static bool TestClassifyCenterRelationDown()
{
  Center Prev = MakeTestCenterFull(0, 12, 14, 12, 15, 11);  // DD=11
  Center Next = MakeTestCenterFull(16, 28, 9, 5, 10, 4);    // GG=10 < 11
  return ClassifyCenterRelation(Prev, Next) == CZSC_CENTER_RELATION_DOWN;
}

static bool TestClassifyCenterRelationExtension()
{
  Center Prev = MakeTestCenterFull(0, 12, 9, 5, 12, 4);
  Center Next = MakeTestCenterFull(16, 28, 15, 10, 16, 8);

  // ZG/ZD 已不重叠(Next.fLow 10 > Prev.fHigh 9，旧判据会判上涨)，
  // 但全幅 GG/DD 仍重叠 → 中心定理二判为中枢扩展，凸显新能力。
  bool bZgZdSeparated = (Next.fLow > Prev.fHigh);
  return bZgZdSeparated &&
         (ClassifyCenterRelation(Prev, Next) == CZSC_CENTER_RELATION_EXTENSION);
}

static bool TestClassifyCenterRelationExtensionAtFullExtentBoundary()
{
  {
    Center Prev = MakeTestCenterFull(0, 12, 9, 5, 12, 4);
    Center Next = MakeTestCenterFull(16, 28, 15, 10, 16, 12); // 后DD == 前GG
    if (ClassifyCenterRelation(Prev, Next) != CZSC_CENTER_RELATION_EXTENSION)
    {
      return false;
    }
  }

  {
    Center Prev = MakeTestCenterFull(0, 12, 15, 10, 16, 12);
    Center Next = MakeTestCenterFull(16, 28, 9, 5, 12, 4);    // 后GG == 前DD
    if (ClassifyCenterRelation(Prev, Next) != CZSC_CENTER_RELATION_EXTENSION)
    {
      return false;
    }
  }

  return true;
}

static bool TestClassifyCenterLifecycleExtension()
{
  Center Prev = MakeTestCenterFull(0, 12, 9, 5, 12, 4);
  Center Next = MakeTestCenterFull(16, 28, 8, 6, 10, 3);

  return ClassifyCenterLifecycle(Prev, Next) == CZSC_CENTER_LIFECYCLE_EXTENSION;
}

static bool TestClassifyCenterLifecycleExpansion()
{
  Center Prev = MakeTestCenterFull(0, 12, 9, 5, 12, 4);
  Center Next = MakeTestCenterFull(16, 28, 15, 10, 16, 8);

  return (ClassifyCenterRelation(Prev, Next) == CZSC_CENTER_RELATION_EXTENSION) &&
         (ClassifyCenterLifecycle(Prev, Next) == CZSC_CENTER_LIFECYCLE_EXPANSION);
}

static bool TestClassifyCenterLifecycleNewborn()
{
  Center Prev = MakeTestCenterFull(0, 12, 9, 5, 10, 4);
  Center Up = MakeTestCenterFull(16, 28, 14, 12, 15, 11);
  Center Down = MakeTestCenterFull(32, 44, 2, 1, 3, 0);

  return (ClassifyCenterRelation(Prev, Up) == CZSC_CENTER_RELATION_UP) &&
         (ClassifyCenterLifecycle(Prev, Up) == CZSC_CENTER_LIFECYCLE_NEWBORN_UP) &&
         (ClassifyCenterRelation(Prev, Down) == CZSC_CENTER_RELATION_DOWN) &&
         (ClassifyCenterLifecycle(Prev, Down) == CZSC_CENTER_LIFECYCLE_NEWBORN_DOWN);
}

static bool TestWriteCenterRelationSignalMarks()
{
  const int nCount = 20;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenterFull(0, 4, 9, 5, 10, 4));
  Centers.push_back(MakeTestCenterFull(5, 9, 15, 13, 16, 12));  // vs 前: DD12 > GG10 → 上涨
  Centers.push_back(MakeTestCenterFull(10, 14, 7, 3, 8, 2));    // vs 前: GG8 < DD12 → 下跌
  Centers.push_back(MakeTestCenterFull(15, 19, 8, 4, 9, 3));    // vs 前: 全幅重叠 → 扩展

  WriteCenterRelationSignal(nCount, pOut, Centers);

  for (int i = 0; i < nCount; i++)
  {
    float fExpected = 0;
    if (i == 5)
    {
      fExpected = 1;   // 上涨延续
    }
    else if (i == 10)
    {
      fExpected = -1;  // 下跌延续
    }
    else if (i == 15)
    {
      fExpected = 2;   // 中枢扩展
    }

    if (!NearlyEqual(pOut[i], fExpected))
    {
      return false;
    }
  }

  return true;
}

static bool TestWriteCenterLifecycleSignalMarks()
{
  const int nCount = 30;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenterFull(0, 4, 9, 5, 12, 4));
  Centers.push_back(MakeTestCenterFull(5, 9, 8, 6, 10, 3));       // [ZD,ZG] 重叠 → 延伸
  Centers.push_back(MakeTestCenterFull(10, 14, 15, 10, 16, 8));   // 全幅重叠 → 扩展
  Centers.push_back(MakeTestCenterFull(15, 19, 25, 21, 26, 20));  // 全幅上移 → 上涨新生
  Centers.push_back(MakeTestCenterFull(20, 24, 10, 5, 12, 4));    // 全幅下移 → 下跌新生

  WriteCenterLifecycleSignal(nCount, pOut, Centers);

  for (int i = 0; i < nCount; i++)
  {
    float fExpected = 0;
    if (i == 5)
    {
      fExpected = (float)CZSC_CENTER_LIFECYCLE_EXTENSION;
    }
    else if (i == 10)
    {
      fExpected = (float)CZSC_CENTER_LIFECYCLE_EXPANSION;
    }
    else if (i == 15)
    {
      fExpected = (float)CZSC_CENTER_LIFECYCLE_NEWBORN_UP;
    }
    else if (i == 20)
    {
      fExpected = (float)CZSC_CENTER_LIFECYCLE_NEWBORN_DOWN;
    }
    if (!NearlyEqual(pOut[i], fExpected))
    {
      return false;
    }
  }

  return true;
}

static bool TestFunc11WritesCenterRelation()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  // 两个相邻中枢(同 TestCentersSplitWhenOverlapBreaks)：中枢1吸收 P4→P5 [5,20] 后
  // GG 扩张至 20，与中枢2 DD=15 重叠 → 中枢关系判为扩展，在后中枢起点(index24)标记 2。
  pIn[0] = -1;
  pHigh[0] = pLow[0] = 1;
  pIn[4] = 1;
  pHigh[4] = pLow[4] = 10;
  pIn[8] = -1;
  pHigh[8] = pLow[8] = 4;
  pIn[12] = 1;
  pHigh[12] = pLow[12] = 9;
  pIn[16] = -1;
  pHigh[16] = pLow[16] = 5;
  pIn[20] = 1;
  pHigh[20] = pLow[20] = 20;
  pIn[24] = -1;
  pHigh[24] = pLow[24] = 15;
  pIn[28] = 1;
  pHigh[28] = pLow[28] = 19;
  pIn[32] = -1;
  pHigh[32] = pLow[32] = 16;
  pIn[36] = 1;
  pHigh[36] = pLow[36] = 18;
  pIn[40] = -1;
  pHigh[40] = pLow[40] = 17;

  Func11(nCount, pOut, pIn, pHigh, pLow);

  for (int i = 0; i < nCount; i++)
  {
    float fExpected = (i == 24) ? 2.0f : 0.0f;  // 后中枢起点(index24)标记扩展
    if (!NearlyEqual(pOut[i], fExpected))
    {
      return false;
    }
  }

  return true;
}

static bool TestFunc11HandlesEmptyInput()
{
  Func11(0, 0, 0, 0, 0);

  const int nCount = 3;
  float pHigh[nCount] = {1, 2, 3};
  float pLow[nCount] = {1, 2, 3};
  float pOut[nCount] = {9, 9, 9};

  Func11(nCount, pOut, 0, pHigh, pLow); // 缺线段信号 → 提前返回，不改写输出

  return (pOut[0] == 9) && (pOut[1] == 9) && (pOut[2] == 9);
}

static bool TestReversalStrengthExtension()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 8));     // 一买点
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 8.5f));     // 反弹仅到 8.5
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 12, 9));             // 中枢 [9,12]

  // 反弹高 8.5 < ZD 9 → 最弱 → 最后中枢扩展
  return ClassifyReversalStrength(Points, Centers, 0, 0, 1.0f) == CZSC_REVERSAL_EXTENSION;
}

static bool TestReversalStrengthConsolidation()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 8));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10.5f));    // 反弹回到中枢内
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 12, 9));

  return ClassifyReversalStrength(Points, Centers, 0, 0, 1.0f) == CZSC_REVERSAL_CONSOLIDATION;
}

static bool TestReversalStrengthTrend()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 8));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 13));       // 反弹突破中枢上沿
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 12, 9));

  return ClassifyReversalStrength(Points, Centers, 0, 0, 1.0f) == CZSC_REVERSAL_TREND;
}

static bool TestReversalStrengthSell()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 12, 9));

  std::vector<SegmentPoint> Cons;
  Cons.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 15));         // 一卖点
  Cons.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 11));      // 回抽进中枢 → 盘整
  if (ClassifyReversalStrength(Cons, Centers, 0, 0, 11.0f) != CZSC_REVERSAL_CONSOLIDATION)
  {
    return false;
  }

  std::vector<SegmentPoint> Trend;
  Trend.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 15));
  Trend.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 8));      // 跌破中枢下沿 → 反趋势
  return ClassifyReversalStrength(Trend, Centers, 0, 0, 11.0f) == CZSC_REVERSAL_TREND;
}

static bool TestReversalStrengthUnknownNoRebound()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 8));     // 一买点，其后尚无反弹
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 12, 9));

  return ClassifyReversalStrength(Points, Centers, 0, 0, 1.0f) == CZSC_REVERSAL_UNKNOWN;
}

static bool TestFunc12WritesReversalCode()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = -1;
  pHigh[0] = pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = pLow[32] = 3.8f;
  pIn[36] = 1;
  pHigh[36] = pLow[36] = 6;
  pIn[40] = -1;
  pHigh[40] = pLow[40] = 4.5f;

  Func12(nCount, pOut, pIn, pHigh, pLow);

  // 一买(index32)带 1/2/3 的背驰-转折编码；二买(index40)与无信号处为 0
  bool bFirstCoded = (pOut[32] >= 0.9f) && (pOut[32] <= 3.1f);
  return bFirstCoded && NearlyEqual(pOut[40], 0.0f) && NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingReversalRequiresFirstSignal()
{
  const int nCount = 4;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate First = MakeTestCandidate(1, 1.0f, 10);
  First.nReversal = CZSC_REVERSAL_TREND;
  TradingSignalCandidate Third = MakeTestCandidate(2, 3.0f, 20);
  Third.nReversal = CZSC_REVERSAL_TREND;
  TradingSignalCandidate SecondSell = MakeTestCandidate(3, 12.0f, 20);
  SecondSell.nReversal = CZSC_REVERSAL_EXTENSION;
  Candidates.push_back(First);
  Candidates.push_back(Third);
  Candidates.push_back(SecondSell);

  ApplyTradingSignalReversal(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], 3.0f) &&
         NearlyEqual(pOut[2], 0.0f) &&
         NearlyEqual(pOut[3], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingReversalPointIdMapsCodes()
{
  const int nCount = 8;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate First = MakeTestCandidate(1, 1.0f, 30);
  First.nPoint = 4;
  First.nReversal = CZSC_REVERSAL_TREND;
  TradingSignalCandidate Unknown = MakeTestCandidate(3, 11.0f, 30);
  Unknown.nPoint = 5;
  Unknown.nReversal = CZSC_REVERSAL_UNKNOWN;
  TradingSignalCandidate Third = MakeTestCandidate(5, 3.0f, 20);
  Third.nPoint = 6;
  Third.nReversal = CZSC_REVERSAL_TREND;
  TradingSignalCandidate MissingPoint = MakeTestCandidate(7, 1.0f, 30);
  MissingPoint.nReversal = CZSC_REVERSAL_EXTENSION;
  Candidates.push_back(First);
  Candidates.push_back(Unknown);
  Candidates.push_back(Third);
  Candidates.push_back(MissingPoint);

  ApplyTradingSignalReversalPointId(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], 6.0f) &&
         NearlyEqual(pOut[3], 0.0f) &&
         NearlyEqual(pOut[5], 0.0f) &&
         NearlyEqual(pOut[7], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestCenterAftermathExtended()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenterFull(0, 12, 10, 5, 12, 4));    // GG=12
  Centers.push_back(MakeTestCenterFull(16, 28, 13, 9, 14, 8));   // 与前全幅重叠 → 扩展
  return ClassifyCenterAftermath(Centers, 0, 3.0f) == CZSC_CENTER_AFTERMATH_EXTENDED;
}

static bool TestCenterAftermathNewborn()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenterFull(0, 12, 9, 5, 10, 4));     // GG=10
  Centers.push_back(MakeTestCenterFull(16, 28, 14, 12, 16, 12)); // DD=12 > 10 → 上涨新生

  if (ClassifyCenterAftermath(Centers, 0, 3.0f) != CZSC_CENTER_AFTERMATH_NEWBORN)
  {
    return false;
  }
  // 三卖 + 上涨(反向) → 未知
  return ClassifyCenterAftermath(Centers, 0, 13.0f) == CZSC_CENTER_AFTERMATH_UNKNOWN;
}

static bool TestCenterAftermathUnknownNoNext()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenterFull(0, 12, 9, 5, 10, 4));     // 仅一个中枢，后续未形成
  return ClassifyCenterAftermath(Centers, 0, 3.0f) == CZSC_CENTER_AFTERMATH_UNKNOWN;
}

static bool TestCenterAftermathSell()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenterFull(0, 12, 14, 12, 16, 12));  // DD=12
  Centers.push_back(MakeTestCenterFull(16, 28, 9, 5, 10, 4));    // GG=10 < 12 → 下跌

  // 三卖 + 下跌 → 中枢新生
  if (ClassifyCenterAftermath(Centers, 0, 13.0f) != CZSC_CENTER_AFTERMATH_NEWBORN)
  {
    return false;
  }
  // 三买 + 下跌(反向) → 未知
  return ClassifyCenterAftermath(Centers, 0, 3.0f) == CZSC_CENTER_AFTERMATH_UNKNOWN;
}

static bool TestCenterAftermathRequiresThirdSignal()
{
  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenterFull(0, 12, 10, 5, 12, 4));
  Centers.push_back(MakeTestCenterFull(16, 28, 13, 9, 14, 8));   // 与前全幅重叠 → 扩展

  return (ClassifyCenterAftermath(Centers, 0, 1.0f) == CZSC_CENTER_AFTERMATH_UNKNOWN) &&
         (ClassifyCenterAftermath(Centers, 0, 12.0f) == CZSC_CENTER_AFTERMATH_UNKNOWN);
}

static bool TestApplyTradingAftermathMapsCodes()
{
  const int nCount = 8;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Extended = MakeTestCandidate(1, 3.0f, 20);
  Extended.nAfterEffect = CZSC_CENTER_AFTERMATH_EXTENDED;
  TradingSignalCandidate Newborn = MakeTestCandidate(3, 3.0f, 20);
  Newborn.nAfterEffect = CZSC_CENTER_AFTERMATH_NEWBORN;
  TradingSignalCandidate Unknown = MakeTestCandidate(5, 3.0f, 20);
  TradingSignalCandidate NonThird = MakeTestCandidate(7, 1.0f, 20);
  NonThird.nAfterEffect = CZSC_CENTER_AFTERMATH_EXTENDED;
  Candidates.push_back(Extended);
  Candidates.push_back(Newborn);
  Candidates.push_back(Unknown);
  Candidates.push_back(NonThird);

  ApplyTradingSignalAftermath(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], 1.0f) && NearlyEqual(pOut[3], 2.0f) &&
         NearlyEqual(pOut[5], 0.0f) && NearlyEqual(pOut[7], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingCenterPositionMapsCodes()
{
  const int nCount = 6;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -9;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Below = MakeTestCandidate(1, 1.0f, 20);
  Below.nCenterPosition = CZSC_CENTER_POSITION_BELOW;
  TradingSignalCandidate Inside = MakeTestCandidate(3, 2.0f, 20);
  Inside.nCenterPosition = CZSC_CENTER_POSITION_INSIDE;
  TradingSignalCandidate Above = MakeTestCandidate(5, 3.0f, 20);
  Above.nCenterPosition = CZSC_CENTER_POSITION_ABOVE;
  Candidates.push_back(Below);
  Candidates.push_back(Inside);
  Candidates.push_back(Above);

  ApplyTradingSignalCenterPosition(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], -1.0f) &&
         NearlyEqual(pOut[3], 0.0f) &&
         NearlyEqual(pOut[5], 1.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingMovementTypeMapsCodes()
{
  const int nCount = 5;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -9;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Low = MakeTestCandidate(2, 1.0f, 10);
  Low.nMovementType = CZSC_MOVEMENT_UP;
  TradingSignalCandidate High = MakeTestCandidate(2, 2.0f, 20);
  High.nMovementType = CZSC_MOVEMENT_DOWN;
  TradingSignalCandidate Flat = MakeTestCandidate(4, 3.0f, 20);
  Flat.nMovementType = CZSC_MOVEMENT_CONSOLIDATION;
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(Flat);

  ApplyTradingSignalMovementType(nCount, pOut, Candidates);

  return NearlyEqual(pOut[2], -1.0f) &&
         NearlyEqual(pOut[4], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingPriorityMapsCodes()
{
  const int nCount = 5;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Low = MakeTestCandidate(2, 2.0f, 10);
  TradingSignalCandidate High = MakeTestCandidate(2, 3.0f, 20);
  TradingSignalCandidate First = MakeTestCandidate(4, 1.0f, 30);
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(First);

  ApplyTradingSignalPriority(nCount, pOut, Candidates);

  return NearlyEqual(pOut[2], 20.0f) &&
         NearlyEqual(pOut[4], 30.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingCenterIdMapsCodes()
{
  const int nCount = 5;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Low = MakeTestCandidate(2, 2.0f, 10);
  Low.nCenter = 4;
  TradingSignalCandidate High = MakeTestCandidate(2, 3.0f, 20);
  High.nCenter = 7;
  TradingSignalCandidate Unknown = MakeTestCandidate(4, 1.0f, 30);
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(Unknown);

  ApplyTradingSignalCenterId(nCount, pOut, Candidates);

  return NearlyEqual(pOut[2], 8.0f) &&
         NearlyEqual(pOut[4], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingBreakoutIdMapsCodes()
{
  const int nCount = 5;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Low = MakeTestCandidate(2, 2.0f, 10);
  Low.nBreakout = 1;
  TradingSignalCandidate High = MakeTestCandidate(2, 3.0f, 20);
  High.nBreakout = 5;
  TradingSignalCandidate None = MakeTestCandidate(4, 1.0f, 30);
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(None);

  ApplyTradingSignalBreakoutId(nCount, pOut, Candidates);

  return NearlyEqual(pOut[2], 6.0f) &&
         NearlyEqual(pOut[4], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingBreakoutPointIdsMapCodes()
{
  const int nCount = 6;
  float pLeave[nCount];
  float pRetest[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pLeave[i] = -1;
    pRetest[i] = -1;
  }

  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(1, 4));
  Breakouts.back().nLeavePoint = 3;
  Breakouts.push_back(MakeTestBreakout(-1, 8));
  Breakouts.back().nLeavePoint = 7;

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Low = MakeTestCandidate(2, 2.0f, 10);
  Low.nBreakout = 0;
  TradingSignalCandidate High = MakeTestCandidate(2, 3.0f, 20);
  High.nBreakout = 1;
  TradingSignalCandidate Missing = MakeTestCandidate(4, 1.0f, 30);
  Missing.nBreakout = 8;
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(Missing);

  ApplyTradingSignalBreakoutLeavePointId(nCount, pLeave, Candidates, Breakouts);
  ApplyTradingSignalBreakoutRetestPointId(nCount, pRetest, Candidates, Breakouts);

  return NearlyEqual(pLeave[2], 8.0f) &&
         NearlyEqual(pRetest[2], 9.0f) &&
         NearlyEqual(pLeave[4], 0.0f) &&
         NearlyEqual(pRetest[4], 0.0f) &&
         NearlyEqual(pLeave[0], 0.0f) &&
         NearlyEqual(pRetest[0], 0.0f);
}

static bool TestApplyTradingPointIdMapsCodes()
{
  const int nCount = 5;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Low = MakeTestCandidate(2, 2.0f, 10);
  Low.nPoint = 4;
  TradingSignalCandidate High = MakeTestCandidate(2, 3.0f, 20);
  High.nPoint = 8;
  TradingSignalCandidate Unknown = MakeTestCandidate(4, 1.0f, 30);
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(Unknown);

  ApplyTradingSignalPointId(nCount, pOut, Candidates);

  return NearlyEqual(pOut[2], 9.0f) &&
         NearlyEqual(pOut[4], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingTrendIdMapsCodes()
{
  const int nCount = 5;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Low = MakeTestCandidate(2, 2.0f, 10);
  Low.nTrend = 1;
  TradingSignalCandidate High = MakeTestCandidate(2, 3.0f, 20);
  High.nTrend = 5;
  TradingSignalCandidate None = MakeTestCandidate(4, 1.0f, 30);
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(None);

  ApplyTradingSignalTrendId(nCount, pOut, Candidates);

  return NearlyEqual(pOut[2], 6.0f) &&
         NearlyEqual(pOut[4], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingSecondContextPointIdsMapCodes()
{
  const int nCount = 14;
  float pBase[nCount];
  float pTurn[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pBase[i] = -1;
    pTurn[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(3, 2.0f, 10);
  Buy.nSource = 2;
  Buy.nPoint = 6;
  Buy.nSecondBasePoint = 4;
  Buy.nSecondTurnPoint = 5;
  TradingSignalCandidate Sell = MakeTestCandidate(5, 12.0f, 10);
  Sell.nSource = 2;
  Sell.nPoint = 9;
  Sell.nSecondBasePoint = 7;
  Sell.nSecondTurnPoint = 8;
  TradingSignalCandidate MissingBase = MakeTestCandidate(7, 2.0f, 10);
  MissingBase.nSource = 2;
  MissingBase.nPoint = 10;
  MissingBase.nSecondTurnPoint = 9;
  TradingSignalCandidate WrongOrder = MakeTestCandidate(9, 2.0f, 10);
  WrongOrder.nSource = 2;
  WrongOrder.nPoint = 12;
  WrongOrder.nSecondBasePoint = 11;
  WrongOrder.nSecondTurnPoint = 10;
  TradingSignalCandidate NonSecond = MakeTestCandidate(11, 3.0f, 20);
  NonSecond.nSource = 3;
  NonSecond.nPoint = 13;
  NonSecond.nSecondBasePoint = 10;
  NonSecond.nSecondTurnPoint = 12;
  TradingSignalCandidate LowerPrioritySecond = MakeTestCandidate(12, 2.0f, 10);
  LowerPrioritySecond.nSource = 2;
  LowerPrioritySecond.nPoint = 6;
  LowerPrioritySecond.nSecondBasePoint = 4;
  LowerPrioritySecond.nSecondTurnPoint = 5;
  TradingSignalCandidate HigherPriorityThird = MakeTestCandidate(12, 3.0f, 20);
  HigherPriorityThird.nSource = 3;
  HigherPriorityThird.nPoint = 6;
  Candidates.push_back(Buy);
  Candidates.push_back(Sell);
  Candidates.push_back(MissingBase);
  Candidates.push_back(WrongOrder);
  Candidates.push_back(NonSecond);
  Candidates.push_back(LowerPrioritySecond);
  Candidates.push_back(HigherPriorityThird);

  ApplyTradingSignalSecondBasePointId(nCount, pBase, Candidates);
  ApplyTradingSignalSecondTurnPointId(nCount, pTurn, Candidates);

  return NearlyEqual(pBase[3], 5.0f) &&
         NearlyEqual(pTurn[3], 6.0f) &&
         NearlyEqual(pBase[5], 8.0f) &&
         NearlyEqual(pTurn[5], 9.0f) &&
         NearlyEqual(pBase[7], 0.0f) &&
         NearlyEqual(pTurn[7], 0.0f) &&
         NearlyEqual(pBase[9], 0.0f) &&
         NearlyEqual(pTurn[9], 0.0f) &&
         NearlyEqual(pBase[11], 0.0f) &&
         NearlyEqual(pTurn[11], 0.0f) &&
         NearlyEqual(pBase[12], 0.0f) &&
         NearlyEqual(pTurn[12], 0.0f) &&
         NearlyEqual(pBase[0], 0.0f) &&
         NearlyEqual(pTurn[0], 0.0f);
}

static bool TestApplyTradingSmallTurnMapsCodes()
{
  const int nCount = 18;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 3.0f, 20);
  Buy.nSource = 3;
  Buy.nPoint = 1;
  Buy.nCenter = 0;
  Buy.nBreakout = 0;
  Buy.nSmallTurn = 1;
  TradingSignalCandidate Sell = MakeTestCandidate(3, 13.0f, 20);
  Sell.nSource = 3;
  Sell.nPoint = 3;
  Sell.nCenter = 0;
  Sell.nBreakout = 1;
  Sell.nSmallTurn = -1;
  TradingSignalCandidate None = MakeTestCandidate(5, 3.0f, 20);
  None.nSource = 3;
  None.nPoint = 5;
  None.nCenter = 0;
  None.nBreakout = 2;
  TradingSignalCandidate WrongBuy = MakeTestCandidate(7, 3.0f, 20);
  WrongBuy.nSource = 3;
  WrongBuy.nPoint = 7;
  WrongBuy.nCenter = 0;
  WrongBuy.nBreakout = 3;
  WrongBuy.nSmallTurn = -1;
  TradingSignalCandidate WrongSell = MakeTestCandidate(9, 13.0f, 20);
  WrongSell.nSource = 3;
  WrongSell.nPoint = 9;
  WrongSell.nCenter = 0;
  WrongSell.nBreakout = 4;
  WrongSell.nSmallTurn = 1;
  TradingSignalCandidate NonThird = MakeTestCandidate(11, 1.0f, 20);
  NonThird.nSource = 1;
  NonThird.nPoint = 11;
  NonThird.nCenter = 0;
  NonThird.nBreakout = 5;
  NonThird.nSmallTurn = 1;
  TradingSignalCandidate MissingSource = MakeTestCandidate(13, 3.0f, 20);
  MissingSource.nPoint = 13;
  MissingSource.nCenter = 0;
  MissingSource.nBreakout = 6;
  MissingSource.nSmallTurn = 1;
  TradingSignalCandidate MissingBreakout = MakeTestCandidate(15, 3.0f, 20);
  MissingBreakout.nSource = 3;
  MissingBreakout.nPoint = 15;
  MissingBreakout.nCenter = 0;
  MissingBreakout.nSmallTurn = 1;
  TradingSignalCandidate MissingCenter = MakeTestCandidate(17, 3.0f, 20);
  MissingCenter.nSource = 3;
  MissingCenter.nPoint = 17;
  MissingCenter.nBreakout = 7;
  MissingCenter.nSmallTurn = 1;
  Candidates.push_back(Buy);
  Candidates.push_back(Sell);
  Candidates.push_back(None);
  Candidates.push_back(WrongBuy);
  Candidates.push_back(WrongSell);
  Candidates.push_back(NonThird);
  Candidates.push_back(MissingSource);
  Candidates.push_back(MissingBreakout);
  Candidates.push_back(MissingCenter);

  ApplyTradingSignalSmallTurn(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], 1.0f) && NearlyEqual(pOut[3], -1.0f) &&
         NearlyEqual(pOut[5], 0.0f) && NearlyEqual(pOut[7], 0.0f) &&
         NearlyEqual(pOut[9], 0.0f) && NearlyEqual(pOut[11], 0.0f) &&
         NearlyEqual(pOut[13], 0.0f) && NearlyEqual(pOut[15], 0.0f) &&
         NearlyEqual(pOut[17], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingSmallTurnPointIdsMapCodes()
{
  const int nCount = 14;
  float pBase[nCount];
  float pLeave[nCount];
  float pRetest[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pBase[i] = -1;
    pLeave[i] = -1;
    pRetest[i] = -1;
  }

  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(1, 4));
  Breakouts.back().nLeavePoint = 2;
  Breakouts.push_back(MakeTestBreakout(-1, 8));
  Breakouts.back().nLeavePoint = 6;

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 3.0f, 20);
  Buy.nSource = 3;
  Buy.nPoint = 1;
  Buy.nCenter = 0;
  Buy.nBreakout = 0;
  Buy.nSmallTurn = 1;
  Buy.nSmallTurnBasePoint = 0;
  TradingSignalCandidate Sell = MakeTestCandidate(3, 13.0f, 20);
  Sell.nSource = 3;
  Sell.nPoint = 3;
  Sell.nCenter = 0;
  Sell.nBreakout = 1;
  Sell.nSmallTurn = -1;
  Sell.nSmallTurnBasePoint = 2;
  TradingSignalCandidate WrongDirection = MakeTestCandidate(5, 3.0f, 20);
  WrongDirection.nSource = 3;
  WrongDirection.nPoint = 5;
  WrongDirection.nCenter = 0;
  WrongDirection.nBreakout = 0;
  WrongDirection.nSmallTurn = -1;
  WrongDirection.nSmallTurnBasePoint = 4;
  TradingSignalCandidate MissingBreakout = MakeTestCandidate(7, 3.0f, 20);
  MissingBreakout.nSource = 3;
  MissingBreakout.nPoint = 7;
  MissingBreakout.nCenter = 0;
  MissingBreakout.nSmallTurn = 1;
  MissingBreakout.nSmallTurnBasePoint = 6;
  TradingSignalCandidate LowerPrioritySmallTurn = MakeTestCandidate(10, 3.0f, 10);
  LowerPrioritySmallTurn.nSource = 3;
  LowerPrioritySmallTurn.nPoint = 10;
  LowerPrioritySmallTurn.nCenter = 0;
  LowerPrioritySmallTurn.nBreakout = 0;
  LowerPrioritySmallTurn.nSmallTurn = 1;
  LowerPrioritySmallTurn.nSmallTurnBasePoint = 9;
  TradingSignalCandidate HigherPriorityNonThird = MakeTestCandidate(10, 2.0f, 20);
  HigherPriorityNonThird.nSource = 2;
  TradingSignalCandidate MissingCenter = MakeTestCandidate(12, 3.0f, 20);
  MissingCenter.nSource = 3;
  MissingCenter.nPoint = 12;
  MissingCenter.nBreakout = 0;
  MissingCenter.nSmallTurn = 1;
  MissingCenter.nSmallTurnBasePoint = 11;
  Candidates.push_back(Buy);
  Candidates.push_back(Sell);
  Candidates.push_back(WrongDirection);
  Candidates.push_back(MissingBreakout);
  Candidates.push_back(LowerPrioritySmallTurn);
  Candidates.push_back(HigherPriorityNonThird);
  Candidates.push_back(MissingCenter);

  ApplyTradingSignalSmallTurnBasePointId(nCount, pBase, Candidates);
  ApplyTradingSignalSmallTurnLeavePointId(nCount, pLeave, Candidates, Breakouts);
  ApplyTradingSignalSmallTurnRetestPointId(nCount, pRetest, Candidates, Breakouts);

  return NearlyEqual(pBase[1], 1.0f) &&
         NearlyEqual(pLeave[1], 3.0f) &&
         NearlyEqual(pRetest[1], 5.0f) &&
         NearlyEqual(pBase[3], 3.0f) &&
         NearlyEqual(pLeave[3], 7.0f) &&
         NearlyEqual(pRetest[3], 9.0f) &&
         NearlyEqual(pBase[5], 0.0f) &&
         NearlyEqual(pLeave[5], 0.0f) &&
         NearlyEqual(pRetest[5], 0.0f) &&
         NearlyEqual(pBase[7], 0.0f) &&
         NearlyEqual(pLeave[7], 0.0f) &&
         NearlyEqual(pRetest[7], 0.0f) &&
         NearlyEqual(pBase[10], 0.0f) &&
         NearlyEqual(pLeave[10], 0.0f) &&
         NearlyEqual(pRetest[10], 0.0f) &&
         NearlyEqual(pBase[12], 0.0f) &&
         NearlyEqual(pLeave[12], 0.0f) &&
         NearlyEqual(pRetest[12], 0.0f) &&
         NearlyEqual(pBase[0], 0.0f) &&
         NearlyEqual(pLeave[0], 0.0f) &&
         NearlyEqual(pRetest[0], 0.0f);
}

static bool TestApplyTradingAbcStructureMapsCodes()
{
  const int nCount = 10;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 1.0f, 30);
  Buy.nAbcStructure = 1;
  Buy.nAbcBreakout = 0;
  TradingSignalCandidate Sell = MakeTestCandidate(3, 11.0f, 30);
  Sell.nAbcStructure = -1;
  Sell.nAbcBreakout = 1;
  TradingSignalCandidate None = MakeTestCandidate(5, 1.0f, 30);
  TradingSignalCandidate WrongDirection = MakeTestCandidate(7, 1.0f, 30);
  WrongDirection.nAbcStructure = -1;
  WrongDirection.nAbcBreakout = 2;
  TradingSignalCandidate NonFirst = MakeTestCandidate(9, 3.0f, 30);
  NonFirst.nAbcStructure = 1;
  NonFirst.nAbcBreakout = 3;
  Candidates.push_back(Buy);
  Candidates.push_back(Sell);
  Candidates.push_back(None);
  Candidates.push_back(WrongDirection);
  Candidates.push_back(NonFirst);

  ApplyTradingSignalAbcStructure(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], 1.0f) && NearlyEqual(pOut[3], -1.0f) &&
         NearlyEqual(pOut[5], 0.0f) && NearlyEqual(pOut[7], 0.0f) &&
         NearlyEqual(pOut[9], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingAbcBreakoutIdMapsCodes()
{
  const int nCount = 12;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 1.0f, 30);
  Buy.nAbcStructure = 1;
  Buy.nAbcBreakout = 2;
  TradingSignalCandidate Sell = MakeTestCandidate(3, 11.0f, 30);
  Sell.nAbcStructure = -1;
  Sell.nAbcBreakout = 0;
  TradingSignalCandidate MissingBreakout = MakeTestCandidate(5, 1.0f, 30);
  MissingBreakout.nAbcStructure = 1;
  TradingSignalCandidate WrongDirection = MakeTestCandidate(7, 1.0f, 30);
  WrongDirection.nAbcStructure = -1;
  WrongDirection.nAbcBreakout = 4;
  TradingSignalCandidate NonFirst = MakeTestCandidate(9, 3.0f, 30);
  NonFirst.nAbcStructure = 1;
  NonFirst.nAbcBreakout = 5;
  TradingSignalCandidate LowerPriorityAbc = MakeTestCandidate(10, 1.0f, 10);
  LowerPriorityAbc.nAbcStructure = 1;
  LowerPriorityAbc.nAbcBreakout = 1;
  TradingSignalCandidate HigherPriorityNonFirst = MakeTestCandidate(10, 2.0f, 20);
  Candidates.push_back(Buy);
  Candidates.push_back(Sell);
  Candidates.push_back(MissingBreakout);
  Candidates.push_back(WrongDirection);
  Candidates.push_back(NonFirst);
  Candidates.push_back(LowerPriorityAbc);
  Candidates.push_back(HigherPriorityNonFirst);

  ApplyTradingSignalAbcBreakoutId(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], 3.0f) && NearlyEqual(pOut[3], 1.0f) &&
         NearlyEqual(pOut[5], 0.0f) && NearlyEqual(pOut[7], 0.0f) &&
         NearlyEqual(pOut[9], 0.0f) && NearlyEqual(pOut[10], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingAbcBreakoutPointIdsMapCodes()
{
  const int nCount = 12;
  float pLeave[nCount];
  float pRetest[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pLeave[i] = -1;
    pRetest[i] = -1;
  }

  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(-1, 4));
  Breakouts.back().nLeavePoint = 3;
  Breakouts.push_back(MakeTestBreakout(1, 8));
  Breakouts.back().nLeavePoint = 7;

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Low = MakeTestCandidate(2, 1.0f, 10);
  Low.nAbcStructure = 1;
  Low.nAbcBreakout = 0;
  TradingSignalCandidate High = MakeTestCandidate(2, 11.0f, 20);
  High.nAbcStructure = -1;
  High.nAbcBreakout = 1;
  TradingSignalCandidate Missing = MakeTestCandidate(4, 1.0f, 30);
  Missing.nAbcStructure = 1;
  Missing.nAbcBreakout = 8;
  TradingSignalCandidate WrongDirection = MakeTestCandidate(6, 1.0f, 30);
  WrongDirection.nAbcStructure = -1;
  WrongDirection.nAbcBreakout = 0;
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(Missing);
  Candidates.push_back(WrongDirection);

  ApplyTradingSignalAbcBreakoutLeavePointId(nCount, pLeave, Candidates, Breakouts);
  ApplyTradingSignalAbcBreakoutRetestPointId(nCount, pRetest, Candidates, Breakouts);

  return NearlyEqual(pLeave[2], 8.0f) &&
         NearlyEqual(pRetest[2], 9.0f) &&
         NearlyEqual(pLeave[4], 0.0f) &&
         NearlyEqual(pRetest[4], 0.0f) &&
         NearlyEqual(pLeave[6], 0.0f) &&
         NearlyEqual(pRetest[6], 0.0f) &&
         NearlyEqual(pLeave[0], 0.0f) &&
         NearlyEqual(pRetest[0], 0.0f);
}

static bool TestApplyTradingStrictAbcFiltersFirstSignals()
{
  const int nCount = 8;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate MissingAbc = MakeTestCandidate(1, 1.0f, 30);
  MissingAbc.nSource = 1;
  MissingAbc.nAbcStructure = 0;
  TradingSignalCandidate ConfirmedAbc = MakeTestCandidate(2, 11.0f, 30);
  ConfirmedAbc.nSource = 1;
  ConfirmedAbc.nAbcStructure = -1;
  ConfirmedAbc.nAbcBreakout = 0;
  TradingSignalCandidate Third = MakeTestCandidate(3, 3.0f, 20);
  Third.nSource = 3;
  TradingSignalCandidate WrongDirection = MakeTestCandidate(4, 1.0f, 30);
  WrongDirection.nSource = 1;
  WrongDirection.nAbcStructure = -1;
  WrongDirection.nAbcBreakout = 1;
  TradingSignalCandidate Second = MakeTestCandidate(5, 2.0f, 10);
  Second.nSource = 2;
  TradingSignalCandidate MismatchedSource = MakeTestCandidate(6, 1.0f, 30);
  MismatchedSource.nSource = 2;
  MismatchedSource.nAbcStructure = 0;
  Candidates.push_back(MissingAbc);
  Candidates.push_back(ConfirmedAbc);
  Candidates.push_back(Third);
  Candidates.push_back(WrongDirection);
  Candidates.push_back(Second);
  Candidates.push_back(MismatchedSource);

  ApplyTradingSignalStrictAbcCandidates(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], 0.0f) &&
         NearlyEqual(pOut[2], 11.0f) &&
         NearlyEqual(pOut[3], 3.0f) &&
         NearlyEqual(pOut[4], 0.0f) &&
         NearlyEqual(pOut[5], 2.0f) &&
         NearlyEqual(pOut[6], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingMacdLineWeaknessMapsCodes()
{
  const int nCount = 8;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 1.0f, 30);
  Buy.Divergence.Previous.fDifHeight = 5;
  Buy.Divergence.Previous.fDeaHeight = 4;
  Buy.Divergence.Current.fDifHeight = 3;
  Buy.Divergence.Current.fDeaHeight = 2;
  TradingSignalCandidate Sell = MakeTestCandidate(3, 11.0f, 30);
  Sell.Divergence.Previous.fDifHeight = 6;
  Sell.Divergence.Previous.fDeaHeight = 5;
  Sell.Divergence.Current.fDifHeight = 4;
  Sell.Divergence.Current.fDeaHeight = 5;
  TradingSignalCandidate None = MakeTestCandidate(5, 1.0f, 30);
  None.Divergence.Previous.fDifHeight = 2;
  None.Divergence.Previous.fDeaHeight = 2;
  None.Divergence.Current.fDifHeight = 3;
  None.Divergence.Current.fDeaHeight = 1;
  TradingSignalCandidate Invalid = MakeTestCandidate(7, 99.0f, 30);
  Invalid.Divergence.Previous.fDifHeight = 6;
  Invalid.Divergence.Previous.fDeaHeight = 5;
  Invalid.Divergence.Current.fDifHeight = 4;
  Invalid.Divergence.Current.fDeaHeight = 3;
  Candidates.push_back(Buy);
  Candidates.push_back(Sell);
  Candidates.push_back(None);
  Candidates.push_back(Invalid);

  ApplyTradingSignalMacdLineWeakness(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], 1.0f) &&
         NearlyEqual(pOut[3], -1.0f) &&
         NearlyEqual(pOut[5], 0.0f) &&
         NearlyEqual(pOut[7], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingMacdZeroPullbackMapsCodes()
{
  const int nCount = 10;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 1.0f, 30);
  Buy.nMacdZeroPullback = 1;
  TradingSignalCandidate Sell = MakeTestCandidate(3, 11.0f, 30);
  Sell.nMacdZeroPullback = -1;
  TradingSignalCandidate None = MakeTestCandidate(5, 1.0f, 30);
  TradingSignalCandidate WrongDirection = MakeTestCandidate(7, 1.0f, 30);
  WrongDirection.nMacdZeroPullback = -1;
  TradingSignalCandidate NonFirst = MakeTestCandidate(9, 3.0f, 30);
  NonFirst.nMacdZeroPullback = 1;
  Candidates.push_back(Buy);
  Candidates.push_back(Sell);
  Candidates.push_back(None);
  Candidates.push_back(WrongDirection);
  Candidates.push_back(NonFirst);

  ApplyTradingSignalMacdZeroPullback(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], 1.0f) &&
         NearlyEqual(pOut[3], -1.0f) &&
         NearlyEqual(pOut[5], 0.0f) &&
         NearlyEqual(pOut[7], 0.0f) &&
         NearlyEqual(pOut[9], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static void MakeStandardDivergence(TradingSignalCandidate *pC, int nSign)
{
  pC->nSource = 1;
  pC->nTrend = 0;
  pC->nMovementType = (nSign > 0) ? CZSC_MOVEMENT_DOWN : CZSC_MOVEMENT_UP;
  pC->nAbcStructure = nSign;
  pC->nAbcBreakout = 0;
  pC->nMacdZeroPullback = nSign;
  pC->Divergence.nDirection = -nSign;
  pC->Divergence.bNewExtreme = true;
  pC->Divergence.bWeakMacd = true;
  pC->Divergence.bDivergence = true;
  pC->Divergence.Previous.fDifHeight = 6;
  pC->Divergence.Previous.fDeaHeight = 5;
  pC->Divergence.Current.fDifHeight = 4;
  pC->Divergence.Current.fDeaHeight = 3;
}

static bool TestApplyTradingStandardDivergenceMapsCodes()
{
  const int nCount = 30;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 1.0f, 30);
  MakeStandardDivergence(&Buy, 1);
  TradingSignalCandidate Sell = MakeTestCandidate(3, 11.0f, 30);
  MakeStandardDivergence(&Sell, -1);
  TradingSignalCandidate MissingAbc = MakeTestCandidate(5, 1.0f, 30);
  MakeStandardDivergence(&MissingAbc, 1);
  MissingAbc.nAbcStructure = 0;
  TradingSignalCandidate MissingAbcBreakout = MakeTestCandidate(29, 1.0f, 30);
  MakeStandardDivergence(&MissingAbcBreakout, 1);
  MissingAbcBreakout.nAbcBreakout = -1;
  TradingSignalCandidate MissingZeroPull = MakeTestCandidate(7, 1.0f, 30);
  MakeStandardDivergence(&MissingZeroPull, 1);
  MissingZeroPull.nMacdZeroPullback = 0;
  TradingSignalCandidate MissingMacdArea = MakeTestCandidate(9, 1.0f, 30);
  MakeStandardDivergence(&MissingMacdArea, 1);
  MissingMacdArea.Divergence.bWeakMacd = false;
  TradingSignalCandidate MissingLineWeak = MakeTestCandidate(11, 1.0f, 30);
  MakeStandardDivergence(&MissingLineWeak, 1);
  MissingLineWeak.Divergence.Current.fDeaHeight = 6;
  TradingSignalCandidate WrongAbcDirection = MakeTestCandidate(13, 1.0f, 30);
  MakeStandardDivergence(&WrongAbcDirection, 1);
  WrongAbcDirection.nAbcStructure = -1;
  TradingSignalCandidate WrongPullbackDirection = MakeTestCandidate(15, 11.0f, 30);
  MakeStandardDivergence(&WrongPullbackDirection, -1);
  WrongPullbackDirection.nMacdZeroPullback = 1;
  TradingSignalCandidate MissingBaseDivergence = MakeTestCandidate(17, 1.0f, 30);
  MakeStandardDivergence(&MissingBaseDivergence, 1);
  MissingBaseDivergence.Divergence.bDivergence = false;
  TradingSignalCandidate WrongDivergenceDirection = MakeTestCandidate(19, 1.0f, 30);
  MakeStandardDivergence(&WrongDivergenceDirection, 1);
  WrongDivergenceDirection.Divergence.nDirection = 1;
  TradingSignalCandidate NonFirstSource = MakeTestCandidate(21, 1.0f, 30);
  MakeStandardDivergence(&NonFirstSource, 1);
  NonFirstSource.nSource = 2;
  TradingSignalCandidate MissingNewExtreme = MakeTestCandidate(23, 1.0f, 30);
  MakeStandardDivergence(&MissingNewExtreme, 1);
  MissingNewExtreme.Divergence.bNewExtreme = false;
  TradingSignalCandidate MissingTrend = MakeTestCandidate(25, 1.0f, 30);
  MakeStandardDivergence(&MissingTrend, 1);
  MissingTrend.nTrend = -1;
  TradingSignalCandidate WrongMovement = MakeTestCandidate(27, 1.0f, 30);
  MakeStandardDivergence(&WrongMovement, 1);
  WrongMovement.nMovementType = CZSC_MOVEMENT_UP;

  Candidates.push_back(Buy);
  Candidates.push_back(Sell);
  Candidates.push_back(MissingAbc);
  Candidates.push_back(MissingZeroPull);
  Candidates.push_back(MissingMacdArea);
  Candidates.push_back(MissingLineWeak);
  Candidates.push_back(WrongAbcDirection);
  Candidates.push_back(WrongPullbackDirection);
  Candidates.push_back(MissingBaseDivergence);
  Candidates.push_back(WrongDivergenceDirection);
  Candidates.push_back(NonFirstSource);
  Candidates.push_back(MissingNewExtreme);
  Candidates.push_back(MissingTrend);
  Candidates.push_back(WrongMovement);
  Candidates.push_back(MissingAbcBreakout);

  ApplyTradingSignalStandardDivergence(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], 1.0f) &&
         NearlyEqual(pOut[3], -1.0f) &&
         NearlyEqual(pOut[5], 0.0f) &&
         NearlyEqual(pOut[7], 0.0f) &&
         NearlyEqual(pOut[9], 0.0f) &&
         NearlyEqual(pOut[11], 0.0f) &&
         NearlyEqual(pOut[13], 0.0f) &&
         NearlyEqual(pOut[15], 0.0f) &&
         NearlyEqual(pOut[17], 0.0f) &&
         NearlyEqual(pOut[19], 0.0f) &&
         NearlyEqual(pOut[21], 0.0f) &&
         NearlyEqual(pOut[23], 0.0f) &&
         NearlyEqual(pOut[25], 0.0f) &&
         NearlyEqual(pOut[27], 0.0f) &&
         NearlyEqual(pOut[29], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingMacdAreaRatioMapsCodes()
{
  const int nCount = 8;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 1.0f, 30);
  Buy.Divergence.Previous.fMacdArea = 200;
  Buy.Divergence.Current.fMacdArea = 80;
  TradingSignalCandidate Low = MakeTestCandidate(3, 2.0f, 10);
  Low.Divergence.Previous.fMacdArea = 300;
  Low.Divergence.Current.fMacdArea = 120;
  TradingSignalCandidate High = MakeTestCandidate(3, 3.0f, 20);
  High.Divergence.Previous.fMacdArea = 300;
  High.Divergence.Current.fMacdArea = 90;
  TradingSignalCandidate MissingBase = MakeTestCandidate(5, 11.0f, 30);
  MissingBase.Divergence.Previous.fMacdArea = 0;
  MissingBase.Divergence.Current.fMacdArea = 50;
  TradingSignalCandidate Invalid = MakeTestCandidate(7, 99.0f, 30);
  Invalid.Divergence.Previous.fMacdArea = 100;
  Invalid.Divergence.Current.fMacdArea = 10;
  Candidates.push_back(Buy);
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(MissingBase);
  Candidates.push_back(Invalid);

  ApplyTradingSignalMacdAreaRatio(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], 40.0f) &&
         NearlyEqual(pOut[3], 30.0f) &&
         NearlyEqual(pOut[5], 0.0f) &&
         NearlyEqual(pOut[7], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingStrengthRatiosMapCodes()
{
  const int nCount = 8;
  float pSpace[nCount];
  float pSpeed[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pSpace[i] = -1;
    pSpeed[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 1.0f, 30);
  Buy.Divergence.Previous.fSpace = 120;
  Buy.Divergence.Current.fSpace = 60;
  Buy.Divergence.Previous.fSpeed = 6;
  Buy.Divergence.Current.fSpeed = 3;
  TradingSignalCandidate Low = MakeTestCandidate(3, 2.0f, 10);
  Low.Divergence.Previous.fSpace = 200;
  Low.Divergence.Current.fSpace = 100;
  Low.Divergence.Previous.fSpeed = 5;
  Low.Divergence.Current.fSpeed = 4;
  TradingSignalCandidate High = MakeTestCandidate(3, 3.0f, 20);
  High.Divergence.Previous.fSpace = 160;
  High.Divergence.Current.fSpace = 40;
  High.Divergence.Previous.fSpeed = 8;
  High.Divergence.Current.fSpeed = 2;
  TradingSignalCandidate MissingBase = MakeTestCandidate(5, 11.0f, 30);
  MissingBase.Divergence.Previous.fSpace = 0;
  MissingBase.Divergence.Current.fSpace = 50;
  MissingBase.Divergence.Previous.fSpeed = 0;
  MissingBase.Divergence.Current.fSpeed = 2;
  TradingSignalCandidate Invalid = MakeTestCandidate(7, 99.0f, 30);
  Invalid.Divergence.Previous.fSpace = 100;
  Invalid.Divergence.Current.fSpace = 10;
  Invalid.Divergence.Previous.fSpeed = 10;
  Invalid.Divergence.Current.fSpeed = 1;
  Candidates.push_back(Buy);
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(MissingBase);
  Candidates.push_back(Invalid);

  ApplyTradingSignalSpaceRatio(nCount, pSpace, Candidates);
  ApplyTradingSignalSpeedRatio(nCount, pSpeed, Candidates);

  return NearlyEqual(pSpace[1], 50.0f) &&
         NearlyEqual(pSpeed[1], 50.0f) &&
         NearlyEqual(pSpace[3], 25.0f) &&
         NearlyEqual(pSpeed[3], 25.0f) &&
         NearlyEqual(pSpace[5], 0.0f) &&
         NearlyEqual(pSpeed[5], 0.0f) &&
         NearlyEqual(pSpace[7], 0.0f) &&
         NearlyEqual(pSpeed[7], 0.0f) &&
         NearlyEqual(pSpace[0], 0.0f) &&
         NearlyEqual(pSpeed[0], 0.0f);
}

static bool TestApplyTradingDivergenceFlagsMapCodes()
{
  const int nCount = 8;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 1.0f, 30);
  Buy.Divergence.bNewExtreme = true;
  Buy.Divergence.bWeakSpace = true;
  Buy.Divergence.bWeakMacd = true;
  Buy.Divergence.bDivergence = true;
  TradingSignalCandidate Low = MakeTestCandidate(3, 2.0f, 10);
  Low.Divergence.bNewExtreme = true;
  Low.Divergence.bWeakSpace = true;
  Low.Divergence.bWeakSpeed = true;
  Low.Divergence.bWeakMacd = true;
  Low.Divergence.bDivergence = true;
  TradingSignalCandidate High = MakeTestCandidate(3, 3.0f, 20);
  High.Divergence.bNewExtreme = true;
  High.Divergence.bWeakSpeed = true;
  TradingSignalCandidate None = MakeTestCandidate(5, 11.0f, 30);
  TradingSignalCandidate Invalid = MakeTestCandidate(7, 99.0f, 30);
  Invalid.Divergence.bNewExtreme = true;
  Invalid.Divergence.bWeakSpace = true;
  Invalid.Divergence.bWeakSpeed = true;
  Invalid.Divergence.bWeakMacd = true;
  Invalid.Divergence.bDivergence = true;
  Candidates.push_back(Buy);
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(None);
  Candidates.push_back(Invalid);

  ApplyTradingSignalDivergenceFlags(nCount, pOut, Candidates);

  float fBuyExpected = (float)(CZSC_DIVERGENCE_NEW_EXTREME |
                               CZSC_DIVERGENCE_WEAK_SPACE |
                               CZSC_DIVERGENCE_WEAK_MACD |
                               CZSC_DIVERGENCE_CONFIRMED);
  float fHighExpected = (float)(CZSC_DIVERGENCE_NEW_EXTREME |
                                CZSC_DIVERGENCE_WEAK_SPEED);

  return NearlyEqual(pOut[1], fBuyExpected) &&
         NearlyEqual(pOut[3], fHighExpected) &&
         NearlyEqual(pOut[5], 0.0f) &&
         NearlyEqual(pOut[7], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingDivergencePointIdsMapCodes()
{
  const int nCount = 12;
  float pPrevStart[nCount];
  float pPrevEnd[nCount];
  float pCurrStart[nCount];
  float pCurrEnd[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pPrevStart[i] = -1;
    pPrevEnd[i] = -1;
    pCurrStart[i] = -1;
    pCurrEnd[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 1.0f, 30);
  Buy.Divergence.nPreviousStartPoint = 0;
  Buy.Divergence.nPreviousEndPoint = 1;
  Buy.Divergence.nCurrentStartPoint = 2;
  Buy.Divergence.nCurrentEndPoint = 3;
  TradingSignalCandidate Low = MakeTestCandidate(3, 2.0f, 10);
  Low.Divergence.nPreviousStartPoint = 8;
  Low.Divergence.nPreviousEndPoint = 9;
  Low.Divergence.nCurrentStartPoint = 10;
  Low.Divergence.nCurrentEndPoint = 11;
  TradingSignalCandidate High = MakeTestCandidate(3, 3.0f, 20);
  High.Divergence.nPreviousStartPoint = 4;
  High.Divergence.nPreviousEndPoint = 5;
  High.Divergence.nCurrentStartPoint = 6;
  High.Divergence.nCurrentEndPoint = 7;
  TradingSignalCandidate Missing = MakeTestCandidate(5, 11.0f, 30);
  TradingSignalCandidate Invalid = MakeTestCandidate(7, 99.0f, 30);
  Invalid.Divergence.nPreviousStartPoint = 0;
  Invalid.Divergence.nPreviousEndPoint = 1;
  Invalid.Divergence.nCurrentStartPoint = 2;
  Invalid.Divergence.nCurrentEndPoint = 3;
  TradingSignalCandidate Shadowed = MakeTestCandidate(9, 1.0f, 10);
  Shadowed.Divergence.nPreviousStartPoint = 0;
  Shadowed.Divergence.nPreviousEndPoint = 1;
  Shadowed.Divergence.nCurrentStartPoint = 2;
  Shadowed.Divergence.nCurrentEndPoint = 3;
  TradingSignalCandidate EmptyWinner = MakeTestCandidate(9, 2.0f, 20);
  Candidates.push_back(Buy);
  Candidates.push_back(Low);
  Candidates.push_back(High);
  Candidates.push_back(Missing);
  Candidates.push_back(Invalid);
  Candidates.push_back(Shadowed);
  Candidates.push_back(EmptyWinner);

  ApplyTradingSignalDivergencePreviousStartPointId(nCount, pPrevStart, Candidates);
  ApplyTradingSignalDivergencePreviousEndPointId(nCount, pPrevEnd, Candidates);
  ApplyTradingSignalDivergenceCurrentStartPointId(nCount, pCurrStart, Candidates);
  ApplyTradingSignalDivergenceCurrentEndPointId(nCount, pCurrEnd, Candidates);

  return NearlyEqual(pPrevStart[1], 1.0f) &&
         NearlyEqual(pPrevEnd[1], 2.0f) &&
         NearlyEqual(pCurrStart[1], 3.0f) &&
         NearlyEqual(pCurrEnd[1], 4.0f) &&
         NearlyEqual(pPrevStart[3], 5.0f) &&
         NearlyEqual(pPrevEnd[3], 6.0f) &&
         NearlyEqual(pCurrStart[3], 7.0f) &&
         NearlyEqual(pCurrEnd[3], 8.0f) &&
         NearlyEqual(pPrevStart[5], 0.0f) &&
         NearlyEqual(pPrevEnd[5], 0.0f) &&
         NearlyEqual(pCurrStart[5], 0.0f) &&
         NearlyEqual(pCurrEnd[5], 0.0f) &&
         NearlyEqual(pPrevStart[7], 0.0f) &&
         NearlyEqual(pCurrEnd[7], 0.0f) &&
         NearlyEqual(pPrevStart[9], 0.0f) &&
         NearlyEqual(pCurrEnd[9], 0.0f) &&
         NearlyEqual(pPrevStart[0], 0.0f) &&
         NearlyEqual(pCurrEnd[0], 0.0f);
}

static bool TestBuildDivergenceFlagsMapsBits()
{
  DivergenceResult D;
  D.nDirection = 1;
  D.bNewExtreme = true;
  D.bWeakSpace = false;
  D.bWeakSpeed = true;
  D.bWeakMacd = false;
  D.bDivergence = true;
  D.Previous = MeasureStrength(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1),
                               MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  D.Current = MeasureStrength(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 2),
                              MakeTestPoint(CZSC_POINT_TOP, 12, 11));

  int nFlags = BuildDivergenceFlags(D);
  int nExpected = CZSC_DIVERGENCE_NEW_EXTREME |
                  CZSC_DIVERGENCE_WEAK_SPEED |
                  CZSC_DIVERGENCE_CONFIRMED;
  if (nFlags != nExpected)
  {
    return false;
  }

  D.bNewExtreme = false;
  D.bWeakSpeed = false;
  D.bDivergence = false;
  return BuildDivergenceFlags(D) == 0;
}

static bool TestApplyTradingContextFlagsMapsCodes()
{
  const int nCount = 12;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Buy = MakeTestCandidate(1, 1.0f, 30);
  MakeStandardDivergence(&Buy, 1);
  Buy.nQuality = CZSC_SIGNAL_QUALITY_STRONG;
  Buy.nSmallTurn = 1;
  Buy.nReversal = CZSC_REVERSAL_TREND;
  Buy.bOverlapped = true;
  Buy.nBreakout = 0;
  TradingSignalCandidate Newborn = MakeTestCandidate(3, 3.0f, 20);
  Newborn.nSource = 3;
  Newborn.nPoint = 3;
  Newborn.nCenter = 0;
  Newborn.nAfterEffect = CZSC_CENTER_AFTERMATH_NEWBORN;
  Newborn.nSmallTurn = 1;
  Newborn.nReversal = CZSC_REVERSAL_CONSOLIDATION;
  Newborn.nBreakout = 1;
  TradingSignalCandidate Extended = MakeTestCandidate(5, 13.0f, 20);
  Extended.nSource = 3;
  Extended.nPoint = 5;
  Extended.nCenter = 0;
  Extended.nAfterEffect = CZSC_CENTER_AFTERMATH_EXTENDED;
  Extended.nSmallTurn = -1;
  Extended.nReversal = CZSC_REVERSAL_EXTENSION;
  Extended.nBreakout = 2;
  TradingSignalCandidate SecondOverlap = MakeTestCandidate(10, 2.0f, 10);
  SecondOverlap.nSource = 2;
  SecondOverlap.nPoint = 10;
  SecondOverlap.nCenter = 0;
  SecondOverlap.nBreakout = 5;
  SecondOverlap.bOverlapped = true;
  TradingSignalCandidate WrongDirection = MakeTestCandidate(7, 1.0f, 30);
  MakeStandardDivergence(&WrongDirection, 1);
  WrongDirection.nAbcStructure = -1;
  WrongDirection.nMacdZeroPullback = -1;
  WrongDirection.nAfterEffect = CZSC_CENTER_AFTERMATH_EXTENDED;
  TradingSignalCandidate Invalid = MakeTestCandidate(9, 99.0f, 30);
  MakeStandardDivergence(&Invalid, 1);
  Invalid.nQuality = CZSC_SIGNAL_QUALITY_STRONG;
  Invalid.bOverlapped = true;
  Invalid.nBreakout = 3;
  TradingSignalCandidate WrongSmallTurn = MakeTestCandidate(11, 3.0f, 20);
  WrongSmallTurn.nSource = 3;
  WrongSmallTurn.nPoint = 11;
  WrongSmallTurn.nCenter = 0;
  WrongSmallTurn.nBreakout = 4;
  WrongSmallTurn.nSmallTurn = -1;
  Candidates.push_back(Buy);
  Candidates.push_back(Newborn);
  Candidates.push_back(Extended);
  Candidates.push_back(SecondOverlap);
  Candidates.push_back(WrongDirection);
  Candidates.push_back(Invalid);
  Candidates.push_back(WrongSmallTurn);

  ApplyTradingSignalContextFlags(nCount, pOut, Candidates);

  float fBuyExpected = (float)(CZSC_SIGNAL_CTX_STRONG_QUALITY |
                               CZSC_SIGNAL_CTX_ABC_STRUCTURE |
                               CZSC_SIGNAL_CTX_MACD_ZERO_PULL |
                               CZSC_SIGNAL_CTX_MACD_LINE_WEAK |
                               CZSC_SIGNAL_CTX_STANDARD_DIV |
                               CZSC_SIGNAL_CTX_REVERSAL_TREND |
                               CZSC_SIGNAL_CTX_CENTER_BREAKOUT);
  float fNewbornExpected = (float)(CZSC_SIGNAL_CTX_AFTERMATH_NEWBORN |
                                   CZSC_SIGNAL_CTX_SMALL_TURN |
                                   CZSC_SIGNAL_CTX_CENTER_BREAKOUT);
  float fExtendedExpected = (float)(CZSC_SIGNAL_CTX_AFTERMATH_EXTEND |
                                    CZSC_SIGNAL_CTX_SMALL_TURN |
                                    CZSC_SIGNAL_CTX_CENTER_BREAKOUT);
  float fSecondOverlapExpected = (float)(CZSC_SIGNAL_CTX_OVERLAPPED |
                                         CZSC_SIGNAL_CTX_CENTER_BREAKOUT);
  float fWrongDirectionExpected = (float)CZSC_SIGNAL_CTX_MACD_LINE_WEAK;
  float fWrongSmallTurnExpected = (float)CZSC_SIGNAL_CTX_CENTER_BREAKOUT;

  return NearlyEqual(pOut[1], fBuyExpected) &&
         NearlyEqual(pOut[3], fNewbornExpected) &&
         NearlyEqual(pOut[5], fExtendedExpected) &&
         NearlyEqual(pOut[7], fWrongDirectionExpected) &&
         NearlyEqual(pOut[9], 0.0f) &&
         NearlyEqual(pOut[10], fSecondOverlapExpected) &&
         (BuildTradingSignalContextFlags(Invalid) == 0) &&
         NearlyEqual(pOut[11], fWrongSmallTurnExpected) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingContextFlagsUsesWinningPriority()
{
  const int nCount = 4;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Low = MakeTestCandidate(2, 1.0f, 10);
  MakeStandardDivergence(&Low, 1);
  Low.nQuality = CZSC_SIGNAL_QUALITY_STRONG;
  Low.nReversal = CZSC_REVERSAL_TREND;
  TradingSignalCandidate High = MakeTestCandidate(2, 3.0f, 20);
  High.nSource = 3;
  High.nPoint = 2;
  High.nCenter = 0;
  High.bOverlapped = true;
  High.nBreakout = 0;
  Candidates.push_back(Low);
  Candidates.push_back(High);

  ApplyTradingSignalContextFlags(nCount, pOut, Candidates);

  float fExpected = (float)(CZSC_SIGNAL_CTX_OVERLAPPED |
                            CZSC_SIGNAL_CTX_CENTER_BREAKOUT);
  return NearlyEqual(pOut[2], fExpected) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingDivergenceSemanticMapsCodes()
{
  const int nCount = 9;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Trend = MakeTestCandidate(1, 1.0f, 30);
  MakeStandardDivergence(&Trend, 1);
  TradingSignalCandidate Consolidation = MakeTestCandidate(3, 2.0f, 10);
  Consolidation.nSource = 2;
  Consolidation.nBreakout = 0;
  Consolidation.Divergence.bDivergence = true;
  TradingSignalCandidate SmallTurn = MakeTestCandidate(5, 3.0f, 20);
  SmallTurn.nSource = 3;
  SmallTurn.nPoint = 5;
  SmallTurn.nCenter = 0;
  SmallTurn.nBreakout = 1;
  SmallTurn.nSmallTurn = 1;
  SmallTurn.Divergence.bDivergence = true;
  TradingSignalCandidate MissingExtreme = MakeTestCandidate(7, 1.0f, 30);
  MakeStandardDivergence(&MissingExtreme, 1);
  MissingExtreme.Divergence.bNewExtreme = false;
  TradingSignalCandidate Invalid = MakeTestCandidate(8, 99.0f, 30);
  Invalid.Divergence.bDivergence = true;

  Candidates.push_back(Trend);
  Candidates.push_back(Consolidation);
  Candidates.push_back(SmallTurn);
  Candidates.push_back(MissingExtreme);
  Candidates.push_back(Invalid);

  ApplyTradingSignalDivergenceSemantic(nCount, pOut, Candidates);

  return NearlyEqual(pOut[1], (float)CZSC_DIVERGENCE_SEM_TREND) &&
         NearlyEqual(pOut[3], (float)CZSC_DIVERGENCE_SEM_CONSOLIDATION) &&
         NearlyEqual(pOut[5], (float)CZSC_DIVERGENCE_SEM_SMALL_TURN) &&
         NearlyEqual(pOut[7], 0.0f) &&
         NearlyEqual(pOut[8], 0.0f) &&
         (BuildTradingSignalDivergenceSemantic(Invalid) == CZSC_DIVERGENCE_SEM_NONE) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingDivergenceSemanticUsesWinningPriority()
{
  const int nCount = 4;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate Low = MakeTestCandidate(2, 1.0f, 10);
  MakeStandardDivergence(&Low, 1);
  TradingSignalCandidate High = MakeTestCandidate(2, 12.0f, 20);
  High.nSource = 2;
  High.nBreakout = 0;
  High.Divergence.bDivergence = true;
  Candidates.push_back(Low);
  Candidates.push_back(High);

  ApplyTradingSignalDivergenceSemantic(nCount, pOut, Candidates);

  return NearlyEqual(pOut[2], (float)CZSC_DIVERGENCE_SEM_CONSOLIDATION) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestApplyTradingFilterReasonsMapsCodes()
{
  const int nCount = 8;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<int> Reasons((std::size_t)nCount, CZSC_FILTER_NONE);
  Reasons[1] = CZSC_FILTER_NO_TREND;
  Reasons[3] = CZSC_FILTER_RETEST_BACK_CENTER;

  ApplyTradingFilterReasons(nCount, pOut, Reasons);

  return NearlyEqual(pOut[1], (float)CZSC_FILTER_NO_TREND) &&
         NearlyEqual(pOut[3], (float)CZSC_FILTER_RETEST_BACK_CENTER) &&
         NearlyEqual(pOut[0], 0.0f) &&
         NearlyEqual(pOut[7], 0.0f);
}

static bool TestBuildTradingFilterReasonsMarksNoTrend()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 18));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 9));

  std::vector<Center> Centers;
  std::vector<TrendStructure> Structures;
  std::vector<CenterBreakout> Breakouts;
  std::vector<TradingSignalCandidate> Candidates;

  std::vector<int> Reasons =
    BuildTradingFilterReasons(Points, Centers, Structures, Breakouts, Candidates);

  return Reasons.size() > 16 &&
         Reasons[16] == CZSC_FILTER_NO_TREND &&
         Reasons[0] == CZSC_FILTER_NONE;
}

static bool TestBuildTradingFilterReasonsMarksSecondOrder()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 22));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 8));

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate First = MakeTestCandidate(4, 1.0f, 30);
  First.nPoint = 1;
  First.nSource = 1;
  Candidates.push_back(First);

  std::vector<Center> Centers;
  std::vector<TrendStructure> Structures;
  std::vector<CenterBreakout> Breakouts;
  std::vector<int> Reasons =
    BuildTradingFilterReasons(Points, Centers, Structures, Breakouts, Candidates);

  return Reasons.size() > 12 &&
         Reasons[12] == CZSC_FILTER_SECOND_ORDER;
}

static bool TestBuildTradingFilterReasonsMarksThirdRetestFailures()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 18));

  std::vector<CenterBreakout> Breakouts;
  CenterBreakout NotFirst = MakeTestBreakout(1, 2);
  NotFirst.nCenter = 0;
  NotFirst.bFirstRetest = false;
  Breakouts.push_back(NotFirst);
  CenterBreakout BackIntoCenter = MakeTestBreakout(-1, 3);
  BackIntoCenter.nCenter = 0;
  BackIntoCenter.bBackIntoCenter = true;
  Breakouts.push_back(BackIntoCenter);

  std::vector<Center> Centers;
  std::vector<TrendStructure> Structures;
  std::vector<TradingSignalCandidate> Candidates;
  std::vector<int> Reasons =
    BuildTradingFilterReasons(Points, Centers, Structures, Breakouts, Candidates);

  return Reasons.size() > 12 &&
         Reasons[8] == CZSC_FILTER_NOT_FIRST_RETEST &&
         Reasons[12] == CZSC_FILTER_RETEST_BACK_CENTER;
}

static bool TestBuildTradingFilterReasonsMarksAbcNotAligned()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 18));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 9));

  std::vector<TradingSignalCandidate> Candidates;
  TradingSignalCandidate First = MakeTestCandidate(16, 1.0f, 30);
  First.nPoint = 4;
  First.nSource = 1;
  First.nCenter = 0;
  First.nAbcBreakout = -1;
  Candidates.push_back(First);

  std::vector<CenterBreakout> Breakouts;
  CenterBreakout B = MakeTestBreakout(-1, 3);
  B.nCenter = 0;
  Breakouts.push_back(B);

  std::vector<Center> Centers;
  std::vector<TrendStructure> Structures;
  std::vector<int> Reasons =
    BuildTradingFilterReasons(Points, Centers, Structures, Breakouts, Candidates);

  return Reasons.size() > 16 &&
         Reasons[16] == CZSC_FILTER_ABC_NOT_ALIGNED;
}

static bool TestNestedDivergenceMarksLowerSegmentInsideHigher()
{
  const int nCount = 61;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<SegmentPoint> HighPoints;
  HighPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 10, 12));
  HighPoints.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 50, 4));

  std::vector<SegmentPoint> LowPoints;
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 9));
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 32, 5));
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 52, 8));
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 58, 6));

  std::vector<TradingSignalCandidate> HighCandidates;
  TradingSignalCandidate High = MakeTestCandidate(50, 1.0f, 30);
  High.nPoint = 1;
  MakeStandardDivergence(&High, 1);
  HighCandidates.push_back(High);

  std::vector<TradingSignalCandidate> LowCandidates;
  TradingSignalCandidate Inside = MakeTestCandidate(32, 1.0f, 30);
  Inside.nPoint = 1;
  MakeStandardDivergence(&Inside, 1);
  TradingSignalCandidate Outside = MakeTestCandidate(58, 1.0f, 30);
  Outside.nPoint = 3;
  MakeStandardDivergence(&Outside, 1);
  LowCandidates.push_back(Inside);
  LowCandidates.push_back(Outside);

  WriteNestedDivergenceSignal(nCount, pOut, HighPoints, HighCandidates, LowPoints, LowCandidates);

  return NearlyEqual(pOut[20], 1.0f) &&
         NearlyEqual(pOut[32], 2.0f) &&
         NearlyEqual(pOut[52], 0.0f) &&
         NearlyEqual(pOut[58], 0.0f);
}

static bool TestNestedDivergenceMarksSellDirection()
{
  const int nCount = 61;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<SegmentPoint> HighPoints;
  HighPoints.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 10, 4));
  HighPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 50, 12));

  std::vector<SegmentPoint> LowPoints;
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 18, 6));
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 40, 10));

  std::vector<TradingSignalCandidate> HighCandidates;
  TradingSignalCandidate High = MakeTestCandidate(50, 11.0f, 30);
  High.nPoint = 1;
  MakeStandardDivergence(&High, -1);
  HighCandidates.push_back(High);

  std::vector<TradingSignalCandidate> LowCandidates;
  TradingSignalCandidate Low = MakeTestCandidate(40, 11.0f, 30);
  Low.nPoint = 1;
  MakeStandardDivergence(&Low, -1);
  LowCandidates.push_back(Low);

  WriteNestedDivergenceSignal(nCount, pOut, HighPoints, HighCandidates, LowPoints, LowCandidates);

  return NearlyEqual(pOut[18], -1.0f) &&
         NearlyEqual(pOut[40], -2.0f);
}

static bool TestNestedDivergenceRequiresFirstSignalCode()
{
  const int nCount = 61;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<SegmentPoint> HighPoints;
  HighPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 10, 12));
  HighPoints.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 50, 4));

  std::vector<SegmentPoint> LowPoints;
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 9));
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 32, 5));

  std::vector<TradingSignalCandidate> HighCandidates;
  TradingSignalCandidate High = MakeTestCandidate(50, 3.0f, 30);
  High.nSource = 1;
  High.nPoint = 1;
  HighCandidates.push_back(High);

  std::vector<TradingSignalCandidate> LowCandidates;
  TradingSignalCandidate Low = MakeTestCandidate(32, 3.0f, 30);
  Low.nSource = 1;
  Low.nPoint = 1;
  LowCandidates.push_back(Low);

  WriteNestedDivergenceSignal(nCount, pOut, HighPoints, HighCandidates, LowPoints, LowCandidates);

  return NearlyEqual(pOut[20], 0.0f) &&
         NearlyEqual(pOut[32], 0.0f) &&
         NearlyEqual(pOut[0], 0.0f);
}

static bool TestNestedDivergenceRequiresTrendExtreme()
{
  const int nCount = 61;
  float pOut[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1;
  }

  std::vector<SegmentPoint> HighPoints;
  HighPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 10, 12));
  HighPoints.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 50, 4));

  std::vector<SegmentPoint> LowPoints;
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 9));
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 32, 5));

  std::vector<TradingSignalCandidate> HighCandidates;
  TradingSignalCandidate High = MakeTestCandidate(50, 1.0f, 30);
  High.nPoint = 1;
  MakeStandardDivergence(&High, 1);
  HighCandidates.push_back(High);

  std::vector<TradingSignalCandidate> LowCandidates;
  TradingSignalCandidate Low = MakeTestCandidate(32, 1.0f, 30);
  Low.nPoint = 1;
  MakeStandardDivergence(&Low, 1);
  Low.Divergence.bNewExtreme = false;
  LowCandidates.push_back(Low);

  std::vector<NestedDivergenceContext> Contexts =
    BuildNestedDivergenceContexts(HighPoints, HighCandidates, LowPoints, LowCandidates);
  WriteNestedDivergenceSignal(nCount, pOut, HighPoints, HighCandidates, LowPoints, LowCandidates);

  return Contexts.empty() &&
         NearlyEqual(pOut[20], 0.0f) &&
         NearlyEqual(pOut[32], 0.0f);
}

static bool TestNestedDivergenceContextOutputs()
{
  const int nCount = 61;
  float pLevel[nCount];
  float pSource[nCount];
  float pStart[nCount];
  float pEnd[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pLevel[i] = -1;
    pSource[i] = -1;
    pStart[i] = -1;
    pEnd[i] = -1;
  }

  std::vector<SegmentPoint> HighPoints;
  HighPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 10, 12));
  HighPoints.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 50, 4));

  std::vector<SegmentPoint> LowPoints;
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 9));
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 32, 5));
  LowPoints.push_back(MakeTestPoint(CZSC_POINT_TOP, 44, 8));

  std::vector<TradingSignalCandidate> HighCandidates;
  TradingSignalCandidate High = MakeTestCandidate(50, 1.0f, 30);
  High.nPoint = 1;
  MakeStandardDivergence(&High, 1);
  HighCandidates.push_back(High);

  std::vector<TradingSignalCandidate> LowCandidates;
  TradingSignalCandidate Low = MakeTestCandidate(32, 1.0f, 30);
  Low.nPoint = 1;
  MakeStandardDivergence(&Low, 1);
  TradingSignalCandidate Third = MakeTestCandidate(44, 3.0f, 20);
  Third.nSource = 3;
  Third.nPoint = 2;
  Third.nCenter = 0;
  Third.nBreakout = 0;
  Third.nSmallTurn = 1;
  Third.nSmallTurnBasePoint = 1;
  LowCandidates.push_back(Low);
  LowCandidates.push_back(Third);

  std::vector<NestedDivergenceContext> Contexts =
    BuildNestedDivergenceContexts(HighPoints, HighCandidates, LowPoints, LowCandidates);
  ApplyNestedDivergenceLevel(nCount, pLevel, Contexts);
  ApplyNestedDivergenceSourceId(nCount, pSource, Contexts);
  ApplyNestedDivergenceStartPointId(nCount, pStart, Contexts);
  ApplyNestedDivergenceEndPointId(nCount, pEnd, Contexts);

  return (Contexts.size() == 1) &&
         Contexts[0].bSmallTurnSatisfied &&
         (Contexts[0].nDirection == 1) &&
         NearlyEqual(pLevel[32], 2.0f) &&
         NearlyEqual(pSource[32], 1.0f) &&
         NearlyEqual(pStart[32], 1.0f) &&
         NearlyEqual(pEnd[32], 2.0f) &&
         NearlyEqual(pLevel[0], 0.0f);
}

static bool TestFunc13HandlesEmptyInput()
{
  Func13(0, 0, 0, 0, 0);

  const int nCount = 3;
  float pHigh[nCount] = {1, 2, 3};
  float pLow[nCount] = {1, 2, 3};
  float pOut[nCount] = {9, 9, 9};

  Func13(nCount, pOut, 0, pHigh, pLow); // 缺线段信号 → 提前返回，不改写输出

  return (pOut[0] == 9) && (pOut[1] == 9) && (pOut[2] == 9);
}

static bool TestSecondBuyConsolidationDivergence()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 14));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 22));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 15));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 30, 8));    // 一买
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 34, 11));      // 反弹
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 40, 9));    // 二买，回抽段(11→9)弱于前段(15→8)

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 22, 13));
  Centers.push_back(MakeTestCenter(16, 24, 12, 9));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pSecond = FindSignalCandidate(Candidates, 40, 2.0f);

  return (pSecond != 0) &&
         pSecond->Divergence.bDivergence &&
         !pSecond->bOverlapped &&
         (pSecond->nQuality == CZSC_SIGNAL_QUALITY_STRONG);
}

static bool TestSecondBuyStrongPullbackConfirmed()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 14));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 22));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 15));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 30, 8));    // 一买
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 34, 17));      // 反弹
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 40, 8.5f)); // 二买，回抽段(17→8.5)强于前段 → 非背驰

  std::vector<Center> Centers;
  Centers.push_back(MakeTestCenter(0, 12, 22, 13));
  Centers.push_back(MakeTestCenter(16, 24, 12, 9));
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pSecond = FindSignalCandidate(Candidates, 40, 2.0f);

  return (pSecond != 0) &&
         !pSecond->Divergence.bDivergence &&
         (pSecond->nQuality == CZSC_SIGNAL_QUALITY_CONFIRMED);
}

static bool TestFunc14MarksDivergenceSegment()
{
  // 复用一买背驰场景：一买在 bar32，其背驰段为 bar28(上一点)→bar32。
  const int nCount = 33;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
    pOut[i] = -1;
  }

  pIn[0] = -1;
  pHigh[0] = pLow[0] = 7;
  pIn[4] = 1;
  pHigh[4] = pLow[4] = 12;
  pIn[8] = -1;
  pHigh[8] = pLow[8] = 8;
  pIn[12] = 1;
  pHigh[12] = pLow[12] = 10;
  pIn[16] = -1;
  pHigh[16] = pLow[16] = 7.5f;
  pIn[20] = 1;
  pHigh[20] = pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = pLow[28] = 4.2f;
  pIn[32] = -1;
  pHigh[32] = pLow[32] = 3.8f;

  Func14(nCount, pOut, pIn, pHigh, pLow);

  for (int i = 0; i < nCount; i++)
  {
    float fExpected = 0;
    if (i == 28)
    {
      fExpected = 1;   // 背驰段起点（买）
    }
    else if (i == 32)
    {
      fExpected = 2;   // 背驰段终点（一买）
    }

    if (!NearlyEqual(pOut[i], fExpected))
    {
      return false;
    }
  }

  return true;
}

static bool TestFunc14HandlesEmptyInput()
{
  Func14(0, 0, 0, 0, 0);

  const int nCount = 3;
  float pHigh[nCount] = {1, 2, 3};
  float pLow[nCount] = {1, 2, 3};
  float pOut[nCount] = {9, 9, 9};

  Func14(nCount, pOut, 0, pHigh, pLow); // 缺线段信号 → 提前返回，不改写输出

  return (pOut[0] == 9) && (pOut[1] == 9) && (pOut[2] == 9);
}

static bool TestMovingAverageComputesSma()
{
  const int nCount = 5;
  float pPrice[nCount] = {2, 4, 6, 8, 10};
  std::vector<float> Ma = ComputeMovingAverage(nCount, pPrice, 3);

  if ((int)Ma.size() != nCount)
  {
    return false;
  }
  // 起始用部分窗口：2、(2+4)/2、(2+4+6)/3、(4+6+8)/3、(6+8+10)/3
  return NearlyEqual(Ma[0], 2.0f) && NearlyEqual(Ma[1], 3.0f) &&
         NearlyEqual(Ma[2], 4.0f) && NearlyEqual(Ma[3], 6.0f) &&
         NearlyEqual(Ma[4], 8.0f);
}

static bool TestMaKissClassifiesThreeTypes()
{
  // 湿吻：短均线由上穿下（间距变号）
  std::vector<float> WetShort;
  std::vector<float> WetLong;
  WetShort.push_back(12);  WetLong.push_back(10.0f);    // +2
  WetShort.push_back(11);  WetLong.push_back(10.5f);    // +0.5
  WetShort.push_back(10);  WetLong.push_back(10.2f);    // -0.2 (变号，局部极小)
  WetShort.push_back(9);   WetLong.push_back(10.5f);    // -1.5
  WetShort.push_back(8);   WetLong.push_back(10.0f);    // -2
  std::vector<int> WetKiss = ClassifyMaKisses(WetShort, WetLong);
  if (WetKiss[2] != CZSC_KISS_WET)
  {
    return false;
  }

  // 唇吻：不变号，贴近（相对间距 < 1%）
  std::vector<float> LipShort;
  std::vector<float> LipLong;
  LipShort.push_back(12);    LipLong.push_back(11);   // +1
  LipShort.push_back(11.1f); LipLong.push_back(11);   // +0.1
  LipShort.push_back(11.05f);LipLong.push_back(11);   // +0.05 (≈0.45%)
  LipShort.push_back(11.1f); LipLong.push_back(11);   // +0.1
  LipShort.push_back(12);    LipLong.push_back(11);   // +1
  std::vector<int> LipKiss = ClassifyMaKisses(LipShort, LipLong);
  if (LipKiss[2] != CZSC_KISS_LIP)
  {
    return false;
  }

  // 飞吻：不变号，仅略走平（相对间距较大）
  std::vector<float> FlyShort;
  std::vector<float> FlyLong;
  FlyShort.push_back(13);   FlyLong.push_back(11);   // +2
  FlyShort.push_back(12);   FlyLong.push_back(11);   // +1
  FlyShort.push_back(11.8f);FlyLong.push_back(11);   // +0.8 (≈7%)
  FlyShort.push_back(12);   FlyLong.push_back(11);   // +1
  FlyShort.push_back(13);   FlyLong.push_back(11);   // +2
  std::vector<int> FlyKiss = ClassifyMaKisses(FlyShort, FlyLong);
  return FlyKiss[2] == CZSC_KISS_FLY;
}

static bool TestFunc15WritesMaDiff()
{
  const int nCount = 30;
  float pHigh[nCount];
  float pLow[nCount];
  float pOut[nCount];
  float fTime = 5;

  for (int i = 0; i < nCount; i++)
  {
    pHigh[i] = 10.5f + (float)i;   // 单调上升
    pLow[i] = 9.5f + (float)i;
    pOut[i] = -99;
  }

  Func15(nCount, pOut, pHigh, pLow, &fTime);

  // 上升趋势：短均线高于长均线（女上位）→ 末端差值为正；起点两均线相等 → 0
  return (pOut[nCount - 1] > 0) && NearlyEqual(pOut[0], 0.0f);
}

static bool TestFunc15Func16HandleEmptyInput()
{
  float pHigh[3] = {1, 2, 3};
  float pLow[3] = {1, 2, 3};
  float pOut[3] = {9, 9, 9};
  float fTime = 5;

  Func15(0, pOut, pHigh, pLow, &fTime);     // 个数为 0 → 不改写
  Func16(0, pOut, pHigh, pLow, &fTime);
  if ((pOut[0] != 9) || (pOut[1] != 9) || (pOut[2] != 9))
  {
    return false;
  }

  Func15(3, pOut, 0, pLow, &fTime);          // 缺最高价 → 不改写
  Func16(3, pOut, 0, pLow, &fTime);
  return (pOut[0] == 9) && (pOut[1] == 9) && (pOut[2] == 9);
}

static bool TestInstantDivergenceWarnsWeakNewLow()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 8));    // 上一完成下跌段 20→8（快）
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 18));     // 末段起点

  const int nCount = 31;
  float pHigh[nCount];
  float pLow[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pHigh[i] = 20;
    pLow[i] = 18;
  }
  // 末段从 @12 缓慢下行创新低（慢 → 力度走弱）
  for (int i = 12; i < nCount; i++)
  {
    float fValue = 18.0f - (float)(i - 12) * 0.58f;
    pHigh[i] = fValue + 0.5f;
    pLow[i] = fValue;
  }

  return DetectInstantDivergence(Points, nCount, pHigh, pLow) == -1;
}

static bool TestInstantDivergenceSkipsStrongLeg()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 20));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 8));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 18));

  const int nCount = 15;
  float pHigh[nCount];
  float pLow[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pHigh[i] = 20;
    pLow[i] = 18;
  }
  // 末段从 @12 急速下行创新低（快 → 力度更强，不算背驰）
  pLow[12] = 18;  pHigh[12] = 18.5f;
  pLow[13] = 12;  pHigh[13] = 12.5f;
  pLow[14] = 6;   pHigh[14] = 6.5f;

  return DetectInstantDivergence(Points, nCount, pHigh, pLow) == 0;
}

static bool TestFunc17HandlesEmptyInput()
{
  Func17(0, 0, 0, 0, 0);

  const int nCount = 3;
  float pHigh[nCount] = {1, 2, 3};
  float pLow[nCount] = {1, 2, 3};
  float pOut[nCount] = {9, 9, 9};

  Func17(nCount, pOut, 0, pHigh, pLow); // 缺线段信号 → 提前返回，不改写输出

  return (pOut[0] == 9) && (pOut[1] == 9) && (pOut[2] == 9);
}

// 笔类型判据：严格笔=处理后合并K线≥5（nMergedIndex 跨度≥4）；
// 新笔放宽为处理后合并K线≥4（跨度≥3）且处理前原始K线≥5（nIndex 跨度≥4）
static bool TestStrictStrokeUsesMergedGap()
{
  CzscConfig NewBi = DefaultConfig();
  NewBi.nStrokeType = CZSC_STROKE_NEW;

  // 场景1：合并跨度3（处理后4根）、原始跨度5（处理前6根）→ 新笔成、严格不成
  std::vector<Fractal> A;
  A.push_back(MakeTestFractalFull(CZSC_POINT_TOP, 1, 1, 12, 7));      // nIndex1, merged1
  A.push_back(MakeTestFractalFull(CZSC_POINT_BOTTOM, 6, 4, 10, 4));   // nIndex6(+5), merged4(+3)
  if (!BuildStrokes(A).empty())            return false;             // 严格(合并3<4)：不成笔
  if (BuildStrokes(A, NewBi).size() != 1)  return false;             // 新笔(合并3≥3且原始5≥4)：成笔

  // 场景2：合并跨度4（处理后5根）→ 严格也成笔
  std::vector<Fractal> B;
  B.push_back(MakeTestFractalFull(CZSC_POINT_TOP, 1, 1, 12, 7));      // merged1
  B.push_back(MakeTestFractalFull(CZSC_POINT_BOTTOM, 10, 5, 10, 4));  // nIndex10, merged5(+4)
  if (BuildStrokes(B).size() != 1)         return false;             // 严格(合并4≥4)：成笔
  if (BuildStrokes(B, NewBi).size() != 1)  return false;             // 新笔：也成笔

  // 场景3：合并跨度3但原始跨度仅3（处理前4根）→ 新笔也不成（原始不足5根）
  std::vector<Fractal> C;
  C.push_back(MakeTestFractalFull(CZSC_POINT_TOP, 1, 1, 12, 7));      // nIndex1, merged1
  C.push_back(MakeTestFractalFull(CZSC_POINT_BOTTOM, 4, 4, 10, 4));   // nIndex4(+3), merged4(+3)
  if (!BuildStrokes(C, NewBi).empty())     return false;             // 新笔(合并3≥3但原始3<4)：不成笔

  return true;
}

// 向上线段的特征序列出现无缺口顶分型：线段在该顶分型高点结束（第67课第一种情况）。
static bool TestFeatureLineSegmentEndsAtTopFractal()
{
  std::vector<Fractal> F;
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 12, 10));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 20, 16));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 8, 18, 14));   // X1=[14,20]
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 12, 25, 21));     // X2=[19,25] 顶分型高点
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 16, 22, 19));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 20, 23, 20));     // X3=[17,23]
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 24, 19, 17));

  std::vector<Stroke> Strokes = BuildStrokes(F);
  std::vector<SegmentPoint> Line = BuildLineSegmentPointsByFeature(Strokes);

  return (Line.size() >= 2) &&
         (Line[0].nType == CZSC_POINT_BOTTOM) && (Line[0].nIndex == 0) &&
         (Line[1].nType == CZSC_POINT_TOP) && (Line[1].nIndex == 12);
}

static bool TestFeatureLineSegmentNeedsFourPoints()
{
  std::vector<Fractal> Fractals;
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 5, 1));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 10, 6));
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  std::vector<SegmentPoint> LinePoints = BuildLineSegmentPointsByFeature(Strokes);
  return LinePoints.empty();  // 不足四个笔端点 → 无法划分线段
}

static bool TestFeatureLineSegmentRequiresFirstThreeOverlap()
{
  std::vector<Fractal> F;
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 2, 1));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 4, 3));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 8, 8, 7));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 12, 10, 9));     // 前三笔无公共重叠
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 16, 14, 13));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 20, 16, 15));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 24, 20, 19));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 28, 22, 21));

  std::vector<Stroke> Strokes = BuildStrokes(F);
  std::vector<SegmentPoint> Line = BuildLineSegmentPointsByFeature(Strokes);
  return Line.empty();
}

// 向下线段的特征序列出现无缺口底分型：线段在该底分型低点结束（第67课第一种情况）。
static bool TestFeatureLineSegmentEndsAtBottomFractal()
{
  std::vector<Fractal> F;
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 0, 100, 96));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 4, 92, 85));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 8, 95, 90));      // S1=[85,95]
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 12, 88, 80));  // S2=[80,90] 底分型低点
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 16, 90, 84));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 20, 89, 84));  // S3=[84,92]
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 24, 92, 88));

  std::vector<Stroke> Strokes = BuildStrokes(F);
  std::vector<SegmentPoint> Line = BuildLineSegmentPointsByFeature(Strokes);

  return (Line.size() >= 2) &&
         (Line[0].nType == CZSC_POINT_TOP) && (Line[0].nIndex == 0) &&
         (Line[1].nType == CZSC_POINT_BOTTOM) && (Line[1].nIndex == 12);
}

// 有缺口的特征序列顶分型，必须等反向特征序列出现底分型确认（第67课第二种情况）。
static bool TestFeatureSegmentGapConfirmedByReversal()
{
  std::vector<Fractal> F;
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 12, 10));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 20, 16));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 8, 18, 14));   // X1=[14,20]
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 12, 30, 26));     // X2=[25,30]，与 X1 有缺口
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 16, 27, 25));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 20, 28, 26));     // 反向序列 S1=[25,28]
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 24, 24, 22));  // X3=[22,28]，确认原顶分型
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 28, 26, 24));     // 反向序列 S2=[22,26]
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 32, 25, 23));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 36, 27, 25));     // 反向序列 S3=[23,27]，S2 为底分型

  std::vector<Stroke> Strokes = BuildStrokes(F);
  std::vector<SegmentPoint> Line = BuildLineSegmentPointsByFeature(Strokes);

  return (Line.size() >= 2) &&
         (Line[0].nType == CZSC_POINT_BOTTOM) && (Line[0].nIndex == 0) &&
         (Line[1].nType == CZSC_POINT_TOP) && (Line[1].nIndex == 12);
}

static bool TestFeatureSegmentGapRequiresReversalFractal()
{
  std::vector<Fractal> F;
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 12, 10));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 20, 16));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 8, 18, 14));   // X1=[14,20]
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 12, 30, 26));     // X2=[25,30]，与 X1 有缺口
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 16, 27, 25));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 20, 28, 26));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 24, 24, 22));  // X3=[22,28]，原序列顶分型成立

  std::vector<Stroke> Strokes = BuildStrokes(F);
  std::vector<SegmentPoint> Line = BuildLineSegmentPointsByFeature(Strokes);

  return Line.size() == 1;  // 未出现反向底分型，不确认线段结束
}

static bool TestFeatureSegmentGapConfirmationStartsNextSegment()
{
  std::vector<Fractal> F;
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 12, 10));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 20, 16));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 8, 18, 14));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 12, 30, 26));     // 有缺口顶分型，旧线段终点
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 16, 27, 25));  // 新向下线段第一笔
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 20, 28, 26));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 24, 24, 22));  // 第三笔破第一笔结束位置，新线段成立
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 28, 26, 24));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 32, 25, 23));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 36, 27, 25));

  std::vector<Stroke> Strokes = BuildStrokes(F);
  std::vector<SegmentPoint> Line = BuildLineSegmentPointsByFeature(Strokes);

  return (Line.size() >= 3) &&
         (Line[0].nType == CZSC_POINT_BOTTOM) && (Line[0].nIndex == 0) &&
         (Line[1].nType == CZSC_POINT_TOP) && (Line[1].nIndex == 12) &&
         (Line[2].nType == CZSC_POINT_BOTTOM) && (Line[2].nIndex == 24);
}

static bool TestFeatureSegmentGapWithoutReverseFractalKeepsOldSegment()
{
  std::vector<Fractal> F;
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 12, 10));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 20, 16));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 8, 18, 14));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 12, 30, 26));     // X2 与 X1 有缺口
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 16, 27, 25));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 20, 28, 26));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 24, 24, 22));  // X3 使原序列顶分型成立
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 28, 27, 24));
  F.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 32, 23, 21));
  F.push_back(MakeTestFractal(CZSC_POINT_TOP, 36, 29, 25));     // 反向序列没有底分型确认

  std::vector<Stroke> Strokes = BuildStrokes(F);
  std::vector<SegmentPoint> Line = BuildLineSegmentPointsByFeature(Strokes);

  return Line.size() == 1;
}

static bool TestDecodeConfig()
{
  CzscConfig c0 = DecodeConfig(0);
  CzscConfig c1 = DecodeConfig(1);        // 个位1 → 新笔
  CzscConfig c11 = DecodeConfig(11);      // 十位1 → 允许次高
  CzscConfig c100 = DecodeConfig(100);    // 百位1 → 线段中枢
  CzscConfig c1000 = DecodeConfig(1000);  // 千位1 → 特征序列法
  CzscConfig c1111 = DecodeConfig(1111);  // 全部非默认

  return (c0.nStrokeType == CZSC_STROKE_STRICT) && (c0.nStrokeEnd == CZSC_END_STRICT) &&
         (c0.nCenterUnit == CZSC_UNIT_STROKE) && (c0.nSegmentMethod == CZSC_SEG_HEURISTIC) &&
         (c1.nStrokeType == CZSC_STROKE_NEW) &&
         (c11.nStrokeType == CZSC_STROKE_NEW) && (c11.nStrokeEnd == CZSC_END_SECOND) &&
         (c100.nCenterUnit == CZSC_UNIT_SEGMENT) &&
         (c1000.nSegmentMethod == CZSC_SEG_FEATURE) &&
         (c1111.nStrokeType == CZSC_STROKE_NEW) && (c1111.nStrokeEnd == CZSC_END_SECOND) &&
         (c1111.nCenterUnit == CZSC_UNIT_SEGMENT) && (c1111.nSegmentMethod == CZSC_SEG_FEATURE);
}

static bool TestStrokeEndConfig()
{
  // 中间有更极端的次低点：严格收笔取最低(Bb@10)，允许次高保留首个(Ba@8)
  std::vector<Fractal> Fractals;
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 5, 1));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 10, 6));
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 8, 7, 5));    // Ba 低=5
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 10, 5, 3));   // Bb 低=3 更极端
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 14, 11, 7));

  std::vector<Stroke> Strict = BuildStrokes(Fractals);  // 默认：严格极值
  CzscConfig Second = DefaultConfig();
  Second.nStrokeEnd = CZSC_END_SECOND;
  std::vector<Stroke> Loose = BuildStrokes(Fractals, Second);  // 允许次高

  return (Strict.size() == 3) && (Loose.size() == 3) &&
         (Strict[2].Start.nIndex == 10) &&   // 严格：末笔从最低点 Bb@10 起
         (Loose[2].Start.nIndex == 8);       // 次高：末笔从次低点 Ba@8 起
}

// 真实上证日线：线段中枢构件的端点（线段）应少于笔中枢构件的端点（笔），体现线段是更高级别
static bool TestConfiguredPointsCenterUnit()
{
  const int n = SSE_DAILY_COUNT;
  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);

  CzscConfig StrokeCfg = DefaultConfig();                 // 笔中枢
  CzscConfig SegmentCfg = DefaultConfig();
  SegmentCfg.nCenterUnit = CZSC_UNIT_SEGMENT;             // 线段中枢

  std::vector<SegmentPoint> StrokePts = BuildConfiguredPoints(n, pH, pL, StrokeCfg);
  std::vector<SegmentPoint> SegmentPts = BuildConfiguredPoints(n, pH, pL, SegmentCfg);

  // 线段是更高级别：线段端点应少于笔端点
  return (StrokePts.size() > SegmentPts.size()) && !SegmentPts.empty();
}

static bool TestFunc20DrivesConfig()
{
  const int nCount = 21;
  float pHigh[nCount] = {
    5, 7, 8, 9, 10, 9, 8, 7, 7, 8, 9, 11, 12, 11, 10, 9, 9, 10, 11, 13, 14
  };
  float pLow[nCount] = {
    1, 2, 3, 5, 6, 5, 4, 3, 3, 4, 5, 7, 8, 7, 6, 5, 5, 6, 7, 9, 10
  };
  float pStroke[nCount];
  float pSegment[nCount];
  float pCfg20Stroke[nCount];
  float pCfg20Segment[nCount];
  float fStrokeCode = 0;     // 笔中枢
  float fSegmentCode = 100;  // 线段中枢

  Func1(nCount, pStroke, pHigh, pLow, &fStrokeCode);
  Func9(nCount, pSegment, pHigh, pLow, &fStrokeCode);
  Func20(nCount, pCfg20Stroke, pHigh, pLow, &fStrokeCode);
  Func20(nCount, pCfg20Segment, pHigh, pLow, &fSegmentCode);

  // Func20(码0) 应与 Func1(笔) 一致；Func20(码100) 应与 Func9(线段) 一致
  for (int i = 0; i < nCount; i++)
  {
    if (!NearlyEqual(pCfg20Stroke[i], pStroke[i]))
    {
      return false;
    }
    if (!NearlyEqual(pCfg20Segment[i], pSegment[i]))
    {
      return false;
    }
  }
  return true;
}

static bool TestFunc20FeatureSegmentModeMatchesFunc19()
{
  std::vector<float> High(SSE_DAILY_HIGH, SSE_DAILY_HIGH + SSE_DAILY_COUNT);
  std::vector<float> Low(SSE_DAILY_LOW, SSE_DAILY_LOW + SSE_DAILY_COUNT);
  std::vector<float> Legacy((std::size_t)SSE_DAILY_COUNT);
  std::vector<float> Configured((std::size_t)SSE_DAILY_COUNT);
  float fUnused = 0;
  float fFeatureSegmentCode = 1100;

  Func19(SSE_DAILY_COUNT, &Legacy[0], &High[0], &Low[0], &fUnused);
  Func20(SSE_DAILY_COUNT, &Configured[0], &High[0], &Low[0], &fFeatureSegmentCode);

  int nNonZero = 0;
  for (int i = 0; i < SSE_DAILY_COUNT; i++)
  {
    if (!NearlyEqual(Legacy[(std::size_t)i], Configured[(std::size_t)i]))
    {
      return false;
    }
    if (!NearlyEqual(Configured[(std::size_t)i], 0.0f))
    {
      nNonZero++;
    }
  }

  return nNonZero > 2;
}

static bool TestFunc20HandlesEmptyInput()
{
  Func20(0, 0, 0, 0, 0);

  const int nCount = 3;
  float pHigh[nCount] = {1, 2, 3};
  float pLow[nCount] = {1, 2, 3};
  float pOut[nCount] = {9, 9, 9};
  float fCode = 0;

  Func20(nCount, pOut, 0, pLow, &fCode);  // 缺最高价 → 提前返回，不改写

  return (pOut[0] == 9) && (pOut[1] == 9) && (pOut[2] == 9);
}

static bool TestAnalyzerFromSignalAggregates()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0;
    pHigh[i] = 0;
    pLow[i] = 0;
  }
  pIn[0] = -1; pHigh[0] = pLow[0] = 7;
  pIn[4] = 1; pHigh[4] = pLow[4] = 12;
  pIn[8] = -1; pHigh[8] = pLow[8] = 8;
  pIn[12] = 1; pHigh[12] = pLow[12] = 10;
  pIn[16] = -1; pHigh[16] = pLow[16] = 7.5f;
  pIn[20] = 1; pHigh[20] = pLow[20] = 7;
  pIn[24] = -1; pHigh[24] = pLow[24] = 4;
  pIn[28] = 1; pHigh[28] = pLow[28] = 4.2f;
  pIn[32] = -1; pHigh[32] = pLow[32] = 3.8f;
  pIn[36] = 1; pHigh[36] = pLow[36] = 6;
  pIn[40] = -1; pHigh[40] = pLow[40] = 4.5f;

  // 分析器一次算成的结果应与手工流水线逐段一致
  CzscAnalyzer An;
  BuildAnalyzerFromSignal(An, nCount, pIn, pHigh, pLow);

  std::vector<SegmentPoint> Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  AssignSegmentEnergy(Points, nCount, pHigh, pLow);
  std::vector<Center> Centers = BuildCenters(Points);
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

  return (An.Points.size() == Points.size()) &&
         (An.Centers.size() == Centers.size()) &&
         (An.Candidates.size() == Candidates.size()) &&
         HasSignalCandidate(An.Candidates, 32, 1.0f) &&   // 一买
         HasSignalCandidate(An.Candidates, 40, 2.0f);     // 二买
}

static bool TestSignalCacheHitAndInvalidate()
{
  const int nCount = 41;
  float pIn[nCount];
  float pHigh[nCount];
  float pLow[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pIn[i] = 0; pHigh[i] = 0; pLow[i] = 0;
  }
  pIn[0] = -1; pHigh[0] = pLow[0] = 7;
  pIn[4] = 1; pHigh[4] = pLow[4] = 12;
  pIn[8] = -1; pHigh[8] = pLow[8] = 8;
  pIn[12] = 1; pHigh[12] = pLow[12] = 10;
  pIn[16] = -1; pHigh[16] = pLow[16] = 7.5f;
  pIn[20] = 1; pHigh[20] = pLow[20] = 7;
  pIn[24] = -1; pHigh[24] = pLow[24] = 4;
  pIn[28] = 1; pHigh[28] = pLow[28] = 4.2f;
  pIn[32] = -1; pHigh[32] = pLow[32] = 3.8f;
  pIn[36] = 1; pHigh[36] = pLow[36] = 6;
  pIn[40] = -1; pHigh[40] = pLow[40] = 4.5f;

  std::size_t candA1 = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow).Candidates.size();
  std::size_t candA2 = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow).Candidates.size();  // 命中
  if (candA1 != candA2)
  {
    return false;
  }

  // 改一个高点 → 缓存须失效、反映新输入（与手工构建一致，而非返回 stale A）
  float pHighB[nCount];
  for (int i = 0; i < nCount; i++)
  {
    pHighB[i] = pHigh[i];
  }
  pHighB[20] = 13;
  std::size_t candB = GetOrBuildSignalAnalyzer(nCount, pIn, pHighB, pLow).Candidates.size();
  CzscAnalyzer ManualB;
  BuildAnalyzerFromSignal(ManualB, nCount, pIn, pHighB, pLow);
  if (candB != ManualB.Candidates.size())
  {
    return false;
  }

  // 再查 A → 仍是 A 的正确结果（非 stale B）
  std::size_t candA3 = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow).Candidates.size();
  return candA3 == candA1;
}

static bool TestFunc30MatchesLegacyPipeline()
{
  const int nCount = 21;
  float pHigh[nCount] = {
    5, 7, 8, 9, 10, 9, 8, 7, 7, 8, 9, 11, 12, 11, 10, 9, 9, 10, 11, 13, 14
  };
  float pLow[nCount] = {
    1, 2, 3, 5, 6, 5, 4, 3, 3, 4, 5, 7, 8, 7, 6, 5, 5, 6, 7, 9, 10
  };

  float pStroke[nCount];   // 笔端点（旧两步法的 DLL 输入）
  float pLegacy[nCount];
  float pFunc30[nCount];
  float fTime = 5;
  float fCode;

  Func1(nCount, pStroke, pHigh, pLow, &fTime);

  // 输出 0（端点，配置 0 笔中枢）：Func30(码0) ≡ Func1
  fCode = 0;
  Func30(nCount, pFunc30, pHigh, pLow, &fCode);
  for (int i = 0; i < nCount; i++)
  {
    if (!NearlyEqual(pFunc30[i], pStroke[i]))
    {
      return false;
    }
  }

  // 输出 1（中枢上沿）：Func30(码10) ≡ Func2(笔端点信号)
  Func2(nCount, pLegacy, pStroke, pHigh, pLow);
  fCode = 10;
  Func30(nCount, pFunc30, pHigh, pLow, &fCode);
  for (int i = 0; i < nCount; i++)
  {
    if (!NearlyEqual(pFunc30[i], pLegacy[i]))
    {
      return false;
    }
  }

  // 输出 4（三类买卖点）：Func30(码40) ≡ Func5(笔端点信号)
  Func5(nCount, pLegacy, pStroke, pHigh, pLow);
  fCode = 40;
  Func30(nCount, pFunc30, pHigh, pLow, &fCode);
  for (int i = 0; i < nCount; i++)
  {
    if (!NearlyEqual(pFunc30[i], pLegacy[i]))
    {
      return false;
    }
  }

  return true;
}

static bool TestFunc30FeatureSegmentModeMatchesFunc19()
{
  std::vector<float> High(SSE_DAILY_HIGH, SSE_DAILY_HIGH + SSE_DAILY_COUNT);
  std::vector<float> Low(SSE_DAILY_LOW, SSE_DAILY_LOW + SSE_DAILY_COUNT);
  std::vector<float> Legacy((std::size_t)SSE_DAILY_COUNT);
  std::vector<float> Unified((std::size_t)SSE_DAILY_COUNT);
  float fUnused = 0;
  float fMode = 1100000;

  Func19(SSE_DAILY_COUNT, &Legacy[0], &High[0], &Low[0], &fUnused);
  Func30(SSE_DAILY_COUNT, &Unified[0], &High[0], &Low[0], &fMode);

  int nNonZero = 0;
  for (int i = 0; i < SSE_DAILY_COUNT; i++)
  {
    if (!NearlyEqual(Legacy[(std::size_t)i], Unified[(std::size_t)i]))
    {
      return false;
    }
    if (!NearlyEqual(Unified[(std::size_t)i], 0.0f))
    {
      nNonZero++;
    }
  }

  return nNonZero > 2;
}

static bool TestFunc30DiagnosticOutputsMatchProjections()
{
  std::vector<float> High(SSE_DAILY_HIGH, SSE_DAILY_HIGH + SSE_DAILY_COUNT);
  std::vector<float> Low(SSE_DAILY_LOW, SSE_DAILY_LOW + SSE_DAILY_COUNT);
  std::vector<float> Expected((std::size_t)SSE_DAILY_COUNT);
  std::vector<float> Unified((std::size_t)SSE_DAILY_COUNT);

  CzscAnalyzer An;
  BuildAnalyzerFromPrice(An, SSE_DAILY_COUNT, &High[0], &Low[0], DefaultConfig());

  const int Outputs[] = {10, 11, 12, 13, 14, 15, 16, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55};
  for (std::size_t i = 0; i < sizeof(Outputs) / sizeof(Outputs[0]); i++)
  {
    int nOutput = Outputs[i];
    switch (nOutput)
    {
      case 10:
        for (int n = 0; n < SSE_DAILY_COUNT; n++)
        {
          Expected[(std::size_t)n] = 0.0f;
        }
        if ((int)An.MaShort.size() >= SSE_DAILY_COUNT && (int)An.MaLong.size() >= SSE_DAILY_COUNT)
        {
          for (int n = 0; n < SSE_DAILY_COUNT; n++)
          {
            Expected[(std::size_t)n] = An.MaShort[(std::size_t)n] - An.MaLong[(std::size_t)n];
          }
        }
        break;
      case 11:
        for (int n = 0; n < SSE_DAILY_COUNT; n++)
        {
          Expected[(std::size_t)n] = 0.0f;
        }
        if ((int)An.Kiss.size() >= SSE_DAILY_COUNT)
        {
          for (int n = 0; n < SSE_DAILY_COUNT; n++)
          {
            Expected[(std::size_t)n] = (float)An.Kiss[(std::size_t)n];
          }
        }
        break;
      case 12:
      {
        for (int n = 0; n < SSE_DAILY_COUNT; n++)
        {
          Expected[(std::size_t)n] = 0.0f;
        }
        int nWarn = DetectInstantDivergence(An.Points, SSE_DAILY_COUNT, &High[0], &Low[0]);
        if (nWarn != 0)
        {
          Expected[(std::size_t)(SSE_DAILY_COUNT - 1)] = (float)nWarn;
        }
        break;
      }
      case 13:
        for (int n = 0; n < SSE_DAILY_COUNT; n++)
        {
          Expected[(std::size_t)n] = 0.0f;
        }
        if ((int)An.KissVol.size() >= SSE_DAILY_COUNT)
        {
          for (int n = 0; n < SSE_DAILY_COUNT; n++)
          {
            Expected[(std::size_t)n] = (float)An.KissVol[(std::size_t)n];
          }
        }
        break;
      case 14: ApplyTradingSignalSmallTurn(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 15: ApplyTradingSignalAbcStructure(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 16: ApplyTradingSignalStrictAbcCandidates(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 18: ApplyTradingSignalMacdLineWeakness(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 19: ApplyTradingSignalMacdZeroPullback(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 20: ApplyTradingSignalStandardDivergence(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 21: ApplyTradingSignalContextFlags(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 22: ApplyTradingSignalCenterPosition(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 23: ApplyTradingSignalMovementType(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 24: ApplyTradingSignalPriority(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 25: ApplyTradingSignalCenterId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 26: ApplyTradingSignalBreakoutId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 27: ApplyTradingSignalPointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 28: ApplyTradingSignalTrendId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 29: ApplyTradingSignalMacdAreaRatio(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 30: ApplyTradingSignalSpaceRatio(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 31: ApplyTradingSignalSpeedRatio(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 32: ApplyTradingSignalDivergenceFlags(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 33: ApplyTradingSignalBreakoutLeavePointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates, An.Breakouts); break;
      case 34: ApplyTradingSignalBreakoutRetestPointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates, An.Breakouts); break;
      case 35: ApplyTradingSignalAbcBreakoutId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 36: ApplyTradingSignalAbcBreakoutLeavePointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates, An.Breakouts); break;
      case 37: ApplyTradingSignalAbcBreakoutRetestPointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates, An.Breakouts); break;
      case 38: ApplyTradingSignalSmallTurnLeavePointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates, An.Breakouts); break;
      case 39: ApplyTradingSignalSmallTurnRetestPointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates, An.Breakouts); break;
      case 40: ApplyTradingSignalSecondBasePointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 41: ApplyTradingSignalSecondTurnPointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 42: ApplyTradingSignalSmallTurnBasePointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 43: ApplyTradingSignalDivergencePreviousStartPointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 44: ApplyTradingSignalDivergencePreviousEndPointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 45: ApplyTradingSignalDivergenceCurrentStartPointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 46: ApplyTradingSignalDivergenceCurrentEndPointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 47: ApplyTradingSignalCenterLifecycle(SSE_DAILY_COUNT, &Expected[0], An.Candidates, An.Centers); break;
      case 48: WriteCenterLifecycleSignal(SSE_DAILY_COUNT, &Expected[0], An.Centers); break;
      case 49:
      case 50:
      case 51:
      case 52:
      {
        CzscConfig HighConfig = DefaultConfig();
        HighConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
        HighConfig.nSegmentMethod = CZSC_SEG_FEATURE;
        CzscAnalyzer HighAn;
        BuildAnalyzerFromPrice(HighAn, SSE_DAILY_COUNT, &High[0], &Low[0], HighConfig);
        CzscAnalyzer LowAn;
        BuildAnalyzerFromPrice(LowAn, SSE_DAILY_COUNT, &High[0], &Low[0], DefaultConfig());
        std::vector<NestedDivergenceContext> Contexts =
          BuildNestedDivergenceContexts(HighAn.Points, HighAn.Candidates,
                                        LowAn.Points, LowAn.Candidates);
        if (nOutput == 49)
        {
          ApplyNestedDivergenceLevel(SSE_DAILY_COUNT, &Expected[0], Contexts);
        }
        else if (nOutput == 50)
        {
          ApplyNestedDivergenceSourceId(SSE_DAILY_COUNT, &Expected[0], Contexts);
        }
        else if (nOutput == 51)
        {
          ApplyNestedDivergenceStartPointId(SSE_DAILY_COUNT, &Expected[0], Contexts);
        }
        else
        {
          ApplyNestedDivergenceEndPointId(SSE_DAILY_COUNT, &Expected[0], Contexts);
        }
        break;
      }
      case 53: ApplyTradingSignalDivergenceSemantic(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 54: ApplyTradingSignalReversalPointId(SSE_DAILY_COUNT, &Expected[0], An.Candidates); break;
      case 55: ApplyTradingFilterReasons(SSE_DAILY_COUNT, &Expected[0], An.TradingFilterReasons); break;
      default: return false;
    }

    float fMode = (float)(nOutput * 10);
    Func30(SSE_DAILY_COUNT, &Unified[0], &High[0], &Low[0], &fMode);

    for (int n = 0; n < SSE_DAILY_COUNT; n++)
    {
      if (!NearlyEqual(Unified[(std::size_t)n], Expected[(std::size_t)n]))
      {
        return false;
      }
    }
  }

  CzscConfig HighConfig = DefaultConfig();
  HighConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
  HighConfig.nSegmentMethod = CZSC_SEG_FEATURE;
  CzscAnalyzer HighAn;
  BuildAnalyzerFromPrice(HighAn, SSE_DAILY_COUNT, &High[0], &Low[0], HighConfig);
  CzscAnalyzer LowAn;
  BuildAnalyzerFromPrice(LowAn, SSE_DAILY_COUNT, &High[0], &Low[0], DefaultConfig());

  WriteNestedDivergenceSignal(SSE_DAILY_COUNT, &Expected[0],
                              HighAn.Points, HighAn.Candidates,
                              LowAn.Points, LowAn.Candidates);
  float fNestedMode = 170;
  Func30(SSE_DAILY_COUNT, &Unified[0], &High[0], &Low[0], &fNestedMode);
  for (int n = 0; n < SSE_DAILY_COUNT; n++)
  {
    if (!NearlyEqual(Unified[(std::size_t)n], Expected[(std::size_t)n]))
    {
      return false;
    }
  }

  return true;
}

static bool TestFunc30RejectsInvalidMode()
{
  const int nCount = 5;
  float pHigh[nCount] = {5, 6, 7, 6, 5};
  float pLow[nCount] = {1, 2, 3, 2, 1};
  float pOut[nCount];

  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = 9;
  }
  float fBadTail = 11;
  Func30(nCount, pOut, pHigh, pLow, &fBadTail);
  for (int i = 0; i < nCount; i++)
  {
    if (!NearlyEqual(pOut[i], 0.0f))
    {
      return false;
    }
  }

  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = 9;
  }
  float fUnknownOutput = 290;
  Func30(nCount, pOut, pHigh, pLow, &fUnknownOutput);
  for (int i = 0; i < nCount; i++)
  {
    if (!NearlyEqual(pOut[i], 0.0f))
    {
      return false;
    }
  }

  std::vector<float> High(SSE_DAILY_HIGH, SSE_DAILY_HIGH + SSE_DAILY_COUNT);
  std::vector<float> Low(SSE_DAILY_LOW, SSE_DAILY_LOW + SSE_DAILY_COUNT);
  std::vector<float> InvalidConfig((std::size_t)SSE_DAILY_COUNT);
  for (int i = 0; i < SSE_DAILY_COUNT; i++)
  {
    InvalidConfig[(std::size_t)i] = 9.0f;
  }
  float fInvalidConfig = 2000;
  Func30(SSE_DAILY_COUNT, &InvalidConfig[0], &High[0], &Low[0], &fInvalidConfig);
  for (int i = 0; i < SSE_DAILY_COUNT; i++)
  {
    if (!NearlyEqual(InvalidConfig[(std::size_t)i], 0.0f))
    {
      return false;
    }
  }

  return true;
}

static bool TestFunc30HandlesEmptyInput()
{
  Func30(0, 0, 0, 0, 0);

  const int nCount = 3;
  float pLow[nCount] = {1, 2, 3};
  float pOut[nCount] = {9, 9, 9};
  float fCode = 0;

  Func30(nCount, pOut, 0, pLow, &fCode);  // 缺最高价 → 提前返回，不改写

  return (pOut[0] == 9) && (pOut[1] == 9) && (pOut[2] == 9);
}

static bool TestInvalidPriceSanitized()
{
  union { unsigned int u; float f; } Invalid;
  Invalid.u = 0xF8F8F8F8u;  // 通达信无效数

  // 前 2 根无效 + 后 7 根（与 TestFractalsAndStrokes 同，产生 顶12 / 底2 两个分型）
  const int nCount = 9;
  float pHigh[nCount] = {Invalid.f, Invalid.f, 10, 12, 11, 10, 9, 8, 10};
  float pLow[nCount]  = {Invalid.f, Invalid.f,  5,  7,  6,  5, 4, 2,  4};

  std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);
  std::vector<Fractal> Fractals = BuildFractals(Bars);

  // 无效前缀被清洗为首个有效值（平台），不产生伪分型：仍是 顶(高12) + 底(低2) 两个
  if (Fractals.size() != 2)
  {
    return false;
  }
  bool bTop = false;
  bool bBottom = false;
  for (std::size_t i = 0; i < Fractals.size(); i++)
  {
    if ((Fractals[i].nType == CZSC_POINT_TOP) && NearlyEqual(Fractals[i].fHigh, 12.0f))
    {
      bTop = true;
    }
    if ((Fractals[i].nType == CZSC_POINT_BOTTOM) && NearlyEqual(Fractals[i].fLow, 2.0f))
    {
      bBottom = true;
    }
  }
  return bTop && bBottom;
}

static bool TestAuxDataValidation()
{
  RegisterAuxData(0, 0, 0);  // 起始清空

  const int n = 8;
  float pHigh[n] = {10, 11, 12, 11, 10, 9, 10, 11};
  float pLow[n]  = { 8,  9, 10,  9,  8, 7,  8,  9};
  float close[n] = { 9, 10, 11, 10,  9, 8,  9, 10};   // 每根都在 [L,H] 内 → 有效
  float vol[n]   = {100, 120, 150, 110, 90, 80, 95, 130};

  bool bOk = true;
  // 未注册 → 空
  if (GetValidatedClose(n, pHigh, pLow) != 0) bOk = false;

  // 注册有效收盘价 → 校验通过
  RegisterAuxData(n, close, vol);
  const std::vector<float> *pVC = GetValidatedClose(n, pHigh, pLow);
  if ((pVC == 0) || ((int)pVC->size() != n) || !NearlyEqual((*pVC)[2], 11.0f)) bOk = false;
  const std::vector<float> *pVV = GetValidatedVolume(n, pHigh, pLow);
  if ((pVV == 0) || !NearlyEqual((*pVV)[2], 150.0f)) bOk = false;

  // 越界收盘价 → 否决
  float badClose[n] = {9, 10, 99, 10, 9, 8, 9, 10};   // bar2 close=99 > high12
  RegisterAuxData(n, badClose, vol);
  if (GetValidatedClose(n, pHigh, pLow) != 0) bOk = false;

  // nCount 不匹配 → 否决
  RegisterAuxData(n, close, vol);
  if (GetValidatedClose(n + 1, pHigh, pLow) != 0) bOk = false;

  RegisterAuxData(0, 0, 0);  // 收尾清空
  return bOk;
}

static bool TestAuxCloseAffectsEnergy()
{
  RegisterAuxData(0, 0, 0);

  const int n = 30;
  float pHigh[n];
  float pLow[n];
  float close[n];
  for (int i = 0; i < n; i++)
  {
    pLow[i] = 10;
    pHigh[i] = 20;
    close[i] = 10.0f + 10.0f * ((float)i / (float)(n - 1));  // 斜坡，∈[10,20]
  }

  std::vector<SegmentPoint> A;
  A.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 5, 15));
  A.push_back(MakeTestPoint(CZSC_POINT_TOP, 25, 15));
  std::vector<SegmentPoint> B = A;

  AssignSegmentEnergy(A, n, pHigh, pLow);        // 无旁路 → (H+L)/2=15 常数 → MACD/能量为 0
  RegisterAuxData(n, close, 0);
  AssignSegmentEnergy(B, n, pHigh, pLow);        // 用真实收盘价(斜坡) → 能量非 0
  RegisterAuxData(0, 0, 0);

  float eA = A[1].fEnergy - A[0].fEnergy;
  float eB = B[1].fEnergy - B[0].fEnergy;
  return !NearlyEqual(eA, eB);  // 真实收盘价改变了 MACD 面积
}

static bool TestFunc30AuxCloseInvalidatesPriceCache()
{
  RegisterAuxData(0, 0, 0);

  const int n = 30;
  float pHigh[n];
  float pLow[n];
  float close[n];
  float badClose[n];
  float pOut[n];
  float fMode = 100;  // Func30 输出10：短长均线差
  for (int i = 0; i < n; i++)
  {
    pLow[i] = 10;
    pHigh[i] = 20;
    close[i] = 10.0f + 10.0f * ((float)i / (float)(n - 1));
    badClose[i] = close[i];
    pOut[i] = -1;
  }
  badClose[15] = 99;  // 内容校验失败，须回落到 (H+L)/2

  Func30(n, pOut, pHigh, pLow, &fMode);
  float fProxy = pOut[n - 1];

  RegisterAuxData(n, close, 0);
  Func30(n, pOut, pHigh, pLow, &fMode);
  float fReal = pOut[n - 1];

  RegisterAuxData(n, badClose, 0);
  Func30(n, pOut, pHigh, pLow, &fMode);
  float fFallback = pOut[n - 1];

  RegisterAuxData(0, 0, 0);
  return NearlyEqual(fProxy, 0.0f) &&
         (fReal > 0.1f) &&
         NearlyEqual(fFallback, fProxy);
}

static bool TestSignalCacheInvalidatesOnAuxClose()
{
  RegisterAuxData(0, 0, 0);

  const int n = 30;
  float pIn[n];
  float pHigh[n];
  float pLow[n];
  float close[n];
  for (int i = 0; i < n; i++)
  {
    pIn[i] = 0;
    pLow[i] = 10;
    pHigh[i] = 20;
    close[i] = 10.0f + 10.0f * ((float)i / (float)(n - 1));
  }
  pIn[5] = -1;
  pIn[25] = 1;

  const CzscAnalyzer &Proxy = GetOrBuildSignalAnalyzer(n, pIn, pHigh, pLow);
  if (Proxy.Points.size() != 2)
  {
    RegisterAuxData(0, 0, 0);
    return false;
  }
  float fProxyEnergy = Proxy.Points[1].fEnergy - Proxy.Points[0].fEnergy;

  RegisterAuxData(n, close, 0);
  const CzscAnalyzer &Real = GetOrBuildSignalAnalyzer(n, pIn, pHigh, pLow);
  if (Real.Points.size() != 2)
  {
    RegisterAuxData(0, 0, 0);
    return false;
  }
  float fRealEnergy = Real.Points[1].fEnergy - Real.Points[0].fEnergy;

  RegisterAuxData(0, 0, 0);
  return NearlyEqual(fProxyEnergy, 0.0f) &&
         !NearlyEqual(fRealEnergy, fProxyEnergy);
}

static bool TestFunc40Registers()
{
  RegisterAuxData(0, 0, 0);

  const int n = 6;
  float pHigh[n] = {12, 13, 14, 13, 12, 11};
  float pLow[n]  = {10, 11, 12, 11, 10, 9};
  float close[n] = {11, 12, 13, 12, 11, 10};
  float vol[n]   = {100, 110, 120, 105, 95, 90};
  float pOut[n]  = {-1, -1, -1, -1, -1, -1};
  float fUnused = 0;

  Func40(n, pOut, close, vol, &fUnused);  // 注册 + 透传

  bool bPass = true;
  for (int i = 0; i < n; i++)
  {
    if (!NearlyEqual(pOut[i], close[i]))
    {
      bPass = false;
    }
  }
  bool bRegistered = (GetValidatedClose(n, pHigh, pLow) != 0);

  RegisterAuxData(0, 0, 0);
  return bPass && bRegistered;
}

static bool TestKissVolumeTrap()
{
  // 构造短长均线：间距 d=Short-Long 在 i=12 处由负转正且 |d| 局部极小 → 湿吻
  const int n = 20;
  std::vector<float> Short((std::size_t)n, 10.0f);
  std::vector<float> Long((std::size_t)n);
  for (int i = 0; i < n; i++)
  {
    float d;
    if (i <= 10)      d = -1.0f;
    else if (i == 11) d = -0.5f;
    else if (i == 12) d = 0.1f;
    else if (i == 13) d = 0.6f;
    else              d = 1.0f;
    Long[(std::size_t)i] = 10.0f - d;
  }

  // 基线：纯价吻在 i=12 为湿吻
  std::vector<int> base = ClassifyMaKisses(Short, Long);
  if (base[12] != CZSC_KISS_WET) return false;

  // 无量 → 退化为纯价吻
  std::vector<int> noVol = ClassifyMaKissesWithVolume(Short, Long, std::vector<float>());
  if (noVol[12] != CZSC_KISS_WET) return false;

  // 平量 → 湿吻不标嫌疑
  std::vector<float> flat((std::size_t)n, 100.0f);
  std::vector<int> normal = ClassifyMaKissesWithVolume(Short, Long, flat);
  if (normal[12] != CZSC_KISS_WET) return false;

  // 放量（湿吻前几根量远高于量均线）→ 标骗线嫌疑
  std::vector<float> spike((std::size_t)n, 100.0f);
  spike[10] = 300.0f; spike[11] = 300.0f; spike[12] = 300.0f;
  std::vector<int> trap = ClassifyMaKissesWithVolume(Short, Long, spike);
  if (trap[12] != CZSC_KISS_WET_TRAP) return false;

  return true;
}

// 向上笔中继延伸：顶 D1 后回踩不足一笔、又创新高到 D2，原向上笔应延伸到 D2（D1 变中继）
static bool TestStrokeRelayExtendsUp()
{
  std::vector<Fractal> Fractals;
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 5, 1));   // B0 起点
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 10, 6));     // D1 顶
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 6, 9, 8));   // B1 回踩(gap 6-4=2<4, 不创新低)
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 10, 12, 7));    // D2 更高顶

  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  if (Strokes.size() != 1) return false;
  if (Strokes[0].Start.nIndex != 0) return false;
  if (Strokes[0].End.nIndex != 10) return false;        // 延伸到 D2，而非停在 D1(idx4)
  if (!NearlyEqual(Strokes[0].End.fHigh, 12)) return false;
  return true;
}

// 向下笔中继延伸：底 B1 后反弹不足一笔、又创新低到 B2，原向下笔应延伸到 B2（B1 变中继）
static bool TestStrokeRelayExtendsDown()
{
  std::vector<Fractal> Fractals;
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 0, 20, 15));    // T0 起点
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 4, 13, 10)); // B1 底
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 6, 11, 9));     // T1 反弹(gap 2<4, 不创新高)
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 10, 12, 8)); // B2 更低底

  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  if (Strokes.size() != 1) return false;
  if (Strokes[0].Start.nIndex != 0) return false;
  if (Strokes[0].End.nIndex != 10) return false;        // 延伸到 B2，而非停在 B1(idx4)
  if (!NearlyEqual(Strokes[0].End.fLow, 8)) return false;
  return true;
}

// 达标笔不被后续不足一笔的分型弹出：D1→B1 已达标(跨度4)，其后 D2 虽创新高但 B1→D2 不足一笔(跨度2)被忽略，
// B1 保留 → 仍是 B0→D1、D1→B1 两笔（修复「9/5→9/13 达标笔被破坏回退误弹」前的回归）
static bool TestStrokeBrokenByNewExtreme()
{
  std::vector<Fractal> Fractals;
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 0, 5, 1));   // B0
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 4, 10, 6));     // D1
  Fractals.push_back(MakeTestFractal(CZSC_POINT_BOTTOM, 8, 7, 3));   // B1（D1→B1 跨度4，达标成笔）
  Fractals.push_back(MakeTestFractal(CZSC_POINT_TOP, 10, 12, 7));    // D2（B1→D2 跨度2 不足一笔，忽略）

  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  if (Strokes.size() != 2) return false;                            // B0→D1、D1→B1 两笔均保留
  if ((Strokes[0].Start.nIndex != 0) || (Strokes[0].End.nIndex != 4)) return false;
  if ((Strokes[1].Start.nIndex != 4) || (Strokes[1].End.nIndex != 8)) return false;
  return true;
}

int main()
{
  if (!TestOutputIsCleared())
  {
    return 1;
  }
  if (!TestMergedBarsHandleInclusion())
  {
    return 2;
  }
  if (!TestMergedBarsTrackExtremeIndexes())
  {
    return 122;
  }
  if (!TestMergedBarsApplySequentialInclusionDirection())
  {
    return 171;
  }
  if (!TestFractalsUseMergedExtremeIndexes())
  {
    return 123;
  }
  if (!TestFractalsAndStrokes())
  {
    return 3;
  }
  if (!TestStrokeRequiresFiveBars())
  {
    return 4;
  }
  if (!TestRealSseMergedBarsAreWellFormed())
  {
    return 172;
  }
  if (!TestRealSseStrokesWellFormed())
  {
    return 115;
  }
  if (!TestRealSseSegmentsSubsetOfStrokes())
  {
    return 116;
  }
  if (!TestRealSseNewBiNotFewerThanStrict())
  {
    return 117;
  }
  if (!TestRealSseSignalsWellFormed())
  {
    return 118;
  }
  if (!TestRealSseDiagnosticCounts())
  {
    return 155;
  }
  if (!TestRealSsePricePointsStayOnStrictStrokeEndpoints())
  {
    return 119;
  }
  if (!TestRealSseFirstCenterStopsBeforeLeave())
  {
    return 120;
  }
  if (!TestRealSseGoldenCentersPresent())
  {
    return 121;
  }
  if (!TestRealSseCentersDoNotShareEndpoints())
  {
    return 156;
  }
  if (!TestRealSseGoldenSegmentCentersPresent())
  {
    return 154;
  }
  if (!TestRealSseRecursiveCenterLifecycleCounts())
  {
    return 203;
  }
  if (!TestRecentSseRecursiveCenterLifecycleCounts())
  {
    return 204;
  }
  if (!TestRealSseGoldenCandidatesPresent())
  {
    return 158;
  }
  if (!TestRealSseGoldenBreakoutsPresent())
  {
    return 188;
  }
  if (!TestFunc1WritesCompatibleSignal())
  {
    return 5;
  }
  if (!TestLineSegmentsAreHigherLevelThanStrokes())
  {
    return 6;
  }
  if (!TestLineSegmentBreakConfirmsTurn())
  {
    return 7;
  }
  if (!TestStrengthMetricsMeasureSpaceAndSpeed())
  {
    return 8;
  }
  if (!TestStrengthMetricsDetectWeakening())
  {
    return 9;
  }
  if (!TestDivergenceResultDetectsUpWeakening())
  {
    return 10;
  }
  if (!TestDivergenceResultDetectsDownWeakening())
  {
    return 11;
  }
  if (!TestDivergenceResultSkipsStrongNewHigh())
  {
    return 12;
  }
  if (!TestTrendStructuresDetectConsolidation())
  {
    return 13;
  }
  if (!TestTrendStructuresDetectUpTrend())
  {
    return 14;
  }
  if (!TestTrendStructuresDetectDownTrend())
  {
    return 15;
  }
  if (!TestTrendStructuresSkipOverlappingTrend())
  {
    return 16;
  }
  if (!TestTrendStructuresUseFullCenterExtent())
  {
    return 124;
  }
  if (!TestTrendStructuresTreatTouchingFullExtentsAsConsolidation())
  {
    return 174;
  }
  if (!TestCenterBreakoutsDetectThirdBuy())
  {
    return 17;
  }
  if (!TestCenterBreakoutsDetectThirdSell())
  {
    return 18;
  }
  if (!TestCenterBreakoutsAllowBoundaryRetest())
  {
    return 178;
  }
  if (!TestCenterBreakoutsUseFirstRetestOnly())
  {
    return 19;
  }
  if (!TestCenterBreakoutsUseCenterEndAsLeavePoint())
  {
    return 153;
  }
  if (!TestCenterBreakoutsSkipBackIntoCenter())
  {
    return 20;
  }
  if (!TestCenterBreakoutsDoNotUseLaterRetestAfterBackIntoCenter())
  {
    return 175;
  }
  if (!TestCenterBreakoutsSkipWithoutLeave())
  {
    return 21;
  }
  if (!TestConsolidationDivergenceDetectsUpWeakening())
  {
    return 22;
  }
  if (!TestConsolidationDivergenceDetectsDownWeakening())
  {
    return 23;
  }
  if (!TestConsolidationDivergenceSkipsStrongBreakout())
  {
    return 24;
  }
  if (!TestTradingCandidatesGenerateFirstAndSecondBuy())
  {
    return 25;
  }
  if (!TestTradingCandidatesAllowEqualSecondExtremes())
  {
    return 183;
  }
  if (!TestTradingCandidatesGenerateThirdBuy())
  {
    return 26;
  }
  if (!TestTradingCandidatesSkipSecondWithoutFirst())
  {
    return 27;
  }
  if (!TestApplyTradingCandidatesKeepsPriority())
  {
    return 28;
  }
  if (!TestApplyTradingCandidatesThirdOverridesSecond())
  {
    return 29;
  }
  if (!TestApplyTradingOutputsSkipInvalidSignals())
  {
    return 170;
  }
  if (!TestFirstCandidateKeepsTrendDivergence())
  {
    return 30;
  }
  if (!TestFirstCandidateRequiresTrendStructure())
  {
    return 151;
  }
  if (!TestFirstCandidateRequiresOutsideLastCenter())
  {
    return 176;
  }
  if (!TestFirstCandidateMarksAbcStructure())
  {
    return 129;
  }
  if (!TestFirstSellCandidateMarksAbcStructure())
  {
    return 130;
  }
  if (!TestFirstCandidateRequiresValidAbcBreakout())
  {
    return 165;
  }
  if (!TestFirstCandidateMarksMacdZeroPullback())
  {
    return 138;
  }
  if (!TestFirstCandidateBuildsStandardMacdDivergence())
  {
    return 160;
  }
  if (!TestFirstSellCandidateBuildsStandardMacdDivergence())
  {
    return 161;
  }
  if (!TestFirstCandidateSkipsAfterLaterCenter())
  {
    return 125;
  }
  if (!TestFirstCandidateUsesPreLastCenterMove())
  {
    return 126;
  }
  if (!TestThirdCandidateKeepsBreakoutDivergence())
  {
    return 31;
  }
  if (!TestThirdCandidateRequiresBreakoutDirection())
  {
    return 181;
  }
  if (!TestThirdCandidateRequiresValidCenter())
  {
    return 182;
  }
  if (!TestThirdCandidateUsesOnlyCompletedTrendStructure())
  {
    return 157;
  }
  if (!TestTradingCandidatesMarkSecondThirdBuyOverlap())
  {
    return 32;
  }
  if (!TestTradingCandidatesMarkSecondThirdSellOverlap())
  {
    return 33;
  }
  if (!TestSecondThirdOverlapRequiresFirstCenter())
  {
    return 163;
  }
  if (!TestSecondThirdSellOverlapRequiresFirstCenter())
  {
    return 164;
  }
  if (!TestSmallTurnRequiresSameCenterAndLaterThird())
  {
    return 162;
  }
  if (!TestSmallTurnSellRequiresSameCenterAndLaterThird())
  {
    return 179;
  }
  if (!TestTradingCandidatesMarkSecondBuyInsideCenter())
  {
    return 34;
  }
  if (!TestTradingCandidatesMarkSecondSellInsideCenter())
  {
    return 152;
  }
  if (!TestFunc9WritesLineSegmentSignal())
  {
    return 35;
  }
  if (!TestCentersUseThreeOverlappingSegments())
  {
    return 36;
  }
  if (!TestCentersTrackEntryDirection())
  {
    return 177;
  }
  if (!TestCenterExtendsWithOverlappingSegment())
  {
    return 37;
  }
  if (!TestCenterExtendsWithCrossingSegment())
  {
    return 159;
  }
  if (!TestCentersSplitWhenOverlapBreaks())
  {
    return 38;
  }
  if (!TestCenterFunctionsWriteSignals())
  {
    return 39;
  }
  if (!TestFunc5WritesTrendDivergenceFirstBuy())
  {
    return 40;
  }
  if (!TestFunc5WritesCenterThirdBuy())
  {
    return 41;
  }
  if (!TestFunc5WritesCenterThirdSell())
  {
    return 42;
  }
  if (!TestFunc5WritesThirdSignalsAtCenterBoundary())
  {
    return 162;
  }
  if (!TestFunc5WritesSecondBuyAfterFirstBuy())
  {
    return 43;
  }
  if (!TestFunc5WritesSecondSellAfterFirstSell())
  {
    return 44;
  }
  if (!TestFunc5WritesTrendDivergenceFirstSell())
  {
    return 45;
  }
  if (!TestFunc5SkipsStrongNewLow())
  {
    return 46;
  }
  if (!TestStrengthAndSlopeUsePreviousExtremes())
  {
    return 47;
  }
  if (!TestEmptyInputReturns())
  {
    return 48;
  }
  if (!TestMacdHistogramFlatSeriesIsZero())
  {
    return 49;
  }
  if (!TestMacdHistogramRisingSeriesIsPositive())
  {
    return 50;
  }
  if (!TestAssignSegmentEnergySetsCumulativeArea())
  {
    return 51;
  }
  if (!TestAssignSegmentEnergySetsMacdLines())
  {
    return 135;
  }
  if (!TestStrengthMetricsUseMacdEnergy())
  {
    return 52;
  }
  if (!TestStrengthMetricsUseMacdLineHeight())
  {
    return 136;
  }
  if (!TestDivergenceDetectsMacdWeakening())
  {
    return 53;
  }
  if (!TestFirstCandidateMacdUpgradesQuality())
  {
    return 54;
  }
  if (!TestFunc10WritesSignalQuality())
  {
    return 55;
  }
  if (!TestMacdHistogramHandlesTinyInput())
  {
    return 56;
  }
  if (!TestAssignSegmentEnergyIgnoresDegenerateInput())
  {
    return 57;
  }
  if (!TestDivergenceMacdRequiresEnergyData())
  {
    return 58;
  }
  if (!TestApplyTradingQualityKeepsWinnerQuality())
  {
    return 59;
  }
  if (!TestFunc10HandlesEmptyInput())
  {
    return 60;
  }
  if (!TestFunc10MatchesFunc5SignalBars())
  {
    return 61;
  }
  if (!TestCenterAccumulatesGGDD())
  {
    return 62;
  }
  if (!TestCenterExtendUpdatesGGDD())
  {
    return 63;
  }
  if (!TestClassifyCenterRelationUp())
  {
    return 64;
  }
  if (!TestClassifyCenterRelationDown())
  {
    return 65;
  }
  if (!TestClassifyCenterRelationExtension())
  {
    return 66;
  }
  if (!TestClassifyCenterRelationExtensionAtFullExtentBoundary())
  {
    return 173;
  }
  if (!TestClassifyCenterLifecycleExtension())
  {
    return 196;
  }
  if (!TestClassifyCenterLifecycleExpansion())
  {
    return 197;
  }
  if (!TestClassifyCenterLifecycleNewborn())
  {
    return 198;
  }
  if (!TestWriteCenterRelationSignalMarks())
  {
    return 67;
  }
  if (!TestWriteCenterLifecycleSignalMarks())
  {
    return 199;
  }
  if (!TestFunc11WritesCenterRelation())
  {
    return 68;
  }
  if (!TestFunc11HandlesEmptyInput())
  {
    return 69;
  }
  if (!TestReversalStrengthExtension())
  {
    return 70;
  }
  if (!TestReversalStrengthConsolidation())
  {
    return 71;
  }
  if (!TestReversalStrengthTrend())
  {
    return 72;
  }
  if (!TestReversalStrengthSell())
  {
    return 73;
  }
  if (!TestReversalStrengthUnknownNoRebound())
  {
    return 74;
  }
  if (!TestFunc12WritesReversalCode())
  {
    return 75;
  }
  if (!TestCenterAftermathExtended())
  {
    return 76;
  }
  if (!TestCenterAftermathNewborn())
  {
    return 77;
  }
  if (!TestCenterAftermathUnknownNoNext())
  {
    return 78;
  }
  if (!TestCenterAftermathSell())
  {
    return 79;
  }
  if (!TestCenterAftermathRequiresThirdSignal())
  {
    return 167;
  }
  if (!TestApplyTradingAftermathMapsCodes())
  {
    return 80;
  }
  if (!TestApplyTradingCenterPositionMapsCodes())
  {
    return 144;
  }
  if (!TestApplyTradingMovementTypeMapsCodes())
  {
    return 145;
  }
  if (!TestApplyTradingPriorityMapsCodes())
  {
    return 146;
  }
  if (!TestApplyTradingCenterIdMapsCodes())
  {
    return 147;
  }
  if (!TestApplyTradingBreakoutIdMapsCodes())
  {
    return 148;
  }
  if (!TestApplyTradingBreakoutPointIdsMapCodes())
  {
    return 189;
  }
  if (!TestApplyTradingPointIdMapsCodes())
  {
    return 149;
  }
  if (!TestApplyTradingTrendIdMapsCodes())
  {
    return 150;
  }
  if (!TestApplyTradingSecondContextPointIdsMapCodes())
  {
    return 193;
  }
  if (!TestApplyTradingSmallTurnMapsCodes())
  {
    return 128;
  }
  if (!TestApplyTradingSmallTurnPointIdsMapCodes())
  {
    return 192;
  }
  if (!TestApplyTradingAbcStructureMapsCodes())
  {
    return 131;
  }
  if (!TestApplyTradingAbcBreakoutIdMapsCodes())
  {
    return 190;
  }
  if (!TestApplyTradingAbcBreakoutPointIdsMapCodes())
  {
    return 191;
  }
  if (!TestApplyTradingStrictAbcFiltersFirstSignals())
  {
    return 132;
  }
  if (!TestApplyTradingMacdLineWeaknessMapsCodes())
  {
    return 137;
  }
  if (!TestApplyTradingMacdZeroPullbackMapsCodes())
  {
    return 139;
  }
  if (!TestApplyTradingStandardDivergenceMapsCodes())
  {
    return 140;
  }
  if (!TestApplyTradingMacdAreaRatioMapsCodes())
  {
    return 184;
  }
  if (!TestApplyTradingStrengthRatiosMapCodes())
  {
    return 185;
  }
  if (!TestApplyTradingDivergenceFlagsMapCodes())
  {
    return 186;
  }
  if (!TestApplyTradingDivergencePointIdsMapCodes())
  {
    return 196;
  }
  if (!TestBuildDivergenceFlagsMapsBits())
  {
    return 187;
  }
  if (!TestApplyTradingContextFlagsMapsCodes())
  {
    return 141;
  }
  if (!TestApplyTradingContextFlagsUsesWinningPriority())
  {
    return 142;
  }
  if (!TestApplyTradingDivergenceSemanticMapsCodes())
  {
    return 197;
  }
  if (!TestApplyTradingDivergenceSemanticUsesWinningPriority())
  {
    return 198;
  }
  if (!TestApplyTradingFilterReasonsMapsCodes())
  {
    return 205;
  }
  if (!TestBuildTradingFilterReasonsMarksNoTrend())
  {
    return 206;
  }
  if (!TestBuildTradingFilterReasonsMarksSecondOrder())
  {
    return 207;
  }
  if (!TestBuildTradingFilterReasonsMarksThirdRetestFailures())
  {
    return 208;
  }
  if (!TestBuildTradingFilterReasonsMarksAbcNotAligned())
  {
    return 209;
  }
  if (!TestNestedDivergenceMarksLowerSegmentInsideHigher())
  {
    return 133;
  }
  if (!TestNestedDivergenceMarksSellDirection())
  {
    return 134;
  }
  if (!TestNestedDivergenceRequiresFirstSignalCode())
  {
    return 169;
  }
  if (!TestNestedDivergenceRequiresTrendExtreme())
  {
    return 200;
  }
  if (!TestNestedDivergenceContextOutputs())
  {
    return 201;
  }
  if (!TestFunc13HandlesEmptyInput())
  {
    return 81;
  }
  if (!TestSecondBuyConsolidationDivergence())
  {
    return 82;
  }
  if (!TestSecondBuyStrongPullbackConfirmed())
  {
    return 83;
  }
  if (!TestFunc14MarksDivergenceSegment())
  {
    return 84;
  }
  if (!TestFunc14HandlesEmptyInput())
  {
    return 85;
  }
  if (!TestMovingAverageComputesSma())
  {
    return 86;
  }
  if (!TestMaKissClassifiesThreeTypes())
  {
    return 87;
  }
  if (!TestFunc15WritesMaDiff())
  {
    return 88;
  }
  if (!TestFunc15Func16HandleEmptyInput())
  {
    return 89;
  }
  if (!TestInstantDivergenceWarnsWeakNewLow())
  {
    return 90;
  }
  if (!TestInstantDivergenceSkipsStrongLeg())
  {
    return 91;
  }
  if (!TestFunc17HandlesEmptyInput())
  {
    return 92;
  }
  if (!TestStrictStrokeUsesMergedGap())
  {
    return 93;
  }
  if (!TestFeatureLineSegmentEndsAtTopFractal())
  {
    return 94;
  }
  if (!TestFeatureLineSegmentNeedsFourPoints())
  {
    return 95;
  }
  if (!TestFeatureLineSegmentRequiresFirstThreeOverlap())
  {
    return 123;
  }
  if (!TestDecodeConfig())
  {
    return 96;
  }
  if (!TestStrokeEndConfig())
  {
    return 97;
  }
  if (!TestConfiguredPointsCenterUnit())
  {
    return 98;
  }
  if (!TestFunc20DrivesConfig())
  {
    return 99;
  }
  if (!TestFunc20FeatureSegmentModeMatchesFunc19())
  {
    return 166;
  }
  if (!TestFunc20HandlesEmptyInput())
  {
    return 100;
  }
  if (!TestAnalyzerFromSignalAggregates())
  {
    return 101;
  }
  if (!TestSignalCacheHitAndInvalidate())
  {
    return 102;
  }
  if (!TestFunc30MatchesLegacyPipeline())
  {
    return 103;
  }
  if (!TestFunc30FeatureSegmentModeMatchesFunc19())
  {
    return 127;
  }
  if (!TestFunc30DiagnosticOutputsMatchProjections())
  {
    return 143;
  }
  if (!TestFunc30RejectsInvalidMode())
  {
    return 180;
  }
  if (!TestFunc30HandlesEmptyInput())
  {
    return 104;
  }
  if (!TestInvalidPriceSanitized())
  {
    return 105;
  }
  if (!TestAuxDataValidation())
  {
    return 106;
  }
  if (!TestAuxCloseAffectsEnergy())
  {
    return 107;
  }
  if (!TestFunc30AuxCloseInvalidatesPriceCache())
  {
    return 194;
  }
  if (!TestSignalCacheInvalidatesOnAuxClose())
  {
    return 195;
  }
  if (!TestFunc40Registers())
  {
    return 108;
  }
  if (!TestKissVolumeTrap())
  {
    return 109;
  }
  if (!TestStrokeRelayExtendsUp())
  {
    return 110;
  }
  if (!TestStrokeRelayExtendsDown())
  {
    return 111;
  }
  if (!TestStrokeBrokenByNewExtreme())
  {
    return 112;
  }
  if (!TestFeatureLineSegmentEndsAtBottomFractal())
  {
    return 113;
  }
  if (!TestFeatureSegmentGapConfirmedByReversal())
  {
    return 114;
  }
  if (!TestFeatureSegmentGapRequiresReversalFractal())
  {
    return 189;
  }
  if (!TestFeatureSegmentGapConfirmationStartsNextSegment())
  {
    return 201;
  }
  if (!TestFeatureSegmentGapWithoutReverseFractalKeepsOldSegment())
  {
    return 202;
  }
  if (!TestApplyTradingReversalRequiresFirstSignal())
  {
    return 168;
  }
  if (!TestApplyTradingReversalPointIdMapsCodes())
  {
    return 199;
  }

  return 0;
}
