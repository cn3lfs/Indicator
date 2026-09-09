#define _CRT_SECURE_NO_WARNINGS

#include "../CzscCore.h"
#include "SseIndexDaily.h"
#include <cstdio>
#include <cstring>

static int g_nSampleOffset = 0;
static int g_nSampleCount = SSE_DAILY_COUNT;

static const char *DateAt(int nIndex)
{
  int nRawIndex = g_nSampleOffset + nIndex;
  if ((nIndex < 0) || (nIndex >= g_nSampleCount) ||
      (nRawIndex < 0) || (nRawIndex >= SSE_DAILY_COUNT))
  {
    return "unknown";
  }
  return SSE_DAILY_DATE[nRawIndex];
}

static int FindDateIndex(const char *pDate)
{
  if (pDate == 0)
  {
    return -1;
  }

  for (int i = 0; i < SSE_DAILY_COUNT; i++)
  {
    if (std::strcmp(SSE_DAILY_DATE[i], pDate) == 0)
    {
      return i;
    }
  }
  return -1;
}

static float PointPrice(const SegmentPoint &P)
{
  return (P.nType == CZSC_POINT_TOP) ? P.fHigh : P.fLow;
}

static const char *PointTypeName(int nType)
{
  if (nType == CZSC_POINT_TOP)
  {
    return "顶";
  }
  if (nType == CZSC_POINT_BOTTOM)
  {
    return "底";
  }
  return "-";
}

static const char *DirectionName(int nDirection)
{
  if (nDirection > 0)
  {
    return "上升";
  }
  if (nDirection < 0)
  {
    return "下降";
  }
  return "未知";
}

static const char *SignalName(float fSignal)
{
  if (fSignal == 1.0f) return "一买";
  if (fSignal == 2.0f) return "二买";
  if (fSignal == 3.0f) return "三买";
  if (fSignal == 11.0f) return "一卖";
  if (fSignal == 12.0f) return "二卖";
  if (fSignal == 13.0f) return "三卖";
  return "未知";
}

static bool IsFirstSignal(float fSignal)
{
  return (fSignal == 1.0f) || (fSignal == 11.0f);
}

static bool IsSecondSignal(float fSignal)
{
  return (fSignal == 2.0f) || (fSignal == 12.0f);
}

static bool IsThirdSignal(float fSignal)
{
  return (fSignal == 3.0f) || (fSignal == 13.0f);
}

static const char *CenterPositionName(int nPosition)
{
  if (nPosition == CZSC_CENTER_POSITION_BELOW) return "下方";
  if (nPosition == CZSC_CENTER_POSITION_INSIDE) return "内部";
  if (nPosition == CZSC_CENTER_POSITION_ABOVE) return "上方";
  return "未知";
}

static const char *ReversalName(int nReversal)
{
  if (nReversal == CZSC_REVERSAL_EXTENSION) return "扩展";
  if (nReversal == CZSC_REVERSAL_CONSOLIDATION) return "盘整";
  if (nReversal == CZSC_REVERSAL_TREND) return "反趋势";
  return "-";
}

static const char *AftermathName(int nAfterEffect)
{
  if (nAfterEffect == CZSC_CENTER_AFTERMATH_EXTENDED) return "扩张";
  if (nAfterEffect == CZSC_CENTER_AFTERMATH_NEWBORN) return "新生";
  return "-";
}

static const char *ScopedReversalName(float fSignal, int nReversal)
{
  return IsFirstSignal(fSignal) ? ReversalName(nReversal) : "-";
}

static const char *ScopedAftermathName(float fSignal, int nAfterEffect)
{
  return IsThirdSignal(fSignal) ? AftermathName(nAfterEffect) : "-";
}

static int ScopedSmallTurn(float fSignal, int nSmallTurn)
{
  return IsThirdSignal(fSignal) ? nSmallTurn : 0;
}

static int ScopedSmallTurnPointId(float fSignal, int nSmallTurn, int nPoint)
{
  return (ScopedSmallTurn(fSignal, nSmallTurn) != 0) ? nPoint : 0;
}

static int ScopedSmallTurnBasePointId(float fSignal, int nSmallTurn, int nPoint)
{
  return (ScopedSmallTurn(fSignal, nSmallTurn) != 0) && (nPoint >= 0) ? nPoint + 1 : 0;
}

static int ScopedSecondSignalPointId(float fSignal, int nPoint)
{
  return (IsSecondSignal(fSignal) && (nPoint >= 0)) ? nPoint + 1 : 0;
}

static int ScopedFirstSignalValue(float fSignal, int nValue)
{
  return IsFirstSignal(fSignal) ? nValue : 0;
}

static int ScopedReversalPointId(float fSignal, int nReversal, int nPoint)
{
  return IsFirstSignal(fSignal) &&
         (nReversal != CZSC_REVERSAL_UNKNOWN) &&
         (nPoint >= 0) ? nPoint + 2 : 0;
}

static const char *CenterRelationName(int nRelation)
{
  if (nRelation == CZSC_CENTER_RELATION_UP) return "上涨";
  if (nRelation == CZSC_CENTER_RELATION_DOWN) return "下跌";
  if (nRelation == CZSC_CENTER_RELATION_EXTENSION) return "扩展";
  return "未知";
}

