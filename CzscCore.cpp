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

struct SegmentInterval
{
  int   nStart;
  int   nEnd;
  float fHigh;
  float fLow;
};

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

static bool IsFirstSignal(float fSignal)
{
  return (fSignal == SIGNAL_FIRST_BUY) || (fSignal == SIGNAL_FIRST_SELL);
}

static bool IsThirdSignal(float fSignal)
{
  return (fSignal == SIGNAL_THIRD_BUY) || (fSignal == SIGNAL_THIRD_SELL);
}

static int GetTradingSignalSide(float fSignal)
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

//=============================================================================
// 形态学辅助：输出校验、K线包含处理、分型/线段点构造
//=============================================================================

// 输出数组是否可用（个数为正且指针非空）
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

static float AbsF(float fValue)
{
  return (fValue < 0) ? -fValue : fValue;
}

// 通达信无效数：位模式 0xF8F8F8F8（约 -4e34 的巨大负 float）。官方规范要求计算前判断并跳过。
static bool IsInvalidFloat(float fValue)
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

// 清洗输入序列：无效数用上一有效值前向填充；开头的无效数用首个有效值回填；整段无效则置 0。
static std::vector<float> SanitizeSeries(int nCount, const float *pSrc)
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

//=============================================================================
// 旁路数据：注册真实收盘价/成交量，消费端按 Close∈[Low,High] 内容校验自动启用（防串台）
//=============================================================================

struct CzscAuxData
{
  int                nCount;
  std::vector<float> Close;
  std::vector<float> Volume;
  bool               bValid;
};

static CzscAuxData g_Aux = {0, std::vector<float>(), std::vector<float>(), false};

// 注册旁路数据：清洗后存入；pClose 为空或 nCount<=0 则失效
void RegisterAuxData(int nCount, float *pClose, float *pVolume)
{
  if ((nCount <= 0) || (pClose == 0))
  {
    g_Aux.bValid = false;
    return;
  }
  g_Aux.nCount = nCount;
  g_Aux.Close = SanitizeSeries(nCount, pClose);
  g_Aux.Volume = (pVolume != 0) ? SanitizeSeries(nCount, pVolume) : std::vector<float>();
  g_Aux.bValid = true;
}

// 内容校验：每根有效K线上 Low-ε ≤ Close ≤ High+ε 全成立才返回注册的收盘价，否则 0（回落代理）
const std::vector<float> *GetValidatedClose(int nCount, const float *pHigh, const float *pLow)
{
  if (!g_Aux.bValid || (g_Aux.nCount != nCount) || ((int)g_Aux.Close.size() != nCount) ||
      (pHigh == 0) || (pLow == 0) || (nCount <= 0))
  {
    return 0;
  }
  for (int i = 0; i < nCount; i++)
  {
    float fHigh = pHigh[i];
    float fLow = pLow[i];
    if (IsInvalidFloat(fHigh) || IsInvalidFloat(fLow))
    {
      continue;  // 该根无法校验，跳过
    }
    float fClose = g_Aux.Close[(std::size_t)i];
    float fTol = (AbsF(fHigh) + 1.0f) * 0.001f;  // 容差防浮点边界误否决
    if ((fClose < fLow - fTol) || (fClose > fHigh + fTol))
    {
      return 0;  // 收盘价越界 → 错配/过期序列，否决
    }
  }
  return &g_Aux.Close;
}

// 成交量随收盘价同次注册，收盘价校验通过即信任配套成交量（V 无法用 H/L 交叉校验）
const std::vector<float> *GetValidatedVolume(int nCount, const float *pHigh, const float *pLow)
{
  if (GetValidatedClose(nCount, pHigh, pLow) == 0)
  {
    return 0;
  }
  if ((int)g_Aux.Volume.size() != nCount)
  {
    return 0;
  }
  return &g_Aux.Volume;
}

// 默认配置：严格笔 + 严格极值收笔 + 笔中枢 + 启发式线段，复现历史默认行为
CzscConfig DefaultConfig()
{
  CzscConfig Config;
  Config.nStrokeType = CZSC_STROKE_STRICT;
  Config.nStrokeEnd = CZSC_END_STRICT;
  Config.nCenterUnit = CZSC_UNIT_STROKE;
  Config.nSegmentMethod = CZSC_SEG_HEURISTIC;
  return Config;
}

// 把单个数字码解出四维配置：个位=笔类型、十位=笔结束、百位=中枢构件、千位=线段法；非法值回落默认
CzscConfig DecodeConfig(float fCode)
{
  int nCode = (int)(fCode + 0.5f);
  if (nCode < 0)
  {
    nCode = 0;
  }

  CzscConfig Config;
  Config.nStrokeType = (nCode % 10 == CZSC_STROKE_NEW) ? CZSC_STROKE_NEW : CZSC_STROKE_STRICT;
  Config.nStrokeEnd = ((nCode / 10) % 10 == CZSC_END_SECOND) ? CZSC_END_SECOND : CZSC_END_STRICT;
  Config.nCenterUnit = ((nCode / 100) % 10 == CZSC_UNIT_SEGMENT) ? CZSC_UNIT_SEGMENT : CZSC_UNIT_STROKE;
  Config.nSegmentMethod = ((nCode / 1000) % 10 == CZSC_SEG_FEATURE) ? CZSC_SEG_FEATURE : CZSC_SEG_HEURISTIC;
  return Config;
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

static SegmentPoint MakeSegmentPoint(const Fractal &F)
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
  Point.fEnergy = 0;
  Point.fDif = 0;
  Point.fDea = 0;
  return Point;
}

// 相邻两点构成一段走势的价格区间 [低, 高]，用于中枢重叠计算
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

// 两价格区间是否重叠
static bool IntervalsOverlap(float fLeftLow, float fLeftHigh, float fRightLow, float fRightHigh)
{
  return (fLeftLow <= fRightHigh) && (fRightLow <= fLeftHigh);
}

// 由连续三段走势的重叠区间构造初始中枢：ZG=min(三段高)、ZD=max(三段低)，GG/DD 取全幅极值
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

  // 全幅极值 GG/DD（union extent，与 ZG/ZD 的 intersection 相反方向聚合）
  float fTop = First.fHigh;
  if (Second.fHigh > fTop)
  {
    fTop = Second.fHigh;
  }
  if (Third.fHigh > fTop)
  {
    fTop = Third.fHigh;
  }

  float fBottom = First.fLow;
  if (Second.fLow < fBottom)
  {
    fBottom = Second.fLow;
  }
  if (Third.fLow < fBottom)
  {
    fBottom = Third.fLow;
  }

  pCenter->nStart = Points[nStart].nIndex;
  pCenter->nEnd = Points[nStart + 3].nIndex;
  pCenter->fHigh = fHigh;
  pCenter->fLow = fLow;
  pCenter->fTop = fTop;
  pCenter->fBottom = fBottom;
  pCenter->nDirection = 0;  // 方向由 BuildCenters 按进入段设定
  return true;
}

// 若新一段与中枢重叠则延伸：ZG/ZD 随重叠收缩、GG/DD 随全幅扩张、终点后移；否则中枢结束。
// 第20课中心定理一按 [dn, gn] 与 [ZD, ZG] 是否重叠判定延伸；穿越整个区间仍属于重叠。
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

  // ZG/ZD 随延伸收缩，GG/DD 随延伸扩张
  if (Interval.fHigh > pCenter->fTop)
  {
    pCenter->fTop = Interval.fHigh;
  }
  if (Interval.fLow < pCenter->fBottom)
  {
    pCenter->fBottom = Interval.fLow;
  }

  pCenter->nEnd = Interval.nEnd;
  return true;
}

static bool IsCenterLeaveAttempt(const Center &C,
                                 const SegmentPoint &Start,
                                 const SegmentPoint &End,
                                 int *pDirection)
{
  float fStart = GetPointPrice(Start);
  float fEnd = GetPointPrice(End);
  int nDirection = (fEnd > fStart) ? 1 : ((fEnd < fStart) ? -1 : 0);
  if (pDirection != 0)
  {
    *pDirection = nDirection;
  }

  if ((nDirection > 0) && (End.nType == CZSC_POINT_TOP) && (End.fHigh > C.fHigh))
  {
    return true;
  }
  if ((nDirection < 0) && (End.nType == CZSC_POINT_BOTTOM) && (End.fLow < C.fLow))
  {
    return true;
  }
  return false;
}

static bool RetestBackIntoCenter(const Center &C, const SegmentPoint &Retest, int nDirection)
{
  if (nDirection > 0)
  {
    return Retest.fLow < C.fHigh;
  }
  if (nDirection < 0)
  {
    return Retest.fHigh > C.fLow;
  }
  return false;
}

//=============================================================================
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

static StrengthMetrics MakeEmptyStrength()
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

static DivergenceResult MakeEmptyDivergence(int nDirection)
{
  DivergenceResult Result;
  Result.nDirection = nDirection;
  Result.bNewExtreme = false;
  Result.bWeakSpace = false;
  Result.bWeakSpeed = false;
  Result.bWeakMacd = false;
  Result.bDivergence = false;
  Result.Previous = MakeEmptyStrength();
  Result.Current = MakeEmptyStrength();
  return Result;
}

// 由两端点代表价判断一段走势的方向（+1 上、-1 下、0 平）
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

// 走势中枢中心定理二（第20课）：用全幅极值 GG/DD 判定前后两个同级别中枢的关系。
// 后DD > 前GG → 上涨延续；后GG < 前DD → 下跌延续；其余（全幅重叠）→ 形成高级别中枢（扩展）。
int ClassifyCenterRelation(const Center &Prev, const Center &Next)
{
  if (Next.fBottom > Prev.fTop)
  {
    return CZSC_CENTER_RELATION_UP;
  }
  if (Next.fTop < Prev.fBottom)
  {
    return CZSC_CENTER_RELATION_DOWN;
  }
  return CZSC_CENTER_RELATION_EXTENSION;
}

