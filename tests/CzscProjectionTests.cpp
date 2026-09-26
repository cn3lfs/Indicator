// SPDX-License-Identifier: GPL-3.0-or-later
#include "../CzscCore.h"
#include "SseIndexDaily.h"
#include <limits>

bool TestNativeProjectionIntegerBounds()
{
  const long long nMax = 16777216;
  int nSlot = -1;
  if ((ProjectionInteger(nMax - 1) != 16777215.0f) ||
      (ProjectionInteger(nMax) != 16777216.0f) ||
      (ProjectionInteger(nMax + 1) != -1.0f) ||
      (ProjectionInteger(-1) != -1.0f) ||
      (ProjectionId(nMax - 2) != 16777215.0f) ||
      (ProjectionId(nMax - 1) != 16777216.0f) ||
      (ProjectionId(nMax) != -1.0f) ||
      (ProjectionId(nMax + 1) != -1.0f) ||
      (ProjectionId(-1) != 0.0f) || (ProjectionId(-2) != -1.0f))
  {
    return false;
  }
  return DecodeProjectionSlot(16777215.0f, &nSlot) && (nSlot == 16777215) &&
         !DecodeProjectionSlot(16777216.0f, &nSlot) &&
         !DecodeProjectionSlot(-1.0f, &nSlot) &&
         !DecodeProjectionSlot(0.5f, &nSlot) &&
         !DecodeProjectionSlot(std::numeric_limits<float>::infinity(), &nSlot) &&
         !DecodeProjectionSlot(std::numeric_limits<float>::quiet_NaN(), &nSlot) &&
         DecodeProjectionSlot(0.0f, &nSlot) && (nSlot == 0);
}

// Fixed synthetic memberships: a two-center trend and an overlapping boundary.
// Expected arrays below are literal contract expectations, not golden captures.
bool TestNativeStructureMemberships()
{
  const int nCount = 9;
  CzscAnalyzer An;
  An.Config = DefaultConfig();
  An.Centers.resize(3);
  TrendStructure A = {1, 1, 5, 0, 1};
  TrendStructure B = {0, 5, 7, 2, 2};
  An.Structures.push_back(A);
  An.Structures.push_back(B);
  float Out[nCount];
  const float Counts[nCount] = {0, 1, 1, 1, 1, 2, 1, 1, 0};
  ApplyNativeStructureProjection(nCount, Out, An, 59, 0);
  for (int i = 0; i < nCount; i++)
  {
    if (Out[i] != Counts[i]) return false;
  }
  // Every column of the first selected structure, at an interior bar.
  const float Expected[] = {1, 1, -1, 1, 2, 6, 1, 2, 2, -1};
  for (int nOutput = 60; nOutput <= 69; nOutput++)
  {
    ApplyNativeStructureProjection(nCount, Out, An, nOutput, 0);
    if ((Out[3] != Expected[nOutput - 60]) || (Out[0] != 0) || (Out[8] != 0)) return false;
  }
  ApplyNativeStructureProjection(nCount, Out, An, 60, 1);
  for (int i = 0; i < nCount; i++)
  {
    if (Out[i] != ((i == 5) ? 2.0f : 0.0f)) return false;
  }
  // Even the earlier structure has UNKNOWN completion: successor != proof.
  ApplyNativeStructureProjection(nCount, Out, An, 62, 1);
  if (Out[5] != -1.0f) return false;
  An.Config.nCenterUnit = CZSC_UNIT_SEGMENT;
  ApplyNativeStructureProjection(nCount, Out, An, 61, 0);
  if (Out[3] != 2) return false;
  An.Structures[0].nLastCenter = 3;
  ApplyNativeStructureProjection(nCount, Out, An, 60, 0);
  for (int i = 0; i < nCount; i++) if (Out[i] != -1) return false;
  An.Structures.clear();
  for (int nOutput = 59; nOutput <= 69; nOutput++)
  {
    ApplyNativeStructureProjection(nCount, Out, An, nOutput, 0);
    for (int i = 0; i < nCount; i++) if (Out[i] != 0) return false;
  }
  return true;
}

