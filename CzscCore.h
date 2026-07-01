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

#ifndef __CZSC_CORE_H__
#define __CZSC_CORE_H__

#include <vector>

// 分型 / 线段点的类型：顶、底或无
enum CzscPointType
{
  CZSC_POINT_BOTTOM = -1,
  CZSC_POINT_NONE   = 0,
  CZSC_POINT_TOP    = 1,
};

// 走势类型（由相邻中枢的上下关系判定）：下跌、盘整、上涨
enum CzscMovementType
{
  CZSC_MOVEMENT_DOWN          = -1,
  CZSC_MOVEMENT_CONSOLIDATION = 0,
  CZSC_MOVEMENT_UP            = 1,
};

// 买卖点信号质量（Func10 输出）：观察、确认、标准背驰（最强）
enum CzscSignalQuality
{
  CZSC_SIGNAL_QUALITY_WATCH     = 0,
  CZSC_SIGNAL_QUALITY_CONFIRMED = 1,
  CZSC_SIGNAL_QUALITY_STRONG    = 2,
};

// 信号点相对所属中枢的位置：下方、内部、上方、未知
enum CzscCenterPosition
{
  CZSC_CENTER_POSITION_BELOW   = -1,
  CZSC_CENTER_POSITION_INSIDE  = 0,
  CZSC_CENTER_POSITION_ABOVE   = 1,
  CZSC_CENTER_POSITION_UNKNOWN = 2,
};

enum CzscCenterRelation
{
  CZSC_CENTER_RELATION_DOWN      = -1,  // 后GG < 前DD：下跌及其延续
  CZSC_CENTER_RELATION_EXTENSION = 0,   // 全幅区间重叠：形成高级别中枢（扩展/扩张）
  CZSC_CENTER_RELATION_UP        = 1,   // 后DD > 前GG：上涨及其延续
};

enum CzscReversalStrength
{
  CZSC_REVERSAL_EXTENSION     = -1,  // 情况一：最后中枢级别扩展（最弱反弹）
  CZSC_REVERSAL_CONSOLIDATION = 0,   // 情况二：更大级别盘整
  CZSC_REVERSAL_TREND         = 1,   // 情况三：反趋势（最强）
  CZSC_REVERSAL_UNKNOWN       = 2,
};

enum CzscCenterAftermath
{
  CZSC_CENTER_AFTERMATH_UNKNOWN  = 0,  // 后续中枢尚未形成或反向异常
  CZSC_CENTER_AFTERMATH_EXTENDED = 1,  // 中枢扩张：与后续中枢形成更大级别中枢
  CZSC_CENTER_AFTERMATH_NEWBORN  = 2,  // 中枢新生：形成同向新中枢（趋势）
};

enum CzscSignalContextFlag
{
  CZSC_SIGNAL_CTX_STRONG_QUALITY    = 1,
  CZSC_SIGNAL_CTX_ABC_STRUCTURE     = 2,
  CZSC_SIGNAL_CTX_MACD_ZERO_PULL    = 4,
  CZSC_SIGNAL_CTX_MACD_LINE_WEAK    = 8,
  CZSC_SIGNAL_CTX_SMALL_TURN        = 16,
  CZSC_SIGNAL_CTX_STANDARD_DIV      = 32,
  CZSC_SIGNAL_CTX_AFTERMATH_NEWBORN = 64,
  CZSC_SIGNAL_CTX_AFTERMATH_EXTEND  = 128,
  CZSC_SIGNAL_CTX_REVERSAL_TREND    = 256,
  CZSC_SIGNAL_CTX_REVERSAL_CONS     = 512,
  CZSC_SIGNAL_CTX_REVERSAL_EXTEND   = 1024,
  CZSC_SIGNAL_CTX_OVERLAPPED        = 2048,
  CZSC_SIGNAL_CTX_CENTER_BREAKOUT   = 4096,
};