static const char *CenterLifecycleName(int nLifecycle)
{
  if (nLifecycle == CZSC_CENTER_LIFECYCLE_EXTENSION) return "延伸";
  if (nLifecycle == CZSC_CENTER_LIFECYCLE_EXPANSION) return "扩展";
  if (nLifecycle == CZSC_CENTER_LIFECYCLE_NEWBORN_UP) return "上涨新生";
  if (nLifecycle == CZSC_CENTER_LIFECYCLE_NEWBORN_DOWN) return "下跌新生";
  return "未知";
}

static const char *FilterReasonName(int nReason)
{
  if (nReason == CZSC_FILTER_NO_TREND) return "无趋势";
  if (nReason == CZSC_FILTER_NON_TREND_DIVERGENCE) return "非趋势背驰";
  if (nReason == CZSC_FILTER_SECOND_ORDER) return "二类顺序失败";
  if (nReason == CZSC_FILTER_NOT_FIRST_RETEST) return "非首次回试";
  if (nReason == CZSC_FILTER_RETEST_BACK_CENTER) return "回中枢";
  if (nReason == CZSC_FILTER_DIRECTION_MISMATCH) return "方向不匹配";
  if (nReason == CZSC_FILTER_ABC_NOT_ALIGNED) return "ABC未对齐";
  if (nReason == CZSC_FILTER_MISSING_CENTER) return "缺中枢";
  return "未知";
}

enum NoTrendAttribution
{
  NO_TREND_ATTR_EARLY_CENTER = 1,
  NO_TREND_ATTR_SAME_DIRECTION_TREND = 2,
  NO_TREND_ATTR_OUTSIDE_COMPLETED_TREND = 3,
  NO_TREND_ATTR_COUNT = 3
};

static const char *NoTrendAttributionName(int nAttribution)
{
  if (nAttribution == NO_TREND_ATTR_EARLY_CENTER) return "早期中心不足";
  if (nAttribution == NO_TREND_ATTR_SAME_DIRECTION_TREND) return "同向趋势不足";
  if (nAttribution == NO_TREND_ATTR_OUTSIDE_COMPLETED_TREND) return "不在已完成趋势";
  return "未知";
}

static const char *MovementName(int nMovement)
{
  if (nMovement == CZSC_MOVEMENT_UP) return "上涨";
  if (nMovement == CZSC_MOVEMENT_DOWN) return "下跌";
  if (nMovement == CZSC_MOVEMENT_CONSOLIDATION) return "盘整";
  return "未知";
}

static const char *DivergenceSemanticName(int nSemantic)
{
  if (nSemantic == CZSC_DIVERGENCE_SEM_TREND) return "趋势背驰";
  if (nSemantic == CZSC_DIVERGENCE_SEM_CONSOLIDATION) return "盘整背驰";
  if (nSemantic == CZSC_DIVERGENCE_SEM_SMALL_TURN) return "小转大";
  return "未知";
}

static int OneBasedId(int nValue)
{
  return (nValue >= 0) ? (nValue + 1) : 0;
}

static void PrintFlag(FILE *pFile, bool *pFirst, int nFlags, int nFlag, const char *pName)
{
  if ((pFile == 0) || (pFirst == 0) || ((nFlags & nFlag) == 0))
  {
    return;
  }

  if (!*pFirst)
  {
    std::fprintf(pFile, ",");
  }
  std::fprintf(pFile, "%s", pName);
  *pFirst = false;
}

static void PrintContextFlags(FILE *pFile, int nFlags)
{
  std::fprintf(pFile, "  flags[");
  if (nFlags == 0)
  {
    std::fprintf(pFile, "-");
  }
  else
  {
    bool bFirst = true;
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_STRONG_QUALITY, "强质");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_ABC_STRUCTURE, "ABC");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_MACD_ZERO_PULL, "回零");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_MACD_LINE_WEAK, "黄白弱");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_SMALL_TURN, "小转大");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_STANDARD_DIV, "标准");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_AFTERMATH_NEWBORN, "新生");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_AFTERMATH_EXTEND, "扩展");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_REVERSAL_TREND, "反趋势");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_REVERSAL_CONS, "盘整");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_REVERSAL_EXTEND, "末中枢扩展");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_OVERLAPPED, "二三重合");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_SIGNAL_CTX_CENTER_BREAKOUT, "首次回试");
  }
  std::fprintf(pFile, "]");
}

static void PrintDivergenceFlags(FILE *pFile, int nFlags)
{
  std::fprintf(pFile, "  dvg[");
  if (nFlags == 0)
  {
    std::fprintf(pFile, "-");
  }
  else
  {
    bool bFirst = true;
    PrintFlag(pFile, &bFirst, nFlags, CZSC_DIVERGENCE_NEW_EXTREME, "创新");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_DIVERGENCE_WEAK_SPACE, "空间弱");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_DIVERGENCE_WEAK_SPEED, "速度弱");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_DIVERGENCE_WEAK_MACD, "柱弱");
    PrintFlag(pFile, &bFirst, nFlags, CZSC_DIVERGENCE_CONFIRMED, "成立");
  }
  std::fprintf(pFile, "]");
}

static float PercentRatio(float fCurrent, float fPrevious)
{
  if (fPrevious <= 0)
  {
    return 0.0f;
  }
  return fCurrent / fPrevious * 100.0f;
}

static void PrintStrengthPair(FILE *pFile, const DivergenceResult &D)
{
  if (pFile == 0)
  {
    return;
  }

  std::fprintf(pFile,
               "  A[价%.1f 速%.3f MACD%.1f DIF%.2f DEA%.2f] C[价%.1f 速%.3f MACD%.1f DIF%.2f DEA%.2f]",
               D.Previous.fSpace,
               D.Previous.fSpeed,
               D.Previous.fMacdArea,
               D.Previous.fDifHeight,
               D.Previous.fDeaHeight,
               D.Current.fSpace,
               D.Current.fSpeed,
               D.Current.fMacdArea,
               D.Current.fDifHeight,
               D.Current.fDeaHeight);
}