bool TestNativeCandidateSlots()
{
  const int nCount = 8;
  CzscAnalyzer An;
  An.Config = DefaultConfig();
  An.Config.nCenterUnit = CZSC_UNIT_SEGMENT;
  An.Points.resize(4);
  An.Points[0].nIndex = 0;
  An.Points[1].nIndex = 2;
  An.Points[2].nIndex = 5;
  An.Points[3].nIndex = 7;
  TradingSignalCandidate A = {};
  A.nIndex = 5;
  A.fSignal = 1;
  A.nPoint = 2;
  A.nTrend = 0;
  A.nCenter = 1;
  A.nSource = 1;
  A.nPriority = 10;
  A.nQuality = 2;
  A.nMovementType = -1;
  A.Divergence.nDirection = -1;
  A.Divergence.nCurrentStartPoint = 0;
  A.Divergence.nCurrentEndPoint = 2;
  A.Divergence.bDivergence = true;
  A.Divergence.bNewExtreme = true;
  A.Divergence.bWeakSpace = true;
  A.Divergence.bWeakSpeed = false;
  A.Divergence.bWeakMacd = true;
  TradingSignalCandidate B = A;
  B.fSignal = 3;
  B.nSource = 3;
  B.nPriority = 30;
  B.nTrend = -1;
  B.Divergence.bNewExtreme = false;
  An.Candidates.push_back(A);
  An.Candidates.push_back(B);
  An.Candidates.push_back(A); // More than two slots; equal rows retain distinct IDs.
  float Out[nCount], WinnerBefore[nCount], WinnerAfter[nCount];
  ApplyTradingSignalCandidates(nCount, WinnerBefore, An.Candidates);
  ApplyNativeCandidateProjection(nCount, Out, An, 70, 0);
  if (Out[5] != 3) return false;
  const float Expected[] = {1, 1, 3, 2, 3, 3, 6, 1, 1, 1, 2, 1, 10, 2, 2, 1, 1, 0, 1, 1, 6};
  for (int nOutput = 71; nOutput <= 91; nOutput++)
  {
    ApplyNativeCandidateProjection(nCount, Out, An, nOutput, 0);
    if (Out[5] != Expected[nOutput - 71]) return false;
    for (int i = 0; i < nCount; i++) if ((i != 5) && (Out[i] != 0)) return false;
  }
  for (int nSlot = 0; nSlot <= 3; nSlot++)
  {
    ApplyNativeCandidateProjection(nCount, Out, An, 71, nSlot);
    if (Out[5] != ((nSlot < 3) ? (float)(nSlot + 1) : 0.0f)) return false;
  }
  ApplyNativeCandidateProjection(nCount, Out, An, 72, 1);
  if (Out[5] != 3) return false;
  ApplyNativeCandidateProjection(nCount, Out, An, 80, 1);
  if (Out[5] != 0) return false;
  ApplyTradingSignalCandidates(nCount, WinnerAfter, An.Candidates);
  for (int i = 0; i < nCount; i++) if (WinnerBefore[i] != WinnerAfter[i]) return false;
  An.Candidates[0].nTrend = 16777215;
  ApplyNativeCandidateProjection(nCount, Out, An, 80, 0);
  if (Out[5] != 16777216.0f) return false;
  An.Candidates[0].nTrend = 16777216;
  ApplyNativeCandidateProjection(nCount, Out, An, 80, 0);
  if (Out[5] != -1.0f) return false;
  An.Candidates[0].nIndex = nCount;
  ApplyNativeCandidateProjection(nCount, Out, An, 71, 0);
  for (int i = 0; i < nCount; i++) if (Out[i] != -1) return false;
  An.Candidates.clear();
  for (int nOutput = 70; nOutput <= 91; nOutput++)
  {
    ApplyNativeCandidateProjection(nCount, Out, An, nOutput, 0);
    for (int i = 0; i < nCount; i++) if (Out[i] != 0) return false;
  }
  return true;
}

