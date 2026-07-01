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
// 中心化分析器：Points 就绪后统一走中枢及以下流水线，供各 Func 投影（消除重复重算）
//=============================================================================

// 从线段点一次算成 中枢→走势→突破→买卖点候选（统一赋 MACD 能量；不读能量的输出不受影响）
static void BuildCentersStage(CzscAnalyzer &An, int nCount, float *pHigh, float *pLow)
{
  AssignSegmentEnergy(An.Points, nCount, pHigh, pLow);
  An.Centers = BuildCenters(An.Points);
  An.Structures = BuildTrendStructures(An.Centers);
  An.Breakouts = BuildCenterBreakouts(An.Points, An.Centers, An.Structures);
  An.Candidates = BuildTradingSignalCandidates(An.Points, An.Centers, An.Structures, An.Breakouts);
  An.TradingFilterReasons = BuildTradingFilterReasons(An.Points,
                                                       An.Centers,
                                                       An.Structures,
                                                       An.Breakouts,
                                                       An.Candidates);
}

// 旁路成交量（注册且校验通过则填，否则空）
static void FillAuxVolume(CzscAnalyzer &An, int nCount, float *pHigh, float *pLow)
{
  const std::vector<float> *pVV = GetValidatedVolume(nCount, pHigh, pLow);
  An.Volume = pVV ? *pVV : std::vector<float>();
}

// 信号 pIn 家族：由通达信传入的 ±1 线段点信号还原 Points，再走统一下游
void BuildAnalyzerFromSignal(CzscAnalyzer &An, int nCount, float *pIn, float *pHigh, float *pLow)
{
  An.nCount = nCount;
  An.Config = DefaultConfig();
  An.Points = BuildSignalPoints(nCount, pIn, pHigh, pLow);
  BuildCentersStage(An, nCount, pHigh, pLow);
  FillAuxVolume(An, nCount, pHigh, pLow);
}

// 原始 H/L+config 家族：按配置直接算出 Points，再走统一下游，并附带均线序列与吻
void BuildAnalyzerFromPrice(CzscAnalyzer &An, int nCount, float *pHigh, float *pLow, const CzscConfig &Config)
{
  An.nCount = nCount;
  An.Config = Config;
  An.Points = BuildConfiguredPoints(nCount, pHigh, pLow, Config);
  BuildCentersStage(An, nCount, pHigh, pLow);
  ComputeShortLongMa(nCount, pHigh, pLow, &An.MaShort, &An.MaLong);
  An.Kiss = ClassifyMaKisses(An.MaShort, An.MaLong);
  FillAuxVolume(An, nCount, pHigh, pLow);
  An.KissVol = ClassifyMaKissesWithVolume(An.MaShort, An.MaLong, An.Volume);  // 带量校验，无量退化为纯价吻
}

//=============================================================================
// 缓存层：按全字节 FNV-1a 指纹做单槽缓存，消除同一序列连调多 Func 的重复计算
//=============================================================================

// 把一段 float 数组按 bit pattern 累积进 FNV-1a 32 位哈希（相同位模式→相同哈希）
static unsigned int FnvAccumFloats(unsigned int hHash, const float *pData, int nCount)
{
  const unsigned char *pBytes = (const unsigned char *)pData;
  int nTotal = nCount * (int)sizeof(float);
  for (int i = 0; i < nTotal; i++)
  {
    hHash ^= (unsigned int)pBytes[i];
    hHash *= 16777619u;
  }
  return hHash;
}

static unsigned int HashHL(const float *pHigh, const float *pLow, int nCount)
{
  unsigned int hHash = 2166136261u;
  hHash = FnvAccumFloats(hHash, pHigh, nCount);
  hHash = FnvAccumFloats(hHash, pLow, nCount);
  return hHash;
}

// 旁路指纹：校验通过的收盘价（+成交量）的 FNV，否则 0；使「是否用了真实 C/V」进入缓存键，
// 注册的 C 或 V 变化即失效重算（KissVol 取决于 V，故指纹须同时覆盖成交量）
static unsigned int HashAux(int nCount, float *pHigh, float *pLow)
{
  const std::vector<float> *pVC = GetValidatedClose(nCount, pHigh, pLow);
  if (pVC == 0)
  {
    return 0u;
  }
  unsigned int hHash = FnvAccumFloats(2166136261u, &(*pVC)[0], nCount);
  const std::vector<float> *pVV = GetValidatedVolume(nCount, pHigh, pLow);
  if ((pVV != 0) && ((int)pVV->size() == nCount))
  {
    hHash = FnvAccumFloats(hHash, &(*pVV)[0], nCount);
  }
  return hHash;
}

const CzscAnalyzer &GetOrBuildSignalAnalyzer(int nCount, float *pIn, float *pHigh, float *pLow)
{
  static CzscAnalyzer s_Analyzer;
  static int s_nCount = -1;
  static unsigned int s_hHL = 0;
  static unsigned int s_hIn = 0;
  static unsigned int s_hAux = 0;
  static bool s_bValid = false;

  unsigned int hHL = HashHL(pHigh, pLow, nCount);
  unsigned int hIn = FnvAccumFloats(2166136261u, pIn, nCount);
  unsigned int hAux = HashAux(nCount, pHigh, pLow);
  if (s_bValid && (s_nCount == nCount) && (s_hHL == hHL) && (s_hIn == hIn) && (s_hAux == hAux))
  {
    return s_Analyzer;  // 命中
  }

  BuildAnalyzerFromSignal(s_Analyzer, nCount, pIn, pHigh, pLow);
  s_nCount = nCount; s_hHL = hHL; s_hIn = hIn; s_hAux = hAux; s_bValid = true;
  return s_Analyzer;
}

const CzscAnalyzer &GetOrBuildPriceAnalyzer(int nCount, float *pHigh, float *pLow, const CzscConfig &Config)
{
  static CzscAnalyzer s_Analyzer;
  static int s_nCount = -1;
  static unsigned int s_hHL = 0;
  static int s_nConfig = -1;
  static unsigned int s_hAux = 0;
  static bool s_bValid = false;

  unsigned int hHL = HashHL(pHigh, pLow, nCount);
  int nConfig = Config.nStrokeType + Config.nStrokeEnd * 10 +
                Config.nCenterUnit * 100 + Config.nSegmentMethod * 1000;
  unsigned int hAux = HashAux(nCount, pHigh, pLow);
  if (s_bValid && (s_nCount == nCount) && (s_hHL == hHL) && (s_nConfig == nConfig) && (s_hAux == hAux))
  {
    return s_Analyzer;  // 命中
  }

  BuildAnalyzerFromPrice(s_Analyzer, nCount, pHigh, pLow, Config);
  s_nCount = nCount; s_hHL = hHL; s_nConfig = nConfig; s_hAux = hAux; s_bValid = true;
  return s_Analyzer;
}