static void PrintStrengthRatios(FILE *pFile, const DivergenceResult &D)
{
  if (pFile == 0)
  {
    return;
  }

  std::fprintf(pFile,
               "  比[价%.1f%% 速%.1f%% MACD%.1f%%]",
               PercentRatio(D.Current.fSpace, D.Previous.fSpace),
               PercentRatio(D.Current.fSpeed, D.Previous.fSpeed),
               PercentRatio(D.Current.fMacdArea, D.Previous.fMacdArea));
}

static const char *PointDateAt(const std::vector<SegmentPoint> &Points, int nPoint)
{
  if ((nPoint < 0) || ((std::size_t)nPoint >= Points.size()))
  {
    return "unknown";
  }
  return DateAt(Points[(std::size_t)nPoint].nIndex);
}

static int BreakoutLeavePointId(const std::vector<CenterBreakout> &Breakouts, int nBreakout)
{
  if ((nBreakout < 0) || ((std::size_t)nBreakout >= Breakouts.size()))
  {
    return 0;
  }
  return OneBasedId(Breakouts[(std::size_t)nBreakout].nLeavePoint);
}

static int BreakoutRetestPointId(const std::vector<CenterBreakout> &Breakouts, int nBreakout)
{
  if ((nBreakout < 0) || ((std::size_t)nBreakout >= Breakouts.size()))
  {
    return 0;
  }
  return OneBasedId(Breakouts[(std::size_t)nBreakout].nRetestPoint);
}

static void PrintBreakoutContext(FILE *pFile,
                                 const std::vector<SegmentPoint> &Points,
                                 const std::vector<CenterBreakout> &Breakouts,
                                 int nBreakout)
{
  if (pFile == 0)
  {
    return;
  }
  if ((nBreakout < 0) || ((std::size_t)nBreakout >= Breakouts.size()))
  {
    std::fprintf(pFile, "  bko[-]");
    return;
  }

  const CenterBreakout &B = Breakouts[(std::size_t)nBreakout];
  std::fprintf(pFile,
               "  bko[离P%d/%s 回P%d/%s 首%d 回中%d 三%d]",
               OneBasedId(B.nLeavePoint),
               PointDateAt(Points, B.nLeavePoint),
               OneBasedId(B.nRetestPoint),
               PointDateAt(Points, B.nRetestPoint),
               B.bFirstRetest ? 1 : 0,
               B.bBackIntoCenter ? 1 : 0,
               B.bThirdSignal ? 1 : 0);
}

static void PrintPoints(FILE *pFile, const char *pTitle, const char *pPrefix,
                        const std::vector<SegmentPoint> &Points)
{
  std::fprintf(pFile, "\n========== %s(%u) ==========\n", pTitle, (unsigned)Points.size());
  for (std::size_t i = 0; i < Points.size(); i++)
  {
    std::fprintf(pFile, "%s%03u  %s  %s  %.2f\n",
                 pPrefix,
                 (unsigned)(i + 1),
                 DateAt(Points[i].nIndex),
                 PointTypeName(Points[i].nType),
                 PointPrice(Points[i]));
  }
}

static void PrintCenters(FILE *pFile, const char *pTitle, const char *pPrefix,
                         const std::vector<Center> &Centers)
{
  std::fprintf(pFile, "\n========== %s(%u, 带方向) ==========\n", pTitle, (unsigned)Centers.size());
  for (std::size_t i = 0; i < Centers.size(); i++)
  {
    const Center &C = Centers[i];
    std::fprintf(pFile, "%s%02u %s  %s~%s  ZG%.0f ZD%.0f  GG%.0f DD%.0f\n",
                 pPrefix,
                 (unsigned)i,
                 DirectionName(C.nDirection),
                 DateAt(C.nStart),
                 DateAt(C.nEnd),
                 C.fHigh,
                 C.fLow,
                 C.fTop,
                 C.fBottom);
  }
}

static void PrintCenterRelations(FILE *pFile, const char *pTitle, const char *pPrefix,
                                 const std::vector<Center> &Centers)
{
  std::fprintf(pFile, "\n========== %s(%u) ==========\n", pTitle,
               (Centers.size() > 0) ? (unsigned)(Centers.size() - 1) : 0u);
  for (std::size_t i = 1; i < Centers.size(); i++)
  {
    int nRelation = ClassifyCenterRelation(Centers[i - 1], Centers[i]);
    std::fprintf(pFile, "%s%02u->%s%02u  %s  前GG%.0f DD%.0f  后GG%.0f DD%.0f\n",
                 pPrefix,
                 (unsigned)(i - 1),
                 pPrefix,
                 (unsigned)i,
                 CenterRelationName(nRelation),
                 Centers[i - 1].fTop,
                 Centers[i - 1].fBottom,
                 Centers[i].fTop,
                 Centers[i].fBottom);
  }
}

