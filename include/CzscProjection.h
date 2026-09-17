// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef __CZSC_PROJECTION_H__
#define __CZSC_PROJECTION_H__

#include "CzscTypes.h"

// New projections only: 0 is absent, -1 is unavailable/invalid. IDs are one-based.
// Integers outside the consecutive float32 range MUST NOT be rounded into IDs.
static const int CZSC_PROJECTION_MAX_INTEGER = 16777216;
float ProjectionInteger(long long nValue);
float ProjectionId(long long nIndex);
bool DecodeProjectionSlot(float fSlot, int *pSlot);

// nSlot is zero-based, in original vector order, separately for each source bar.
// These functions never mutate An or select a priority winner.
void ApplyNativeStructureProjection(int nCount, float *pOut,
                                    const CzscAnalyzer &An, int nOutput, int nSlot);
void ApplyNativeCandidateProjection(int nCount, float *pOut,
                                    const CzscAnalyzer &HighAn, int nOutput, int nSlot);

// 93-99: 所有行只写当前前缀末根；nField 是独立字段选择器。
void ApplyTrendEvidenceProjection(int nCount, float *pOut, const CzscAnalyzer &An,
                                   int nOutput, int nSlot, int nField);

#endif
