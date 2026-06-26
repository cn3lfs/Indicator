#include "../CzscCore.h"

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
  Top.fHigh = 12;
  Top.fLow = 7;

  Fractal Bottom;
  Bottom.nType = CZSC_POINT_BOTTOM;
  Bottom.nIndex = 3;
  Bottom.fHigh = 10;
  Bottom.fLow = 4;

  std::vector<Fractal> Fractals;
  Fractals.push_back(Top);
  Fractals.push_back(Bottom);

  std::vector<Stroke> Strokes = BuildStrokes(Fractals);

  return Strokes.empty();
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
  F.fHigh = fHigh;
  F.fLow = fLow;
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
  C.nQuality = CZSC_SIGNAL_QUALITY_WATCH;
  C.nCenterPosition = CZSC_CENTER_POSITION_UNKNOWN;
  C.nReversal = CZSC_REVERSAL_UNKNOWN;
  C.bOverlapped = false;
  C.Divergence.nDirection = 0;
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
  pHigh[16] = 3;
  pLow[16] = 3;
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
  std::vector<Center> Centers = BuildCenters(Points);
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts = BuildCenterBreakouts(Points, Centers, Structures);
  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);

  return HasSignalCandidate(Candidates, 32, 1.0f) &&
         HasSignalCandidate(Candidates, 40, 2.0f);
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
  pHigh[16] = 3;
  pLow[16] = 3;
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
  std::vector<Center> Centers = BuildCenters(Points);
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
         (pFirst->nCenterPosition == CZSC_CENTER_POSITION_BELOW);
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
         (pThird->nCenterPosition == CZSC_CENTER_POSITION_ABOVE);
}

static bool TestTradingCandidatesMarkSecondThirdBuyOverlap()
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
  pHigh[16] = 3;
  pLow[16] = 3;
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
  std::vector<Center> Centers = BuildCenters(Points);
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(1, 10));

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pSecond = FindSignalCandidate(Candidates, 40, 2.0f);
  ApplyTradingSignalCandidates(nCount, pOut, Candidates);

  return (pSecond != 0) &&
         pSecond->bOverlapped &&
         (pSecond->nBreakout == 0) &&
         (pSecond->Divergence.nDirection == 1) &&
         (pSecond->nQuality == CZSC_SIGNAL_QUALITY_CONFIRMED) &&
         (pSecond->nCenterPosition == CZSC_CENTER_POSITION_ABOVE) &&
         NearlyEqual(pOut[40], 3.0f);
}

static bool TestTradingCandidatesMarkSecondThirdSellOverlap()
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
  pHigh[16] = 14;
  pLow[16] = 14;
  pIn[20] = -1;
  pHigh[20] = 10;
  pLow[20] = 10;
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
  std::vector<Center> Centers = BuildCenters(Points);
  std::vector<TrendStructure> Structures = BuildTrendStructures(Centers);
  std::vector<CenterBreakout> Breakouts;
  Breakouts.push_back(MakeTestBreakout(-1, 10));

  std::vector<TradingSignalCandidate> Candidates =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pSecond = FindSignalCandidate(Candidates, 40, 12.0f);
  ApplyTradingSignalCandidates(nCount, pOut, Candidates);

  return (pSecond != 0) &&
         pSecond->bOverlapped &&
         (pSecond->nBreakout == 0) &&
         (pSecond->Divergence.nDirection == -1) &&
         (pSecond->nQuality == CZSC_SIGNAL_QUALITY_CONFIRMED) &&
         (pSecond->nCenterPosition == CZSC_CENTER_POSITION_BELOW) &&
         NearlyEqual(pOut[40], 13.0f);
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
  pHigh[16] = 3;
  pLow[16] = 3;
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
  std::vector<Center> Centers = BuildCenters(Points);
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

