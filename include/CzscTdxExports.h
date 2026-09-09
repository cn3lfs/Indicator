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
#ifndef __CZSC_TDX_EXPORTS_H__
#define __CZSC_TDX_EXPORTS_H__

#include "CzscAnalyzer.h"

// 通达信导出函数（按编号在 Main.cpp 的 Info[] 注册，公式示例见 README）：
//  1 线段点  2/3 中枢高/低  4 中枢起止  5 三类买卖点  6 形态买卖点  7 强度  8 斜率
//  9 线段(笔)  10 信号质量  11 中枢关系  12 背驰-转折  13 三买后续  14 背驰段
//  15 均线差(体位符号/力度幅度)  16 均线吻(飞/唇/湿)  17 即时背驰预警
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
// Func30 输出 29 为胜出买卖点 C/A 段 MACD 柱面积比(百分比)，0=无有效面积，不占用旧函数编号。
// Func30 输出 30 为胜出买卖点 C/A 段价差力度比(百分比)，0=无有效 A 段价差，不占用旧函数编号。
// Func30 输出 31 为胜出买卖点 C/A 段平均力度比(百分比)，0=无有效 A 段速度，不占用旧函数编号。
// Func30 输出 32 为胜出买卖点背驰要素位图，位值见 CzscDivergenceFlag，不占用旧函数编号。
// Func30 输出 33/34 为胜出买卖点关联突破的离开/回试端点一基编号，0=无关联突破。
// Func30 输出 35 为一类ABC结构关联的三买/三卖突破一基编号，0=无ABC确认。
// Func30 输出 36/37 为一类ABC结构关联突破的离开/回试端点一基编号，0=无ABC确认。
// Func30 输出 38/39 为小转大必要条件关联突破的离开/回试端点一基编号，0=无小转大确认。
// Func30 输出 40/41 为第二类买卖点关联的一类端点/中间反向端点一基编号，0=无二类确认。
// Func30 输出 42 为小转大必要条件关联的小级别一类买卖点端点一基编号，0=无小转大确认。
// Func30 输出 43/44 为胜出买卖点背驰 A 段起点/终点一基编号，0=无 A 段端点。
// Func30 输出 45/46 为胜出买卖点背驰 C 段起点/终点一基编号，0=无 C 段端点。
// Func30 输出 47 为胜出买卖点所属中枢生命周期，0未知/1延伸/2扩展/3上涨新生/-3下跌新生。
// Func30 输出 48 为相邻中枢生命周期关系，在后中枢起点标记，编码同输出47。
// Func30 输出 49 为区间套层级，1=一层区间套，2=低级别小转大必要条件成立。
// Func30 输出 50 为区间套源高级别背驰段一基编号，0=无。
// Func30 输出 51/52 为区间套低级别背驰段起点/终点端点一基编号，0=无。
// Func30 输出 53 为胜出候选背驰语义，0无/1趋势背驰/2盘整背驰/3小转大必要条件。
// Func30 输出 54 为一类背驰后首段回拉/反弹端点一基编号，0=无。
// Func30 输出 55 为买卖点候选过滤原因，0无/1无趋势/2非趋势背驰/3二类顺序失败/4非首次回试/5回中枢/6方向不匹配/7 ABC未对齐/8缺中枢。
// Func30 输出 56 为区间套递归背驰语义，0无/1趋势/2盘整/3小转大。
// Func30 输出 57 为区间套递归确认位图，1内部/2背驰成立/4创新极值/8小转大。
// Func30 输出 58 为区间套低级别背驰方向，1买/-1卖。
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
