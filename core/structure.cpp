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

// 中枢流：每尝试成枢前记录检查点（扫描位置、已成中枢数、至此读取视界）；输入从 dirty 起变化时
// 从视界早于 dirty 的最后检查点续算。批量构建即空状态下 Update(pivots, 0)。
int CenterStream::Update(const std::vector<Pivot> &p, std::size_t dirty)
{
  while (!checkpoints_.empty() && !checkpoints_.back().horizon.Before(dirty)) checkpoints_.pop_back();
  std::size_t i = 1, keep = 0;
  horizon_ = Horizon();
  if (!checkpoints_.empty())
  {
    Checkpoint cp = checkpoints_.back();
    checkpoints_.pop_back();
    keep = cp.outSize;
    horizon_ = cp.horizon;
    i = cp.i;
  }
  // 只保留并比较被续算覆盖的尾部，之前的中枢不变
  std::vector<Center> oldTail(out_.begin() + static_cast<std::ptrdiff_t>(keep), out_.end());
  out_.resize(keep);
  scoped_ = false;
  Run(p, i, p.empty() ? 0 : p.size() - 1, 0, false);
  std::size_t k = 0;
  while (k < oldTail.size() && keep + k < out_.size() && Same(oldTail[k], out_[keep + k])) k++;
  return (k == oldTail.size() && keep + k == out_.size()) ? -1 : static_cast<int>(keep + k);
}

std::size_t CenterStream::FinalCount(std::size_t pivotFinal) const
{
  std::size_t count = 0;
  for (std::size_t k = checkpoints_.size(); k-- > 0;)
    if (checkpoints_[k].horizon.Before(pivotFinal)) { count = checkpoints_[k].outSize; break; }
  if (scoped_)
  {
    std::size_t stable = 0;
    while (stable < count && out_[stable].end <= finalScopeBar_) ++stable;
    count = stable;
  }
  return count;
}

std::vector<CenterScope> CenterScopes(const std::vector<Pivot> &strokes, const std::vector<Pivot> &segments)
{
  std::vector<CenterScope> scopes;
  if (strokes.empty() || segments.empty()) return scopes;
  auto position = [&](int bar) {
    return static_cast<std::size_t>(std::lower_bound(strokes.begin(), strokes.end(), bar,
      [](const Pivot &p, int index) { return p.index < index; }) - strokes.begin());
  };
  for (std::size_t k = 0; k < segments.size(); ++k)
  {
    std::size_t first = position(segments[k].index);
    bool closed = k + 1 < segments.size();
    std::size_t last = closed ? position(segments[k + 1].index) : strokes.size() - 1;
    if (first >= strokes.size() || last >= strokes.size() || first >= last) continue;
    scopes.push_back({first, last, segments[k].kind == Kind::Bottom ? 1 : -1, closed});
  }
  return scopes;
}

int CenterStream::UpdateScoped(const std::vector<Pivot> &p, const std::vector<CenterScope> &scopes, int finalScopeBar)
{
  // 父线段归属可在笔dirty之前改写；新口径暂以全区间重建保证因果一致。
  std::vector<Center> old = out_;
  out_.clear(); checkpoints_.clear(); horizon_ = Horizon();
  scoped_ = true; finalScopeBar_ = finalScopeBar;
  for (const auto &scope : scopes)
  {
    Run(p, scope.first + 1, scope.last, scope.direction, scope.closed);
    // 即使没有后一个成枢尝试，也记录父段边界读取视界。
    horizon_.Read(scope.last);
    if (!scope.closed) horizon_.Bound();
    checkpoints_.push_back({scope.last, out_.size(), horizon_});
  }
  std::size_t k = 0;
  while (k < old.size() && k < out_.size() && Same(old[k], out_[k])) ++k;
  return k == old.size() && k == out_.size() ? -1 : static_cast<int>(k);
}

std::vector<Center> BuildCentersInSegments(const std::vector<Pivot> &p, const std::vector<Pivot> &segments)
{
  CenterStream stream;
  stream.UpdateScoped(p, CenterScopes(p, segments));
  return stream.Centers();
}

bool CenterStream::HorizonAfter(std::size_t center, Horizon &out) const
{
  for (const Checkpoint &cp : checkpoints_)
    if (cp.outSize > center)
    {
      out = cp.horizon;
      return true;
    }
  return false;
}

bool CenterStream::Same(const Center &a, const Center &b)
{
  return a.firstPivot == b.firstPivot && a.lastPivot == b.lastPivot && a.start == b.start && a.end == b.end &&
         a.zg == b.zg && a.zd == b.zd && a.gg == b.gg && a.dd == b.dd && a.direction == b.direction;
}

