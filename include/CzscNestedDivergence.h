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
#ifndef __CZSC_NESTED_DIVERGENCE_H__
#define __CZSC_NESTED_DIVERGENCE_H__

#include "CzscTrading.h"

void WriteNestedDivergenceSignal(int nCount,
                                 float *pOut,
                                 const std::vector<SegmentPoint> &HighPoints,
                                 const std::vector<TradingSignalCandidate> &HighCandidates,
                                 const std::vector<SegmentPoint> &LowPoints,
                                 const std::vector<TradingSignalCandidate> &LowCandidates);
std::vector<NestedDivergenceContext> BuildNestedDivergenceContexts(
  const std::vector<SegmentPoint> &HighPoints,
  const std::vector<TradingSignalCandidate> &HighCandidates,
  const std::vector<SegmentPoint> &LowPoints,
  const std::vector<TradingSignalCandidate> &LowCandidates);
void ApplyNestedDivergenceLevel(int nCount, float *pOut, const std::vector<NestedDivergenceContext> &Contexts);
void ApplyNestedDivergenceSourceId(int nCount, float *pOut, const std::vector<NestedDivergenceContext> &Contexts);
void ApplyNestedDivergenceStartPointId(int nCount, float *pOut, const std::vector<NestedDivergenceContext> &Contexts);
void ApplyNestedDivergenceEndPointId(int nCount, float *pOut, const std::vector<NestedDivergenceContext> &Contexts);
void ApplyNestedDivergenceSemantic(int nCount, float *pOut, const std::vector<NestedDivergenceContext> &Contexts);
void ApplyNestedDivergenceConfirmFlags(int nCount, float *pOut, const std::vector<NestedDivergenceContext> &Contexts);
void ApplyNestedDivergenceDirection(int nCount, float *pOut, const std::vector<NestedDivergenceContext> &Contexts);

#endif
