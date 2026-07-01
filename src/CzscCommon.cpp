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