// 三类买卖点后续（第21课/第53课）：离开中枢后若与后一个中枢形成高级别中枢则为「中枢扩张」，
// 若形成同向新中枢则为「中枢新生（趋势）」。后续中枢尚未形成或方向相反则未知。
int ClassifyCenterAftermath(const std::vector<Center> &Centers, int nCenter, float fSignal)
{
  if ((nCenter < 0) || ((std::size_t)nCenter + 1 >= Centers.size()))
  {
    return CZSC_CENTER_AFTERMATH_UNKNOWN;
  }
  if (!IsThirdSignal(fSignal))
  {
    return CZSC_CENTER_AFTERMATH_UNKNOWN;
  }

  int nRelation = ClassifyCenterRelation(Centers[(std::size_t)nCenter],
                                         Centers[(std::size_t)nCenter + 1]);
  if (nRelation == CZSC_CENTER_RELATION_EXTENSION)
  {
    return CZSC_CENTER_AFTERMATH_EXTENDED;
  }
  if ((fSignal == SIGNAL_THIRD_BUY) && (nRelation == CZSC_CENTER_RELATION_UP))
  {
    return CZSC_CENTER_AFTERMATH_NEWBORN;
  }
  if ((fSignal == SIGNAL_THIRD_SELL) && (nRelation == CZSC_CENTER_RELATION_DOWN))
  {
    return CZSC_CENTER_AFTERMATH_NEWBORN;
  }
  return CZSC_CENTER_AFTERMATH_UNKNOWN;
}

static TrendStructure MakeTrendStructure(const std::vector<Center> &Centers,
                                         int nType,
                                         std::size_t nFirst,
                                         std::size_t nLast)
{
  TrendStructure T;
  T.nType = nType;
  T.nStart = Centers[nFirst].nStart;
  T.nEnd = Centers[nLast].nEnd;
  T.nFirstCenter = (int)nFirst;
  T.nLastCenter = (int)nLast;
  return T;
}

static int CenterRelationToMovement(int nRelation)
{
  if (nRelation == CZSC_CENTER_RELATION_UP)
  {
    return CZSC_MOVEMENT_UP;
  }
  if (nRelation == CZSC_CENTER_RELATION_DOWN)
  {
    return CZSC_MOVEMENT_DOWN;
  }
  return CZSC_MOVEMENT_CONSOLIDATION;
}

// 把中枢序列归并为走势类型：连续同向（同级中枢全幅不重叠）的中枢合并为一段趋势，
// 单个或不同向的归为盘整（第17/18课：趋势含两个以上依次同向中枢）
std::vector<TrendStructure> BuildTrendStructures(const std::vector<Center> &Centers)
{
  std::vector<TrendStructure> Structures;
  std::size_t i = 0;
  while (i < Centers.size())
  {
    if (i + 1 >= Centers.size())
    {
      Structures.push_back(MakeTrendStructure(Centers, CZSC_MOVEMENT_CONSOLIDATION, i, i));
      break;
    }

    int nType = CenterRelationToMovement(ClassifyCenterRelation(Centers[i], Centers[i + 1]));

    if (nType == CZSC_MOVEMENT_CONSOLIDATION)
    {
      Structures.push_back(MakeTrendStructure(Centers, CZSC_MOVEMENT_CONSOLIDATION, i, i));
      i++;
      continue;
    }

    std::size_t nLast = i + 1;
    while (nLast + 1 < Centers.size())
    {
      int nNextType = CenterRelationToMovement(ClassifyCenterRelation(Centers[nLast], Centers[nLast + 1]));
      if (nNextType == nType)
      {
        nLast++;
        continue;
      }
      break;
    }

    Structures.push_back(MakeTrendStructure(Centers, nType, i, nLast));
    i = nLast + 1;
  }

  return Structures;
}

static bool FindLastTrendStructure(const std::vector<TrendStructure> &Structures,
                                   int nIndex,
                                   int nDirection,
                                   int *pTrend)
{
  int nType = (nDirection > 0) ? CZSC_MOVEMENT_UP : CZSC_MOVEMENT_DOWN;
  int nTrend = -1;
  for (std::size_t i = 0; i < Structures.size(); i++)
  {
    if (Structures[i].nStart >= nIndex)
    {
      continue;
    }
    if (Structures[i].nType == nType)
    {
      nTrend = (int)i;
    }
  }

  if (nTrend < 0)
  {
    return false;
  }
  if (pTrend != 0)
  {
    *pTrend = nTrend;
  }
  return true;
}

static int FindLastCenterBeforeIndex(const std::vector<Center> &Centers, int nIndex);

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

static bool FindPreviousSameDirectionMoveBeforeIndex(const std::vector<SegmentPoint> &Points,
                                                     std::size_t nPoint,
                                                     int nDirection,
                                                     int nBeforeIndex,
                                                     std::size_t *pMove)
{
  if ((pMove == 0) || (nPoint < 2))
  {
    return false;
  }

  for (std::size_t i = nPoint - 2; ; i--)
  {
    if ((Points[i + 1].nIndex < nBeforeIndex) &&
        (GetMoveDirection(Points[i], Points[i + 1]) == nDirection))
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
                                      const std::vector<TrendStructure> &Structures,
                                      std::size_t nPoint,
                                      DivergenceResult *pDivergence,
                                      int *pTrend)
{
  if (pDivergence != 0)
  {
    *pDivergence = MakeEmptyDivergence(-1);
  }
  if (pTrend != 0)
  {
    *pTrend = -1;
  }
  if ((nPoint < 4) || (Points[nPoint].nType != CZSC_POINT_BOTTOM))
  {
    return false;
  }

  int nTrend = -1;
  if (!FindLastTrendStructure(Structures, Points[nPoint].nIndex, -1, &nTrend))
  {
    return false;
  }
  if (Structures[(std::size_t)nTrend].nLastCenter <= Structures[(std::size_t)nTrend].nFirstCenter)
  {
    return false;  // 第一类买点只在至少两个同向中枢构成的下跌趋势背驰后出现
  }
  int nLastCenter = Structures[nTrend].nLastCenter;
  if (FindLastCenterBeforeIndex(Centers, Points[nPoint].nIndex) != nLastCenter)
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
  if (!FindPreviousSameDirectionMoveBeforeIndex(Points, nPoint, -1,
                                               Centers[nLastCenter].nStart,
                                               &nPrevMove))
  {
    return false;
  }

  const SegmentPoint &PrevStart = Points[nPrevMove];
  const SegmentPoint &PrevEnd = Points[nPrevMove + 1];
  if ((PrevStart.nType != CZSC_POINT_TOP) || (PrevEnd.nType != CZSC_POINT_BOTTOM))
  {
    return false;
  }

  DivergenceResult Divergence = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, -1);
  if (pDivergence != 0)
  {
    *pDivergence = Divergence;
  }
  if (pTrend != 0)
  {
    *pTrend = nTrend;
  }
  return Divergence.bDivergence;
}

static bool IsTrendDivergenceFirstSell(const std::vector<SegmentPoint> &Points,
                                       const std::vector<Center> &Centers,
                                       const std::vector<TrendStructure> &Structures,
                                       std::size_t nPoint,
                                       DivergenceResult *pDivergence,
                                       int *pTrend)
{
  if (pDivergence != 0)
  {
    *pDivergence = MakeEmptyDivergence(1);
  }
  if (pTrend != 0)
  {
    *pTrend = -1;
  }
  if ((nPoint < 4) || (Points[nPoint].nType != CZSC_POINT_TOP))
  {
    return false;
  }

  int nTrend = -1;
  if (!FindLastTrendStructure(Structures, Points[nPoint].nIndex, 1, &nTrend))
  {
    return false;
  }
  if (Structures[(std::size_t)nTrend].nLastCenter <= Structures[(std::size_t)nTrend].nFirstCenter)
  {
    return false;  // 第一类卖点只在至少两个同向中枢构成的上涨趋势背驰后出现
  }
  int nLastCenter = Structures[nTrend].nLastCenter;
  if (FindLastCenterBeforeIndex(Centers, Points[nPoint].nIndex) != nLastCenter)
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
  if (!FindPreviousSameDirectionMoveBeforeIndex(Points, nPoint, 1,
                                               Centers[nLastCenter].nStart,
                                               &nPrevMove))
  {
    return false;
  }

  const SegmentPoint &PrevStart = Points[nPrevMove];
  const SegmentPoint &PrevEnd = Points[nPrevMove + 1];
  if ((PrevStart.nType != CZSC_POINT_BOTTOM) || (PrevEnd.nType != CZSC_POINT_TOP))
  {
    return false;
  }

  DivergenceResult Divergence = MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, 1);
  if (pDivergence != 0)
  {
    *pDivergence = Divergence;
  }
  if (pTrend != 0)
  {
    *pTrend = nTrend;
  }
  return Divergence.bDivergence;
}

static DivergenceResult MeasureConsolidationDivergence(const std::vector<SegmentPoint> &Points,
                                                       std::size_t nLeavePoint,
                                                       int nDirection)
{
  DivergenceResult Empty = MakeEmptyDivergence(nDirection);
  if ((nLeavePoint == 0) || (nLeavePoint >= Points.size()))
  {
    return Empty;
  }

  std::size_t nPrevMove = 0;
  if (!FindPreviousSameDirectionMove(Points, nLeavePoint, nDirection, &nPrevMove))
  {
    return Empty;
  }

  const SegmentPoint &PrevStart = Points[nPrevMove];
  const SegmentPoint &PrevEnd = Points[nPrevMove + 1];
  const SegmentPoint &CurrentStart = Points[nLeavePoint - 1];
  const SegmentPoint &CurrentEnd = Points[nLeavePoint];

  if (nDirection > 0)
  {
    if ((PrevStart.nType != CZSC_POINT_BOTTOM) || (PrevEnd.nType != CZSC_POINT_TOP) ||
        (CurrentStart.nType != CZSC_POINT_BOTTOM) || (CurrentEnd.nType != CZSC_POINT_TOP) ||
        (CurrentEnd.fHigh <= PrevEnd.fHigh))
    {
      return Empty;
    }
  }
  else if (nDirection < 0)
  {
    if ((PrevStart.nType != CZSC_POINT_TOP) || (PrevEnd.nType != CZSC_POINT_BOTTOM) ||
        (CurrentStart.nType != CZSC_POINT_TOP) || (CurrentEnd.nType != CZSC_POINT_BOTTOM) ||
        (CurrentEnd.fLow >= PrevEnd.fLow))
    {
      return Empty;
    }
  }
  else
  {
    return Empty;
  }

  return MeasureDivergence(PrevStart, PrevEnd, CurrentStart, CurrentEnd, nDirection);
}

