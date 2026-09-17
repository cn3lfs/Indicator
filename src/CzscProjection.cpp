// SPDX-License-Identifier: GPL-3.0-or-later
#include "CzscInternal.h"
#include "CzscProjection.h"

float ProjectionInteger(long long nValue)
{
  if ((nValue < 0) || (nValue > CZSC_PROJECTION_MAX_INTEGER))
  {
    return -1.0f;
  }
  return (float)nValue;
}

float ProjectionId(long long nIndex)
{
  if (nIndex == -1)
  {
    return 0.0f;
  }
  if ((nIndex < 0) || (nIndex >= CZSC_PROJECTION_MAX_INTEGER))
  {
    return -1.0f;
  }
  return (float)(nIndex + 1);
}

bool DecodeProjectionSlot(float fSlot, int *pSlot)
{
  // Ordered comparisons reject NaN and infinity before any float-to-int cast.
  if ((pSlot == 0) || !(fSlot >= 0.0f) ||
      !(fSlot < (float)CZSC_PROJECTION_MAX_INTEGER))
  {
    return false;
  }
  int nSlot = (int)fSlot;
  if ((float)nSlot != fSlot)
  {
    return false;
  }
  *pSlot = nSlot;
  return true;
}

static void UnavailableProjection(int nCount, float *pOut)
{
  for (int i = 0; i < nCount; i++)
  {
    pOut[i] = -1.0f;
  }
}

static bool PrepareProjection(int nCount, float *pOut, int nSlot)
{
  if (!HasOutput(nCount, pOut))
  {
    return false;
  }
  if ((nCount > CZSC_PROJECTION_MAX_INTEGER) ||
      (nSlot < 0) || (nSlot >= CZSC_PROJECTION_MAX_INTEGER))
  {
    UnavailableProjection(nCount, pOut);
    return false;
  }
  ClearOutput(nCount, pOut);
  return true;
}

static float PointBarId(const CzscAnalyzer &An, int nPoint, int nCount)
{
  if (nPoint == -1)
  {
    return 0.0f;
  }
  if ((nPoint < 0) || ((std::size_t)nPoint >= An.Points.size()))
  {
    return -1.0f;
  }
  int nBar = An.Points[(std::size_t)nPoint].nIndex;
  return ((nBar >= 0) && (nBar < nCount)) ? ProjectionId(nBar) : -1.0f;
}

void ApplyNativeStructureProjection(int nCount, float *pOut,
                                    const CzscAnalyzer &An, int nOutput, int nSlot)
{
  if (!PrepareProjection(nCount, pOut, nSlot))
  {
    return;
  }
  if ((An.Structures.size() > (std::size_t)CZSC_PROJECTION_MAX_INTEGER) ||
      (An.Centers.size() > (std::size_t)CZSC_PROJECTION_MAX_INTEGER))
  {
    UnavailableProjection(nCount, pOut);
    return;
  }
  // This is a coverage join, not a trend/completion algorithm. Center membership
  // is exactly the inclusive [nFirstCenter,nLastCenter] stored by the analyzer.
  std::vector<int> Slots((std::size_t)nCount, 0);
  for (std::size_t i = 0; i < An.Structures.size(); i++)
  {
    const TrendStructure &T = An.Structures[i];
    if ((T.nStart < 0) || (T.nEnd < T.nStart) || (T.nEnd >= nCount) ||
        (T.nFirstCenter < 0) || (T.nLastCenter < T.nFirstCenter) ||
        ((std::size_t)T.nLastCenter >= An.Centers.size()))
    {
      UnavailableProjection(nCount, pOut);
      return;
    }
    for (int nBar = T.nStart; nBar <= T.nEnd; nBar++)
    {
      int nCurrentSlot = Slots[(std::size_t)nBar]++;
      if (nOutput == 59)
      {
        pOut[nBar] = ProjectionInteger(Slots[(std::size_t)nBar]);
        continue;
      }
      if (nCurrentSlot != nSlot)
      {
        continue;
      }
      switch (nOutput)
      {
        case 60: pOut[nBar] = ProjectionId((long long)i); break;
        // Structural unit only, NOT a proved Chan theoretical/timeframe level.
        case 61: pOut[nBar] = (float)(An.Config.nCenterUnit + 1); break;
        case 62: pOut[nBar] = -1.0f; break; // completion is NOT stored
        case 63: pOut[nBar] = (float)T.nType; break;
        case 64: pOut[nBar] = ProjectionId(T.nStart); break;
        case 65: pOut[nBar] = ProjectionId(T.nEnd); break;
        case 66: pOut[nBar] = ProjectionId(T.nFirstCenter); break;
        case 67: pOut[nBar] = ProjectionId(T.nLastCenter); break;
        case 68: pOut[nBar] = ProjectionInteger((long long)T.nLastCenter - T.nFirstCenter + 1); break;
        case 69: pOut[nBar] = -1.0f; break; // theoretical level is NOT stored
      }
    }
  }
}

