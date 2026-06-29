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

static const char *CenterRelationName(int nRelation)
{
  if (nRelation == CZSC_CENTER_RELATION_UP) return "上涨";
  if (nRelation == CZSC_CENTER_RELATION_DOWN) return "下跌";
  if (nRelation == CZSC_CENTER_RELATION_EXTENSION) return "扩展";
  return "未知";
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
    std::fprintf(pFile,
                 "  %s  %s  质量%d  中枢%d  点%d  突破%d  位置%s  背驰%s  后续%s%s\n",
                 DateAt(C.nIndex),
                 SignalName(C.fSignal),
                 C.nQuality,
                 C.nCenter,
                 C.nPoint,
                 C.nBreakout,
                 CenterPositionName(C.nCenterPosition),
                 ReversalName(C.nReversal),
                 AftermathName(C.nAfterEffect),
                 C.bOverlapped ? "  二三重合" : "");
  }
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
  PrintCandidates(pFile, "买卖点(笔中枢)", StrokeAn.Candidates);
  PrintCandidates(pFile, "买卖点(线段中枢)", SegmentAn.Candidates);
  PrintPoints(pFile, "笔端点", "B", StrokeAn.Points);

  if (pFile != stdout)
  {
    std::fclose(pFile);
  }
  return 0;
}
