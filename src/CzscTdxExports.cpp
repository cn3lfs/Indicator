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
//=============================================================================
// 输出函数20号：配置驱动的端点信号。第4参 pTime[0] 为配置码（笔类型/笔结束/中枢构件/线段法），
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
  if ((nMode % 10) != 0)
  {
    ClearOutput(nCount, pOut);
    return;
  }
  int nConfig = nMode / 1000;
  if (!IsValidConfigCode(nConfig))
  {
    ClearOutput(nCount, pOut);
    return;
  }
  CzscConfig Config = DecodeConfig((float)nConfig);
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
    case 29: ApplyTradingSignalMacdAreaRatio(nCount, pOut, An.Candidates); break; // C/A段MACD面积比
    case 30: ApplyTradingSignalSpaceRatio(nCount, pOut, An.Candidates); break; // C/A段价差力度比
    case 31: ApplyTradingSignalSpeedRatio(nCount, pOut, An.Candidates); break; // C/A段平均力度比
    case 32: ApplyTradingSignalDivergenceFlags(nCount, pOut, An.Candidates); break; // 背驰要素位图
    case 33: ApplyTradingSignalBreakoutLeavePointId(nCount, pOut, An.Candidates, An.Breakouts); break; // 突破离开端点编号
    case 34: ApplyTradingSignalBreakoutRetestPointId(nCount, pOut, An.Candidates, An.Breakouts); break; // 突破回试端点编号
    case 35: ApplyTradingSignalAbcBreakoutId(nCount, pOut, An.Candidates); break; // ABC关联突破编号
    case 36: ApplyTradingSignalAbcBreakoutLeavePointId(nCount, pOut, An.Candidates, An.Breakouts); break; // ABC关联突破离开端点编号
    case 37: ApplyTradingSignalAbcBreakoutRetestPointId(nCount, pOut, An.Candidates, An.Breakouts); break; // ABC关联突破回试端点编号
    case 38: ApplyTradingSignalSmallTurnLeavePointId(nCount, pOut, An.Candidates, An.Breakouts); break; // 小转大突破离开端点编号
    case 39: ApplyTradingSignalSmallTurnRetestPointId(nCount, pOut, An.Candidates, An.Breakouts); break; // 小转大突破回试端点编号
    case 40: ApplyTradingSignalSecondBasePointId(nCount, pOut, An.Candidates); break; // 二类关联一类端点编号
    case 41: ApplyTradingSignalSecondTurnPointId(nCount, pOut, An.Candidates); break; // 二类中间反向端点编号
    case 42: ApplyTradingSignalSmallTurnBasePointId(nCount, pOut, An.Candidates); break; // 小转大关联一类端点编号
    case 43: ApplyTradingSignalDivergencePreviousStartPointId(nCount, pOut, An.Candidates); break; // 背驰A段起点端点编号
    case 44: ApplyTradingSignalDivergencePreviousEndPointId(nCount, pOut, An.Candidates); break; // 背驰A段终点端点编号
    case 45: ApplyTradingSignalDivergenceCurrentStartPointId(nCount, pOut, An.Candidates); break; // 背驰C段起点端点编号
    case 46: ApplyTradingSignalDivergenceCurrentEndPointId(nCount, pOut, An.Candidates); break; // 背驰C段终点端点编号
    case 47: ApplyTradingSignalCenterLifecycle(nCount, pOut, An.Candidates, An.Centers); break; // 胜出候选所属中枢生命周期
    case 48: WriteCenterLifecycleSignal(nCount, pOut, An.Centers); break; // 相邻中枢生命周期关系
    case 49:
    case 50:
    case 51:
    case 52:
    case 56:
    case 57:
    case 58:
    {
      CzscConfig HighConfig = DefaultConfig();
      HighConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
      HighConfig.nSegmentMethod = CZSC_SEG_FEATURE;
      const CzscAnalyzer &HighAn = GetOrBuildPriceAnalyzer(nCount, pHigh, pLow, HighConfig);
      const CzscAnalyzer &LowAn = GetOrBuildPriceAnalyzer(nCount, pHigh, pLow, DefaultConfig());
      std::vector<NestedDivergenceContext> Contexts =
        BuildNestedDivergenceContexts(HighAn.Points, HighAn.Candidates,
                                      LowAn.Points, LowAn.Candidates);
      if (nOutput == 49)
      {
        ApplyNestedDivergenceLevel(nCount, pOut, Contexts);
      }
      else if (nOutput == 50)
      {
        ApplyNestedDivergenceSourceId(nCount, pOut, Contexts);
      }
      else if (nOutput == 51)
      {
        ApplyNestedDivergenceStartPointId(nCount, pOut, Contexts);
      }
      else if (nOutput == 52)
      {
        ApplyNestedDivergenceEndPointId(nCount, pOut, Contexts);
      }
      else if (nOutput == 56)
      {
        ApplyNestedDivergenceSemantic(nCount, pOut, Contexts);
      }
      else if (nOutput == 57)
      {
        ApplyNestedDivergenceConfirmFlags(nCount, pOut, Contexts);
      }
      else
      {
        ApplyNestedDivergenceDirection(nCount, pOut, Contexts);
      }
      break;
    }
    case 53: ApplyTradingSignalDivergenceSemantic(nCount, pOut, An.Candidates); break; // 胜出候选背驰语义
    case 54: ApplyTradingSignalReversalPointId(nCount, pOut, An.Candidates); break; // 一类背驰后首段回拉端点编号
    case 55: ApplyTradingFilterReasons(nCount, pOut, An.TradingFilterReasons); break; // 买卖点候选过滤原因
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