// 均线吻分类（第11课）：短长均线靠近/相交处的反抗强度，递增
enum CzscKissType
{
  CZSC_KISS_NONE = 0,
  CZSC_KISS_FLY  = 1,  // 飞吻：短均线略走平即继续原趋势（反抗最弱）
  CZSC_KISS_LIP  = 2,  // 唇吻：短均线贴近长均线但不破
  CZSC_KISS_WET  = 3,  // 湿吻：短均线升破/跌破长均线（反抗最强，转折多由此始）
  CZSC_KISS_WET_TRAP = 4,  // 湿吻+放量：转折处异常放量，骗线嫌疑（第12课，需旁路成交量校验）
};

// 笔类型：严格笔=按原始K线间隔；新笔=按合并K线间隔（第62/65课）
enum CzscStrokeType
{
  CZSC_STROKE_STRICT = 0,
  CZSC_STROKE_NEW    = 1,
};

// 笔结束：相邻同型分型取最严格极值，还是保留首个（允许次高/次低点收笔）
enum CzscStrokeEnd
{
  CZSC_END_STRICT = 0,  // 严格最高/最低点
  CZSC_END_SECOND = 1,  // 允许次高/次低点
};

// 中枢构件：中枢由笔端点构成（笔中枢），还是由线段端点构成（线段中枢）
enum CzscCenterUnit
{
  CZSC_UNIT_STROKE  = 0,  // 笔中枢
  CZSC_UNIT_SEGMENT = 1,  // 线段中枢
};

// 线段划分法（仅线段中枢时生效）：保护点启发式 / 第67课特征序列法
enum CzscSegmentMethod
{
  CZSC_SEG_HEURISTIC = 0,  // 保护点启发式
  CZSC_SEG_FEATURE   = 1,  // 特征序列法
};

// 缠论计算配置：把可选的笔类型/笔结束/中枢构件/线段法集中为一处配置
struct CzscConfig
{
  int nStrokeType;     // 见 CzscStrokeType
  int nStrokeEnd;      // 见 CzscStrokeEnd
  int nCenterUnit;     // 见 CzscCenterUnit
  int nSegmentMethod;  // 见 CzscSegmentMethod
};

// 原始 K 线（下标 + 高低价）
struct KBar
{
  int   nIndex;
  float fHigh;
  float fLow;
};

// 包含处理后的合并 K 线，记录覆盖的原始下标区间与高低点出处
struct MergedBar
{
  int   nStart;       // 合并区间起止原始下标
  int   nEnd;
  int   nHighIndex;   // 高/低点所在的原始下标
  int   nLowIndex;
  float fHigh;
  float fLow;
};

// 分型：顶分型或底分型（nType 见 CzscPointType），定位在合并 K 线上
struct Fractal
{
  int   nType;
  int   nIndex;        // 分型极值所在的原始 K 线下标
  int   nMergedIndex;  // 分型中点所在的合并 K 线下标（新笔标准按合并 K 线判间隔）
  float fHigh;
  float fLow;
};

// 笔：相邻一顶一底之间的一段（nDirection: +1 向上、-1 向下）
struct Stroke
{
  Fractal Start;
  Fractal End;
  int     nDirection;
};

// 线段点 / 信号点：笔或线段的端点，fEnergy 为该点的累积 MACD 柱面积（动力学）
struct SegmentPoint
{
  int   nType;
  int   nIndex;
  float fHigh;
  float fLow;
  float fEnergy;
  float fDif;
  float fDea;
};

struct Center
{
  int   nStart;
  int   nEnd;
  float fHigh;       // ZG = min(各段高点)，重叠区间上沿
  float fLow;        // ZD = max(各段低点)，重叠区间下沿
  float fTop;        // GG = max(各段高点)，全幅上沿，恒 >= fHigh
  float fBottom;     // DD = min(各段低点)，全幅下沿，恒 <= fLow
  int   nDirection;  // 进入段方向：+1 上升中枢(下上下,后向上离开) / -1 下降中枢(上下上,后向下离开)
};

