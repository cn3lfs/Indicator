#include "core/structure.h"

#include <algorithm>

namespace chan
{
namespace
{

struct Range
{
  float high, low;
};

Range Between(const Pivot &a, const Pivot &b)
{
  float pa = a.Price(), pb = b.Price();
  return pa > pb ? Range{pa, pb} : Range{pb, pa};
}

bool Overlap(float l1, float h1, float l2, float h2) { return l1 <= h2 && l2 <= h1; }

int MoveDirection(const Pivot &a, const Pivot &b)
{
  float d = b.Price() - a.Price();
  return d > 0 ? 1 : (d < 0 ? -1 : 0);
}

// 连续三段 [i,i+3] 成枢：ZG=三段高点最小、ZD=三段低点最大（第18课 [max(a2,b2,c2), min(a1,b1,c1)]）
bool TryForm(const std::vector<Pivot> &p, std::size_t i, Center &c)
{
  Range a = Between(p[i], p[i + 1]), b = Between(p[i + 1], p[i + 2]), d = Between(p[i + 2], p[i + 3]);
  float zd = std::max({a.low, b.low, d.low});
  float zg = std::min({a.high, b.high, d.high});
  if (zd > zg) return false;
  c.firstPivot = static_cast<int>(i);
  c.lastPivot = static_cast<int>(i + 3);
  c.start = p[i].index;
  c.end = p[i + 3].index;
  c.zg = zg;
  c.zd = zd;
  c.gg = std::max({a.high, b.high, d.high});
  c.dd = std::min({a.low, b.low, d.low});
  return true;
}

// 延伸：与 [ZD,ZG] 重叠的段扩张 GG/DD 并后移终点；不重叠为空操作
void Extend(Center &c, const std::vector<Pivot> &p, std::size_t from)
{
  Range r = Between(p[from], p[from + 1]);
  if (!Overlap(c.zd, c.zg, r.low, r.high)) return;
  c.gg = std::max(c.gg, r.high);
  c.dd = std::min(c.dd, r.low);
  c.end = p[from + 1].index;
  c.lastPivot = static_cast<int>(from + 1);
}

bool LeaveAttempt(const Center &c, const Pivot &a, const Pivot &b, int &dir)
{
  dir = MoveDirection(a, b);
  if (dir > 0 && b.kind == Kind::Top && b.high > c.zg) return true;
  if (dir < 0 && b.kind == Kind::Bottom && b.low < c.zd) return true;
  return false;
}

bool RetestBackInto(const Center &c, const Pivot &retest, int dir)
{
  if (dir > 0) return retest.low < c.zg;
  if (dir < 0) return retest.high > c.zd;
  return false;
}

}  // namespace

std::vector<Center> BuildCenters(const std::vector<Pivot> &p)
{
  std::vector<Center> out;
  if (p.size() < 4) return out;
  std::size_t i = 1;
  while (i + 3 < p.size())
  {
    Center c;
    if (!TryForm(p, i, c))
    {
      i++;
      continue;
    }
    c.direction = p[i - 1].kind == Kind::Bottom ? 1 : -1;

    std::size_t k = i + 3;
    bool leftByPrevious = false;
    while (k + 1 < p.size())
    {
      Range r = Between(p[k], p[k + 1]);
      if (Overlap(c.zd, c.zg, r.low, r.high))
      {
        Extend(c, p, k);
        k++;
        continue;
      }
      int leaveDir = 0;
      if (LeaveAttempt(c, p[k], p[k + 1], leaveDir) && k + 2 < p.size())
      {
        int retestDir = MoveDirection(p[k + 1], p[k + 2]);
        if (retestDir != 0 && retestDir != leaveDir)
        {
          if (!RetestBackInto(c, p[k + 2], leaveDir)) break;  // 离开+回试不回 → 三类买卖点，封闭中枢
          // 离开+回试回中枢：两段本身不与 [ZD,ZG] 重叠，Extend 为空操作，仅越过这两段继续考察
          Extend(c, p, k);
          k++;
          Extend(c, p, k);
          k++;
          continue;
        }
      }
      leftByPrevious = true;
      break;
    }

    // 第18课定理三：离开段不属于本中枢，退回终点并按保留端点重算 GG/DD，离开段作下一中枢进入段
    if (leftByPrevious && k > i + 3)
    {
      c.lastPivot = static_cast<int>(k - 1);
      c.end = p[k - 1].index;
      c.gg = c.dd = p[i].Price();
      for (std::size_t j = i + 1; j < k; j++)
      {
        c.gg = std::max(c.gg, p[j].Price());
        c.dd = std::min(c.dd, p[j].Price());
      }
      out.push_back(c);
      i = k;
      continue;
    }
    out.push_back(c);
    i = k + 1;
  }
  return out;
}

CenterRelation Relate(const Center &prev, const Center &next)
{
  if (next.dd > prev.gg) return CenterRelation::Up;
  if (next.gg < prev.dd) return CenterRelation::Down;
  return CenterRelation::Expansion;
}

std::vector<Movement> BuildMovements(const std::vector<Center> &centers)
{
  auto typeOf = [](CenterRelation r) {
    return r == CenterRelation::Up ? MovementType::Up
           : r == CenterRelation::Down ? MovementType::Down : MovementType::Consolidation;
  };
  auto make = [&](MovementType t, std::size_t a, std::size_t b) {
    Movement m;
    m.type = t;
    m.firstCenter = static_cast<int>(a);
    m.lastCenter = static_cast<int>(b);
    m.start = centers[a].start;
    m.end = centers[b].end;
    return m;
  };
  std::vector<Movement> out;
  std::size_t i = 0;
  while (i < centers.size())
  {
    if (i + 1 >= centers.size())
    {
      out.push_back(make(MovementType::Consolidation, i, i));
      break;
    }
    MovementType t = typeOf(Relate(centers[i], centers[i + 1]));
    if (t == MovementType::Consolidation)
    {
      out.push_back(make(t, i, i));
      i++;
      continue;
    }
    std::size_t last = i + 1;
    while (last + 1 < centers.size() && typeOf(Relate(centers[last], centers[last + 1])) == t) last++;
    out.push_back(make(t, i, last));
    i = last + 1;
  }
  return out;
}

}  // namespace chan
