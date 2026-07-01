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

// 动力学：MACD 柱子面积（第24课「C段面积小于A段即背驰」的能量基础）
//=============================================================================

// 标准 TDX EMA：EMA(i) = price(i) * k + EMA(i-1) * (1-k)，k = 2/(period+1)
static std::vector<float> ComputeEma(int nCount, const float *pPrice, int nPeriod)
{
  std::vector<float> Ema;
  if ((nCount <= 0) || (pPrice == 0) || (nPeriod <= 0))
  {
    return Ema;
  }

  Ema.resize((std::size_t)nCount);
  float fK = 2.0f / (float)(nPeriod + 1);
  Ema[0] = pPrice[0];
  for (int i = 1; i < nCount; i++)
  {
    Ema[(std::size_t)i] = pPrice[i] * fK + Ema[(std::size_t)(i - 1)] * (1.0f - fK);
  }
  return Ema;
}

struct MacdComponents
{
  std::vector<float> Dif;
  std::vector<float> Dea;
  std::vector<float> Histogram;
};

static MacdComponents ComputeMacdComponents(int nCount, const float *pPrice)
{
  MacdComponents M;
  if ((nCount <= 0) || (pPrice == 0))
  {
    return M;
  }

  std::vector<float> Fast = ComputeEma(nCount, pPrice, 12);
  std::vector<float> Slow = ComputeEma(nCount, pPrice, 26);

  M.Dif.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    M.Dif[(std::size_t)i] = Fast[(std::size_t)i] - Slow[(std::size_t)i];
  }

  M.Dea = ComputeEma(nCount, &M.Dif[0], 9);

  M.Histogram.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    M.Histogram[(std::size_t)i] = (M.Dif[(std::size_t)i] - M.Dea[(std::size_t)i]) * 2.0f;
  }
  return M;
}

// 标准 MACD 柱：DIF = EMA12 - EMA26，DEA = EMA(DIF,9)，柱 = (DIF - DEA) * 2
std::vector<float> ComputeMacdHistogram(int nCount, const float *pPrice)
{
  return ComputeMacdComponents(nCount, pPrice).Histogram;
}

// 给每个线段点赋累积 MACD 柱面积，使任一走势段的能量 = 终点能量 - 起点能量。
// TDX 的 Func5 只传入 H/L（无收盘价），故以 (H+L)/2 作为收盘价代理计算 MACD。
void AssignSegmentEnergy(std::vector<SegmentPoint> &Points, int nCount, const float *pHigh, const float *pLow)
{
  if ((nCount <= 0) || (pHigh == 0) || (pLow == 0))
  {
    return;
  }

  std::vector<float> High = SanitizeSeries(nCount, pHigh);
  std::vector<float> Low = SanitizeSeries(nCount, pLow);
  const std::vector<float> *pClose = GetValidatedClose(nCount, pHigh, pLow);  // 有真实收盘价则用之
  std::vector<float> Price;
  Price.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Price[(std::size_t)i] = pClose ? (*pClose)[(std::size_t)i]
                                   : (High[(std::size_t)i] + Low[(std::size_t)i]) * 0.5f;
  }

  MacdComponents Macd = ComputeMacdComponents(nCount, &Price[0]);
  if (Macd.Histogram.empty())
  {
    return;
  }

  std::vector<float> Cumulative;
  Cumulative.resize((std::size_t)nCount);
  float fAccumulator = 0;
  for (int i = 0; i < nCount; i++)
  {
    fAccumulator += Macd.Histogram[(std::size_t)i];
    Cumulative[(std::size_t)i] = fAccumulator;
  }

  for (std::size_t i = 0; i < Points.size(); i++)
  {
    int nIndex = Points[i].nIndex;
    if ((nIndex >= 0) && (nIndex < nCount))
    {
      Points[i].fEnergy = Cumulative[(std::size_t)nIndex];
      Points[i].fDif = Macd.Dif[(std::size_t)nIndex];
      Points[i].fDea = Macd.Dea[(std::size_t)nIndex];
    }
  }
}

//=============================================================================
// 均线系统（第11-15课）：简单移动平均 + 吻分类（飞吻/唇吻/湿吻）
//=============================================================================

static const int   MA_SHORT_PERIOD  = 5;
static const int   MA_LONG_PERIOD   = 20;
static const float MA_LIP_THRESHOLD = 0.01f;  // 唇吻：短长均线相对间距 < 1% 视为贴近

static const int   VOL_MA_PERIOD     = 10;    // 成交量均线周期（放量基线）
static const int   VOL_TRAP_LOOKBACK = 3;     // 湿吻前若干根（含当根）取近端平均量
static const float VOL_TRAP_RATIO    = 1.5f;  // 近端量 > 量均线*该倍数 视为放量（骗线嫌疑）

