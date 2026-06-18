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
  if (!TestStrengthAndSlopeUsePreviousExtremes())
  {
    return 2;
  }
  if (!TestEmptyInputReturns())
  {
    return 3;
  }

  return 0;
}