void CenterStream::Run(const std::vector<Pivot> &p, std::size_t i, std::size_t limit, int direction, bool closed)
{
  if (p.size() < 4)
  {
    horizon_.Bound();
    return;
  }
  while (i + 3 <= limit)
  {
    // 父段上升从顶起笔，下跌从底起笔；不将社区归属规则冒充第17课定义。
    if (direction != 0 && (p[i].kind == Kind::Top ? 1 : -1) != direction) { ++i; continue; }
    checkpoints_.push_back({i, out_.size(), horizon_});
    Center c;
    horizon_.Read(i + 3);
    if (!TryForm(p, i, c))
    {
      i += direction == 0 ? 1 : 2;
      continue;
    }
    c.direction = direction != 0 ? direction : (p[i - 1].kind == Kind::Bottom ? 1 : -1);

    std::size_t k = i + 3;
    bool leftByPrevious = false;
    bool ended = false;
    while (k + 1 <= limit)
    {
      horizon_.Read(k + 1);
      Range r = Between(p[k], p[k + 1]);
      if (Overlap(c.zd, c.zg, r.low, r.high))
      {
        Extend(c, p, k);
        k++;
        continue;
      }
      int leaveDir = 0;
      bool leave = LeaveAttempt(c, p[k], p[k + 1], leaveDir);
      if (leave && k + 2 > limit && !closed) horizon_.Bound();  // 离开后尚无回试：取决于数据尽头
      if (leave && k + 2 <= limit)
      {
        horizon_.Read(k + 2);
        int retestDir = MoveDirection(p[k + 1], p[k + 2]);
        if (retestDir != 0 && retestDir != leaveDir)
        {
          if (!RetestBackInto(c, p[k + 2], leaveDir))
          {
            ended = true;  // 离开+回试不回 → 三类买卖点，封闭中枢
            break;
          }
          // 离开+回试回中枢：两段本身不与 [ZD,ZG] 重叠，Extend 为空操作，仅越过这两段继续考察
          Extend(c, p, k);
          k++;
          Extend(c, p, k);
          k++;
          continue;
        }
      }
      leftByPrevious = true;
      ended = true;
      break;
    }
    if (!ended) { if (closed) horizon_.Read(limit); else horizon_.Bound(); }  // 延伸到数据尽头，中枢未结束

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
      out_.push_back(c);
      i = k;
      continue;
    }
    out_.push_back(c);
    i = k + 1;
  }
  if (!closed) horizon_.Bound();
}

std::vector<Center> BuildCenters(const std::vector<Pivot> &p)
{
  CenterStream s;
  s.Update(p, 0);
  return s.Centers();
}

CenterRelation Relate(const Center &prev, const Center &next)
{
  if (next.dd > prev.gg) return CenterRelation::Up;
  if (next.gg < prev.dd) return CenterRelation::Down;
  return CenterRelation::Expansion;
}

namespace
{
// 从第 from 个中枢起按贪心分组追加走势：连续同向关系的中枢并为趋势，否则单中枢盘整
void AppendMovements(const std::vector<Center> &centers, std::size_t i, std::vector<Movement> &out)
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
}

bool SameMovement(const Movement &a, const Movement &b)
{
  return a.type == b.type && a.firstCenter == b.firstCenter && a.lastCenter == b.lastCenter && a.start == b.start &&
         a.end == b.end;
}
}  // namespace

std::vector<Movement> BuildMovements(const std::vector<Center> &centers)
{
  std::vector<Movement> out;
  AppendMovements(centers, 0, out);
  return out;
}

int UpdateMovements(const std::vector<Center> &centers, std::vector<Movement> &moves, int dirtyCenter)
{
  if (dirtyCenter < 0) return -1;
  // 走势 [a,b] 由关系 (a,a+1)..(b,b+1) 决定：b+1 触及变化中枢者及其后重建
  std::size_t m = 0;
  while (m < moves.size() && moves[m].lastCenter + 1 < dirtyCenter) m++;
  std::vector<Movement> oldTail(moves.begin() + static_cast<std::ptrdiff_t>(m), moves.end());
  std::size_t from = m < moves.size() ? static_cast<std::size_t>(moves[m].firstCenter)
                                      : (moves.empty() ? 0 : static_cast<std::size_t>(moves.back().lastCenter) + 1);
  moves.resize(m);
  AppendMovements(centers, from, moves);
  std::size_t k = 0;
  while (k < oldTail.size() && m + k < moves.size() && SameMovement(oldTail[k], moves[m + k])) k++;
  return (k == oldTail.size() && m + k == moves.size()) ? -1 : static_cast<int>(m + k);
}

}  // namespace chan