// 一段走势的力度度量：价差、平均速度、MACD 柱面积（fDif/fDea/bRsi 预留）
struct StrengthMetrics
{
  float fSpace;        // 价差（终点价-起点价的绝对值）
  float fSpeed;        // 平均速度（价差/时间）
  float fDifHeight;
  float fDeaHeight;
  float fMacdArea;     // 区间累积 MACD 柱面积（第24课背驰判断）
  bool  bRsiDivergence;
};

// 背驰判断结果：方向、是否创新极值，以及价差/速度/MACD 三个维度是否走弱
struct DivergenceResult
{
  int             nDirection;
  bool            bNewExtreme;   // 是否创新高/新低（趋势背驰必需，盘整背驰不需要）
  bool            bWeakSpace;
  bool            bWeakSpeed;
  bool            bWeakMacd;
  bool            bDivergence;    // 综合判定是否构成背驰
  StrengthMetrics Previous;       // A 段（前一同向走势）
  StrengthMetrics Current;        // C 段（当前走势）
};

// 走势类型结构：一段连续同向（或单个盘整）中枢构成的走势，记录起止与首末中枢下标
struct TrendStructure
{
  int nType;          // 见 CzscMovementType
  int nStart;
  int nEnd;
  int nFirstCenter;
  int nLastCenter;
};

// 中枢突破：离开中枢 + 首次回试的记录，用于判定第三类买卖点
struct CenterBreakout
{
  int  nCenter;
  int  nDirection;
  int  nLeavePoint;              // 离开中枢的线段点
  int  nRetestPoint;            // 回试的线段点
  bool bFirstRetest;            // 是否首次回试（第20课：必须第一次）
  bool bBackIntoCenter;        // 回试是否回到中枢内
  bool bThirdSignal;           // 是否构成第三类买卖点
  bool bConsolidationDivergence;
  DivergenceResult Divergence;
};

// 买卖点候选：携带全部上下文（来源/优先级/质量/中枢位置/背驰-转折/三买后续等）
struct TradingSignalCandidate
{
  int   nIndex;             // 信号所在原始 K 线下标
  float fSignal;            // 信号编码（1/2/3 买，11/12/13 卖）
  int   nPriority;          // 同一根 K 线上的取胜优先级
  int   nPoint;             // 对应线段点下标
  int   nCenter;            // 所属中枢下标
  int   nBreakout;          // 对应中枢突破下标（无则 -1）
  int   nSource;            // 来源：第一/二/三类
  int   nTrend;             // 所属走势结构下标（无则 -1）
  int   nMovementType;      // 所属走势类型，见 CzscMovementType
  int   nQuality;           // 见 CzscSignalQuality
  int   nCenterPosition;    // 见 CzscCenterPosition
  int   nReversal;          // 见 CzscReversalStrength（第29课，仅一类）
  int   nAfterEffect;       // 见 CzscCenterAftermath（第21课，仅三类）
  int   nSmallTurn;         // 第44课小转大必要条件：三买=1、三卖=-1、无=0
  int   nAbcStructure;      // 第37课a+A+b+B+c结构：一买=1、一卖=-1、无=0
  int   nMacdZeroPullback;  // 第24/25课B中枢黄白线回拉0轴：一买=1、一卖=-1、无=0
  bool  bOverlapped;        // 二三类买卖点是否重合
  DivergenceResult Divergence;
};

// 中心化分析器：对一组输入一次算成全部中间与最终结果，各 Func 只做投影，消除重复重算。
// 两个 Build 入口对应两类输入家族（信号 pIn / 原始 H/L+config），Points 就绪后共用下游。
struct CzscAnalyzer
{
  int        nCount;
  CzscConfig Config;
  std::vector<SegmentPoint>           Points;      // 已 AssignSegmentEnergy
  std::vector<Center>                 Centers;
  std::vector<TrendStructure>         Structures;
  std::vector<CenterBreakout>         Breakouts;
  std::vector<TradingSignalCandidate> Candidates;
  std::vector<float>                  MaShort;     // 仅 H/L 家族填充
  std::vector<float>                  MaLong;
  std::vector<int>                    Kiss;
  std::vector<int>                    KissVol;     // 带成交量校验的吻（湿吻放量=骗线嫌疑），无量退化为纯价吻
  std::vector<float>                  Volume;      // 旁路注册的成交量（未注册则空）
};

