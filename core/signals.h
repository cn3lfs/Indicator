// 买卖点（第20/21/24/27/29/37课）：一类=趋势背驰，二类=一类后第二段不创新低/新高，三类=离开中枢后首次回试不回。
#pragma once

#include "core/model.h"

#include <vector>

namespace chan
{

// 中枢首次离开与回试（第20课）；third=回试不回 [ZD,ZG]，构成三类买卖点
struct Breakout
{
  int center = -1;
  int direction = 0;
  int leavePivot = -1;
  int retestPivot = -1;
  bool third = false;
  Divergence divergence;  // 离开段相对前一同向段的盘整背驰（第24课）
};

std::vector<Breakout> BuildBreakouts(const std::vector<Pivot> &pivots, const std::vector<Center> &centers);

// 全部候选：顺序为 二类、三类、一类（与同根取胜规则配合：一类 30 > 三类 20 > 二类 10）
std::vector<Signal> BuildSignals(const std::vector<Pivot> &pivots, const std::vector<Center> &centers,
                                 const std::vector<Movement> &movements, const std::vector<Breakout> &breakouts);

}  // namespace chan
