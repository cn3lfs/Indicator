#define _CRT_SECURE_NO_WARNINGS

#include "../CzscCore.h"
#include "SseIndexDaily.h"
#include <cstdio>

static const char *DateAt(int nIndex)
{
  if ((nIndex < 0) || (nIndex >= SSE_DAILY_COUNT))
  {
    return "unknown";
  }
  return SSE_DAILY_DATE[nIndex];
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

static int ScopedFirstSignalValue(float fSignal, int nValue)
{
  return IsFirstSignal(fSignal) ? nValue : 0;
}

static const char *CenterRelationName(int nRelation)
{
  if (nRelation == CZSC_CENTER_RELATION_UP) return "上涨";
  if (nRelation == CZSC_CENTER_RELATION_DOWN) return "下跌";
  if (nRelation == CZSC_CENTER_RELATION_EXTENSION) return "扩展";
  return "未知";
}

static const char *MovementName(int nMovement)
{
  if (nMovement == CZSC_MOVEMENT_UP) return "上涨";
  if (nMovement == CZSC_MOVEMENT_DOWN) return "下跌";
  if (nMovement == CZSC_MOVEMENT_CONSOLIDATION) return "盘整";
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

static int BuildDivergenceFlags(const DivergenceResult &D)
{
  int nFlags = 0;
  if (D.bNewExtreme)
  {
    nFlags |= CZSC_DIVERGENCE_NEW_EXTREME;
  }
  if (D.bWeakSpace)
  {
    nFlags |= CZSC_DIVERGENCE_WEAK_SPACE;
  }
  if (D.bWeakSpeed)
  {
    nFlags |= CZSC_DIVERGENCE_WEAK_SPEED;
  }
  if (D.bWeakMacd)
  {
    nFlags |= CZSC_DIVERGENCE_WEAK_MACD;
  }
  if (D.bDivergence)
  {
    nFlags |= CZSC_DIVERGENCE_CONFIRMED;
  }
  return nFlags;
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

static void PrintCandidates(FILE *pFile, const char *pTitle,
                            const std::vector<TradingSignalCandidate> &Candidates)
{
  std::fprintf(pFile, "\n========== %s(%u, 带候选上下文) ==========\n", pTitle, (unsigned)Candidates.size());
  for (std::size_t i = 0; i < Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = Candidates[i];
    int nCtx = BuildTradingSignalContextFlags(C);
    std::fprintf(pFile,
                 "  %s  %s  质量%d  优先级%d  中枢%d  趋势%d/%s  点%d  突破%d  位置%s  背驰%s  后续%s  小转大%d  ABC%d  回零%d  调试CEN%d BKO%d PID%d TID%d  ctx%d",
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
                 ScopedSmallTurn(C.fSignal, C.nSmallTurn),
                 ScopedFirstSignalValue(C.fSignal, C.nAbcStructure),
                 ScopedFirstSignalValue(C.fSignal, C.nMacdZeroPullback),
                 OneBasedId(C.nCenter),
                 OneBasedId(C.nBreakout),
                 OneBasedId(C.nPoint),
                 OneBasedId(C.nTrend),
                 nCtx);
    PrintStrengthPair(pFile, C.Divergence);
    PrintStrengthRatios(pFile, C.Divergence);
    PrintDivergenceFlags(pFile, BuildDivergenceFlags(C.Divergence));
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

  float *pH = const_cast<float *>(SSE_DAILY_HIGH);
  float *pL = const_cast<float *>(SSE_DAILY_LOW);

  std::vector<MergedBar> Bars = BuildMergedBars(SSE_DAILY_COUNT, pH, pL);
  std::vector<Fractal> Fractals = BuildFractals(Bars);
  std::vector<Stroke> Strokes = BuildStrokes(Fractals);

  CzscAnalyzer StrokeAn;
  BuildAnalyzerFromPrice(StrokeAn, SSE_DAILY_COUNT, pH, pL, DefaultConfig());

  CzscConfig SegmentConfig = DefaultConfig();
  SegmentConfig.nCenterUnit = CZSC_UNIT_SEGMENT;
  SegmentConfig.nSegmentMethod = CZSC_SEG_FEATURE;
  CzscAnalyzer SegmentAn;
  BuildAnalyzerFromPrice(SegmentAn, SSE_DAILY_COUNT, pH, pL, SegmentConfig);

  std::fprintf(pFile, "上证指数(000001.SH) 前复权日线 %d 根: %s ~ %s\n",
               SSE_DAILY_COUNT,
               SSE_DAILY_DATE[0],
               SSE_DAILY_DATE[SSE_DAILY_COUNT - 1]);
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
  PrintCandidateSummary(pFile, "买卖点(笔中枢)", StrokeAn.Candidates);
  PrintCandidateSummary(pFile, "买卖点(线段中枢)", SegmentAn.Candidates);
  PrintCandidates(pFile, "买卖点(笔中枢)", StrokeAn.Candidates);
  PrintCandidates(pFile, "买卖点(线段中枢)", SegmentAn.Candidates);
  PrintPoints(pFile, "笔端点", "B", StrokeAn.Points);

  if (pFile != stdout)
  {
    std::fclose(pFile);
  }
  return 0;
}