static CenterBreakout MakeCenterBreakout(const std::vector<SegmentPoint> &Points,
                                         const Center &C,
                                         std::size_t nCenter,
                                         std::size_t nLeavePoint,
                                         std::size_t nRetestPoint,
                                         int nDirection)
{
  CenterBreakout B;
  B.nCenter = (int)nCenter;
  B.nDirection = nDirection;
  B.nLeavePoint = (int)nLeavePoint;
  B.nRetestPoint = (int)nRetestPoint;
  B.bFirstRetest = true;
  B.bBackIntoCenter = false;
  B.bThirdSignal = false;
  B.Divergence = MeasureConsolidationDivergence(Points, nLeavePoint, nDirection);
  B.bConsolidationDivergence = B.Divergence.bDivergence;

  const SegmentPoint &Retest = Points[nRetestPoint];
  if (nDirection > 0)
  {
    B.bBackIntoCenter = Retest.fLow < C.fHigh;
    B.bThirdSignal = !B.bBackIntoCenter;
  }
  else
  {
    B.bBackIntoCenter = Retest.fHigh > C.fLow;
    B.bThirdSignal = !B.bBackIntoCenter;
  }

  return B;
}

std::vector<CenterBreakout> BuildCenterBreakouts(const std::vector<SegmentPoint> &Points,
                                                 const std::vector<Center> &Centers,
                                                 const std::vector<TrendStructure> &Structures)
{
  (void)Structures;
  std::vector<CenterBreakout> Breakouts;
  if (Points.size() < 2)
  {
    return Breakouts;
  }

  for (std::size_t i = 0; i < Centers.size(); i++)
  {
    const Center &C = Centers[i];
    // 离开点 = 中枢结束后第一个突破 ZG/ZD 的笔端点（次级别走势离开中枢，第20课）。
    // 注：中枢首尾相连，离开段同时是下一中枢的首段，故不以下一中枢起点为界。
    for (std::size_t j = 1; j < Points.size(); j++)
    {
      const SegmentPoint &Start = Points[j - 1];
      const SegmentPoint &End = Points[j];
      if (End.nIndex <= C.nEnd)
      {
        continue;  // 中枢内部，跳过
      }

      int nDirection = GetMoveDirection(Start, End);

      // 离开点可能是中枢末端点本身：若 Start 已突破 ZG/ZD，离开已发生，
      // 当前段即为离开后第一段。若其反向，则是回试（第20课「首次回试」）。
      if ((Start.nType == CZSC_POINT_BOTTOM) && (Start.fLow < C.fLow))
      {
        int nBreakDir = -1;
        if (nDirection < 0) { continue; }  // 继续同向离开，等反向段
        Breakouts.push_back(MakeCenterBreakout(Points, C, i, j - 1, j, nBreakDir));
        break;
      }
      if ((Start.nType == CZSC_POINT_TOP) && (Start.fHigh > C.fHigh))
      {
        int nBreakDir = 1;
        if (nDirection > 0) { continue; }  // 继续同向离开，等反向段
        Breakouts.push_back(MakeCenterBreakout(Points, C, i, j - 1, j, nBreakDir));
        break;
      }

      bool bLeavesUp = (nDirection > 0) && (End.nType == CZSC_POINT_TOP) && (End.fHigh > C.fHigh);
      bool bLeavesDown = (nDirection < 0) && (End.nType == CZSC_POINT_BOTTOM) && (End.fLow < C.fLow);
      if (!bLeavesUp && !bLeavesDown)
      {
        continue;  // 尚未突破中枢（回拉/震荡），继续找
      }

      int nBreakDirection = bLeavesUp ? 1 : -1;
      // 回试点 = 离开后第一个反向端点（第20课「必须是第一次」回试）
      for (std::size_t k = j + 1; k < Points.size(); k++)
      {
        if (((nBreakDirection > 0) && (Points[k].nType == CZSC_POINT_BOTTOM)) ||
            ((nBreakDirection < 0) && (Points[k].nType == CZSC_POINT_TOP)))
        {
          Breakouts.push_back(MakeCenterBreakout(Points, C, i, j, k, nBreakDirection));
          break;
        }
      }
      break;  // 每个中枢只取第一次离开+回试
    }
  }

  return Breakouts;
}

// 找到某下标处信号所属（最靠近的、起点不晚于它）的中枢，无则返回 -1
static int FindLastCenterBeforeIndex(const std::vector<Center> &Centers, int nIndex)
{
  int nCenter = -1;
  for (std::size_t i = 0; i < Centers.size(); i++)
  {
    if (Centers[i].nStart <= nIndex)
    {
      nCenter = (int)i;
    }
  }
  return nCenter;
}

static int FindCompletedTrendByCenter(const std::vector<TrendStructure> &Structures,
                                      int nCenter,
                                      int nIndex)
{
  if (nCenter < 0)
  {
    return -1;
  }

  for (std::size_t i = 0; i < Structures.size(); i++)
  {
    if ((Structures[i].nFirstCenter <= nCenter) &&
        (nCenter <= Structures[i].nLastCenter) &&
        (Structures[i].nEnd <= nIndex))
    {
      return (int)i;
    }
  }
  return -1;
}

// 信号质量分级（第24/27/61课）：一类看价差+速度或 MACD 背驰，二类看盘整背驰/重合，三类看背驰
static int ClassifyTradingSignalQuality(int nSource, const DivergenceResult &Divergence)
{
  if (nSource == SIGNAL_SOURCE_FIRST)
  {
    // 一类买卖点：价差与速度同时走弱，或 MACD 柱面积走弱，即为强质量信号。
    bool bStrong = Divergence.bDivergence &&
                   ((Divergence.bWeakSpace && Divergence.bWeakSpeed) || Divergence.bWeakMacd);
    return bStrong ? CZSC_SIGNAL_QUALITY_STRONG : CZSC_SIGNAL_QUALITY_CONFIRMED;
  }

  if (nSource == SIGNAL_SOURCE_SECOND)
  {
    // 二类买卖点：与三类重合（第61课）或回抽段构成盘整背驰（第27课）即为强信号。
    return Divergence.bDivergence ?
           CZSC_SIGNAL_QUALITY_STRONG :
           CZSC_SIGNAL_QUALITY_CONFIRMED;
  }

  if (nSource == SIGNAL_SOURCE_THIRD)
  {
    return Divergence.bDivergence ?
           CZSC_SIGNAL_QUALITY_STRONG :
           CZSC_SIGNAL_QUALITY_CONFIRMED;
  }

  return CZSC_SIGNAL_QUALITY_WATCH;
}

static int ClassifyCenterPosition(const std::vector<SegmentPoint> &Points,
                                  const std::vector<Center> &Centers,
                                  int nPoint,
                                  int nCenter)
{
  if ((nPoint < 0) || ((std::size_t)nPoint >= Points.size()) ||
      (nCenter < 0) || ((std::size_t)nCenter >= Centers.size()))
  {
    return CZSC_CENTER_POSITION_UNKNOWN;
  }

  float fPrice = GetPointPrice(Points[(std::size_t)nPoint]);
  const Center &C = Centers[(std::size_t)nCenter];
  if (fPrice < C.fLow)
  {
    return CZSC_CENTER_POSITION_BELOW;
  }
  if (fPrice > C.fHigh)
  {
    return CZSC_CENTER_POSITION_ABOVE;
  }
  return CZSC_CENTER_POSITION_INSIDE;
}

// 背驰-转折定理（第29课）：一买/一卖出现后，看其后第一段反弹相对最后中枢的高度，
// 区分 中枢扩展(最弱) / 更大级别盘整 / 反趋势(最强)。需要存在反弹点，否则未知。
int ClassifyReversalStrength(const std::vector<SegmentPoint> &Points,
                             const std::vector<Center> &Centers,
                             int nPoint,
                             int nCenter,
                             float fSignal)
{
  if ((nCenter < 0) || ((std::size_t)nCenter >= Centers.size()) || (nPoint < 0))
  {
    return CZSC_REVERSAL_UNKNOWN;
  }

  std::size_t nNext = (std::size_t)nPoint + 1;
  if (nNext >= Points.size())
  {
    return CZSC_REVERSAL_UNKNOWN;
  }

  const Center &C = Centers[(std::size_t)nCenter];
  const SegmentPoint &Rebound = Points[nNext];

  if (fSignal == SIGNAL_FIRST_BUY)
  {
    if (Rebound.nType != CZSC_POINT_TOP)
    {
      return CZSC_REVERSAL_UNKNOWN;
    }
    if (Rebound.fHigh > C.fHigh)     // 突破中枢上沿 → 反趋势
    {
      return CZSC_REVERSAL_TREND;
    }
    if (Rebound.fHigh >= C.fLow)     // 回到中枢区间 → 更大级别盘整
    {
      return CZSC_REVERSAL_CONSOLIDATION;
    }
    return CZSC_REVERSAL_EXTENSION;  // 仅触及下沿/DD → 最弱 → 最后中枢扩展
  }

  if (fSignal == SIGNAL_FIRST_SELL)
  {
    if (Rebound.nType != CZSC_POINT_BOTTOM)
    {
      return CZSC_REVERSAL_UNKNOWN;
    }
    if (Rebound.fLow < C.fLow)       // 跌破中枢下沿 → 反趋势
    {
      return CZSC_REVERSAL_TREND;
    }
    if (Rebound.fLow <= C.fHigh)     // 回到中枢区间 → 更大级别盘整
    {
      return CZSC_REVERSAL_CONSOLIDATION;
    }
    return CZSC_REVERSAL_EXTENSION;  // 仅触及上沿/GG → 最弱 → 最后中枢扩展
  }

  return CZSC_REVERSAL_UNKNOWN;
}

static TradingSignalCandidate MakeTradingSignalCandidate(int nIndex,
                                                         float fSignal,
                                                         int nPriority,
                                                         int nPoint,
                                                         int nCenter,
                                                         int nBreakout,
                                                         int nSource,
                                                         int nTrend,
                                                         int nMovementType,
                                                         int nCenterPosition,
                                                         bool bOverlapped,
                                                         const DivergenceResult &Divergence)
{
  TradingSignalCandidate C;
  C.nIndex = nIndex;
  C.fSignal = fSignal;
  C.nPriority = nPriority;
  C.nPoint = nPoint;
  C.nCenter = nCenter;
  C.nBreakout = nBreakout;
  C.nSource = nSource;
  C.nTrend = nTrend;
  C.nMovementType = nMovementType;
  C.nQuality = ClassifyTradingSignalQuality(nSource, Divergence);
  C.nCenterPosition = nCenterPosition;
  C.nReversal = CZSC_REVERSAL_UNKNOWN;
  C.nAfterEffect = CZSC_CENTER_AFTERMATH_UNKNOWN;
  C.nSmallTurn = 0;
  C.nAbcStructure = 0;
  C.nMacdZeroPullback = 0;
  C.bOverlapped = bOverlapped;
  C.Divergence = Divergence;
  return C;
}