// 动力学：MACD 柱面积及其在线段点上的累积能量
std::vector<float> ComputeMacdHistogram(int nCount, const float *pPrice);
void AssignSegmentEnergy(std::vector<SegmentPoint> &Points, int nCount, const float *pHigh, const float *pLow);

// 均线系统（第11-15课）：简单移动平均；短长均线的吻分类（飞吻/唇吻/湿吻）
std::vector<float> ComputeMovingAverage(int nCount, const float *pPrice, int nPeriod);
std::vector<int> ClassifyMaKisses(const std::vector<float> &Short, const std::vector<float> &Long);
std::vector<int> ClassifyMaKissesWithVolume(const std::vector<float> &Short, const std::vector<float> &Long,
                                            const std::vector<float> &Volume);
void ComputeShortLongMa(int nCount, float *pHigh, float *pLow,
                        std::vector<float> *pShort, std::vector<float> *pLong);

// 配置：默认配置复现现状；DecodeConfig 把单个数字码解出三维配置（供通达信传参）
CzscConfig DefaultConfig();
CzscConfig DecodeConfig(float fCode);

// 形态学流水线：合并K线 → 分型 → 笔 → 线段点 / 线段；以及由信号还原线段点
std::vector<MergedBar> BuildMergedBars(int nCount, float *pHigh, float *pLow);
std::vector<Fractal> BuildFractals(const std::vector<MergedBar> &Bars);
std::vector<Stroke> BuildStrokes(const std::vector<Fractal> &Fractals, const CzscConfig &Config = DefaultConfig());
std::vector<SegmentPoint> BuildSegmentPoints(const std::vector<Stroke> &Strokes);
std::vector<SegmentPoint> BuildLineSegmentPoints(const std::vector<Stroke> &Strokes);
std::vector<SegmentPoint> BuildLineSegmentPointsByFeature(const std::vector<Stroke> &Strokes);
std::vector<SegmentPoint> BuildSignalPoints(int nCount, float *pIn, float *pHigh, float *pLow);
// 按配置（笔类型/笔结束/中枢构件）从 H/L 直接产出供中枢使用的端点（笔端点或线段端点）
std::vector<SegmentPoint> BuildConfiguredPoints(int nCount, float *pHigh, float *pLow, const CzscConfig &Config);

// 中心化分析器的两个构建入口：信号 pIn 家族 / 原始 H/L+config 家族
void BuildAnalyzerFromSignal(CzscAnalyzer &An, int nCount, float *pIn, float *pHigh, float *pLow);
void BuildAnalyzerFromPrice(CzscAnalyzer &An, int nCount, float *pHigh, float *pLow, const CzscConfig &Config);

// 带缓存的入口：通达信就同一序列依次调多个 Func，首调用计算+缓存、后续命中（单槽+全字节指纹）
const CzscAnalyzer &GetOrBuildSignalAnalyzer(int nCount, float *pIn, float *pHigh, float *pLow);
const CzscAnalyzer &GetOrBuildPriceAnalyzer(int nCount, float *pHigh, float *pLow, const CzscConfig &Config);

// 旁路数据：通达信 3 槽不够同时传 H/L/C/V，故用一个注册函数把真实收盘价/成交量存入全局旁路缓存，
// 其余函数在 nCount 匹配且 Close∈[Low,High] 内容校验通过时自动改用真实 C（否则回落 (H+L)/2）。
void RegisterAuxData(int nCount, float *pClose, float *pVolume);
const std::vector<float> *GetValidatedClose(int nCount, const float *pHigh, const float *pLow);
const std::vector<float> *GetValidatedVolume(int nCount, const float *pHigh, const float *pLow);