// 简单移动平均；序列开头不足一个周期时用已有数据的部分窗口平均
std::vector<float> ComputeMovingAverage(int nCount, const float *pPrice, int nPeriod)
{
  std::vector<float> Ma;
  if ((nCount <= 0) || (pPrice == 0) || (nPeriod <= 0))
  {
    return Ma;
  }

  Ma.resize((std::size_t)nCount);
  float fSum = 0;
  for (int i = 0; i < nCount; i++)
  {
    fSum += pPrice[i];
    if (i >= nPeriod)
    {
      fSum -= pPrice[i - nPeriod];
    }
    int nWindow = (i + 1 < nPeriod) ? (i + 1) : nPeriod;
    Ma[(std::size_t)i] = fSum / (float)nWindow;
  }
  return Ma;
}

// 在短长均线间距的每个局部极小处判一次「吻」：升破/跌破=湿吻，贴近不破=唇吻，略走平=飞吻
std::vector<int> ClassifyMaKisses(const std::vector<float> &Short, const std::vector<float> &Long)
{
  std::size_t n = Short.size();
  std::vector<int> Kiss;
  Kiss.assign(n, CZSC_KISS_NONE);
  if ((n < 3) || (Long.size() != n))
  {
    return Kiss;
  }

  for (std::size_t i = 1; i + 1 < n; i++)
  {
    float fPrev = Short[i - 1] - Long[i - 1];
    float fCurr = Short[i] - Long[i];
    float fNext = Short[i + 1] - Long[i + 1];
    float fAbsPrev = AbsF(fPrev);
    float fAbsCurr = AbsF(fCurr);
    float fAbsNext = AbsF(fNext);

    // 间距先收窄后放大 → 这一根是一次吻
    if ((fAbsCurr < fAbsPrev) && (fAbsCurr <= fAbsNext))
    {
      bool bCross = ((fPrev > 0) != (fCurr > 0)) || ((fCurr > 0) != (fNext > 0));
      float fBase = AbsF(Long[i]);
      float fRel = (fBase > 0) ? (fAbsCurr / fBase) : fAbsCurr;
      if (bCross)
      {
        Kiss[i] = CZSC_KISS_WET;
      }
      else if (fRel < MA_LIP_THRESHOLD)
      {
        Kiss[i] = CZSC_KISS_LIP;
      }
      else
      {
        Kiss[i] = CZSC_KISS_FLY;
      }
    }
  }
  return Kiss;
}