static int FindOverlappedBreakout(const std::vector<CenterBreakout> &Breakouts,
                                  int nPoint,
                                  int nCenter,
                                  int nDirection)
{
  for (std::size_t i = 0; i < Breakouts.size(); i++)
  {
    const CenterBreakout &B = Breakouts[i];
    if (B.bFirstRetest && B.bThirdSignal &&
        (B.nCenter == nCenter) &&
        (B.nDirection == nDirection) &&
        (B.nRetestPoint == nPoint))
    {
      return (int)i;
    }
  }
  return -1;
}

static void AppendFirstSignalCandidates(std::vector<TradingSignalCandidate> *pCandidates,
                                        const std::vector<SegmentPoint> &Points,
                                        const std::vector<Center> &Centers,
                                        const std::vector<TrendStructure> &Structures)
{
  if (pCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < Points.size(); i++)
  {
    DivergenceResult Divergence = MakeEmptyDivergence(0);
    int nTrend = -1;
    if (IsTrendDivergenceFirstBuy(Points, Centers, Structures, i, &Divergence, &nTrend))
    {
      int nCenter = Structures[(std::size_t)nTrend].nLastCenter;
      pCandidates->push_back(MakeTradingSignalCandidate(Points[i].nIndex,
                                                        SIGNAL_FIRST_BUY,
                                                        SIGNAL_PRIORITY_FIRST,
                                                        (int)i,
                                                        nCenter,
                                                        -1,
                                                        SIGNAL_SOURCE_FIRST,
                                                        nTrend,
                                                        Structures[(std::size_t)nTrend].nType,
                                                        ClassifyCenterPosition(Points, Centers, (int)i, nCenter),
                                                        false,
                                                        Divergence));
      pCandidates->back().nReversal =
        ClassifyReversalStrength(Points, Centers, (int)i, nCenter, SIGNAL_FIRST_BUY);
    }
    else if (IsTrendDivergenceFirstSell(Points, Centers, Structures, i, &Divergence, &nTrend))
    {
      int nCenter = Structures[(std::size_t)nTrend].nLastCenter;
      pCandidates->push_back(MakeTradingSignalCandidate(Points[i].nIndex,
                                                        SIGNAL_FIRST_SELL,
                                                        SIGNAL_PRIORITY_FIRST,
                                                        (int)i,
                                                        nCenter,
                                                        -1,
                                                        SIGNAL_SOURCE_FIRST,
                                                        nTrend,
                                                        Structures[(std::size_t)nTrend].nType,
                                                        ClassifyCenterPosition(Points, Centers, (int)i, nCenter),
                                                        false,
                                                        Divergence));
      pCandidates->back().nReversal =
        ClassifyReversalStrength(Points, Centers, (int)i, nCenter, SIGNAL_FIRST_SELL);
    }
  }

  // 去重：同一中枢区域内只保留价格最极端的一类买卖点（每中枢至多一个一买/一卖）。
  // 全趋势去重会丢失中间中枢的区域极值（如连续下跌趋势只保留最低点），改用中枢分组。
  std::vector<TradingSignalCandidate> Deduped;
  for (std::size_t a = 0; a < pCandidates->size(); a++)
  {
    const TradingSignalCandidate &Ca = (*pCandidates)[a];
    float fA = (Ca.fSignal == SIGNAL_FIRST_BUY) ? Points[Ca.nPoint].fLow : Points[Ca.nPoint].fHigh;

    bool bKeep = true;
    for (std::size_t b = 0; b < pCandidates->size(); b++)
    {
      if ((b == a) || ((*pCandidates)[b].fSignal != Ca.fSignal))
      {
        continue;
      }
      const TradingSignalCandidate &Cb = (*pCandidates)[b];
      if (Cb.nCenter != Ca.nCenter)
      {
        continue;  // 不同中枢，各保留
      }
      float fB = (Cb.fSignal == SIGNAL_FIRST_BUY) ? Points[Cb.nPoint].fLow : Points[Cb.nPoint].fHigh;
      bool bBmoreExtreme = (Ca.fSignal == SIGNAL_FIRST_BUY) ? (fB < fA) : (fB > fA);
      if (bBmoreExtreme || ((AbsF(fB - fA) < 0.0001f) && (b < a)))
      {
        bKeep = false;  // 同中枢内 b 更极端（或同值取靠前）→ 丢弃 a
        break;
      }
    }
    if (bKeep)
    {
      Deduped.push_back(Ca);
    }
  }
  *pCandidates = Deduped;
}

static void AnnotateAbcDivergenceStructure(std::vector<TradingSignalCandidate> *pFirstCandidates,
                                           const std::vector<CenterBreakout> &Breakouts)
{
  if (pFirstCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < pFirstCandidates->size(); i++)
  {
    TradingSignalCandidate &C = (*pFirstCandidates)[i];
    if ((C.nSource != SIGNAL_SOURCE_FIRST) ||
        ((C.fSignal != SIGNAL_FIRST_BUY) && (C.fSignal != SIGNAL_FIRST_SELL)))
    {
      continue;
    }

    for (std::size_t j = 0; j < Breakouts.size(); j++)
    {
      const CenterBreakout &B = Breakouts[j];
      if (!B.bFirstRetest || !B.bThirdSignal ||
          (B.nCenter != C.nCenter) ||
          (B.nRetestPoint < 0) || (B.nRetestPoint >= C.nPoint))
      {
        continue;
      }
      if ((C.fSignal == SIGNAL_FIRST_BUY) && (B.nDirection < 0))
      {
        C.nAbcStructure = 1;
        break;
      }
      if ((C.fSignal == SIGNAL_FIRST_SELL) && (B.nDirection > 0))
      {
        C.nAbcStructure = -1;
        break;
      }
    }
  }
}

static bool CenterHasMacdZeroPullback(const std::vector<SegmentPoint> &Points,
                                      const Center &C,
                                      const DivergenceResult &D)
{
  float fBase = D.Previous.fDifHeight;
  if (D.Previous.fDeaHeight > fBase)
  {
    fBase = D.Previous.fDeaHeight;
  }
  if (D.Current.fDifHeight > fBase)
  {
    fBase = D.Current.fDifHeight;
  }
  if (D.Current.fDeaHeight > fBase)
  {
    fBase = D.Current.fDeaHeight;
  }
  if (fBase <= 0)
  {
    return false;
  }

  float fThreshold = fBase * 0.25f;
  if (fThreshold < 0.0001f)
  {
    fThreshold = 0.0001f;
  }

  for (std::size_t i = 0; i < Points.size(); i++)
  {
    const SegmentPoint &P = Points[i];
    if ((P.nIndex < C.nStart) || (P.nIndex > C.nEnd))
    {
      continue;
    }
    if ((AbsF(P.fDif) <= fThreshold) && (AbsF(P.fDea) <= fThreshold))
    {
      return true;
    }
  }
  return false;
}

static void AnnotateMacdZeroPullback(std::vector<TradingSignalCandidate> *pFirstCandidates,
                                     const std::vector<SegmentPoint> &Points,
                                     const std::vector<Center> &Centers)
{
  if (pFirstCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < pFirstCandidates->size(); i++)
  {
    TradingSignalCandidate &C = (*pFirstCandidates)[i];
    if ((C.nSource != SIGNAL_SOURCE_FIRST) ||
        (C.nCenter < 0) || ((std::size_t)C.nCenter >= Centers.size()))
    {
      continue;
    }
    if (!CenterHasMacdZeroPullback(Points, Centers[(std::size_t)C.nCenter], C.Divergence))
    {
      continue;
    }
    if (C.fSignal == SIGNAL_FIRST_BUY)
    {
      C.nMacdZeroPullback = 1;
    }
    else if (C.fSignal == SIGNAL_FIRST_SELL)
    {
      C.nMacdZeroPullback = -1;
    }
  }
}

// 第二类买卖点的盘整背驰（第27课）：比较进入一类买卖点的那一段(A段)与转折后回抽到
// 二类买卖点的那一段(C段)的力度。C段更弱（价差与速度同时走弱，或 MACD 柱面积走弱）即构成
// 盘整背驰。盘整背驰不要求创新高/新低。
static DivergenceResult MeasureSecondDivergence(const std::vector<SegmentPoint> &Points,
                                                std::size_t nFirstPoint,
                                                int nDirection)
{
  DivergenceResult Result = MakeEmptyDivergence(nDirection);
  if ((nFirstPoint < 1) || (nFirstPoint + 2 >= Points.size()))
  {
    return Result;
  }

  Result.Previous = MeasureStrength(Points[nFirstPoint - 1], Points[nFirstPoint]);
  Result.Current = MeasureStrength(Points[nFirstPoint + 1], Points[nFirstPoint + 2]);
  Result.bWeakSpace = Result.Current.fSpace < Result.Previous.fSpace;
  Result.bWeakSpeed = Result.Current.fSpeed < Result.Previous.fSpeed;
  Result.bWeakMacd = (Result.Current.fMacdArea > 0) &&
                     (Result.Previous.fMacdArea > 0) &&
                     (Result.Current.fMacdArea < Result.Previous.fMacdArea);
  Result.bNewExtreme = false;
  Result.bDivergence = (Result.bWeakSpace && Result.bWeakSpeed) || Result.bWeakMacd;
  return Result;
}

