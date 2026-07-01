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
#ifndef __CZSC_ANALYZER_H__
#define __CZSC_ANALYZER_H__

#include "CzscNestedDivergence.h"

// 中心化分析器的两个构建入口：信号 pIn 家族 / 原始 H/L+config 家族
void BuildAnalyzerFromSignal(CzscAnalyzer &An, int nCount, float *pIn, float *pHigh, float *pLow);
void BuildAnalyzerFromPrice(CzscAnalyzer &An, int nCount, float *pHigh, float *pLow, const CzscConfig &Config);

// 带缓存的入口：通达信就同一序列依次调多个 Func，首调用计算+缓存、后续命中（单槽+全字节指纹）
const CzscAnalyzer &GetOrBuildSignalAnalyzer(int nCount, float *pIn, float *pHigh, float *pLow);
const CzscAnalyzer &GetOrBuildPriceAnalyzer(int nCount, float *pHigh, float *pLow, const CzscConfig &Config);

#endif
