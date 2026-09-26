// 结构层（第17/18/20课）：中枢、相邻中枢关系、走势类型。输入为端点序列。
#pragma once

#include "core/model.h"

#include <vector>

namespace chan
{

// 中枢：由进入段之后的连续三段重叠成枢（第17课），[ZD,ZG] 成枢即固定（第20课），与之重叠的段延伸；
// 离开段后回抽不回 [ZD,ZG] 即破坏，离开段归入下一中枢的进入段（第18课定理一/三）。
std::vector<Center> BuildCenters(const std::vector<Pivot> &pivots);

// 中心定理二（第20课）：后DD>前GG 上涨，后GG<前DD 下跌，否则扩展（形成高级别中枢）
CenterRelation Relate(const Center &previous, const Center &next);

// 走势类型（第17课）：连续同向关系的中枢合并为趋势，其余单中枢为盘整
std::vector<Movement> BuildMovements(const std::vector<Center> &centers);

}  // namespace chan