static void AppendSecondSignalCandidates(std::vector<TradingSignalCandidate> *pCandidates,
                                         const std::vector<SegmentPoint> &Points,
                                         const std::vector<Center> &Centers,
                                         const std::vector<CenterBreakout> &Breakouts,
                                         const std::vector<TradingSignalCandidate> &FirstCandidates)
{
  if (pCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < FirstCandidates.size(); i++)
  {
    const TradingSignalCandidate &FirstSignal = FirstCandidates[i];
    if ((FirstSignal.nPoint < 0) || ((std::size_t)FirstSignal.nPoint + 2 >= Points.size()))
    {
      continue;
    }

    std::size_t nPoint = (std::size_t)FirstSignal.nPoint;
    const SegmentPoint &First = Points[nPoint];
    const SegmentPoint &Turn = Points[nPoint + 1];
    const SegmentPoint &Second = Points[nPoint + 2];
    if ((FirstSignal.fSignal == SIGNAL_FIRST_BUY) &&
        (Turn.nType == CZSC_POINT_TOP) &&
        (Second.nType == CZSC_POINT_BOTTOM) &&
        (Second.fLow > First.fLow))
    {
      int nBreakout = FindOverlappedBreakout(Breakouts, (int)nPoint + 2, FirstSignal.nCenter, 1);
      DivergenceResult Divergence = (nBreakout >= 0) ?
                                    Breakouts[(std::size_t)nBreakout].Divergence :
                                    MeasureSecondDivergence(Points, nPoint, 1);
      int nSecondPoint = (int)nPoint + 2;
      int nCenter = FindLastCenterBeforeIndex(Centers, Second.nIndex);
      pCandidates->push_back(MakeTradingSignalCandidate(Second.nIndex,
                                                        SIGNAL_SECOND_BUY,
                                                        SIGNAL_PRIORITY_SECOND,
                                                        nSecondPoint,
                                                        nCenter,
                                                        nBreakout,
                                                        SIGNAL_SOURCE_SECOND,
                                                        FirstSignal.nTrend,
                                                        FirstSignal.nMovementType,
                                                        ClassifyCenterPosition(Points, Centers, nSecondPoint, nCenter),
                                                        nBreakout >= 0,
                                                        Divergence));
    }
    else if ((FirstSignal.fSignal == SIGNAL_FIRST_SELL) &&
             (Turn.nType == CZSC_POINT_BOTTOM) &&
             (Second.nType == CZSC_POINT_TOP) &&
             (Second.fHigh < First.fHigh))
    {
      int nBreakout = FindOverlappedBreakout(Breakouts, (int)nPoint + 2, FirstSignal.nCenter, -1);
      DivergenceResult Divergence = (nBreakout >= 0) ?
                                    Breakouts[(std::size_t)nBreakout].Divergence :
                                    MeasureSecondDivergence(Points, nPoint, -1);
      int nSecondPoint = (int)nPoint + 2;
      int nCenter = FindLastCenterBeforeIndex(Centers, Second.nIndex);
      pCandidates->push_back(MakeTradingSignalCandidate(Second.nIndex,
                                                        SIGNAL_SECOND_SELL,
                                                        SIGNAL_PRIORITY_SECOND,
                                                        nSecondPoint,
                                                        nCenter,
                                                        nBreakout,
                                                        SIGNAL_SOURCE_SECOND,
                                                        FirstSignal.nTrend,
                                                        FirstSignal.nMovementType,
                                                        ClassifyCenterPosition(Points, Centers, nSecondPoint, nCenter),
                                                        nBreakout >= 0,
                                                        Divergence));
    }
  }
}

static void AppendThirdSignalCandidates(std::vector<TradingSignalCandidate> *pCandidates,
                                        const std::vector<SegmentPoint> &Points,
                                        const std::vector<Center> &Centers,
                                        const std::vector<TrendStructure> &Structures,
                                        const std::vector<CenterBreakout> &Breakouts)
{
  if (pCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < Breakouts.size(); i++)
  {
    const CenterBreakout &B = Breakouts[i];
    if (!B.bFirstRetest || !B.bThirdSignal ||
        (B.nRetestPoint < 0) || ((std::size_t)B.nRetestPoint >= Points.size()))
    {
      continue;
    }

    float fSignal = (B.nDirection > 0) ? SIGNAL_THIRD_BUY : SIGNAL_THIRD_SELL;
    bool bOverlapped = false;
    for (std::size_t j = 0; j < pCandidates->size(); j++)
    {
      const TradingSignalCandidate &Second = (*pCandidates)[j];
      if ((Second.nSource == SIGNAL_SOURCE_SECOND) &&
          (Second.nBreakout == (int)i) &&
          (Second.nIndex == Points[B.nRetestPoint].nIndex))
      {
        bOverlapped = true;
        break;
      }
    }
    int nTrend = FindCompletedTrendByCenter(Structures,
                                            B.nCenter,
                                            Points[B.nRetestPoint].nIndex);
    int nMovementType = (nTrend >= 0) ? Structures[(std::size_t)nTrend].nType : CZSC_MOVEMENT_CONSOLIDATION;
    pCandidates->push_back(MakeTradingSignalCandidate(Points[B.nRetestPoint].nIndex,
                                                      fSignal,
                                                      SIGNAL_PRIORITY_THIRD,
                                                      B.nRetestPoint,
                                                      B.nCenter,
                                                      (int)i,
                                                      SIGNAL_SOURCE_THIRD,
                                                      nTrend,
                                                      nMovementType,
                                                      ClassifyCenterPosition(Points, Centers, B.nRetestPoint, B.nCenter),
                                                      bOverlapped,
                                                      B.Divergence));
    pCandidates->back().nAfterEffect = ClassifyCenterAftermath(Centers, B.nCenter, fSignal);
  }
}

static void AnnotateSmallTurnConditions(std::vector<TradingSignalCandidate> *pCandidates,
                                        const std::vector<TradingSignalCandidate> &FirstCandidates)
{
  if (pCandidates == 0)
  {
    return;
  }

  for (std::size_t i = 0; i < pCandidates->size(); i++)
  {
    TradingSignalCandidate &C = (*pCandidates)[i];
    if ((C.fSignal != SIGNAL_THIRD_BUY) && (C.fSignal != SIGNAL_THIRD_SELL))
    {
      continue;
    }

    for (std::size_t j = 0; j < FirstCandidates.size(); j++)
    {
      const TradingSignalCandidate &First = FirstCandidates[j];
      if ((First.nCenter != C.nCenter) || (First.nPoint >= C.nPoint))
      {
        continue;
      }
      if ((First.fSignal == SIGNAL_FIRST_BUY) && (C.fSignal == SIGNAL_THIRD_BUY))
      {
        C.nSmallTurn = 1;
        break;
      }
      if ((First.fSignal == SIGNAL_FIRST_SELL) && (C.fSignal == SIGNAL_THIRD_SELL))
      {
        C.nSmallTurn = -1;
        break;
      }
    }
  }
}

std::vector<TradingSignalCandidate> BuildTradingSignalCandidates(const std::vector<SegmentPoint> &Points,
                                                                  const std::vector<Center> &Centers,
                                                                  const std::vector<TrendStructure> &Structures,
                                                                  const std::vector<CenterBreakout> &Breakouts)
{
  std::vector<TradingSignalCandidate> Candidates;
  std::vector<TradingSignalCandidate> FirstCandidates;

  AppendFirstSignalCandidates(&FirstCandidates, Points, Centers, Structures);
  AnnotateAbcDivergenceStructure(&FirstCandidates, Breakouts);
  AnnotateMacdZeroPullback(&FirstCandidates, Points, Centers);
  AppendSecondSignalCandidates(&Candidates, Points, Centers, Breakouts, FirstCandidates);
  AppendThirdSignalCandidates(&Candidates, Points, Centers, Structures, Breakouts);
  AnnotateSmallTurnConditions(&Candidates, FirstCandidates);
  Candidates.insert(Candidates.end(), FirstCandidates.begin(), FirstCandidates.end());

  return Candidates;
}