// 中枢与走势类型，以及中枢关系/三买后续/背驰-转折的分类
std::vector<Center> BuildCenters(const std::vector<SegmentPoint> &Points);
std::vector<TrendStructure> BuildTrendStructures(const std::vector<Center> &Centers);
int ClassifyCenterRelation(const Center &Prev, const Center &Next);
int ClassifyCenterAftermath(const std::vector<Center> &Centers, int nCenter, float fSignal);
int ClassifyReversalStrength(const std::vector<SegmentPoint> &Points,
                             const std::vector<Center> &Centers,
                             int nPoint,
                             int nCenter,
                             float fSignal);
// 买卖点：中枢突破 → 三类买卖点候选；力度/背驰度量
std::vector<CenterBreakout> BuildCenterBreakouts(const std::vector<SegmentPoint> &Points,
                                                 const std::vector<Center> &Centers,
                                                 const std::vector<TrendStructure> &Structures);
std::vector<TradingSignalCandidate> BuildTradingSignalCandidates(const std::vector<SegmentPoint> &Points,
                                                                  const std::vector<Center> &Centers,
                                                                  const std::vector<TrendStructure> &Structures,
                                                                  const std::vector<CenterBreakout> &Breakouts);
StrengthMetrics MeasureStrength(const SegmentPoint &Start, const SegmentPoint &End);
DivergenceResult MeasureDivergence(const SegmentPoint &PrevStart,
                                   const SegmentPoint &PrevEnd,
                                   const SegmentPoint &CurrentStart,
                                   const SegmentPoint &CurrentEnd,
                                   int nDirection);
bool IsWeakerStrength(const StrengthMetrics &Current, const StrengthMetrics &Previous);
// 即时趋势平均力度（第15课）：对未走完的末段提前判背驰，返回 1 见顶 / -1 见底 / 0 无
int DetectInstantDivergence(const std::vector<SegmentPoint> &Points,
                            int nCount,
                            const float *pHigh,
                            const float *pLow);
int BuildTradingSignalContextFlags(const TradingSignalCandidate &C);