void ApplyNativeCandidateProjection(int nCount, float *pOut,
                                    const CzscAnalyzer &HighAn, int nOutput, int nSlot)
{
  if (!PrepareProjection(nCount, pOut, nSlot))
  {
    return;
  }
  if ((HighAn.Candidates.size() > (std::size_t)CZSC_PROJECTION_MAX_INTEGER) ||
      (HighAn.Points.size() > (std::size_t)CZSC_PROJECTION_MAX_INTEGER))
  {
    UnavailableProjection(nCount, pOut);
    return;
  }
  std::vector<int> Slots((std::size_t)nCount, 0);
  for (std::size_t i = 0; i < HighAn.Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = HighAn.Candidates[i];
    if ((C.nIndex < 0) || (C.nIndex >= nCount))
    {
      // Never silently discard an unaddressable row from the "complete" table.
      UnavailableProjection(nCount, pOut);
      return;
    }
    int nCurrentSlot = Slots[(std::size_t)C.nIndex]++;
    if (nOutput == 70)
    {
      pOut[C.nIndex] = ProjectionInteger(Slots[(std::size_t)C.nIndex]);
      continue;
    }
    if (nCurrentSlot != nSlot)
    {
      continue;
    }
    float fValue = 0.0f;
    switch (nOutput)
    {
      case 71: fValue = ProjectionId((long long)i); break;
      case 72: fValue = C.fSignal; break;
      case 73: fValue = ProjectionId(C.nPoint); break;
      // The enclosing segment used by BuildNestedDivergenceContexts is
      // Points[C.nPoint-1] -> C.nIndex; it is NOT necessarily Divergence.Current.
      case 74: fValue = (C.nPoint >= 1) ? ProjectionId((long long)C.nPoint - 1) : 0.0f; break;
      case 75: fValue = (C.nPoint >= 1) ? ProjectionId(C.nPoint) : 0.0f; break;
      case 76: fValue = (C.nPoint >= 1) ? PointBarId(HighAn, C.nPoint - 1, nCount) : 0.0f; break;
      case 77: fValue = (C.nPoint >= 1) ? ProjectionId(C.nIndex) : 0.0f; break;
      case 78: fValue = (float)BuildTradingSignalDivergenceSemantic(C); break;
      case 79: fValue = C.Divergence.bDivergence ? 1.0f : 0.0f; break;
      case 80: fValue = ProjectionId(C.nTrend); break;
      case 81: fValue = ProjectionId(C.nCenter); break;
      case 82: fValue = ProjectionInteger(C.nSource); break;
      case 83: fValue = ProjectionInteger(C.nPriority); break;
      case 84: fValue = ProjectionInteger(C.nQuality); break;
      case 85: fValue = (float)(HighAn.Config.nCenterUnit + 1); break;
      case 86: fValue = C.Divergence.bNewExtreme ? 1.0f : 0.0f; break;
      case 87: fValue = C.Divergence.bWeakSpace ? 1.0f : 0.0f; break;
      case 88: fValue = C.Divergence.bWeakSpeed ? 1.0f : 0.0f; break;
      case 89: fValue = C.Divergence.bWeakMacd ? 1.0f : 0.0f; break;
      case 90: fValue = PointBarId(HighAn, C.Divergence.nCurrentStartPoint, nCount); break;
      case 91: fValue = PointBarId(HighAn, C.Divergence.nCurrentEndPoint, nCount); break;
    }
    pOut[C.nIndex] = fValue;
  }
}