static void PrintCenterLifecycles(FILE *pFile, const char *pTitle, const char *pPrefix,
                                  const std::vector<Center> &Centers)
{
  std::fprintf(pFile, "\n========== %s(%u) ==========\n", pTitle,
               (Centers.size() > 0) ? (unsigned)(Centers.size() - 1) : 0u);
  for (std::size_t i = 1; i < Centers.size(); i++)
  {
    int nLifecycle = ClassifyCenterLifecycle(Centers[i - 1], Centers[i]);
    std::fprintf(pFile, "%s%02u->%s%02u  %s(%d)  前ZG%.0f ZD%.0f GG%.0f DD%.0f  后ZG%.0f ZD%.0f GG%.0f DD%.0f\n",
                 pPrefix,
                 (unsigned)(i - 1),
                 pPrefix,
                 (unsigned)i,
                 CenterLifecycleName(nLifecycle),
                 nLifecycle,
                 Centers[i - 1].fHigh,
                 Centers[i - 1].fLow,
                 Centers[i - 1].fTop,
                 Centers[i - 1].fBottom,
                 Centers[i].fHigh,
                 Centers[i].fLow,
                 Centers[i].fTop,
                 Centers[i].fBottom);
  }
}

static void PrintCenterLifecycleSummary(FILE *pFile, const char *pTitle,
                                        const std::vector<Center> &Centers)
{
  int nExtension = 0;
  int nExpansion = 0;
  int nNewbornUp = 0;
  int nNewbornDown = 0;
  int nUnknown = 0;
  for (std::size_t i = 1; i < Centers.size(); i++)
  {
    int nLifecycle = ClassifyCenterLifecycle(Centers[i - 1], Centers[i]);
    if (nLifecycle == CZSC_CENTER_LIFECYCLE_EXTENSION)
    {
      nExtension++;
    }
    else if (nLifecycle == CZSC_CENTER_LIFECYCLE_EXPANSION)
    {
      nExpansion++;
    }
    else if (nLifecycle == CZSC_CENTER_LIFECYCLE_NEWBORN_UP)
    {
      nNewbornUp++;
    }
    else if (nLifecycle == CZSC_CENTER_LIFECYCLE_NEWBORN_DOWN)
    {
      nNewbornDown++;
    }
    else
    {
      nUnknown++;
    }
  }

  std::fprintf(pFile, "\n========== %s摘要 ==========\n", pTitle);
  std::fprintf(pFile,
               "延伸%d 扩展%d 上涨新生%d 下跌新生%d 未知%d\n",
               nExtension,
               nExpansion,
               nNewbornUp,
               nNewbornDown,
               nUnknown);
}

static int CandidateCenterLifecycle(const std::vector<Center> &Centers, int nCenter)
{
  if ((nCenter < 0) || ((std::size_t)nCenter + 1 >= Centers.size()))
  {
    return CZSC_CENTER_LIFECYCLE_UNKNOWN;
  }
  return ClassifyCenterLifecycle(Centers[(std::size_t)nCenter], Centers[(std::size_t)nCenter + 1]);
}

static void PrintCandidates(FILE *pFile, const char *pTitle,
                            const std::vector<Center> &Centers,
                            const std::vector<SegmentPoint> &Points,
                            const std::vector<CenterBreakout> &Breakouts,
                            const std::vector<TradingSignalCandidate> &Candidates)
{
  std::fprintf(pFile, "\n========== %s(%u, 带候选上下文) ==========\n", pTitle, (unsigned)Candidates.size());
  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    int nCtx = BuildTradingSignalContextFlags(C);
    int nDgs = BuildTradingSignalDivergenceSemantic(C);
    int nLifecycle = CandidateCenterLifecycle(Centers, C.nCenter);
    std::fprintf(pFile,
                 "  %s  %s  质量%d  优先级%d  中枢%d  趋势%d/%s  点%d  突破%d  位置%s  背驰%s  后续%s  生命周期%s  小转大%d  ABC%d  回零%d  调试CEN%d BKO%d BLP%d BRP%d ABK%d ABL%d ABR%d STL%d STR%d STF%d SFP%d SMP%d APS%d APE%d CPS%d CPE%d PID%d TID%d DGS%d RVP%d  ctx%d",
                 DateAt(C.nIndex),
                 SignalName(C.fSignal),
                 C.nQuality,
                 C.nPriority,
                 C.nCenter,
                 C.nTrend,
                 MovementName(C.nMovementType),
                 C.nPoint,
                 C.nBreakout,
                 CenterPositionName(C.nCenterPosition),
                 ScopedReversalName(C.fSignal, C.nReversal),
                 ScopedAftermathName(C.fSignal, C.nAfterEffect),
                 CenterLifecycleName(nLifecycle),
                 ScopedSmallTurn(C.fSignal, C.nSmallTurn),
                 ScopedFirstSignalValue(C.fSignal, C.nAbcStructure),
                 ScopedFirstSignalValue(C.fSignal, C.nMacdZeroPullback),
                 OneBasedId(C.nCenter),
                 OneBasedId(C.nBreakout),
                 BreakoutLeavePointId(Breakouts, C.nBreakout),
                 BreakoutRetestPointId(Breakouts, C.nBreakout),
                 ScopedFirstSignalValue(C.fSignal, OneBasedId(C.nAbcBreakout)),
                 ScopedFirstSignalValue(C.fSignal, BreakoutLeavePointId(Breakouts, C.nAbcBreakout)),
                 ScopedFirstSignalValue(C.fSignal, BreakoutRetestPointId(Breakouts, C.nAbcBreakout)),
                 ScopedSmallTurnPointId(C.fSignal, C.nSmallTurn, BreakoutLeavePointId(Breakouts, C.nBreakout)),
                 ScopedSmallTurnPointId(C.fSignal, C.nSmallTurn, BreakoutRetestPointId(Breakouts, C.nBreakout)),
                 ScopedSmallTurnBasePointId(C.fSignal, C.nSmallTurn, C.nSmallTurnBasePoint),
                 ScopedSecondSignalPointId(C.fSignal, C.nSecondBasePoint),
                 ScopedSecondSignalPointId(C.fSignal, C.nSecondTurnPoint),
                 OneBasedId(C.Divergence.nPreviousStartPoint),
                 OneBasedId(C.Divergence.nPreviousEndPoint),
                 OneBasedId(C.Divergence.nCurrentStartPoint),
                 OneBasedId(C.Divergence.nCurrentEndPoint),
                 OneBasedId(C.nPoint),
                 OneBasedId(C.nTrend),
                 nDgs,
                 ScopedReversalPointId(C.fSignal, C.nReversal, C.nPoint),
                 nCtx);
    PrintStrengthPair(pFile, C.Divergence);
    PrintStrengthRatios(pFile, C.Divergence);
    PrintDivergenceFlags(pFile, BuildDivergenceFlags(C.Divergence));
    PrintBreakoutContext(pFile, Points, Breakouts, C.nBreakout);
    PrintContextFlags(pFile, nCtx);
    if (C.bOverlapped)
    {
      std::fprintf(pFile, "  二三重合");
    }
    std::fprintf(pFile, "\n");
  }
}

