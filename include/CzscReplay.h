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
#ifndef __CZSC_REPLAY_H__
#define __CZSC_REPLAY_H__

#include "CzscAnalyzer.h"

// 逐步重放（无未来函数）：在每个分型成立时刻 t 只用 [0,t] 已知数据重算笔→端点→中枢→买卖点，
// 记录候选“出现 / 失效”事件。全量结果只保留事后胜出的信号，会隐藏当下出现过、其后被新极值
// 否定的一类点等（幸存者偏差）；回测应以本事件流为准。
struct ReplaySignalEvent
{
  int   nBar;        // 事件发生（当下可知）的原始 K 线下标
  int   nIndex;      // 信号所指的端点 K 线下标
  float fSignal;     // 1/2/3 买、11/12/13 卖
  int   nPriority;   // 同根取胜优先级（同 TradingSignalCandidate）
  bool  bRevoke;     // true=此前出现过的信号在 nBar 失效
  float fStop;       // 信号失效价（止损参考）：一/二买=信号点低点、三买=中枢 ZG；卖点对称(高点/ZD)
};

// nWindow>0 时只逐步重放最近 nWindow 根K线内的事件（窗口起点先算一次基线、不产生事件），
// 耗时随总长度线性；nWindow<=0 为全量重放（O(分型数×端点数)）。
std::vector<ReplaySignalEvent> BuildReplaySignalEvents(int nCount, float *pHigh, float *pLow,
                                                       const CzscConfig &Config, int nWindow = 0);

// 带单槽缓存的入口（键：nCount、配置、窗口、H/L 与旁路收盘价指纹）
const std::vector<ReplaySignalEvent> &GetOrBuildReplaySignalEvents(int nCount, float *pHigh, float *pLow,
                                                                   const CzscConfig &Config, int nWindow);

// 输出：bRevoke=false 的事件写在 nBar 上（当下确认信号）；bRevoke=true 写失效信号码
void WriteReplaySignals(int nCount, float *pOut, const std::vector<ReplaySignalEvent> &Events, bool bRevoke);
// 输出：在信号当下确认的 K 线上写该信号的失效价（止损参考位）
void WriteReplayStops(int nCount, float *pOut, const std::vector<ReplaySignalEvent> &Events);

#endif
