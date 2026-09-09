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
#ifndef __CZSC_DYNAMICS_H__
#define __CZSC_DYNAMICS_H__

#include "CzscCenter.h"

// 动力学：MACD 柱面积及其在线段点上的累积能量
std::vector<float> ComputeMacdHistogram(int nCount, const float *pPrice);
void AssignSegmentEnergy(std::vector<SegmentPoint> &Points, int nCount, const float *pHigh, const float *pLow);

// 均线系统（第11-15课）：简单移动平均；短长均线的吻分类（飞吻/唇吻/湿吻）
std::vector<float> ComputeMovingAverage(int nCount, const float *pPrice, int nPeriod);
std::vector<int> ClassifyMaKisses(const std::vector<float> &Short, const std::vector<float> &Long);
std::vector<int> ClassifyMaKissesWithVolume(const std::vector<float> &Short, const std::vector<float> &Long,
                                            const std::vector<float> &Volume);
void ComputeShortLongMa(int nCount, float *pHigh, float *pLow,
                        std::vector<float> *pShort, std::vector<float> *pLong);

// 力度/背驰度量
StrengthMetrics MeasureStrength(const SegmentPoint &Start, const SegmentPoint &End);
DivergenceResult MeasureDivergence(const SegmentPoint &PrevStart,
                                   const SegmentPoint &PrevEnd,
                                   const SegmentPoint &CurrentStart,
                                   const SegmentPoint &CurrentEnd,
                                   int nDirection);
bool IsWeakerStrength(const StrengthMetrics &Current, const StrengthMetrics &Previous);
int DetectInstantDivergence(const std::vector<SegmentPoint> &Points,
                            int nCount,
                            const float *pHigh,
                            const float *pLow);
int BuildDivergenceFlags(const DivergenceResult &D);

#endif
