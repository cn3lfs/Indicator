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
#ifndef __CZSC_MORPHOLOGY_H__
#define __CZSC_MORPHOLOGY_H__

#include "CzscCommon.h"

// 形态学流水线：合并K线 → 分型 → 笔 → 线段点 / 线段；以及由信号还原线段点
std::vector<MergedBar> BuildMergedBars(int nCount, float *pHigh, float *pLow);
std::vector<Fractal> BuildFractals(const std::vector<MergedBar> &Bars);
std::vector<Stroke> BuildStrokes(const std::vector<Fractal> &Fractals, const CzscConfig &Config = DefaultConfig());
std::vector<SegmentPoint> BuildSegmentPoints(const std::vector<Stroke> &Strokes);
std::vector<SegmentPoint> BuildLineSegmentPoints(const std::vector<Stroke> &Strokes);
std::vector<SegmentPoint> BuildLineSegmentPointsByFeature(const std::vector<Stroke> &Strokes);
std::vector<SegmentPoint> BuildSignalPoints(int nCount, float *pIn, float *pHigh, float *pLow);
std::vector<SegmentPoint> BuildConfiguredPoints(int nCount, float *pHigh, float *pLow, const CzscConfig &Config);
void WriteSegmentSignal(int nCount, float *pOut, const std::vector<SegmentPoint> &Points);

// 顶底扫描与化简（早期版本的笔识别，保留供通达信公式使用）
void Parse1(int nCount, float *pOut, float *pHigh, float *pLow);
void Parse2(int nCount, float *pOut, float *pHigh, float *pLow);

#endif