static void PrintCandidateSummary(FILE *pFile, const char *pTitle,
                                  const std::vector<TradingSignalCandidate> &Candidates)
{
  int nFirstBuy = 0;
  int nSecondBuy = 0;
  int nThirdBuy = 0;
  int nFirstSell = 0;
  int nSecondSell = 0;
  int nThirdSell = 0;
  int nStrong = 0;
  int nAbc = 0;
  int nZeroPull = 0;
  int nLineWeak = 0;
  int nStandard = 0;
  int nSmallTurn = 0;
  int nOverlapped = 0;
  int nBreakout = 0;

  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    if (C.fSignal == 1.0f)
    {
      nFirstBuy++;
    }
    else if (C.fSignal == 2.0f)
    {
      nSecondBuy++;
    }
    else if (C.fSignal == 3.0f)
    {
      nThirdBuy++;
    }
    else if (C.fSignal == 11.0f)
    {
      nFirstSell++;
    }
    else if (C.fSignal == 12.0f)
    {
      nSecondSell++;
    }
    else if (C.fSignal == 13.0f)
    {
      nThirdSell++;
    }

    int nCtx = BuildTradingSignalContextFlags(C);
    if ((nCtx & CZSC_SIGNAL_CTX_STRONG_QUALITY) != 0)
    {
      nStrong++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_ABC_STRUCTURE) != 0)
    {
      nAbc++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_MACD_ZERO_PULL) != 0)
    {
      nZeroPull++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_MACD_LINE_WEAK) != 0)
    {
      nLineWeak++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_STANDARD_DIV) != 0)
    {
      nStandard++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_SMALL_TURN) != 0)
    {
      nSmallTurn++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_OVERLAPPED) != 0)
    {
      nOverlapped++;
    }
    if ((nCtx & CZSC_SIGNAL_CTX_CENTER_BREAKOUT) != 0)
    {
      nBreakout++;
    }
  }

  std::fprintf(pFile, "\n========== %s摘要 ==========\n", pTitle);
  std::fprintf(pFile,
               "买: 一%d 二%d 三%d | 卖: 一%d 二%d 三%d | 上下文: 强质%d ABC%d 回零%d 黄白弱%d 标准%d 小转大%d 二三重合%d 首次回试%d\n",
               nFirstBuy,
               nSecondBuy,
               nThirdBuy,
               nFirstSell,
               nSecondSell,
               nThirdSell,
               nStrong,
               nAbc,
               nZeroPull,
               nLineWeak,
               nStandard,
               nSmallTurn,
               nOverlapped,
               nBreakout);
}

static void PrintFilterReasonSummary(FILE *pFile, const char *pTitle, const std::vector<int> &Reasons)
{
  int Counts[9] = {0};
  std::vector<const char *> Examples[9];
  int nUnknown = 0;
  int nTotal = 0;
  for (std::size_t i = 0; i < Reasons.size(); i++)
  {
    int nReason = Reasons[i];
    if (nReason == CZSC_FILTER_NONE)
    {
      continue;
    }
    if ((nReason >= CZSC_FILTER_NO_TREND) && (nReason <= CZSC_FILTER_MISSING_CENTER))
    {
      Counts[nReason]++;
      if (Examples[nReason].size() < 5)
      {
        Examples[nReason].push_back(DateAt((int)i));
      }
      nTotal++;
    }
    else
    {
      nUnknown++;
    }
  }

  std::fprintf(pFile, "\n========== %s(8,total=%d,unknown=%d) ==========\n",
               pTitle,
               nTotal,
               nUnknown);
  for (int i = CZSC_FILTER_NO_TREND; i <= CZSC_FILTER_MISSING_CENTER; i++)
  {
    std::fprintf(pFile, "  原因%d  %s  %d  样例 ", i, FilterReasonName(i), Counts[i]);
    if (Examples[i].empty())
    {
      std::fprintf(pFile, "-");
    }
    else
    {
      for (std::size_t j = 0; j < Examples[i].size(); j++)
      {
        if (j > 0)
        {
          std::fprintf(pFile, ",");
        }
        std::fprintf(pFile, "%s", Examples[i][j]);
      }
    }
    std::fprintf(pFile, "\n");
  }
}