// 把候选/中枢结果按优先级写入通达信输出数组（信号、质量、背驰-转折、三买后续、中枢关系）
void ApplyTradingSignalCandidates(int nCount,
                                  float *pOut,
                                  const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalQuality(int nCount,
                               float *pOut,
                               const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalReversal(int nCount,
                                float *pOut,
                                const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalAftermath(int nCount,
                                 float *pOut,
                                 const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalCenterPosition(int nCount,
                                      float *pOut,
                                      const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalMovementType(int nCount,
                                    float *pOut,
                                    const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalPriority(int nCount,
                                float *pOut,
                                const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalCenterId(int nCount,
                                float *pOut,
                                const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalBreakoutId(int nCount,
                                  float *pOut,
                                  const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalPointId(int nCount,
                               float *pOut,
                               const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalTrendId(int nCount,
                               float *pOut,
                               const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalSmallTurn(int nCount,
                                 float *pOut,
                                 const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalAbcStructure(int nCount,
                                    float *pOut,
                                    const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalStrictAbcCandidates(int nCount,
                                           float *pOut,
                                           const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalMacdLineWeakness(int nCount,
                                        float *pOut,
                                        const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalMacdZeroPullback(int nCount,
                                        float *pOut,
                                        const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalStandardDivergence(int nCount,
                                          float *pOut,
                                          const std::vector<TradingSignalCandidate> &Candidates);
void ApplyTradingSignalContextFlags(int nCount,
                                    float *pOut,
                                    const std::vector<TradingSignalCandidate> &Candidates);
void WriteNestedDivergenceSignal(int nCount,
                                 float *pOut,
                                 const std::vector<SegmentPoint> &HighPoints,
                                 const std::vector<TradingSignalCandidate> &HighCandidates,
                                 const std::vector<SegmentPoint> &LowPoints,
                                 const std::vector<TradingSignalCandidate> &LowCandidates);
void WriteSegmentSignal(int nCount, float *pOut, const std::vector<SegmentPoint> &Points);
void WriteCenterRelationSignal(int nCount, float *pOut, const std::vector<Center> &Centers);

// 顶底扫描与化简（早期版本的笔识别，保留供通达信公式使用）
void Parse1(int nCount, float *pOut, float *pHigh, float *pLow);
void Parse2(int nCount, float *pOut, float *pHigh, float *pLow);

// 通达信导出函数（按编号在 Main.cpp 的 Info[] 注册，公式示例见 README）：
//  1 线段点  2/3 中枢高/低  4 中枢起止  5 三类买卖点  6 形态买卖点  7 强度  8 斜率
//  9 线段(笔)  10 信号质量  11 中枢关系  12 背驰-转折  13 三买后续  14 背驰段
//  15 均线差(体位符号/力度幅度)  16 均线吻(飞吻1/唇吻2/湿吻3)  17 即时背驰预警
//  18 笔(新笔标准)  19 线段(特征序列法)  20 配置驱动端点(笔/线段中枢)  30 mode 统一入口
// 配置码：个位=笔类型(0严格/1新)，十位=笔结束(0严格/1次高)，百位=中枢构件(0笔/1线段)，千位=线段法(0启发/1特征)
// 30 号 mode 码(第4参 pTime[0]) = 配置码*1000 + 输出类型*10；输出类型见 exports 里 Func30 的 switch
// Func30 输出 14 为第44课小转大必要条件标记（1=三买，-1=三卖），不占用旧函数编号。
// Func30 输出 15 为第37课a+A+b+B+c结构标记（1=一买，-1=一卖），不占用旧函数编号。
// Func30 输出 16 为ABC严格买卖点（过滤无ABC结构的一类买卖点），不占用旧函数编号。
// Func30 输出 17 为区间套背驰段（高级别背驰段内的低级别一类背驰段），不占用旧函数编号。
// Func30 输出 18 为MACD黄白线高度走弱标记（买=1、卖=-1），不占用旧函数编号。
// Func30 输出 19 为B中枢MACD黄白线回拉0轴标记（买=1、卖=-1），不占用旧函数编号。
// Func30 输出 20 为标准趋势背驰确认标记（买=1、卖=-1），不占用旧函数编号。
// Func30 输出 21 为胜出买卖点候选上下文位图，位值见 CzscSignalContextFlag。
// Func30 输出 22 为胜出买卖点相对中枢位置（-1下/0内/1上/2未知），不占用旧函数编号。
// Func30 输出 23 为胜出买卖点所属走势类型（-1下跌/0盘整/1上涨），不占用旧函数编号。
// Func30 输出 24 为胜出买卖点候选优先级（二类10/三类20/一类30），不占用旧函数编号。
// Func30 输出 25 为胜出买卖点所属中枢一基编号，0=无信号/未知，不占用旧函数编号。
// Func30 输出 26 为胜出买卖点关联突破一基编号，0=无突破，不占用旧函数编号。
// Func30 输出 27 为胜出买卖点对应端点一基编号，0=无信号/未知，不占用旧函数编号。
// Func30 输出 28 为胜出买卖点所属走势结构一基编号，0=无走势结构，不占用旧函数编号。
void Func1(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime);
void Func2(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func3(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func4(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func5(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func6(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func7(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func8(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func9(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime);
void Func10(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func11(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func12(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func13(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func14(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func15(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime);
void Func16(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime);
void Func17(int nCount, float *pOut, float *pIn, float *pHigh, float *pLow);
void Func18(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime);
void Func19(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime);
void Func20(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime);
void Func30(int nCount, float *pOut, float *pHigh, float *pLow, float *pTime);
void Func40(int nCount, float *pOut, float *pClose, float *pVolume, float *pUnused);

#endif