// 在纯价吻基础上用成交量校验湿吻（第12课）：转折处的湿吻若其前 N 根近端平均量显著高于成交量均线
// （放量），多为诱多/诱空的骗线 → 标 CZSC_KISS_WET_TRAP。无成交量（空）时退化为纯价吻分类。
std::vector<int> ClassifyMaKissesWithVolume(const std::vector<float> &Short, const std::vector<float> &Long,
                                            const std::vector<float> &Volume)
{
  std::vector<int> Kiss = ClassifyMaKisses(Short, Long);
  std::size_t n = Kiss.size();
  if ((n == 0) || (Volume.size() != n))
  {
    return Kiss;  // 无量或长度不符 → 退化为纯价吻
  }

  std::vector<float> VolMa = ComputeMovingAverage((int)n, &Volume[0], VOL_MA_PERIOD);
  for (std::size_t i = 0; i < n; i++)
  {
    if (Kiss[i] != CZSC_KISS_WET)
    {
      continue;  // 仅校验湿吻（转折信号）
    }
    // 近端平均量（湿吻当根及前若干根）
    float fRecent = 0;
    int nLook = 0;
    for (int k = (int)i; (k >= 0) && (k > (int)i - VOL_TRAP_LOOKBACK); k--)
    {
      fRecent += Volume[(std::size_t)k];
      nLook++;
    }
    fRecent /= (nLook > 0) ? (float)nLook : 1.0f;

    float fMa = VolMa[i];
    if ((fMa > 0) && (fRecent > fMa * VOL_TRAP_RATIO))
    {
      Kiss[i] = CZSC_KISS_WET_TRAP;  // 放量湿吻 → 骗线嫌疑
    }
  }
  return Kiss;
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

StrengthMetrics MeasureStrength(const SegmentPoint &Start, const SegmentPoint &End)
{
  StrengthMetrics Strength;
  Strength.fSpace = GetPointPrice(End) - GetPointPrice(Start);
  if (Strength.fSpace < 0)
  {
    Strength.fSpace = -Strength.fSpace;
  }
  Strength.fSpeed = GetMovePower(Start, End);
  Strength.fDifHeight = End.fDif - Start.fDif;
  if (Strength.fDifHeight < 0)
  {
    Strength.fDifHeight = -Strength.fDifHeight;
  }
  Strength.fDeaHeight = End.fDea - Start.fDea;
  if (Strength.fDeaHeight < 0)
  {
    Strength.fDeaHeight = -Strength.fDeaHeight;
  }

  // 走势段的 MACD 能量 = 区间累积柱面积之差的绝对值（上涨看红柱、下跌看绿柱）。
  Strength.fMacdArea = End.fEnergy - Start.fEnergy;
  if (Strength.fMacdArea < 0)
  {
    Strength.fMacdArea = -Strength.fMacdArea;
  }

  Strength.bRsiDivergence = false;
  return Strength;
}

bool IsWeakerStrength(const StrengthMetrics &Current, const StrengthMetrics &Previous)
{
  return (Current.fSpace < Previous.fSpace) || (Current.fSpeed < Previous.fSpeed);
}

DivergenceResult MeasureDivergence(const SegmentPoint &PrevStart,
                                   const SegmentPoint &PrevEnd,
                                   const SegmentPoint &CurrentStart,
                                   const SegmentPoint &CurrentEnd,
                                   int nDirection)
{
  DivergenceResult Result;
  Result.nDirection = nDirection;
  Result.nPreviousStartPoint = -1;
  Result.nPreviousEndPoint = -1;
  Result.nCurrentStartPoint = -1;
  Result.nCurrentEndPoint = -1;
  Result.Previous = MeasureStrength(PrevStart, PrevEnd);
  Result.Current = MeasureStrength(CurrentStart, CurrentEnd);
  Result.bWeakSpace = Result.Current.fSpace < Result.Previous.fSpace;
  Result.bWeakSpeed = Result.Current.fSpeed < Result.Previous.fSpeed;
  // 第24课标准背驰：C段 MACD 柱面积小于 A段。仅在两段都有能量数据时成立。
  Result.bWeakMacd = (Result.Current.fMacdArea > 0) &&
                     (Result.Previous.fMacdArea > 0) &&
                     (Result.Current.fMacdArea < Result.Previous.fMacdArea);
  Result.bNewExtreme = false;
  if (nDirection > 0)
  {
    Result.bNewExtreme = CurrentEnd.fHigh > PrevEnd.fHigh;
  }
  else if (nDirection < 0)
  {
    Result.bNewExtreme = CurrentEnd.fLow < PrevEnd.fLow;
  }
  Result.bDivergence = Result.bNewExtreme &&
                       ((Result.bWeakSpace && Result.bWeakSpeed) || Result.bWeakMacd);
  return Result;
}

void ComputeShortLongMa(int nCount, float *pHigh, float *pLow,
                        std::vector<float> *pShort, std::vector<float> *pLong)
{
  if (nCount <= 0)
  {
    pShort->clear();
    pLong->clear();
    return;
  }

  std::vector<float> High = SanitizeSeries(nCount, pHigh);
  std::vector<float> Low = SanitizeSeries(nCount, pLow);
  const std::vector<float> *pClose = GetValidatedClose(nCount, pHigh, pLow);  // 有真实收盘价则用之
  std::vector<float> Price;
  Price.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Price[(std::size_t)i] = pClose ? (*pClose)[(std::size_t)i]
                                   : (High[(std::size_t)i] + Low[(std::size_t)i]) * 0.5f;
  }
  *pShort = ComputeMovingAverage(nCount, &Price[0], MA_SHORT_PERIOD);
  *pLong = ComputeMovingAverage(nCount, &Price[0], MA_LONG_PERIOD);
}

int DetectInstantDivergence(const std::vector<SegmentPoint> &Points,
                            int nCount,
                            const float *pHigh,
                            const float *pLow)
{
  if ((Points.size() < 4) || (nCount <= 1) || (pHigh == 0) || (pLow == 0))
  {
    return 0;
  }

  const SegmentPoint &Pivot = Points.back();
  int nLast = Pivot.nIndex;
  if ((nLast < 0) || (nLast >= nCount - 1))
  {
    return 0;  // 末段至少要有一根新K线
  }

  int nDir = (Pivot.nType == CZSC_POINT_TOP) ? -1 : 1;

  // 末段当下的合成终点：向下取区间最低、向上取区间最高
  SegmentPoint Now;
  Now.nIndex = nCount - 1;
  Now.fEnergy = 0;
  Now.fDif = 0;
  Now.fDea = 0;
  if (nDir < 0)
  {
    float fLowest = pLow[nLast];
    for (int i = nLast; i < nCount; i++)
    {
      if (pLow[i] < fLowest)
      {
        fLowest = pLow[i];
      }
    }
    Now.nType = CZSC_POINT_BOTTOM;
    Now.fHigh = fLowest;
    Now.fLow = fLowest;
  }
  else
  {
    float fHighest = pHigh[nLast];
    for (int i = nLast; i < nCount; i++)
    {
      if (pHigh[i] > fHighest)
      {
        fHighest = pHigh[i];
      }
    }
    Now.nType = CZSC_POINT_TOP;
    Now.fHigh = fHighest;
    Now.fLow = fHighest;
  }

  // 上一完成的同向段 = Points 倒数第三、第二点
  const SegmentPoint &PrevStart = Points[Points.size() - 3];
  const SegmentPoint &PrevEnd = Points[Points.size() - 2];
  if (GetMoveDirection(PrevStart, PrevEnd) != nDir)
  {
    return 0;
  }

  DivergenceResult Result = MeasureDivergence(PrevStart, PrevEnd, Pivot, Now, nDir);
  return Result.bDivergence ? nDir : 0;
}