static bool TestFunc9WritesLineSegmentSignal()
{
  const int nCount = 21;
  float pHigh[nCount] = {
    5, 7, 8, 9, 10,
    9, 8, 7, 7,
    8, 9, 11, 12,
    11, 10, 9, 9,
    10, 11, 13, 14
  };
  float pLow[nCount] = {
    1, 2, 3, 5, 6,
    5, 4, 3, 3,
    4, 5, 7, 8,
    7, 6, 5, 5,
    6, 7, 9, 10
  };
  float pOut[nCount];
  float fTime = 5;

  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -99;
  }

  Func9(nCount, pOut, pHigh, pLow, &fTime);

  for (int i = 0; i < nCount; i++)
  {
    float fExpected = 0;
    if (i == 4)
    {
      fExpected = 1;
    }
    else if (i == 16)
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

static bool TestCentersUseThreeOverlappingSegments()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));

  std::vector<Center> Centers = BuildCenters(Points);

  if (Centers.size() != 1)
  {
    return false;
  }
  if ((Centers[0].nStart != 0) || (Centers[0].nEnd != 12))
  {
    return false;
  }
  if (!NearlyEqual(Centers[0].fHigh, 9.0f) || !NearlyEqual(Centers[0].fLow, 4.0f))
  {
    return false;
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

static bool TestCentersSplitWhenOverlapBreaks()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 12));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 16, 11));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 20, 14));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 24, 12));

  std::vector<Center> Centers = BuildCenters(Points);

  if (Centers.size() != 2)
  {
    return false;
  }
  if ((Centers[0].nStart != 0) || (Centers[0].nEnd != 12))
  {
    return false;
  }
  if ((Centers[1].nStart != 12) || (Centers[1].nEnd != 24))
  {
    return false;
  }
  if (!NearlyEqual(Centers[0].fHigh, 10.0f) || !NearlyEqual(Centers[0].fLow, 4.0f))
  {
    return false;
  }
  if (!NearlyEqual(Centers[1].fHigh, 12.0f) || !NearlyEqual(Centers[1].fLow, 12.0f))
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

  Func2(nCount, pOut, pIn, pHigh, pLow);
  for (int i = 0; i < nCount; i++)
  {
    if (!NearlyEqual(pOut[i], 9.0f))
    {
      return false;
    }
  }

  Func3(nCount, pOut, pIn, pHigh, pLow);
  for (int i = 0; i < nCount; i++)
  {
    if (!NearlyEqual(pOut[i], 5.0f))
    {
      return false;
    }
  }

  Func4(nCount, pOut, pIn, pHigh, pLow);
  for (int i = 0; i < nCount; i++)
  {
    float fExpected = 0;
    if (i == 0)
    {
      fExpected = 1;
    }
    else if (i == 16)
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
  pHigh[16] = 3;
  pLow[16] = 3;
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

  for (int i = 0; i < nCount; i++)
  {
    if ((i == 32) && !NearlyEqual(pOut[i], 1.0f))
    {
      return false;
    }
    if ((i != 32) && NearlyEqual(pOut[i], 1.0f))
    {
      return false;
    }
  }

  return true;
}

static bool TestFunc5WritesCenterThirdBuy()
{
  const int nCount = 21;
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
  pHigh[4] = 1;
  pLow[4] = 1;
  pIn[8] = 1;
  pHigh[8] = 7;
  pLow[8] = 7;
  pIn[12] = -1;
  pHigh[12] = 8;
  pLow[12] = 8;
  pIn[16] = 1;
  pHigh[16] = 9;
  pLow[16] = 9;
  pIn[20] = -1;
  pHigh[20] = 7.5f;
  pLow[20] = 7.5f;

  Func5(nCount, pOut, pIn, pHigh, pLow);

  return NearlyEqual(pOut[20], 3.0f);
}

static bool TestFunc5WritesCenterThirdSell()
{
  const int nCount = 21;
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
  pHigh[4] = 10;
  pLow[4] = 10;
  pIn[8] = -1;
  pHigh[8] = 4;
  pLow[8] = 4;
  pIn[12] = 1;
  pHigh[12] = 3;
  pLow[12] = 3;
  pIn[16] = -1;
  pHigh[16] = 2;
  pLow[16] = 2;
  pIn[20] = 1;
  pHigh[20] = 3.5f;
  pLow[20] = 3.5f;

  Func5(nCount, pOut, pIn, pHigh, pLow);

  return NearlyEqual(pOut[20], 13.0f);
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
  pHigh[16] = 3;
  pLow[16] = 3;
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
  pHigh[16] = 14;
  pLow[16] = 14;
  pIn[20] = -1;
  pHigh[20] = 10;
  pLow[20] = 10;
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
  pHigh[16] = 14;
  pLow[16] = 14;
  pIn[20] = -1;
  pHigh[20] = 10;
  pLow[20] = 10;
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
  pHigh[16] = 3;
  pLow[16] = 3;
  pIn[20] = 1;
  pHigh[20] = 7;
  pLow[20] = 7;
  pIn[24] = -1;
  pHigh[24] = 4;
  pLow[24] = 4;
  pIn[28] = 1;
  pHigh[28] = 8;
  pLow[28] = 8;
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

static bool TestStrengthMetricsUseMacdEnergy()
{
  SegmentPoint Start = MakeTestEnergyPoint(CZSC_POINT_TOP, 2, 10, 100);
  SegmentPoint End = MakeTestEnergyPoint(CZSC_POINT_BOTTOM, 7, 4, 70);

  StrengthMetrics Strength = MeasureStrength(Start, End);

  return NearlyEqual(Strength.fMacdArea, 30.0f) &&
         NearlyEqual(Strength.fSpace, 6.0f) &&
         NearlyEqual(Strength.fSpeed, 1.2f);
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
  pHigh[16] = 3;
  pLow[16] = 3;
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
  pHigh[16] = 3;
  pLow[16] = 3;
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

  for (int i = 0; i < nCount; i++)
  {
    float fExpected = (i == 32) ? (float)CZSC_SIGNAL_QUALITY_STRONG : 0.0f;
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

  // 末段（点4→点5）价差更大但速度更慢：几何上仅速度走弱，质量只能到 CONFIRMED。
  std::vector<TradingSignalCandidate> Plain =
    BuildTradingSignalCandidates(Points, Centers, Structures, Breakouts);
  const TradingSignalCandidate *pPlain = FindSignalCandidate(Plain, 30, 1.0f);
  if ((pPlain == 0) ||
      pPlain->Divergence.bWeakSpace ||
      !pPlain->Divergence.bWeakSpeed ||
      pPlain->Divergence.bWeakMacd ||
      (pPlain->nQuality != CZSC_SIGNAL_QUALITY_CONFIRMED))
  {
    return false;
  }

  // 注入末段 MACD 面积小于前段：第24课标准背驰，质量升级为 STRONG。
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
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 0, 1));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 4, 10));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 8, 4));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 12, 9));

  std::vector<Center> Centers = BuildCenters(Points);
  if (Centers.size() != 1)
  {
    return false;
  }

  // ZG/ZD 为重叠区间[4,9]，GG/DD 为全幅极值[1,10]
  return NearlyEqual(Centers[0].fHigh, 9.0f) && NearlyEqual(Centers[0].fLow, 4.0f) &&
         NearlyEqual(Centers[0].fTop, 10.0f) && NearlyEqual(Centers[0].fBottom, 1.0f);
}