void ApplyTrendEvidenceProjection(int nCount, float *pOut, const CzscAnalyzer &An,
                                   int nOutput, int nSlot, int nField)
{
  if (!PrepareProjection(nCount, pOut, nSlot)) return;
  if (nOutput >= 100 && nOutput <= 108)
  {
    const RecursiveMovementHierarchy &H = An.MovementHierarchy;
    if (!H.bAvailable || An.nCount != nCount || nField < 0 ||
        H.Centers.size() > CZSC_PROJECTION_MAX_INTEGER ||
        H.Movements.size() > CZSC_PROJECTION_MAX_INTEGER ||
        H.Connections.size() > CZSC_PROJECTION_MAX_INTEGER ||
        H.Associations.size() > CZSC_PROJECTION_MAX_INTEGER)
    { UnavailableProjection(nCount,pOut); return; }
    float &Out = pOut[nCount-1];
    if (nOutput == 100 || nOutput % 2 == 1)
    {
      if (nField != 0) { UnavailableProjection(nCount,pOut); return; }
      if (nOutput == 100) Out = 4; // capability/version; 旧DLL不会返回4
      if (nOutput == 101) Out = ProjectionInteger(H.Centers.size());
      if (nOutput == 103) Out = ProjectionInteger(H.Movements.size());
      if (nOutput == 105) Out = ProjectionInteger(H.Connections.size());
      if (nOutput == 107) Out = ProjectionInteger(H.Associations.size());
      return;
    }
    if (nOutput == 102)
    {
      if (nField > 16 && nField < 100) { UnavailableProjection(nCount,pOut); return; }
      if (static_cast<std::size_t>(nSlot) >= H.Centers.size()) return;
      const SubTrendNode &N = H.Centers[nSlot];
      switch (nField)
      {
        case 0: Out=ProjectionId(nSlot); break;
        case 1: Out=ProjectionInteger(N.nLevel); break;
        case 2: Out=ProjectionId(N.nStart); break;
        case 3: Out=ProjectionId(N.nEnd); break;
        case 4: Out=ProjectionId(N.nCenterStart); break;
        case 5: Out=ProjectionId(N.nCenterEnd); break;
        case 6: Out=ProjectionId(N.nEstablishedAt); break;
        case 7: Out=ProjectionId(N.nConnection); break;
        case 8: Out=ProjectionId(N.nCompletedAt); break;
        case 9: Out=ProjectionId(N.nSuccessor); break;
        case 10: Out=ProjectionInteger(N.Children.size()); break;
        case 11: Out=ProjectionInteger(H.nAnchorVersion); break;
        case 12: Out=2; break;
        case 13: Out=N.fHigh; break;
        case 14: Out=N.fLow; break;
        case 15: Out=N.fCenterHigh; break;
        case 16: Out=N.fCenterLow; break;
        default: if (static_cast<std::size_t>(nField-100)<N.Children.size())
          Out=ProjectionId(N.Children[nField-100]); break;
      }
    }
    else if (nOutput == 104)
    {
      if (nField > 13 && nField < 100) { UnavailableProjection(nCount,pOut); return; }
      if (static_cast<std::size_t>(nSlot) >= H.Movements.size()) return;
      const RecursiveMovement &M = H.Movements[nSlot];
      switch (nField)
      {
        case 0: Out=ProjectionId(nSlot); break;
        case 1: Out=ProjectionInteger(M.nLevel); break;
        case 2: Out=static_cast<float>(M.nType); break;
        case 3: Out=ProjectionId(M.nStart); break;
        case 4: Out=ProjectionId(M.nEnd); break;
        case 5: Out=ProjectionId(M.nEstablishedAt); break;
        case 6: Out=ProjectionId(M.nCompletedAt); break;
        case 7: Out=ProjectionId(M.nSuccessor); break;
        case 8: Out=ProjectionInteger(M.Centers.size()); break;
        case 9: Out=ProjectionInteger(M.Connections.size()); break;
        case 10: Out=ProjectionInteger(H.nAnchorVersion); break;
        case 11: Out=2; break;
        case 12: Out=M.fHigh; break;
        case 13: Out=M.fLow; break;
        default:
          // 变长域以偶/奇字段交错，不设成员数量的隐式上限。
          if ((nField-100)%2 == 0 && static_cast<std::size_t>((nField-100)/2)<M.Centers.size())
            Out=ProjectionId(M.Centers[(nField-100)/2]);
          if ((nField-100)%2 == 1 && static_cast<std::size_t>((nField-100)/2)<M.Connections.size())
            Out=ProjectionId(M.Connections[(nField-100)/2]);
          break;
      }
    }
    else if (nOutput == 106)
    {
      if (nField > 8 && nField < 100) { UnavailableProjection(nCount,pOut); return; }
      if (static_cast<std::size_t>(nSlot) >= H.Connections.size()) return;
      const RecursiveConnection &C = H.Connections[nSlot];
      switch (nField)
      {
        case 0: Out=ProjectionId(nSlot); break;
        case 1: Out=ProjectionId(C.nLeftCenter); break;
        case 2: Out=ProjectionId(C.nRightCenter); break;
        case 3: Out=static_cast<float>(C.nLevel); break;
        case 4: Out=ProjectionId(C.nStart); break;
        case 5: Out=ProjectionId(C.nEnd); break;
        case 6: Out=ProjectionId(C.nRequiredAt); break;
        case 7: Out=static_cast<float>(C.nMemberSpace); break;
        case 8: Out=ProjectionInteger(C.Members.size()); break;
        default: if (static_cast<std::size_t>(nField-100)<C.Members.size())
          Out=ProjectionId(C.Members[nField-100]); break;
      }
    }
    else if (nOutput == 108)
    {
      if (nField > 8 && nField < 100) { UnavailableProjection(nCount,pOut); return; }
      if (static_cast<std::size_t>(nSlot) >= H.Associations.size()) return;
      const StructureAssociation &A = H.Associations[nSlot];
      switch (nField)
      {
        case 0: Out=ProjectionId(nSlot); break;
        case 1: Out=ProjectionId(A.nStructure); break;
        case 2: Out=ProjectionId(A.nMovement); break;
        case 3: Out=ProjectionId(A.nCompletion); break;
        case 4: Out=static_cast<float>(A.nLevel); break;
        case 5: Out=static_cast<float>(A.nStatus); break;
        case 6: Out=ProjectionInteger(A.Centers.size()); break;
        case 7: Out=ProjectionInteger(H.nAnchorVersion); break;
        case 8: Out=1; break;
        default: if (static_cast<std::size_t>(nField-100)<A.Centers.size())
          Out=ProjectionId(A.Centers[nField-100]); break;
      }
    }
    return;
  }
  if (!An.Hierarchy.bAvailable || An.nCount != nCount || nField < 0 ||
      An.Hierarchy.Nodes.size() > static_cast<std::size_t>(CZSC_PROJECTION_MAX_INTEGER) ||
      An.CompletionEvidence.size() > static_cast<std::size_t>(CZSC_PROJECTION_MAX_INTEGER) ||
      An.ZhongYin.size() > static_cast<std::size_t>(CZSC_PROJECTION_MAX_INTEGER))
  {
    UnavailableProjection(nCount, pOut);
    return;
  }
  // 快照表只在观察位置投影，不向 nConnection/nLatestBar 回填事件。
  float &Out = pOut[nCount-1];
  if (nOutput == 93 || nOutput == 94 || nOutput == 96 || nOutput == 98)
  {
    if (nField != 0) { UnavailableProjection(nCount, pOut); return; }
    if (nOutput == 93) Out = 1;
    if (nOutput == 94) Out = ProjectionInteger(An.Hierarchy.Nodes.size());
    if (nOutput == 96) Out = ProjectionInteger(An.CompletionEvidence.size());
    if (nOutput == 98) Out = ProjectionInteger(An.ZhongYin.size());
    return;
  }
  if (nOutput == 95)
  {
    if (nField > 16 && nField < 100) { UnavailableProjection(nCount, pOut); return; }
    if (static_cast<std::size_t>(nSlot) >= An.Hierarchy.Nodes.size()) return;
    const SubTrendNode &N = An.Hierarchy.Nodes[nSlot];
    switch (nField)
    {
      case 0: Out = ProjectionId(nSlot); break;
      case 1: Out = ProjectionInteger(N.nLevel); break;
      case 2: Out = ProjectionId(N.nStart); break;
      case 3: Out = ProjectionId(N.nEnd); break;
      case 4: Out = ProjectionId(N.nCenterStart); break;
      case 5: Out = ProjectionId(N.nCenterEnd); break;
      case 6: Out = ProjectionId(N.nEstablishedAt); break;
      case 7: Out = ProjectionId(N.nConnection); break;
      case 8: Out = ProjectionId(N.nCompletedAt); break;
      case 9: Out = ProjectionId(N.nSuccessor); break;
      case 10: Out = ProjectionInteger(N.Children.size()); break;
      case 11: Out = ProjectionInteger(An.Hierarchy.nAnchorVersion); break;
      case 12: Out = 1; break; // 具名分解规则版本
      case 13: Out = N.fHigh; break;
      case 14: Out = N.fLow; break;
      case 15: Out = N.fCenterHigh; break;
      case 16: Out = N.fCenterLow; break;
      default:
        if (static_cast<std::size_t>(nField - 100) < N.Children.size())
          Out = ProjectionId(N.Children[nField-100]);
        break;
    }
  }
  else if (nOutput == 97)
  {
    if (nField > 14) { UnavailableProjection(nCount, pOut); return; }
    if (static_cast<std::size_t>(nSlot) >= An.CompletionEvidence.size()) return;
    const TrendCompletionEvidence &E = An.CompletionEvidence[nSlot];
    switch (nField)
    {
      case 0: Out = ProjectionId(nSlot); break;
      case 1: Out = ProjectionId(E.nTrend); break;
      case 2: Out = ProjectionInteger(E.nTrendSpace); break;
      case 3: Out = ProjectionId(E.nConnectionPoint); break;
      case 4: Out = ProjectionId(E.nConnectionBar); break;
      case 5: Out = ProjectionInteger(E.nReason); break;
      case 6: Out = ProjectionId(E.nLatestPoint); break;
      case 7: Out = ProjectionId(E.nLatestBar); break;
      case 8: Out = ProjectionId(E.nObservedAt); break;
      case 9: Out = ProjectionId(E.nSuccessor); break;
      case 10: Out = ProjectionId(E.nSuccessorEstablishedAt); break;
      case 11: Out = E.nTheoreticalLevel < 0 ? -1 : ProjectionInteger(E.nTheoreticalLevel); break;
      case 12: Out = ProjectionInteger(E.nAnchorVersion); break;
      case 13: Out = ProjectionInteger(E.nDecompositionRule); break;
      case 14: Out = 1; break; // 只有完成证据行才入表
    }
  }
  else if (nOutput == 99)
  {
    if (nField > 7) { UnavailableProjection(nCount, pOut); return; }
    if (static_cast<std::size_t>(nSlot) >= An.ZhongYin.size()) return;
    const ZhongYinEvidence &Z = An.ZhongYin[nSlot];
    switch (nField)
    {
      case 0: Out = ProjectionId(nSlot); break;
      case 1: Out = ProjectionId(Z.nCompletion); break;
      case 2: Out = ProjectionInteger(Z.nVersion); break;
      case 3: Out = ProjectionId(Z.nEnter); break;
      case 4: Out = ProjectionId(Z.nEnd); break;
      case 5: Out = ProjectionId(Z.nObservedAt); break;
      case 6: Out = ProjectionId(Z.nContraction); break;
      case 7: Out = Z.bAvailable ? 1 : -1; break;
    }
  }
  else UnavailableProjection(nCount, pOut);
}