bool TestNativeProjectionSseRouting()
{
  // Scope guard restores the global aux slot even on an assertion failure.
  struct AuxScope
  {
    AuxScope() { RegisterAuxData(0, 0, 0); }
    ~AuxScope() { RegisterAuxData(0, 0, 0); }
  } Scope;
  const int nCount = SSE_DAILY_COUNT;
  std::vector<float> High(SSE_DAILY_HIGH, SSE_DAILY_HIGH + nCount);
  std::vector<float> Low(SSE_DAILY_LOW, SSE_DAILY_LOW + nCount);
  std::vector<float> Mode((std::size_t)nCount, 0.0f);
  std::vector<float> Out((std::size_t)nCount, 0.0f);
  CzscConfig HighConfig = DefaultConfig();
  HighConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
  HighConfig.nSegmentMethod = CZSC_SEG_FEATURE;
  CzscAnalyzer HighAn;
  BuildAnalyzerFromPrice(HighAn, nCount, &High[0], &Low[0], HighConfig);
  // SSE 线段级只有 1 个中枢（第71课起点顺延后），不构成趋势 → 无线段级候选；下方循环按实际候选数校验
  if (HighAn.Centers.size() != 1 || !HighAn.Candidates.empty()) return false;
  const int ConfigCodes[] = {0, 1100};
  for (int c = 0; c < 2; c++)
  {
    CzscAnalyzer An;
    BuildAnalyzerFromPrice(An, nCount, &High[0], &Low[0], DecodeConfig((float)ConfigCodes[c]));
    std::vector<float> Expected((std::size_t)nCount, 0.0f);
    for (std::size_t t = 0; t < An.Structures.size(); t++)
    {
      const TrendStructure &T = An.Structures[t];
      for (int i = T.nStart; i <= T.nEnd; i++) Expected[(std::size_t)i] += 1;
    }
    Mode[0] = (float)(ConfigCodes[c] * 1000 + 590);
    Func30(nCount, &Out[0], &High[0], &Low[0], &Mode[0]);
    if (Out != Expected) return false;
    // The high table ignores requested low config, exactly like output 50.
    for (std::size_t k = 0; k < HighAn.Candidates.size(); k++)
    {
      const TradingSignalCandidate &C = HighAn.Candidates[k];
      int nSlot = 0;
      for (std::size_t j = 0; j < k; j++) if (HighAn.Candidates[j].nIndex == C.nIndex) nSlot++;
      Mode[1] = (float)nSlot;
      Mode[0] = (float)(ConfigCodes[c] * 1000 + 710);
      Func30(nCount, &Out[0], &High[0], &Low[0], &Mode[0]);
      if (Out[(std::size_t)C.nIndex] != (float)(k + 1)) return false;
      Mode[0] = (float)(ConfigCodes[c] * 1000 + 760);
      Func30(nCount, &Out[0], &High[0], &Low[0], &Mode[0]);
      if (Out[(std::size_t)C.nIndex] != (float)(HighAn.Points[(std::size_t)(C.nPoint - 1)].nIndex + 1)) return false;
    }
    Mode[0] = (float)(ConfigCodes[c] * 1000 + 710);
    Mode[1] = 0.5f;
    Func30(nCount, &Out[0], &High[0], &Low[0], &Mode[0]);
    for (int i = 0; i < nCount; i++) if (Out[(std::size_t)i] != -1) return false;
    Mode[1] = 0;
  }
  return true;
}

bool TestNativeCompletedSequenceUnavailable()
{
  // Neither a full history with several structures nor a flat empty history
  // establishes completion. -1 must not silently turn into "known empty" 0.
  std::vector<float> High(SSE_DAILY_HIGH, SSE_DAILY_HIGH + SSE_DAILY_COUNT);
  std::vector<float> Low(SSE_DAILY_LOW, SSE_DAILY_LOW + SSE_DAILY_COUNT);
  std::vector<float> Out((std::size_t)SSE_DAILY_COUNT, 9.0f);
  float fMode = 920;
  Func30(SSE_DAILY_COUNT, &Out[0], &High[0], &Low[0], &fMode);
  for (int i = 0; i < SSE_DAILY_COUNT; i++) if (Out[(std::size_t)i] != -1) return false;
  float HighEmpty[] = {2, 2, 2};
  float LowEmpty[] = {1, 1, 1};
  float EmptyOut[] = {9, 9, 9};
  Func30(3, EmptyOut, HighEmpty, LowEmpty, &fMode);
  return (EmptyOut[0] == -1) && (EmptyOut[1] == -1) && (EmptyOut[2] == -1);
}