void ApplyTradingSignalCandidates(int nCount,
                                  float *pOut,
                                  const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = C.fSignal;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 与 ApplyTradingSignalCandidates 同样按优先级取胜，但导出胜出信号的质量等级
// （0=观察，1=确认，2=强质量）。两者并用即可在图上区分 MACD 动力学确认的强信号。
void ApplyTradingSignalQuality(int nCount,
                               float *pOut,
                               const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)C.nQuality;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 与 ApplyTradingSignalCandidates 同样按优先级取胜，导出胜出信号的背驰-转折分类（第29课）。
// 输出编码：1=最后中枢扩展、2=更大级别盘整、3=反趋势，0=非一类买卖点/无反弹（未知）。
void ApplyTradingSignalReversal(int nCount,
                                float *pOut,
                                const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0;
      bool bFirstSignal = IsFirstSignal(C.fSignal);
      if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_EXTENSION))
      {
        fCode = 1;
      }
      else if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_CONSOLIDATION))
      {
        fCode = 2;
      }
      else if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_TREND))
      {
        fCode = 3;
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号的三类买卖点后续（第21课）：1=中枢扩张、2=中枢新生，0=其它。
void ApplyTradingSignalAftermath(int nCount,
                                 float *pOut,
                                 const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0;
      bool bThirdSignal = IsThirdSignal(C.fSignal);
      if (bThirdSignal && (C.nAfterEffect == CZSC_CENTER_AFTERMATH_EXTENDED))
      {
        fCode = 1;
      }
      else if (bThirdSignal && (C.nAfterEffect == CZSC_CENTER_AFTERMATH_NEWBORN))
      {
        fCode = 2;
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号相对所属中枢的位置：-1=下方、0=内部、1=上方、2=未知。
void ApplyTradingSignalCenterPosition(int nCount,
                                      float *pOut,
                                      const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)C.nCenterPosition;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号所属走势类型：-1=下跌、0=盘整、1=上涨。
void ApplyTradingSignalMovementType(int nCount,
                                    float *pOut,
                                    const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)C.nMovementType;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号本身的候选优先级：二类10、三类20、一类30。
void ApplyTradingSignalPriority(int nCount,
                                float *pOut,
                                const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)C.nPriority;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号所属中枢的一基编号；0 表示无信号或未知中枢。
void ApplyTradingSignalCenterId(int nCount,
                                float *pOut,
                                const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (C.nCenter >= 0) ? (float)(C.nCenter + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号关联中枢突破的一基编号；0 表示无关联突破。
void ApplyTradingSignalBreakoutId(int nCount,
                                  float *pOut,
                                  const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (C.nBreakout >= 0) ? (float)(C.nBreakout + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号对应端点的一基编号；0 表示无信号或未知端点。
void ApplyTradingSignalPointId(int nCount,
                               float *pOut,
                               const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (C.nPoint >= 0) ? (float)(C.nPoint + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 按优先级取胜，导出胜出信号所属走势结构的一基编号；0 表示无走势结构。
void ApplyTradingSignalTrendId(int nCount,
                               float *pOut,
                               const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (C.nTrend >= 0) ? (float)(C.nTrend + 1) : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

static bool HasMatchingSmallTurn(const TradingSignalCandidate &C)
{
  if (!IsThirdSignal(C.fSignal))
  {
    return false;
  }
  int nSide = GetTradingSignalSide(C.fSignal);
  return (nSide != 0) && (C.nSmallTurn == nSide);
}

// 第44课小转大必要条件：小级别底/顶背驰后，最后中枢出现三买/三卖才可能引发大级别转折。
// 输出 1=底背驰后的三买必要条件成立，-1=顶背驰后的三卖必要条件成立，0=无。
void ApplyTradingSignalSmallTurn(int nCount,
                                 float *pOut,
                                 const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = HasMatchingSmallTurn(C) ? (float)C.nSmallTurn : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 第37课a+A+b+B+c：一类背驰的c段至少包含对最后中枢B的三买/三卖。
static int GetFirstSignalDirection(const TradingSignalCandidate &C)
{
  if (C.fSignal == SIGNAL_FIRST_BUY)
  {
    return 1;
  }
  if (C.fSignal == SIGNAL_FIRST_SELL)
  {
    return -1;
  }
  return 0;
}

static bool HasMatchingAbcStructure(const TradingSignalCandidate &C)
{
  int nSign = GetFirstSignalDirection(C);
  return (nSign != 0) && (C.nAbcStructure == nSign);
}

static bool HasMatchingMacdZeroPullback(const TradingSignalCandidate &C)
{
  int nSign = GetFirstSignalDirection(C);
  return (nSign != 0) && (C.nMacdZeroPullback == nSign);
}

// 输出 1=一买前具备三卖结构，-1=一卖前具备三买结构，0=无。
void ApplyTradingSignalAbcStructure(int nCount,
                                    float *pOut,
                                    const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = HasMatchingAbcStructure(C) ? (float)C.nAbcStructure : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 与常规买卖点输出相同，但一类买卖点必须满足第37课A-B-C结构标记。
// 二、三类买卖点不按此条件过滤，保持第三类“首次离开+首次回试”的独立判定。
void ApplyTradingSignalStrictAbcCandidates(int nCount,
                                           float *pOut,
                                           const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (IsFirstSignal(C.fSignal) && !HasMatchingAbcStructure(C))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = C.fSignal;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

static bool IsMacdLineWeak(const DivergenceResult &D)
{
  return (D.Previous.fDifHeight > 0) &&
         (D.Previous.fDeaHeight > 0) &&
         (D.Current.fDifHeight > 0) &&
         (D.Current.fDeaHeight > 0) &&
         (D.Current.fDifHeight < D.Previous.fDifHeight) &&
         (D.Current.fDeaHeight <= D.Previous.fDeaHeight);
}

// 第25课 MACD 黄白线辅助：候选信号对应的C段黄白线高度弱于A段时标记。
// 输出 1=买点黄白线走弱，-1=卖点黄白线走弱，0=无。
void ApplyTradingSignalMacdLineWeakness(int nCount,
                                        float *pOut,
                                        const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0;
      int nSide = GetTradingSignalSide(C.fSignal);
      if ((nSide != 0) && IsMacdLineWeak(C.Divergence))
      {
        fCode = (float)nSide;
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

// 第24/25课 MACD 辅助：B中枢一般会把黄白线回拉到0轴附近。
// 输出 1=一买对应B中枢回拉0轴，-1=一卖对应B中枢回拉0轴，0=无。
void ApplyTradingSignalMacdZeroPullback(int nCount,
                                        float *pOut,
                                        const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = HasMatchingMacdZeroPullback(C) ? (float)C.nMacdZeroPullback : 0.0f;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

static bool IsStandardMacdDivergence(const TradingSignalCandidate &C)
{
  if (C.nSource != SIGNAL_SOURCE_FIRST)
  {
    return false;
  }
  int nSignalDirection = GetFirstSignalDirection(C);
  if ((nSignalDirection == 0) || (C.Divergence.nDirection != -nSignalDirection))
  {
    return false;
  }
  if (!C.Divergence.bDivergence)
  {
    return false;
  }
  if (!HasMatchingAbcStructure(C) || !HasMatchingMacdZeroPullback(C))
  {
    return false;
  }
  if (!C.Divergence.bNewExtreme || !C.Divergence.bWeakMacd)
  {
    return false;
  }
  return IsMacdLineWeak(C.Divergence);
}

// 第24/25/37课标准趋势背驰组合诊断：
// 一类趋势背驰 + C段创新极值 + 柱面积走弱 + 黄白线高度走弱 + B中枢回零 + ABC结构。
// 输出 1=标准一买背驰，-1=标准一卖背驰，0=未满足完整组合。
void ApplyTradingSignalStandardDivergence(int nCount,
                                          float *pOut,
                                          const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      float fCode = 0;
      if (IsStandardMacdDivergence(C))
      {
        fCode = (C.fSignal == SIGNAL_FIRST_BUY) ? 1.0f : -1.0f;
      }
      pOut[C.nIndex] = fCode;
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

int BuildTradingSignalContextFlags(const TradingSignalCandidate &C)
{
  int nFlags = 0;
  if (C.nQuality == CZSC_SIGNAL_QUALITY_STRONG)
  {
    nFlags |= CZSC_SIGNAL_CTX_STRONG_QUALITY;
  }
  if (HasMatchingAbcStructure(C))
  {
    nFlags |= CZSC_SIGNAL_CTX_ABC_STRUCTURE;
  }
  if (HasMatchingMacdZeroPullback(C))
  {
    nFlags |= CZSC_SIGNAL_CTX_MACD_ZERO_PULL;
  }
  if ((GetTradingSignalSide(C.fSignal) != 0) && IsMacdLineWeak(C.Divergence))
  {
    nFlags |= CZSC_SIGNAL_CTX_MACD_LINE_WEAK;
  }
  if (HasMatchingSmallTurn(C))
  {
    nFlags |= CZSC_SIGNAL_CTX_SMALL_TURN;
  }
  if (IsStandardMacdDivergence(C))
  {
    nFlags |= CZSC_SIGNAL_CTX_STANDARD_DIV;
  }
  bool bThirdSignal = IsThirdSignal(C.fSignal);
  if (bThirdSignal && (C.nAfterEffect == CZSC_CENTER_AFTERMATH_NEWBORN))
  {
    nFlags |= CZSC_SIGNAL_CTX_AFTERMATH_NEWBORN;
  }
  else if (bThirdSignal && (C.nAfterEffect == CZSC_CENTER_AFTERMATH_EXTENDED))
  {
    nFlags |= CZSC_SIGNAL_CTX_AFTERMATH_EXTEND;
  }
  bool bFirstSignal = IsFirstSignal(C.fSignal);
  if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_TREND))
  {
    nFlags |= CZSC_SIGNAL_CTX_REVERSAL_TREND;
  }
  else if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_CONSOLIDATION))
  {
    nFlags |= CZSC_SIGNAL_CTX_REVERSAL_CONS;
  }
  else if (bFirstSignal && (C.nReversal == CZSC_REVERSAL_EXTENSION))
  {
    nFlags |= CZSC_SIGNAL_CTX_REVERSAL_EXTEND;
  }
  if (C.bOverlapped)
  {
    nFlags |= CZSC_SIGNAL_CTX_OVERLAPPED;
  }
  if (C.nBreakout >= 0)
  {
    nFlags |= CZSC_SIGNAL_CTX_CENTER_BREAKOUT;
  }
  return nFlags;
}

// 胜出买卖点候选上下文位图：把质量、ABC、MACD、小转大、转折/后续等离散诊断压成一列，
// 方便通达信公式用一个 DLL 输出做组合筛选；同柱仍按候选优先级取胜。
void ApplyTradingSignalContextFlags(int nCount,
                                    float *pOut,
                                    const std::vector<TradingSignalCandidate> &Candidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  std::vector<int> Priorities;
  Priorities.resize((std::size_t)nCount);
  for (int i = 0; i < nCount; i++)
  {
    Priorities[(std::size_t)i] = -1;
  }

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      continue;
    }
    if (C.nPriority >= Priorities[(std::size_t)C.nIndex])
    {
      pOut[C.nIndex] = (float)BuildTradingSignalContextFlags(C);
      Priorities[(std::size_t)C.nIndex] = C.nPriority;
    }
  }
}

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

// 定位线段终点（第64/67课「线段只能被线段破坏」，逆向三笔自身构成反向线段级别结构即破坏）：
//  逆向笔 j 占端点 P[nStart+1+2j]（内端=线段终点候选）与 P[nStart+2+2j]（外端）——
//  第二笔回调确认（下降 higher low：下一内端高于本内端 / 上升 lower high：下一内端低于本内端）后，
//  反向「创新极值」(下降 higher high / 上升 lower low) 可由两种情况之一给出：
//   · 情况1（终点优先落在极值点）：第一笔不破前一逆向笔外端，但第三笔（下一逆向笔外端）破本笔外端；
//   · 情况2（终点为局部点）：第一笔即破前一逆向笔外端（外端突破），第三笔仅需成笔确认。
//  两者线段终点都是本逆向笔的内端，不一定是全局最高/最低点（如 3723→2635 的下跌实终于 2863，非 2635）。
static int FindFeatureSegmentEnd(const std::vector<SegmentPoint> &P, std::size_t nStart, int nDir)
{
  for (std::size_t j = 1; (nStart + 3 + 2 * j) < P.size(); j++)
  {
    float fOuter     = GetPointPrice(P[nStart + 2 + 2 * j]);  // 逆向笔 j 外端（下降:顶 / 上升:底）
    float fOuterPrev = GetPointPrice(P[nStart + 2 * j]);      // 逆向笔 j-1 外端
    float fInner     = GetPointPrice(P[nStart + 1 + 2 * j]);  // 逆向笔 j 内端（下降:底 / 上升:顶）
    float fInnerNext = GetPointPrice(P[nStart + 3 + 2 * j]);  // 逆向笔 j+1 内端

    // 第二笔回调确认：下降 higher low / 上升 lower high
    bool bPullback = (nDir < 0) ? (fInnerNext > fInner) : (fInnerNext < fInner);
    if (!bPullback)
    {
      continue;
    }

    // 情况2：第一笔即破前一逆向笔外端（下降 higher high / 上升 lower low）
    bool bBreakFirst = (nDir < 0) ? (fOuter > fOuterPrev) : (fOuter < fOuterPrev);

    // 情况1：第三笔（下一逆向笔外端）破本笔外端（终点优先落在极值点）
    bool bBreakThird = false;
    if ((nStart + 4 + 2 * j) < P.size())
    {
      float fOuterNext = GetPointPrice(P[nStart + 4 + 2 * j]);  // 逆向笔 j+1 外端
      bBreakThird = (nDir < 0) ? (fOuterNext > fOuter) : (fOuterNext < fOuter);
    }

    if (bBreakFirst || bBreakThird)
    {
      return (int)(nStart + 1 + 2 * j);  // 线段终点 = 逆向笔 j 的内端
    }
  }
  return -1;  // 未出现反向线段结构 → 线段当下未完成
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

// 扫描端点序列构造同级中枢（第17/20课）：连续三段走势有重叠即形成一个候选中枢。
// 中枢成形后以 ExtendCenter 吸收后续与 ZG/ZD 重叠的段来延伸（第20课中枢延伸）。
// 每个完成中枢都输出；相邻中枢的上涨/下跌/扩展关系由 ClassifyCenterRelation 单独标注。
// 方向由进入段定：前一点为底则进入段向上，前一点为顶则进入段向下。
std::vector<Center> BuildCenters(const std::vector<SegmentPoint> &Points)
{
  std::vector<Center> Centers;
  if (Points.size() < 4)
  {
    return Centers;
  }

  std::size_t i = 1;
  while (i + 3 < Points.size())
  {
    Center C;
    if (!TryBuildInitialCenter(Points, i, &C))
    {
      i++;
      continue;
    }
    C.nDirection = (Points[i - 1].nType == CZSC_POINT_BOTTOM) ? 1 : -1;

    // 中枢延伸：后续段与 ZG/ZD 重叠则吸收；若离开段+回试段构成三买卖则立即封死（第20课）
    std::size_t nExtend = i + 3;
    while (nExtend + 1 < Points.size())
    {
      SegmentInterval Interval = MakeSegmentInterval(Points[nExtend], Points[nExtend + 1]);

      // 先检查是否与 ZG/ZD 有重叠 → 正常延伸；穿越整个区间也按重叠吸收。
      if (IntervalsOverlap(C.fLow, C.fHigh, Interval.fLow, Interval.fHigh))
      {
        if (!ExtendCenter(&C, Interval))
        {
          break;
        }
        nExtend++;
        continue;
      }

      // 无重叠 → 检测是否为离开段，前探回试是否构成三买卖
      int nLeaveDir = 0;
      if (IsCenterLeaveAttempt(C, Points[nExtend], Points[nExtend + 1], &nLeaveDir))
      {
        if (nExtend + 2 < Points.size())
        {
          int nRetestMove = GetMoveDirection(Points[nExtend + 1], Points[nExtend + 2]);
          if ((nRetestMove != 0) && (nRetestMove != nLeaveDir))
          {
            if (!RetestBackIntoCenter(C, Points[nExtend + 2], nLeaveDir))
            {
              break;  // 离开+回试不回 → 三买卖点，封死中枢
            }
            // 离开+回试回中枢 → 中枢延伸：吸收离开段与回试段
            ExtendCenter(&C, Interval);
            nExtend++;
            SegmentInterval RetestInterval = MakeSegmentInterval(Points[nExtend], Points[nExtend + 1]);
            ExtendCenter(&C, RetestInterval);
            nExtend++;
            continue;
          }
        }
      }

      break;  // 无重叠且非延伸型离开 → 中枢结束
    }

    Centers.push_back(C);
    i = nExtend + 1;
  }

  return Centers;
}

// 在每个中枢的时间跨度内写出其上沿 ZG（fHigh），通达信据此画中枢上边
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

// 在每个中枢的时间跨度内写出其下沿 ZD（fLow），通达信据此画中枢下边
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

// 在每个中枢的起点标 1、终点标 2，通达信据此标注中枢起止
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

// 相邻中枢关系（第20课中心定理二）：在后中枢起点处标记
// 2=中枢扩展（形成高级别中枢）、1=上涨延续、-1=下跌延续。
void WriteCenterRelationSignal(int nCount, float *pOut, const std::vector<Center> &Centers)
{
  ClearOutput(nCount, pOut);
  for (std::size_t i = 1; i < Centers.size(); i++)
  {
    int nMark = Centers[i].nStart;
    if ((nMark < 0) || (nMark >= nCount))
    {
      continue;
    }

    int nRelation = ClassifyCenterRelation(Centers[i - 1], Centers[i]);
    if (nRelation == CZSC_CENTER_RELATION_EXTENSION)
    {
      pOut[nMark] = 2;
    }
    else if (nRelation == CZSC_CENTER_RELATION_UP)
    {
      pOut[nMark] = 1;
    }
    else
    {
      pOut[nMark] = -1;
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

  const CzscAnalyzer &An = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow);
  WriteCenterHighSignal(nCount, pOut, An.Centers);
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

  const CzscAnalyzer &An = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow);
  WriteCenterLowSignal(nCount, pOut, An.Centers);
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

  const CzscAnalyzer &An = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow);
  WriteCenterMarkSignal(nCount, pOut, An.Centers);
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

  const CzscAnalyzer &An = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow);
  ApplyTradingSignalCandidates(nCount, pOut, An.Candidates);
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

//=============================================================================
// 输出函数10号：三类买卖点信号质量（0=观察，1=确认，2=强质量）
//=============================================================================

void Func10(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  const CzscAnalyzer &An = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow);
  ApplyTradingSignalQuality(nCount, pOut, An.Candidates);
}

//=============================================================================
// 输出函数11号：相邻中枢关系（1=上涨延续 -1=下跌延续 2=中枢扩展，第20课中心定理二）
//=============================================================================

void Func11(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  const CzscAnalyzer &An = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow);
  WriteCenterRelationSignal(nCount, pOut, An.Centers);
}

//=============================================================================
// 输出函数12号：一类买卖点的背驰-转折分类（1=中枢扩展 2=更大盘整 3=反趋势，第29课）
//=============================================================================

void Func12(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  const CzscAnalyzer &An = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow);
  ApplyTradingSignalReversal(nCount, pOut, An.Candidates);
}

//=============================================================================
// 输出函数13号：三类买卖点后续（1=中枢扩张 2=中枢新生，第21课）
//=============================================================================

void Func13(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  const CzscAnalyzer &An = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow);
  ApplyTradingSignalAftermath(nCount, pOut, An.Candidates);
}

// 背驰段（第27课）：一类买卖点最后那段构成背驰的走势。在其起点标记 ±1、终点（买卖点）标记 ±2，
// 买为正、卖为负，便于在通达信用 DRAWLINE 画出背驰段，作为区间套入场参考。
static void WriteDivergenceSegmentSignal(int nCount,
                                         float *pOut,
                                         const std::vector<SegmentPoint> &Points,
                                         const std::vector<TradingSignalCandidate> &Candidates)
{
  ClearOutput(nCount, pOut);
  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if ((C.nSource != SIGNAL_SOURCE_FIRST) ||
        !IsFirstSignal(C.fSignal) ||
        (C.nPoint < 1) || ((std::size_t)C.nPoint >= Points.size()))
    {
      continue;
    }

    int nStartBar = Points[(std::size_t)(C.nPoint - 1)].nIndex;
    int nEndBar = C.nIndex;
    int nSign = (C.fSignal == SIGNAL_FIRST_BUY) ? 1 : -1;
    if ((nStartBar >= 0) && (nStartBar < nCount))
    {
      pOut[nStartBar] = (float)(nSign * 1);
    }
    if ((nEndBar >= 0) && (nEndBar < nCount))
    {
      pOut[nEndBar] = (float)(nSign * 2);
    }
  }
}

static bool GetDivergenceSegmentBars(const std::vector<SegmentPoint> &Points,
                                     const TradingSignalCandidate &C,
                                     int *pStart,
                                     int *pEnd)
{
  if ((pStart == 0) || (pEnd == 0) ||
      (C.nSource != SIGNAL_SOURCE_FIRST) ||
      !IsFirstSignal(C.fSignal) ||
      (C.nPoint < 1) || ((std::size_t)C.nPoint >= Points.size()))
  {
    return false;
  }

  *pStart = Points[(std::size_t)(C.nPoint - 1)].nIndex;
  *pEnd = C.nIndex;
  if (*pStart > *pEnd)
  {
    int nTmp = *pStart;
    *pStart = *pEnd;
    *pEnd = nTmp;
  }
  return true;
}

// 第27/61课区间套：在高级别背驰段中继续找低级别一类背驰段。输出编码沿用背驰段：
// 买 起点1/终点2，卖 起点-1/终点-2。该函数只做几何包含标记，不改变买卖点判定。
void WriteNestedDivergenceSignal(int nCount,
                                 float *pOut,
                                 const std::vector<SegmentPoint> &HighPoints,
                                 const std::vector<TradingSignalCandidate> &HighCandidates,
                                 const std::vector<SegmentPoint> &LowPoints,
                                 const std::vector<TradingSignalCandidate> &LowCandidates)
{
  if (!HasOutput(nCount, pOut))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  for (std::size_t i = 0; i < HighCandidates.size(); i++)
  {
    const TradingSignalCandidate &High = HighCandidates[i];
    int nHighStart = 0;
    int nHighEnd = 0;
    if (!GetDivergenceSegmentBars(HighPoints, High, &nHighStart, &nHighEnd))
    {
      continue;
    }

    for (std::size_t j = 0; j < LowCandidates.size(); j++)
    {
      const TradingSignalCandidate &Low = LowCandidates[j];
      if (Low.fSignal != High.fSignal)
      {
        continue;
      }

      int nLowStart = 0;
      int nLowEnd = 0;
      if (!GetDivergenceSegmentBars(LowPoints, Low, &nLowStart, &nLowEnd))
      {
        continue;
      }
      if ((nLowStart < nHighStart) || (nLowEnd > nHighEnd))
      {
        continue;
      }
      if ((nLowStart == nHighStart) && (nLowEnd == nHighEnd))
      {
        continue;
      }

      int nSign = (Low.fSignal == SIGNAL_FIRST_BUY) ? 1 : -1;
      if ((nLowStart >= 0) && (nLowStart < nCount))
      {
        pOut[nLowStart] = (float)(nSign * 1);
      }
      if ((nLowEnd >= 0) && (nLowEnd < nCount))
      {
        pOut[nLowEnd] = (float)(nSign * 2);
      }
    }
  }
}

//=============================================================================
// 输出函数14号：一类买卖点背驰段（买 起点1/终点2，卖 起点-1/终点-2，第27课）
//=============================================================================

void Func14(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  const CzscAnalyzer &An = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow);
  WriteDivergenceSegmentSignal(nCount, pOut, An.Points, An.Candidates);
}

// 由 H/L 算出短长均线（收盘价用 (H+L)/2 代理），写出 Price 缓冲供 Func15/16 与分析器复用
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

//=============================================================================
// 输出函数15号：短长均线差 = 短均线 - 长均线（符号=体位，幅度=趋势力度，第11/15课）
//=============================================================================

void Func15(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow))
  {
    return;
  }
  (void)pTime;

  ClearOutput(nCount, pOut);
  std::vector<float> Short;
  std::vector<float> Long;
  ComputeShortLongMa(nCount, pHigh, pLow, &Short, &Long);
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = Short[(std::size_t)i] - Long[(std::size_t)i];
  }
}

//=============================================================================
// 输出函数16号：均线吻类型（飞吻1 / 唇吻2 / 湿吻3，第11课），非吻处为 0
//=============================================================================

void Func16(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow))
  {
    return;
  }
  (void)pTime;

  ClearOutput(nCount, pOut);
  std::vector<float> Short;
  std::vector<float> Long;
  ComputeShortLongMa(nCount, pHigh, pLow, &Short, &Long);
  std::vector<int> Kiss = ClassifyMaKisses(Short, Long);
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = (float)Kiss[(std::size_t)i];
  }
}

