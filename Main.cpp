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


#include "Main.h"
#include "tdx/exports.h"

// 通达信插件函数注册表：编号 → 函数，以 {0,NULL} 结尾。公式 TDXDLL1(编号,H,L,配置码)，含义见 README。
static PluginTCalcFuncInfo Info[] =
{
  {1, &tdx::Pivots},            // 端点 顶+1/底-1
  {2, &tdx::CenterHigh},        // 中枢 ZG
  {3, &tdx::CenterLow},         // 中枢 ZD
  {4, &tdx::CenterRelation},    // 相邻中枢关系 1上涨/-1下跌/2扩展
  {5, &tdx::Signals},           // 当下确认买卖点 1/2/3、11/12/13
  {6, &tdx::Revokes},           // 信号失效
  {7, &tdx::Stops},             // 失效价（止损参考）
  {8, &tdx::Divergence},        // c/b MACD 面积比 %
  {9, &tdx::Movements},         // 走势类型 0/1/-1
  {10, &tdx::Kisses},           // 均线吻 1-4
  {11, &tdx::Gaps},             // 缺口 ±1
  {12, &tdx::FractalStrength},  // 分型强弱 ±1/±2
  {13, &tdx::HindsightSignals}, // 事后买卖点（含未来函数，仅复盘）
  {40, &tdx::RegisterCloseVolume},  // 注册真实 C/V
  {41, &tdx::EarlySignals},
  {42, &tdx::EarlyRevokes},
  {43, &tdx::EarlyStops},
  {0, NULL},
};

// 通达信加载插件时调用的导出入口：首次调用返回函数表，重复调用返回 FALSE
BOOL RegisterTdxFunc(PluginTCalcFuncInfo **pInfo)
{
  if (*pInfo == NULL)
  {
    *pInfo = Info;

    return TRUE;
  }

  return FALSE;
}

