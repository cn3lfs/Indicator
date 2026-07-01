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
#ifndef __CZSC_TRADING_H__
#define __CZSC_TRADING_H__

#include "CzscDynamics.h"

// 买卖点：中枢突破 → 三类买卖点候选
std::vector<CenterBreakout> BuildCenterBreakouts(const std::vector<SegmentPoint> &Points,
                                                 const std::vector<Center> &Centers,
                                                 const std::vector<TrendStructure> &Structures);
std::vector<TradingSignalCandidate> BuildTradingSignalCandidates(const std::vector<SegmentPoint> &Points,
                                                                  const std::vector<Center> &Centers,
                                                                  const std::vector<TrendStructure> &Structures,
                                                                  const std::vector<CenterBreakout> &Breakouts);
int BuildTradingSignalContextFlags(const TradingSignalCandidate &C);
int BuildTradingSignalDivergenceSemantic(const TradingSignalCandidate &C);

// 把候选结果按优先级写入通达信输出数组
void ApplyTradingSignalCandidates(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalQuality(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalReversal(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalReversalPointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalAftermath(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalCenterPosition(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalMovementType(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalPriority(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalCenterId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalBreakoutId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalBreakoutLeavePointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates, const std::vector<CenterBreakout> &Breakouts);
void ApplyTradingSignalBreakoutRetestPointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates, const std::vector<CenterBreakout> &Breakouts);
void ApplyTradingSignalPointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalTrendId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalSecondBasePointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalSecondTurnPointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalSmallTurn(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalSmallTurnBasePointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalSmallTurnLeavePointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates, const std::vector<CenterBreakout> &Breakouts);
void ApplyTradingSignalSmallTurnRetestPointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates, const std::vector<CenterBreakout> &Breakouts);
void ApplyTradingSignalAbcStructure(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalAbcBreakoutId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalAbcBreakoutLeavePointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates, const std::vector<CenterBreakout> &Breakouts);
void ApplyTradingSignalAbcBreakoutRetestPointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates, const std::vector<CenterBreakout> &Breakouts);
void ApplyTradingSignalStrictAbcCandidates(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalMacdLineWeakness(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalMacdZeroPullback(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalStandardDivergence(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalMacdAreaRatio(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalSpaceRatio(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalSpeedRatio(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalDivergenceFlags(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalDivergencePreviousStartPointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalDivergencePreviousEndPointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalDivergenceCurrentStartPointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalDivergenceCurrentEndPointId(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalContextFlags(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalDivergenceSemantic(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalCenterLifecycle(int nCount, float *pOut, const std::vector<TradingSignalCandidate> &Candidates, const std::vector<Center> &Centers);

#endif
