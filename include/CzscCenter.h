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
#ifndef __CZSC_CENTER_H__
#define __CZSC_CENTER_H__

#include "CzscMorphology.h"

// 中枢与走势类型，以及中枢关系/三买后续/背驰-转折的分类
std::vector<Center> BuildCenters(const std::vector<SegmentPoint> &Points);
std::vector<TrendStructure> BuildTrendStructures(const std::vector<Center> &Centers);
int ClassifyCenterRelation(const Center &Prev, const Center &Next);
int ClassifyCenterLifecycle(const Center &Prev, const Center &Next);
int ClassifyCenterAftermath(const std::vector<Center> &Centers, int nCenter, float fSignal);
int ClassifyReversalStrength(const std::vector<SegmentPoint> &Points,
                             const std::vector<Center> &Centers,
                             int nPoint,
                             int nCenter,
                             float fSignal);
void WriteCenterRelationSignal(int nCount, float *pOut, const std::vector<Center> &Centers);
void WriteCenterLifecycleSignal(int nCount, float *pOut, const std::vector<Center> &Centers);

// 第17/33课具名旁路分解；原始价格非法时拒绝，不把填补价格当递归证据。
SubTrendHierarchy BuildSubTrendHierarchy(int nCount, const float *pHigh, const float *pLow,
                                         const TrendAnchorContract &Contract);
std::vector<TrendCompletionEvidence> BuildTrendCompletionEvidence(
  const std::vector<SegmentPoint> &Points, const std::vector<Center> &Centers,
  const std::vector<TrendStructure> &Structures, const SubTrendHierarchy &Hierarchy,
  int nCount);
std::vector<ZhongYinEvidence> BuildZhongYinEvidence(
  const std::vector<TrendCompletionEvidence> &Evidence, int nCount,
  const std::vector<float> *pClose);

RecursiveMovementHierarchy BuildRecursiveMovements(int nCount, const float *pHigh,
  const float *pLow, const TrendAnchorContract &Contract);
std::vector<StructureAssociation> BuildStructureAssociations(
  const std::vector<Center> &Centers, const std::vector<TrendStructure> &Structures,
  const std::vector<TrendCompletionEvidence> &Evidence,
  const RecursiveMovementHierarchy &Hierarchy, const float *pHigh, const float *pLow);

#endif