// 即时趋势平均力度（第15课）：把最后一个线段点到当下视作未走完的末段，构造合成终点，
// 与上一完成同向段比较力度。末段已创新极值且力度走弱即提前预警（不必等末段走完）。
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

//=============================================================================
// 输出函数17号：即时背驰预警（在当下末段未走完时预警：1 见顶 / -1 见底，第15课）
//=============================================================================

void Func17(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow) || (pIn == 0))
  {
    return;
  }

  ClearOutput(nCount, pOut);
  const CzscAnalyzer &An = GetOrBuildSignalAnalyzer(nCount, pIn, pHigh, pLow);
  int nWarn = DetectInstantDivergence(An.Points, nCount, pHigh, pLow);
  if (nWarn != 0)
  {
    pOut[nCount - 1] = (float)nWarn;  // 在当下（最右一根）给出预警
  }
}

//=============================================================================
// 输出函数18号：笔（新笔标准，按合并K线间隔；与1号的原始K线间隔版并存，第62/65课）
//=============================================================================

void Func18(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow))
  {
    return;
  }
  (void)pTime;

  std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  CzscConfig Config = DefaultConfig();
  Config.nStrokeType = CZSC_STROKE_NEW;
  std::vector<Stroke> Strokes = BuildStrokes(Fractals, Config);
  std::vector<SegmentPoint> Points = BuildSegmentPoints(Strokes);
  WriteSegmentSignal(nCount, pOut, Points);
}