static int CompletedCenterCountBeforeIndex(const std::vector<Center> &Centers, int nIndex)
{
  int nCount = 0;
  for (std::size_t i = 0; i < Centers.size(); i++)
  {
    if (Centers[i].nEnd <= nIndex)
    {
      nCount++;
    }
  }
  return nCount;
}

static int PointDirectionAtIndex(const std::vector<SegmentPoint> &Points, int nIndex)
{
  for (std::size_t i = 0; i < Points.size(); i++)
  {
    if (Points[i].nIndex != nIndex)
    {
      continue;
    }
    if (Points[i].nType == CZSC_POINT_BOTTOM)
    {
      return -1;
    }
    if (Points[i].nType == CZSC_POINT_TOP)
    {
      return 1;
    }
  }
  return 0;
}

static bool IsSameDirectionTrend(const TrendStructure &T, int nDirection)
{
  int nType = (nDirection > 0) ? CZSC_MOVEMENT_UP : CZSC_MOVEMENT_DOWN;
  return (T.nType == nType) && (T.nLastCenter > T.nFirstCenter);
}

static bool HasSameDirectionTrendBeforeIndex(const std::vector<TrendStructure> &Structures,
                                             int nIndex,
                                             int nDirection)
{
  for (std::size_t i = 0; i < Structures.size(); i++)
  {
    if (!IsSameDirectionTrend(Structures[i], nDirection))
    {
      continue;
    }
    if (Structures[i].nStart < nIndex)
    {
      return true;
    }
  }
  return false;
}

static bool HasSameDirectionTrendContainingIndex(const std::vector<TrendStructure> &Structures,
                                                 int nIndex,
                                                 int nDirection)
{
  for (std::size_t i = 0; i < Structures.size(); i++)
  {
    if (!IsSameDirectionTrend(Structures[i], nDirection))
    {
      continue;
    }
    if ((Structures[i].nStart <= nIndex) && (nIndex <= Structures[i].nEnd))
    {
      return true;
    }
  }
  return false;
}

static int ClassifyNoTrendAttribution(const std::vector<SegmentPoint> &Points,
                                      const std::vector<Center> &Centers,
                                      const std::vector<TrendStructure> &Structures,
                                      int nIndex)
{
  if (CompletedCenterCountBeforeIndex(Centers, nIndex) < 2)
  {
    return NO_TREND_ATTR_EARLY_CENTER;
  }

  int nDirection = PointDirectionAtIndex(Points, nIndex);
  if ((nDirection == 0) ||
      !HasSameDirectionTrendBeforeIndex(Structures, nIndex, nDirection))
  {
    return NO_TREND_ATTR_SAME_DIRECTION_TREND;
  }

  if (!HasSameDirectionTrendContainingIndex(Structures, nIndex, nDirection))
  {
    return NO_TREND_ATTR_OUTSIDE_COMPLETED_TREND;
  }

  return NO_TREND_ATTR_SAME_DIRECTION_TREND;
}

static void PrintNoTrendAttributionSummary(FILE *pFile,
                                           const char *pTitle,
                                           const std::vector<int> &Reasons,
                                           const std::vector<SegmentPoint> &Points,
                                           const std::vector<Center> &Centers,
                                           const std::vector<TrendStructure> &Structures)
{
  int Counts[NO_TREND_ATTR_COUNT + 1] = {0};
  std::vector<const char *> Examples[NO_TREND_ATTR_COUNT + 1];
  int nTotal = 0;

  for (std::size_t i = 0; i < Reasons.size(); i++)
  {
    if (Reasons[i] != CZSC_FILTER_NO_TREND)
    {
      continue;
    }

    int nAttribution = ClassifyNoTrendAttribution(Points, Centers, Structures, (int)i);
    if ((nAttribution < 1) || (nAttribution > NO_TREND_ATTR_COUNT))
    {
      continue;
    }

    Counts[nAttribution]++;
    if (Examples[nAttribution].size() < 5)
    {
      Examples[nAttribution].push_back(DateAt((int)i));
    }
    nTotal++;
  }

  std::fprintf(pFile, "\n========== %s(3,total=%d) ==========\n", pTitle, nTotal);
  for (int i = 1; i <= NO_TREND_ATTR_COUNT; i++)
  {
    std::fprintf(pFile, "  归因%d  %s  %d  样例 ",
                 i,
                 NoTrendAttributionName(i),
                 Counts[i]);
    if (Examples[i].empty())
    {
      std::fprintf(pFile, "-");
    }
    else
    {
      for (std::size_t j = 0; j < Examples[i].size(); j++)
      {
        if (j > 0)
        {
          std::fprintf(pFile, ",");
        }
        std::fprintf(pFile, "%s", Examples[i][j]);
      }
    }
    std::fprintf(pFile, "\n");
  }
}

static int CountPointsInRange(const std::vector<SegmentPoint> &Points, int nStart, int nEnd)
{
  int nCount = 0;
  for (std::size_t i = 0; i < Points.size(); i++)
  {
    if ((Points[i].nIndex >= nStart) && (Points[i].nIndex <= nEnd))
    {
      nCount++;
    }
  }
  return nCount;
}

