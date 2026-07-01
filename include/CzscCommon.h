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
#ifndef __CZSC_COMMON_H__
#define __CZSC_COMMON_H__

#include "CzscTypes.h"

// 配置：默认配置复现现状；DecodeConfig 把单个数字码解出四维配置（供通达信传参）
CzscConfig DefaultConfig();
CzscConfig DecodeConfig(float fCode);

// 旁路数据：通达信 3 槽不够同时传 H/L/C/V，故用一个注册函数把真实收盘价/成交量存入全局旁路缓存，
// 其余函数在 nCount 匹配且 Close∈[Low,High] 内容校验通过时自动改用真实 C（否则回落 (H+L)/2）。
void RegisterAuxData(int nCount, float *pClose, float *pVolume);
const std::vector<float> *GetValidatedClose(int nCount, const float *pHigh, const float *pLow);
const std::vector<float> *GetValidatedVolume(int nCount, const float *pHigh, const float *pLow);

#endif