//=============================================================================
// 输出函数19号：线段（特征序列法，第67课；与9号的保护点启发式版并存）
//=============================================================================

void Func19(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow))
  {
    return;
  }
  (void)pTime;

  std::vector<MergedBar> Bars = BuildMergedBars(nCount, pHigh, pLow);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);
  std::vector<SegmentPoint> Points = BuildLineSegmentPointsByFeature(Strokes);
  WriteSegmentSignal(nCount, pOut, Points);
}

// 按配置从 H/L 直接产出供中枢使用的端点：笔类型/笔结束驱动笔，中枢构件选笔端点或线段端点
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

//=============================================================================
// 输出函数20号：配置驱动的端点信号。第4参 pTime[0] 为配置码（笔类型/笔结束/中枢构件），
// 输出可直接喂给 2-5 号中枢/买卖点函数，从而得到对应的笔中枢或线段中枢（第17/62/65/67课）
//=============================================================================

void Func20(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow))
  {
    return;
  }

  float fCode = (pTime != 0) ? pTime[0] : 0;
  CzscConfig Config = DecodeConfig(fCode);
  std::vector<SegmentPoint> Points = BuildConfiguredPoints(nCount, pHigh, pLow, Config);
  WriteSegmentSignal(nCount, pOut, Points);
}


//=============================================================================
// 输出函数30号：mode 驱动的统一入口。pTime[0] = 配置码*1000 + 输出类型*10。
// 内部走 H/L 家族中心化分析器（带缓存），一步算出从笔到买卖点的全部结果再按输出类型投影，
// 无需先 Func1/9 产端点再喂中枢函数；旧 Func1-20 保留兼容。
//=============================================================================

void Func30(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime)
{
  if (!HasPriceInput(nCount, pOut, pHigh, pLow))
  {
    return;
  }

  int nMode = (pTime != 0) ? (int)(pTime[0] + 0.5f) : 0;
  if (nMode < 0)
  {
    nMode = 0;
  }
  CzscConfig Config = DecodeConfig((float)(nMode / 1000));
  int nOutput = (nMode % 1000) / 10;

  const CzscAnalyzer &An = GetOrBuildPriceAnalyzer(nCount, pHigh, pLow, Config);

  switch (nOutput)
  {
    case 0:  WriteSegmentSignal(nCount, pOut, An.Points); break;            // 端点
    case 1:  WriteCenterHighSignal(nCount, pOut, An.Centers); break;        // 中枢上沿 ZG
    case 2:  WriteCenterLowSignal(nCount, pOut, An.Centers); break;         // 中枢下沿 ZD
    case 3:  WriteCenterMarkSignal(nCount, pOut, An.Centers); break;        // 中枢起止
    case 4:  ApplyTradingSignalCandidates(nCount, pOut, An.Candidates); break; // 三类买卖点
    case 5:  ApplyTradingSignalQuality(nCount, pOut, An.Candidates); break;    // 信号质量
    case 6:  WriteCenterRelationSignal(nCount, pOut, An.Centers); break;       // 中枢关系
    case 7:  ApplyTradingSignalReversal(nCount, pOut, An.Candidates); break;   // 背驰-转折
    case 8:  ApplyTradingSignalAftermath(nCount, pOut, An.Candidates); break;  // 三买后续
    case 9:  WriteDivergenceSegmentSignal(nCount, pOut, An.Points, An.Candidates); break; // 背驰段
    case 10:                                                                 // 均线差
      ClearOutput(nCount, pOut);
      if ((int)An.MaShort.size() >= nCount && (int)An.MaLong.size() >= nCount)
      {
        for (int i = 0; i < nCount; i++)
        {
          pOut[i] = An.MaShort[(std::size_t)i] - An.MaLong[(std::size_t)i];
        }
      }
      break;
    case 11:                                                                 // 均线吻
      ClearOutput(nCount, pOut);
      if ((int)An.Kiss.size() >= nCount)
      {
        for (int i = 0; i < nCount; i++)
        {
          pOut[i] = (float)An.Kiss[(std::size_t)i];
        }
      }
      break;
    case 12:                                                                 // 即时背驰预警
    {
      ClearOutput(nCount, pOut);
      int nWarn = DetectInstantDivergence(An.Points, nCount, pHigh, pLow);
      if (nWarn != 0)
      {
        pOut[nCount - 1] = (float)nWarn;
      }
      break;
    }
    case 13:                                                                 // 带量校验的吻（湿吻放量=骗线嫌疑4，需先 Func40 注册 V）
      ClearOutput(nCount, pOut);
      if ((int)An.KissVol.size() >= nCount)
      {
        for (int i = 0; i < nCount; i++)
        {
          pOut[i] = (float)An.KissVol[(std::size_t)i];
        }
      }
      break;
    case 14: ApplyTradingSignalSmallTurn(nCount, pOut, An.Candidates); break; // 小转大必要条件
    case 15: ApplyTradingSignalAbcStructure(nCount, pOut, An.Candidates); break; // A-B-C背驰结构
    case 16: ApplyTradingSignalStrictAbcCandidates(nCount, pOut, An.Candidates); break; // ABC严格买卖点
    case 17:                                                                 // 区间套背驰段：线段级背驰段内的笔级一类背驰段
    {
      CzscConfig HighConfig = DefaultConfig();
      HighConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
      HighConfig.nSegmentMethod = CZSC_SEG_FEATURE;
      const CzscAnalyzer &HighAn = GetOrBuildPriceAnalyzer(nCount, pHigh, pLow, HighConfig);
      const CzscAnalyzer &LowAn = GetOrBuildPriceAnalyzer(nCount, pHigh, pLow, DefaultConfig());
      WriteNestedDivergenceSignal(nCount, pOut,
                                  HighAn.Points, HighAn.Candidates,
                                  LowAn.Points, LowAn.Candidates);
      break;
    }
    case 18: ApplyTradingSignalMacdLineWeakness(nCount, pOut, An.Candidates); break; // MACD黄白线高度走弱
    case 19: ApplyTradingSignalMacdZeroPullback(nCount, pOut, An.Candidates); break; // B中枢MACD黄白线回拉0轴
    case 20: ApplyTradingSignalStandardDivergence(nCount, pOut, An.Candidates); break; // 标准趋势背驰确认
    case 21: ApplyTradingSignalContextFlags(nCount, pOut, An.Candidates); break; // 胜出候选上下文位图
    case 22: ApplyTradingSignalCenterPosition(nCount, pOut, An.Candidates); break; // 胜出候选相对中枢位置
    case 23: ApplyTradingSignalMovementType(nCount, pOut, An.Candidates); break; // 胜出候选所属走势类型
    case 24: ApplyTradingSignalPriority(nCount, pOut, An.Candidates); break; // 胜出候选优先级
    case 25: ApplyTradingSignalCenterId(nCount, pOut, An.Candidates); break; // 胜出候选所属中枢编号
    case 26: ApplyTradingSignalBreakoutId(nCount, pOut, An.Candidates); break; // 胜出候选关联突破编号
    case 27: ApplyTradingSignalPointId(nCount, pOut, An.Candidates); break; // 胜出候选端点编号
    case 28: ApplyTradingSignalTrendId(nCount, pOut, An.Candidates); break; // 胜出候选走势结构编号
    default: ClearOutput(nCount, pOut); break;
  }
}

//=============================================================================
// 输出函数40号：旁路注册真实收盘价 C 与成交量 V。公式 XC:=TDXDLL1(40,C,V,0); 置于消费函数之前，
// 之后中枢/买卖点/均线等会在内容校验通过时自动改用真实 C；输出透传 C（便于核对）。
//=============================================================================

void Func40(int nCount, float *pOut, float *pClose, float *pVolume, float *pUnused)
{
  (void)pUnused;
  RegisterAuxData(nCount, pClose, pVolume);

  if (!HasOutput(nCount, pOut))
  {
    return;
  }
  if (pClose == 0)
  {
    ClearOutput(nCount, pOut);
    return;
  }
  std::vector<float> Close = SanitizeSeries(nCount, pClose);
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = Close[(std::size_t)i];
  }
}
