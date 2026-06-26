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
#include "CzscCore.h"

// 通达信插件函数注册表：编号 → 函数指针，以 {0,NULL} 结尾。
// 通达信公式用 TDXDLL1(编号,...) 调用，编号含义见 CzscCore.h 与 README。
static PluginTCalcFuncInfo Info[] =
{
  {1, &Func1},
  {2, &Func2},
  {3, &Func3},
  {4, &Func4},
  {5, &Func5},
  {6, &Func6},
  {7, &Func7},
  {8, &Func8},
  {9, &Func9},
  {10, &Func10},
  {11, &Func11},
  {12, &Func12},
  {13, &Func13},
  {14, &Func14},
  {15, &Func15},
  {16, &Func16},
  {17, &Func17},
  {18, &Func18},
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