static bool TestCenterExtendUpdatesGGDD()
{
  std::vector<SegmentPoint> Points;
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 0, 8));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 4, 5));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 8, 9));
  Points.push_back(MakeTestPoint(CZSC_POINT_BOTTOM, 12, 6));
  Points.push_back(MakeTestPoint(CZSC_POINT_TOP, 16, 10));

  std::vector<Center> Centers = BuildCenters(Points);
  if (Centers.size() != 1)
  {
    return false;
  }

  // 延伸段(高点10)把 GG 从初始 9 扩张到 10，而 ZG/ZD 仍是收缩后的重叠区间
  return (Centers[0].nEnd == 16) &&
         NearlyEqual(Centers[0].fHigh, 8.0f) && NearlyEqual(Centers[0].fLow, 6.0f) &&
         NearlyEqual(Centers[0].fTop, 10.0f) && NearlyEqual(Centers[0].fBottom, 5.0f);
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

static bool TestFunc11WritesCenterRelation()
{
  const int nCount = 25;
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

  // 两个相邻中枢(同 TestCentersSplitWhenOverlapBreaks)，全幅区间重叠 → 中枢扩展
  pIn[0] = -1;
  pHigh[0] = pLow[0] = 1;
  pIn[4] = 1;
  pHigh[4] = pLow[4] = 10;
  pIn[8] = -1;
  pHigh[8] = pLow[8] = 4;
  pIn[12] = 1;
  pHigh[12] = pLow[12] = 12;
  pIn[16] = -1;
  pHigh[16] = pLow[16] = 11;
  pIn[20] = 1;
  pHigh[20] = pLow[20] = 14;
  pIn[24] = -1;
  pHigh[24] = pLow[24] = 12;

  Func11(nCount, pOut, pIn, pHigh, pLow);

  for (int i = 0; i < nCount; i++)
  {
    float fExpected = (i == 12) ? 2.0f : 0.0f;  // 后中枢起点(index12)标记扩展
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
  pHigh[36] = pLow[36] = 6;
  pIn[40] = -1;
  pHigh[40] = pLow[40] = 4.5f;

  Func12(nCount, pOut, pIn, pHigh, pLow);

  // 一买(index32)带 1/2/3 的背驰-转折编码；二买(index40)与无信号处为 0
  bool bFirstCoded = (pOut[32] >= 0.9f) && (pOut[32] <= 3.1f);
  return bFirstCoded && NearlyEqual(pOut[40], 0.0f) && NearlyEqual(pOut[0], 0.0f);
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
  if (!TestFractalsAndStrokes())
  {
    return 3;
  }
  if (!TestStrokeRequiresFiveBars())
  {
    return 4;
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
  if (!TestCenterBreakoutsDetectThirdBuy())
  {
    return 17;
  }
  if (!TestCenterBreakoutsDetectThirdSell())
  {
    return 18;
  }
  if (!TestCenterBreakoutsUseFirstRetestOnly())
  {
    return 19;
  }
  if (!TestCenterBreakoutsSkipBackIntoCenter())
  {
    return 20;
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
  if (!TestFirstCandidateKeepsTrendDivergence())
  {
    return 30;
  }
  if (!TestThirdCandidateKeepsBreakoutDivergence())
  {
    return 31;
  }
  if (!TestTradingCandidatesMarkSecondThirdBuyOverlap())
  {
    return 32;
  }
  if (!TestTradingCandidatesMarkSecondThirdSellOverlap())
  {
    return 33;
  }
  if (!TestTradingCandidatesMarkSecondBuyInsideCenter())
  {
    return 34;
  }
  if (!TestFunc9WritesLineSegmentSignal())
  {
    return 35;
  }
  if (!TestCentersUseThreeOverlappingSegments())
  {
    return 36;
  }
  if (!TestCenterExtendsWithOverlappingSegment())
  {
    return 37;
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
  if (!TestStrengthMetricsUseMacdEnergy())
  {
    return 52;
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
  if (!TestWriteCenterRelationSignalMarks())
  {
    return 67;
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

  return 0;
}
