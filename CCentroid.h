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

#ifndef __CCENTROID_H__
#define __CCENTROID_H__

// 早期的中枢（质心）状态机，保留供参考。现行中枢计算见 CzscCore.h 的 Center 与 BuildCenters。
// 增量送入高/低点（PushHigh/PushLow），内部维护前两顶两底以判定中枢区间。
struct CCentroid
{
  bool  bValid;
  int   nTop1, nTop2, nBot1, nBot2;   // 最近两个顶、两个底的下标
  float fTop1, fTop2, fBot1, fBot2;   // 对应价位
  int   nLines, nStart, nEnd;
  float fHigh, fLow, fPHigh, fPLow;

  CCentroid();
  ~CCentroid();

  bool PushHigh(int nIndex, float fValue);
  bool PushLow (int nIndex, float fValue);
};

#endif