static void PrintTrendStructureSummary(FILE *pFile,
                                       const char *pTitle,
                                       const std::vector<Center> &Centers,
                                       const std::vector<SegmentPoint> &Points,
                                       const std::vector<TrendStructure> &Structures)
{
  int nUp = 0;
  int nDown = 0;
  int nConsolidation = 0;
  int nInvalidCenter = 0;
  int nInvalidSpan = 0;

  for (std::size_t i = 0; i < Structures.size(); i++)
  {
    const TrendStructure &T = Structures[i];
    if (T.nType == CZSC_MOVEMENT_UP)
    {
      nUp++;
    }
    else if (T.nType == CZSC_MOVEMENT_DOWN)
    {
      nDown++;
    }
    else if (T.nType == CZSC_MOVEMENT_CONSOLIDATION)
    {
      nConsolidation++;
    }

    if ((T.nFirstCenter < 0) ||
        (T.nLastCenter < T.nFirstCenter) ||
        ((std::size_t)T.nLastCenter >= Centers.size()))
    {
      nInvalidCenter++;
    }
    if (CountPointsInRange(Points, T.nStart, T.nEnd) < 4)
    {
      nInvalidSpan++;
    }
  }

  std::fprintf(pFile,
               "\n========== %s(3,total=%u,invalid_center=%d,invalid_span=%d) ==========\n",
               pTitle,
               (unsigned)Structures.size(),
               nInvalidCenter,
               nInvalidSpan);
  std::fprintf(pFile, "  走势 %s  %d\n", MovementName(CZSC_MOVEMENT_UP), nUp);
  std::fprintf(pFile, "  走势 %s  %d\n", MovementName(CZSC_MOVEMENT_DOWN), nDown);
  std::fprintf(pFile, "  走势 %s  %d\n", MovementName(CZSC_MOVEMENT_CONSOLIDATION), nConsolidation);
}

static void PrintNestedDivergenceSemanticSummary(FILE *pFile,
                                                 const char *pTitle,
                                                 const std::vector<NestedDivergenceContext> &Contexts)
{
  int Counts[4] = {0};
  int nNewExtreme = 0;
  int nBuy = 0;
  int nSell = 0;

  for (std::size_t i = 0; i < Contexts.size(); i++)
  {
    int nSemantic = Contexts[i].nSemantic;
    if ((nSemantic >= CZSC_DIVERGENCE_SEM_TREND) &&
        (nSemantic <= CZSC_DIVERGENCE_SEM_SMALL_TURN))
    {
      Counts[nSemantic]++;
    }
    if ((Contexts[i].nConfirmFlags & CZSC_NESTED_NEW_EXTREME) != 0)
    {
      nNewExtreme++;
    }
    if (Contexts[i].nDirection > 0)
    {
      nBuy++;
    }
    else if (Contexts[i].nDirection < 0)
    {
      nSell++;
    }
  }

  std::fprintf(pFile,
               "\n========== %s(3,total=%u,new_extreme=%d,buy=%d,sell=%d) ==========\n",
               pTitle,
               (unsigned)Contexts.size(),
               nNewExtreme,
               nBuy,
               nSell);
  for (int i = CZSC_DIVERGENCE_SEM_TREND; i <= CZSC_DIVERGENCE_SEM_SMALL_TURN; i++)
  {
    std::fprintf(pFile, "  语义%d  %s  %d\n",
                 i,
                 DivergenceSemanticName(i),
                 Counts[i]);
  }
}

static void PrintNestedDivergenceContexts(FILE *pFile,
                                          const char *pTitle,
                                          const std::vector<SegmentPoint> &HighPoints,
                                          const std::vector<TradingSignalCandidate> &HighCandidates,
                                          const std::vector<SegmentPoint> &LowPoints,
                                          const std::vector<TradingSignalCandidate> &LowCandidates)
{
  std::vector<NestedDivergenceContext> Contexts =
    BuildNestedDivergenceContexts(HighPoints, HighCandidates, LowPoints, LowCandidates);
  std::fprintf(pFile, "\n========== %s(%u) ==========\n", pTitle, (unsigned)Contexts.size());
  for (std::size_t i = 0; i < Contexts.size(); i++)
  {
    const NestedDivergenceContext &C = Contexts[i];
    std::fprintf(pFile,
                 "  %s  级别%d  语义%d  确认%d  源H%02d  低P%d/%s->P%d/%s  方向%d  小转大%d\n",
                 DateAt(C.nIndex),
                 C.nLevel,
                 C.nSemantic,
                 C.nConfirmFlags,
                 C.nSourceDivergence + 1,
                 OneBasedId(C.nLowStartPoint),
                 PointDateAt(LowPoints, C.nLowStartPoint),
                 OneBasedId(C.nLowEndPoint),
                 PointDateAt(LowPoints, C.nLowEndPoint),
                 C.nDirection,
                 C.bSmallTurnSatisfied ? 1 : 0);
  }
  PrintNestedDivergenceSemanticSummary(pFile, "区间套语义摘要", Contexts);
}

