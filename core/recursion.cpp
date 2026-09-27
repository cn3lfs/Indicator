#include "core/recursion.h"

#include "core/structure.h"

#include <algorithm>

namespace chan
{
namespace
{

// -1 表示未定型；若任一为 -1 则整体未定型，否则取最晚者
int Later(int a, int b) { return (a < 0 || b < 0) ? -1 : std::max(a, b); }

float Mid(const Center &c) { return (c.zg + c.zd) * 0.5f; }

// 相邻走势的分界：前走势最后中枢末端点 → 后走势首中枢首端点之间的极值点；
// 后继中枢更高取最低点（上一层向上段由此开始），更低取最高点（等值取先出现者）
std::vector<int> Boundaries(const RecursiveLevel &l)
{
  std::vector<int> b;
  const std::vector<Movement> &m = l.movements;
  if (m.empty()) return b;
  const Center &first = l.centers[static_cast<std::size_t>(m[0].firstCenter)];
  b.push_back(first.firstPivot > 0 ? first.firstPivot - 1 : first.firstPivot);  // 首个走势自首中枢的进入点起
  for (std::size_t i = 0; i + 1 < m.size(); i++)
  {
    const Center &a = l.centers[static_cast<std::size_t>(m[i].lastCenter)];
    const Center &c = l.centers[static_cast<std::size_t>(m[i + 1].firstCenter)];
    bool up = Mid(c) > Mid(a);
    int best = a.lastPivot;
    for (int k = a.lastPivot; k <= c.firstPivot; k++)
    {
      float v = l.pivots[static_cast<std::size_t>(k)].Price(), w = l.pivots[static_cast<std::size_t>(best)].Price();
      if (up ? v < w : v > w) best = k;
    }
    b.push_back(best);
  }
  b.push_back(static_cast<int>(l.pivots.size()) - 1);  // 最后一个走势暂以最后端点结束
  return b;
}

// 由下层分界点得到上层端点：相邻同型保留更极端者（等值保留先者），并给出定型时刻
void NextPivots(const RecursiveLevel &l, std::vector<Pivot> &pivots, std::vector<int> &finalAt)
{
  const std::vector<int> &b = l.boundaries;
  std::size_t nm = l.movements.size();
  auto boundaryFinal = [&](std::size_t j) {
    if (j == 0) return l.pivotFinalAt[static_cast<std::size_t>(b[0])];
    if (j >= nm) return -1;  // 最后一个走势的终点尚未确定
    return Later(l.movementFinalAt[j - 1], l.movementFinalAt[j]);
  };
  std::vector<std::size_t> groupStart;
  for (std::size_t j = 0; j < b.size(); j++)
  {
    Pivot p = l.pivots[static_cast<std::size_t>(b[j])];
    p.fractalAt = -1;
    int f = boundaryFinal(j);
    if (!pivots.empty() && pivots.back().kind == p.kind)
    {
      bool more = p.kind == Kind::Top ? p.high > pivots.back().high : p.low < pivots.back().low;
      if (more) pivots.back() = p;
      finalAt.back() = Later(finalAt.back(), f);
      continue;
    }
    // 上一组的取舍在本组首个异型点出现时才确定
    if (!finalAt.empty()) finalAt.back() = Later(finalAt.back(), f);
    pivots.push_back(p);
    finalAt.push_back(f);
  }
}

void BuildStructure(RecursiveLevel &l)
{
  CenterStream stream;
  stream.Update(l.pivots, 0);
  l.centers = stream.Centers();
  l.centerFinalAt.assign(l.centers.size(), -1);
  int running = 0;
  for (std::size_t c = 0; c < l.centers.size(); c++)
  {
    Horizon h;
    if (!stream.HorizonAfter(c, h) || h.unbounded || h.max >= l.pivotFinalAt.size()) break;
    int f = running;
    for (std::size_t k = 0; k <= h.max && f >= 0; k++) f = Later(f, l.pivotFinalAt[k]);
    if (f < 0) break;
    l.centerFinalAt[c] = running = f;
  }
  l.movements = BuildMovements(l.centers);
  l.movementFinalAt.assign(l.movements.size(), -1);
  for (std::size_t m = 0; m < l.movements.size(); m++)
  {
    std::size_t next = static_cast<std::size_t>(l.movements[m].lastCenter) + 1;  // 分组由其后一个中枢截止
    if (next < l.centers.size()) l.movementFinalAt[m] = l.centerFinalAt[next];
  }
  l.boundaries = Boundaries(l);
}

}  // namespace

std::vector<RecursiveLevel> BuildRecursion(const Analysis &a, int maxLevels)
{
  std::vector<RecursiveLevel> levels;
  RecursiveLevel base;
  base.pivots = a.snapshot.pivots;
  base.pivotFinalAt = a.pivotFinalAt;
  base.centers = a.snapshot.centers;
  base.centerFinalAt = a.centerFinalAt;
  base.movements = a.snapshot.movements;
  base.movementFinalAt = a.movementFinalAt;
  base.boundaries = Boundaries(base);
  if (base.movements.empty()) return levels;
  levels.push_back(base);
  while (static_cast<int>(levels.size()) < maxLevels && levels.back().movements.size() >= 3)
  {
    RecursiveLevel next;
    NextPivots(levels.back(), next.pivots, next.pivotFinalAt);
    if (next.pivots.size() < 4) break;
    BuildStructure(next);
    if (next.centers.empty()) break;
    levels.push_back(std::move(next));
  }
  return levels;
}

}  // namespace chan
