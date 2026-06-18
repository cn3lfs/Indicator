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
  return Point;
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
  if (!TestFunc9WritesLineSegmentSignal())
  {
    return 8;
  }
  if (!TestCentersUseThreeOverlappingSegments())
  {
    return 9;
  }
  if (!TestCenterExtendsWithOverlappingSegment())
  {
    return 10;
  }
  if (!TestCentersSplitWhenOverlapBreaks())
  {
    return 11;
  }
  if (!TestCenterFunctionsWriteSignals())
  {
    return 12;
  }
  if (!TestFunc5WritesTrendDivergenceFirstBuy())
  {
    return 13;
  }
  if (!TestFunc5WritesTrendDivergenceFirstSell())
  {
    return 14;
  }
  if (!TestFunc5SkipsStrongNewLow())
  {
    return 15;
  }
  if (!TestStrengthAndSlopeUsePreviousExtremes())
  {
    return 16;
  }
  if (!TestEmptyInputReturns())
  {
    return 17;
  }

  return 0;
}
