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

#ifndef __CZSC_CORE_H__
#define __CZSC_CORE_H__

#include "CCentroid.h"

void Parse1(int nCount, float *pOut, float *pHigh, float *pLow);
void Parse2(int nCount, float *pOut, float *pHigh, float *pLow);

void Func1(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime);
void Func2(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func3(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func4(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func5(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func6(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func7(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func8(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);

#endif