static bool DumpSample(FILE *pFile, const char *pTitle, int nStart, int nEnd)
{
  if ((pFile == 0) || (pTitle == 0) ||
      (nStart < 0) || (nEnd < nStart) || (nEnd >= SSE_DAILY_COUNT))
  {
    return false;
  }

  g_nSampleOffset = nStart;
  g_nSampleCount = nEnd - nStart + 1;

  float *pH = const_cast<float *>(SSE_DAILY_HIGH + nStart);
  float *pL = const_cast<float *>(SSE_DAILY_LOW + nStart);

  std::vector<MergedBar> Bars = BuildMergedBars(g_nSampleCount, pH, pL);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);

  CzscAnalyzer StrokeAn;
  BuildAnalyzerFromPrice(StrokeAn, g_nSampleCount, pH, pL, DefaultConfig());

  CzscConfig SegmentConfig = DefaultConfig();
  SegmentConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
  SegmentConfig.nSegmentMethod = CZSC_SEG_FEATURE;
  CzscAnalyzer SegmentAn;
  BuildAnalyzerFromPrice(SegmentAn, g_nSampleCount, pH, pL, SegmentConfig);

  std::fprintf(pFile, "\n########## 样本: %s ##########\n", pTitle);
  std::fprintf(pFile, "上证指数(000001.SH) 前复权日线 %d 根: %s ~ %s\n",
               g_nSampleCount,
               DateAt(0),
               DateAt(g_nSampleCount - 1));
  std::fprintf(pFile,
               "严格笔 %u(端点%u) | 线段 %u | 笔中枢 %u | 线段中枢 %u | 笔买卖点 %u | 线段买卖点 %u\n",
               (unsigned)Strokes.size(),
               (unsigned)StrokeAn.Points.size(),
               (unsigned)SegmentAn.Points.size(),
               (unsigned)StrokeAn.Centers.size(),
               (unsigned)SegmentAn.Centers.size(),
               (unsigned)StrokeAn.Candidates.size(),
               (unsigned)SegmentAn.Candidates.size());

  PrintPoints(pFile, "线段端点", "L", SegmentAn.Points);
  PrintCenters(pFile, "笔中枢", "BZ", StrokeAn.Centers);
  PrintCenters(pFile, "线段中枢", "SZ", SegmentAn.Centers);
  PrintCenterRelations(pFile, "笔中枢关系", "BZ", StrokeAn.Centers);
  PrintCenterRelations(pFile, "线段中枢关系", "SZ", SegmentAn.Centers);
  PrintCenterLifecycles(pFile, "笔中枢生命周期", "BZ", StrokeAn.Centers);
  PrintCenterLifecycles(pFile, "线段中枢生命周期", "SZ", SegmentAn.Centers);
  PrintCenterLifecycleSummary(pFile, "笔中枢生命周期", StrokeAn.Centers);
  PrintCenterLifecycleSummary(pFile, "线段中枢生命周期", SegmentAn.Centers);
  PrintTrendStructureSummary(pFile,
                             "走势结构摘要(笔中枢)",
                             StrokeAn.Centers,
                             StrokeAn.Points,
                             StrokeAn.Structures);
  PrintTrendStructureSummary(pFile,
                             "走势结构摘要(线段中枢)",
                             SegmentAn.Centers,
                             SegmentAn.Points,
                             SegmentAn.Structures);
  PrintCandidateSummary(pFile, "买卖点(笔中枢)", StrokeAn.Candidates);
  PrintCandidateSummary(pFile, "买卖点(线段中枢)", SegmentAn.Candidates);
  PrintFilterReasonSummary(pFile, "买卖点过滤原因(笔中枢)", StrokeAn.TradingFilterReasons);
  PrintNoTrendAttributionSummary(pFile,
                                 "买卖点无趋势归因(笔中枢)",
                                 StrokeAn.TradingFilterReasons,
                                 StrokeAn.Points,
                                 StrokeAn.Centers,
                                 StrokeAn.Structures);
  PrintFilterReasonSummary(pFile, "买卖点过滤原因(线段中枢)", SegmentAn.TradingFilterReasons);
  PrintNoTrendAttributionSummary(pFile,
                                 "买卖点无趋势归因(线段中枢)",
                                 SegmentAn.TradingFilterReasons,
                                 SegmentAn.Points,
                                 SegmentAn.Centers,
                                 SegmentAn.Structures);
  PrintNestedDivergenceContexts(pFile, "区间套背驰上下文",
                                SegmentAn.Points, SegmentAn.Candidates,
                                StrokeAn.Points, StrokeAn.Candidates);
  PrintCandidates(pFile, "买卖点(笔中枢)",
                  StrokeAn.Centers, StrokeAn.Points, StrokeAn.Breakouts, StrokeAn.Candidates);
  PrintCandidates(pFile, "买卖点(线段中枢)",
                  SegmentAn.Centers, SegmentAn.Points, SegmentAn.Breakouts, SegmentAn.Candidates);
  PrintPoints(pFile, "笔端点", "B", StrokeAn.Points);

  return true;
}

static bool DumpSampleByDate(FILE *pFile, const char *pTitle, const char *pStartDate, const char *pEndDate)
{
  int nStart = FindDateIndex(pStartDate);
  int nEnd = FindDateIndex(pEndDate);
  return DumpSample(pFile, pTitle, nStart, nEnd);
}

int main(int argc, char **argv)
{
  FILE *pFile = stdout;
  if (argc > 1)
  {
    pFile = std::fopen(argv[1], "wb");
    if (pFile == 0)
    {
      return 1;
    }
  }

  bool bOk = DumpSample(pFile, "全量日线", 0, SSE_DAILY_COUNT - 1);
  bOk = bOk && DumpSampleByDate(pFile, "2018-2019 下跌修复切片", "2018-01-26", "2019-12-31");
  bOk = bOk && DumpSampleByDate(pFile, "2020-2021 上涨震荡切片", "2020-01-02", "2021-12-31");
  bOk = bOk && DumpSampleByDate(pFile, "2022-2023 下跌盘整切片", "2022-01-04", "2023-12-29");
  bOk = bOk && DumpSampleByDate(pFile, "2024-2026 震荡上行切片", "2024-01-02", "2026-06-26");

  if (pFile != stdout)
  {
    std::fclose(pFile);
  }
  return bOk ? 0 : 1;
}
