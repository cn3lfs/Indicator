#include "core/signals.h"

#include "core/dynamics.h"

#include <algorithm>
#include <climits>
#include <cmath>

namespace chan
{
namespace
{

const int kPrioritySecond = 10;
const int kPriorityThird = 20;
const int kPriorityFirst = 30;

int MoveDirection(const Pivot &a, const Pivot &b)
{
  float d = b.Price() - a.Price();
  return d > 0 ? 1 : (d < 0 ? -1 : 0);
}

bool MoreExtreme(const Pivot &base, const Pivot &p)
{
  return base.kind == Kind::Top ? p.high >= base.high : p.low <= base.low;
}

Divergence WithIds(Divergence d, const std::vector<Pivot> &p, int ps, int pe, int cs, int ce)
{
  d.previousStart = ps;
  d.previousEnd = pe;
  d.currentStart = cs;
  d.currentEnd = ce;
  d.previousStartIndex = p[static_cast<std::size_t>(ps)].index;
  d.previousEndIndex = p[static_cast<std::size_t>(pe)].index;
  d.currentStartIndex = p[static_cast<std::size_t>(cs)].index;
  d.currentEndIndex = p[static_cast<std::size_t>(ce)].index;
  return d;
}

// 最后一个起点不晚于 index 的中枢（中枢按起点递增）
int CenterAt(const std::vector<Center> &centers, int index)
{
  auto it = std::upper_bound(centers.begin(), centers.end(), index,
                             [](int idx, const Center &c) { return idx < c.start; });
  return static_cast<int>(it - centers.begin()) - 1;
}

// 起点早于 index 的最后一个同向趋势
int LastTrend(const std::vector<Movement> &moves, int index, int dir)
{
  MovementType want = dir > 0 ? MovementType::Up : MovementType::Down;
  auto it = std::lower_bound(moves.begin(), moves.end(), index,
                             [](const Movement &m, int idx) { return m.start < idx; });
  for (auto r = std::make_reverse_iterator(it); r != moves.rend(); ++r)
    if (r->type == want) return static_cast<int>(moves.rend() - r) - 1;
  return -1;
}

int PivotAt(const std::vector<Pivot> &p, std::size_t before, int index)
{
  for (std::size_t i = before + 1; i-- > 0;)
  {
    if (p[i].index == index) return static_cast<int>(i);
    if (p[i].index < index) break;
  }
  return -1;
}

int FirstPivotAtOrAfter(const std::vector<Pivot> &p, int index)
{
  auto it = std::lower_bound(p.begin(), p.end(), index, [](const Pivot &x, int idx) { return x.index < idx; });
  return static_cast<int>(it - p.begin());
}

// 第24/37课 a+A+b+B+c：c 起点=当前点之前最后一次回到 B 内的端点（第29课：背驰后回到 B 使其事后延伸），
// 当前点须为 c 段极值；b=前一中枢终点→B 起点。定位失败回落为“末笔 vs B 之前最近同向笔”。
bool LocateSegments(const std::vector<Pivot> &p, const std::vector<Center> &centers, int last,
                    std::size_t at, int dir, std::size_t &ps, std::size_t &pe, std::size_t &cs)
{
  const Center &b = centers[static_cast<std::size_t>(last)];
  Kind startKind = dir < 0 ? Kind::Top : Kind::Bottom;
  int c = -1;
  for (std::size_t k = at; k-- > 0;)
  {
    if (p[k].index < b.start) break;
    if (p[k].kind == startKind && (dir > 0 ? p[k].low <= b.zg : p[k].high >= b.zd))
    {
      c = static_cast<int>(k);
      break;
    }
  }
  bool extreme = c >= 0;
  for (std::size_t k = static_cast<std::size_t>(c + 1); extreme && k < at; k++)
    if (p[k].kind == p[at].kind && MoreExtreme(p[at], p[k])) extreme = false;
  int bs = last > 0 ? PivotAt(p, at, centers[static_cast<std::size_t>(last) - 1].end) : -1;
  int be = PivotAt(p, at, b.start);
  if (extreme && bs >= 0 && be > bs && p[static_cast<std::size_t>(bs)].kind == startKind &&
      MoveDirection(p[static_cast<std::size_t>(bs)], p[static_cast<std::size_t>(be)]) == dir)
  {
    ps = static_cast<std::size_t>(bs);
    pe = static_cast<std::size_t>(be);
    cs = static_cast<std::size_t>(c);
    return true;
  }
  if (at < 2 || p[at - 1].kind != startKind) return false;
  for (std::size_t i = at - 2;; i--)
  {
    if (p[i + 1].index < b.start && MoveDirection(p[i], p[i + 1]) == dir)
    {
      ps = i;
      pe = i + 1;
      cs = at - 1;
      return true;
    }
    if (i == 0) return false;
  }
}

// 一类买卖点（第21/24/27课）：至少两个同向中枢的趋势，最后中枢外创新低/新高且 c 相对 b 背驰
bool FirstClass(const std::vector<Pivot> &p, const std::vector<Center> &centers, const std::vector<Movement> &moves,
                std::size_t at, int dir, Signal &out)
{
  Kind want = dir < 0 ? Kind::Bottom : Kind::Top;
  if (at < 4 || p[at].kind != want) return false;
  int trend = LastTrend(moves, p[at].index, dir);
  if (trend < 0) return false;
  const Movement &m = moves[static_cast<std::size_t>(trend)];
  if (m.lastCenter <= m.firstCenter) return false;
  int last = m.lastCenter;
  if (CenterAt(centers, p[at].index) != last) return false;
  const Center &b = centers[static_cast<std::size_t>(last)];
  if (dir < 0 ? p[at].low >= b.zd : p[at].high <= b.zg) return false;
  std::size_t ps = 0, pe = 0, cs = 0;
  if (!LocateSegments(p, centers, last, at, dir, ps, pe, cs)) return false;
  Kind startKind = dir < 0 ? Kind::Top : Kind::Bottom;
  if (p[ps].kind != startKind || p[pe].kind == startKind) return false;
  Divergence d = WithIds(MeasureDivergence(p[ps], p[pe], p[cs], p[at], dir), p, static_cast<int>(ps),
                         static_cast<int>(pe), static_cast<int>(cs), static_cast<int>(at));
  if (!d.holds) return false;
  out.type = dir < 0 ? SignalType::Buy1 : SignalType::Sell1;
  out.pivot = static_cast<int>(at);
  out.index = p[at].index;
  out.center = last;
  out.centerStart = b.start;
  out.priority = kPriorityFirst;
  out.stop = dir < 0 ? p[at].low : p[at].high;
  out.divergence = d;
  return true;
}

std::optional<Signal> FirstAt(const std::vector<Pivot> &p, const std::vector<Center> &centers,
                              const std::vector<Movement> &moves, std::size_t at)
{
  Signal s;
  if (FirstClass(p, centers, moves, at, -1, s) || FirstClass(p, centers, moves, at, 1, s)) return s;
  return std::nullopt;
}

// 同组（类型, 中枢）一类点去重：存在严格更极端者则淘汰；价格在 1e-4 内且更早者存在也淘汰。
// 全量口径保留每个中枢区域最极端的一类点；当下口径由引擎的出现/失效事件体现。
std::vector<int> GroupSurvivors(const std::vector<int> &members, const std::vector<Pivot> &p, bool buy)
{
  auto price = [&](int k) { return buy ? p[static_cast<std::size_t>(k)].low : p[static_cast<std::size_t>(k)].high; };
  std::vector<int> out;
  for (std::size_t a = 0; a < members.size(); a++)
  {
    bool keep = true;
    float fa = price(members[a]);
    for (std::size_t b = 0; b < members.size() && keep; b++)
    {
      if (b == a) continue;
      float fb = price(members[b]);
      bool moreExtreme = buy ? fb < fa : fb > fa;
      if (moreExtreme || (std::fabs(fb - fa) < 0.0001f && b < a)) keep = false;
    }
    if (keep) out.push_back(members[a]);
  }
  return out;
}

// 盘整背驰（第24课）：离开段相对前一同向段
Divergence Consolidation(const std::vector<Pivot> &p, std::size_t leave, int dir)
{
  Divergence none;
  if (leave < 2 || leave >= p.size()) return none;
  std::size_t prev = 0;
  bool found = false;
  for (std::size_t i = leave - 2;; i--)
  {
    if (MoveDirection(p[i], p[i + 1]) == dir)
    {
      prev = i;
      found = true;
      break;
    }
    if (i == 0) break;
  }
  if (!found) return none;
  const Pivot &ps = p[prev], &pe = p[prev + 1], &cs = p[leave - 1], &ce = p[leave];
  Kind lo = dir > 0 ? Kind::Bottom : Kind::Top, hi = Opposite(lo);
  if (ps.kind != lo || pe.kind != hi || cs.kind != lo || ce.kind != hi) return none;
  if (dir > 0 ? ce.high <= pe.high : ce.low >= pe.low) return none;
  return WithIds(MeasureDivergence(ps, pe, cs, ce, dir), p, static_cast<int>(prev), static_cast<int>(prev + 1),
                 static_cast<int>(leave - 1), static_cast<int>(leave));
}

Divergence SecondDivergence(const std::vector<Pivot> &p, std::size_t first)
{
  Divergence d;
  if (first < 1 || first + 2 >= p.size()) return d;
  d.previous = MeasureStrength(p[first - 1], p[first]);
  d.current = MeasureStrength(p[first + 1], p[first + 2]);
  d = WithIds(d, p, static_cast<int>(first) - 1, static_cast<int>(first), static_cast<int>(first) + 1,
              static_cast<int>(first) + 2);
  d.weakSpace = d.current.space < d.previous.space;
  d.weakSpeed = d.current.speed < d.previous.speed;
  d.weakArea = d.current.area > 0 && d.previous.area > 0 && d.current.area < d.previous.area;
  d.holds = (d.weakSpace && d.weakSpeed) || d.weakArea;
  return d;
}

// 二类（第21课）：一类后第二段次级别走势不创新低/新高（恰等于不算，等值口径见 docs）。
// breakoutOf(center) 返回该中枢的突破（若有），用于二三类重合时沿用突破的盘整背驰。
template <class BreakoutOf>
std::optional<Signal> SecondFrom(const Signal &f, const std::vector<Pivot> &p, const std::vector<Center> &centers,
                                 BreakoutOf breakoutOf)
{
  std::size_t at = static_cast<std::size_t>(f.pivot);
  if (at + 2 >= p.size()) return std::nullopt;
  const Pivot &first = p[at], &turn = p[at + 1], &second = p[at + 2];
  bool buy = f.type == SignalType::Buy1;
  if (buy ? !(turn.kind == Kind::Top && second.kind == Kind::Bottom && second.low >= first.low)
          : !(turn.kind == Kind::Bottom && second.kind == Kind::Top && second.high <= first.high))
    return std::nullopt;
  int dir = buy ? 1 : -1;
  const Breakout *b = breakoutOf(f.center);
  bool overlapped = b && b->third && b->direction == dir && b->retestPivot == static_cast<int>(at) + 2;
  Signal s;
  s.type = buy ? SignalType::Buy2 : SignalType::Sell2;
  s.pivot = static_cast<int>(at) + 2;
  s.index = second.index;
  s.center = CenterAt(centers, second.index);
  s.centerStart = s.center >= 0 ? centers[static_cast<std::size_t>(s.center)].start : -1;
  s.basedOnIndex = f.index;
  s.priority = kPrioritySecond;
  s.stop = buy ? second.low : second.high;
  s.divergence = overlapped ? b->divergence : SecondDivergence(p, at);
  return s;
}

Signal ThirdFrom(const Breakout &b, const std::vector<Pivot> &p, const Center &c)
{
  Signal s;
  s.type = b.direction > 0 ? SignalType::Buy3 : SignalType::Sell3;
  s.pivot = b.retestPivot;
  s.index = p[static_cast<std::size_t>(b.retestPivot)].index;
  s.center = b.center;
  s.centerStart = c.start;
  s.priority = kPriorityThird;
  s.stop = b.direction > 0 ? c.zg : c.zd;
  s.divergence = b.divergence;
  return s;
}

}  // namespace

std::optional<Breakout> BreakoutFor(const std::vector<Pivot> &p, const std::vector<Center> &centers, std::size_t ci,
                                    Horizon &h)
{
  const Center &c = centers[ci];
  auto make = [&](std::size_t leave, std::size_t retest, int dir) {
    Breakout b;
    b.center = static_cast<int>(ci);
    b.direction = dir;
    b.leavePivot = static_cast<int>(leave);
    b.retestPivot = static_cast<int>(retest);
    // 回试恰等于 ZG/ZD 不算回到中枢（第20课“不跌破”，等值口径见 docs）
    b.third = dir > 0 ? !(p[retest].low < c.zg) : !(p[retest].high > c.zd);
    b.divergence = Consolidation(p, leave, dir);
    return b;
  };
  std::size_t first = static_cast<std::size_t>(c.lastPivot) + 1;  // 首个下标晚于中枢终点的端点
  for (std::size_t j = std::max<std::size_t>(first, 1); j < p.size(); j++)
  {
    h.Read(j);
    const Pivot &s = p[j - 1], &e = p[j];
    int dir = MoveDirection(s, e);
    if (s.kind == Kind::Bottom && s.low < c.zd)
    {
      if (dir < 0) continue;
      return make(j - 1, j, -1);
    }
    if (s.kind == Kind::Top && s.high > c.zg)
    {
      if (dir > 0) continue;
      return make(j - 1, j, 1);
    }
    bool up = dir > 0 && e.kind == Kind::Top && e.high > c.zg;
    bool down = dir < 0 && e.kind == Kind::Bottom && e.low < c.zd;
    if (!up && !down) continue;
    int bd = up ? 1 : -1;
    for (std::size_t k = j + 1; k < p.size(); k++)
    {
      h.Read(k);
      if ((bd > 0 && p[k].kind == Kind::Bottom) || (bd < 0 && p[k].kind == Kind::Top)) return make(j, k, bd);
    }
    h.Bound();
    return std::nullopt;  // 离开后尚无回试（第20课“必须是第一次”，只取第一次离开）
  }
  h.Bound();
  return std::nullopt;
}

std::vector<Breakout> BuildBreakouts(const std::vector<Pivot> &p, const std::vector<Center> &centers)
{
  std::vector<Breakout> out;
  if (p.size() < 2) return out;
  for (std::size_t ci = 0; ci < centers.size(); ci++)
  {
    Horizon h;
    if (std::optional<Breakout> b = BreakoutFor(p, centers, ci, h)) out.push_back(*b);
  }
  return out;
}

std::vector<Signal> BuildSignals(const std::vector<Pivot> &p, const std::vector<Center> &centers,
                                 const std::vector<Movement> &moves, const std::vector<Breakout> &breakouts)
{
  // 一类：逐端点判定后按 (类型, 中枢) 分组去重，保持端点顺序
  std::vector<std::optional<Signal>> raw(p.size());
  std::map<std::pair<int, int>, std::vector<int>> groups;
  for (std::size_t i = 0; i < p.size(); i++)
  {
    raw[i] = FirstAt(p, centers, moves, i);
    if (raw[i]) groups[{static_cast<int>(raw[i]->type), raw[i]->center}].push_back(static_cast<int>(i));
  }
  std::vector<bool> keep(p.size(), false);
  for (const auto &g : groups)
    for (int k : GroupSurvivors(g.second, p, g.first.first == static_cast<int>(SignalType::Buy1)))
      keep[static_cast<std::size_t>(k)] = true;
  std::vector<Signal> firsts;
  for (std::size_t i = 0; i < p.size(); i++)
    if (keep[i]) firsts.push_back(*raw[i]);

  auto breakoutOf = [&](int center) -> const Breakout * {
    for (const Breakout &b : breakouts)
      if (b.center == center && b.third) return &b;
    return nullptr;
  };
  std::vector<Signal> out;
  for (const Signal &f : firsts)
    if (std::optional<Signal> s = SecondFrom(f, p, centers, breakoutOf)) out.push_back(*s);
  for (const Breakout &b : breakouts)
    if (b.third && b.direction != 0) out.push_back(ThirdFrom(b, p, centers[static_cast<std::size_t>(b.center)]));
  out.insert(out.end(), firsts.begin(), firsts.end());
  return out;
}

//----------------------------------------------------------------------------
// 买卖点流
//----------------------------------------------------------------------------

void SignalStream::Put(const Signal &s, Source source)
{
  byKey_[KeyOf(s)][source] = s;
  touched_.insert(KeyOf(s));
}

void SignalStream::Drop(const Signal &s, Source source)
{
  SignalKey key = KeyOf(s);
  auto it = byKey_.find(key);
  if (it != byKey_.end())
  {
    it->second.erase(source);
    if (it->second.empty()) byKey_.erase(it);
  }
  touched_.insert(key);
}

void SignalStream::Update(const std::vector<Pivot> &p, const std::vector<Center> &centers,
                          const std::vector<Movement> &moves, int dirtyPivot, int dirtyCenter, int dirtyMove, int bar,
                          bool emit, std::vector<SignalEvent> &events)
{
  const int P = static_cast<int>(p.size());
  const int oldP = static_cast<int>(pivotCount_);
  const std::size_t C = centers.size();
  touched_.clear();

  // 受影响端点阈值 T：变化端点、首个变化中枢/走势起点（新旧取早）所对应的端点
  int T = dirtyPivot >= 0 ? std::min(dirtyPivot, P) : P;
  auto earliest = [](int a, int b) { return std::min(a, b); };
  if (dirtyCenter >= 0)
  {
    int sc = INT_MAX;
    if (dirtyCenter < static_cast<int>(C)) sc = centers[static_cast<std::size_t>(dirtyCenter)].start;
    if (dirtyCenter < static_cast<int>(centerStarts_.size())) sc = earliest(sc, centerStarts_[static_cast<std::size_t>(dirtyCenter)]);
    if (sc != INT_MAX) T = std::min(T, FirstPivotAtOrAfter(p, sc));
  }
  if (dirtyMove >= 0)
  {
    int sm = INT_MAX;
    if (dirtyMove < static_cast<int>(moves.size())) sm = moves[static_cast<std::size_t>(dirtyMove)].start;
    if (dirtyMove < static_cast<int>(moveStarts_.size())) sm = earliest(sm, moveStarts_[static_cast<std::size_t>(dirtyMove)]);
    if (sm != INT_MAX) T = std::min(T, FirstPivotAtOrAfter(p, sm));
  }

  // 1) 一类原始候选：端点 >= T 重算，维护 (类型, 中枢) 分组
  std::set<std::pair<int, int>> groups;
  for (std::size_t k = static_cast<std::size_t>(T); k < rawFirst_.size(); k++)
  {
    if (const std::optional<Signal> &f = rawFirst_[k])
    {
      std::pair<int, int> g{static_cast<int>(f->type), f->center};
      members_[g].erase(static_cast<int>(k));
      groups.insert(g);
    }
  }
  rawFirst_.resize(static_cast<std::size_t>(P));
  for (std::size_t k = static_cast<std::size_t>(T); k < rawFirst_.size(); k++)
  {
    rawFirst_[k] = FirstAt(p, centers, moves, k);
    if (rawFirst_[k])
    {
      std::pair<int, int> g{static_cast<int>(rawFirst_[k]->type), rawFirst_[k]->center};
      members_[g].insert(static_cast<int>(k));
      groups.insert(g);
    }
  }

  // 2) 受影响分组重算去重幸存者
  std::set<int> survivorChanged;
  for (const std::pair<int, int> &g : groups)
  {
    auto oldIt = groupSurvivors_.find(g);
    if (oldIt != groupSurvivors_.end())
    {
      for (int k : oldIt->second)
      {
        auto sIt = survivors_.find(k);
        if (sIt != survivors_.end())
        {
          Drop(sIt->second, {1, k});
          survivors_.erase(sIt);
        }
        survivorChanged.insert(k);
      }
      groupSurvivors_.erase(oldIt);
    }
    auto mIt = members_.find(g);
    if (mIt == members_.end()) continue;
    if (mIt->second.empty())
    {
      members_.erase(mIt);
      continue;
    }
    std::vector<int> now = GroupSurvivors(std::vector<int>(mIt->second.begin(), mIt->second.end()), p,
                                          g.first == static_cast<int>(SignalType::Buy1));
    for (int k : now)
    {
      survivors_[k] = *rawFirst_[static_cast<std::size_t>(k)];
      Put(survivors_[k], {1, k});
      survivorChanged.insert(k);
    }
    groupSurvivors_[g] = now;
  }

  // 3) 突破：中枢 >= 首个变化中枢，或扫描视界触及变化端点
  std::set<int> breakoutChanged;
  int dc = dirtyCenter >= 0 ? dirtyCenter : static_cast<int>(C);
  std::size_t dp = static_cast<std::size_t>(dirtyPivot >= 0 ? dirtyPivot : P);
  auto unindex = [&](int ci) {
    if (!breakoutIndexed_[static_cast<std::size_t>(ci)]) return;
    breakoutIndexed_[static_cast<std::size_t>(ci)] = 0;
    const Horizon &h = breakoutHorizons_[static_cast<std::size_t>(ci)];
    if (h.unbounded)
    {
      openBreakouts_.erase(ci);
      return;
    }
    auto range = breakoutByHorizon_.equal_range(h.max);
    for (auto it = range.first; it != range.second; ++it)
      if (it->second == ci)
      {
        breakoutByHorizon_.erase(it);
        break;
      }
  };
  // 需重算：变化中枢及其后、视界无界者、视界触及变化端点者
  std::set<int> redoBreakouts;
  for (std::size_t ci = C; ci < breakouts_.size(); ci++)
  {
    unindex(static_cast<int>(ci));
    breakoutChanged.insert(static_cast<int>(ci));
  }
  breakouts_.resize(C);
  breakoutHorizons_.resize(C);
  breakoutIndexed_.resize(C, 0);
  for (int ci = dc; ci < static_cast<int>(C); ci++) redoBreakouts.insert(ci);
  for (int ci : openBreakouts_)
    if (ci < static_cast<int>(C)) redoBreakouts.insert(ci);
  for (auto it = breakoutByHorizon_.lower_bound(dp); it != breakoutByHorizon_.end(); ++it)
    if (it->second < static_cast<int>(C)) redoBreakouts.insert(it->second);
  for (int ci : redoBreakouts)
  {
    std::size_t u = static_cast<std::size_t>(ci);
    unindex(ci);
    Horizon h;
    breakouts_[u] = BreakoutFor(p, centers, u, h);
    breakoutHorizons_[u] = h;
    if (h.unbounded) openBreakouts_.insert(ci); else breakoutByHorizon_.insert({h.max, ci});
    breakoutIndexed_[u] = 1;
    breakoutChanged.insert(ci);
  }

  // 4) 三类：随突破更新
  for (int ci : breakoutChanged)
  {
    auto tIt = thirds_.find(ci);
    if (tIt != thirds_.end())
    {
      Drop(tIt->second, {3, ci});
      thirds_.erase(tIt);
    }
    if (ci >= static_cast<int>(C)) continue;
    const std::optional<Breakout> &b = breakouts_[static_cast<std::size_t>(ci)];
    if (!b || !b->third || b->direction == 0) continue;
    thirds_[ci] = ThirdFrom(*b, p, centers[static_cast<std::size_t>(ci)]);
    Put(thirds_[ci], {3, ci});
  }

  // 5) 二类：幸存者变化、读取范围（端点+2、所在中枢）触及 T、或所依中枢突破变化者重算
  std::set<int> redo(survivorChanged.begin(), survivorChanged.end());
  for (auto it = survivors_.lower_bound(std::max(T - 2, 0)); it != survivors_.end(); ++it) redo.insert(it->first);
  for (int ci : breakoutChanged)
    for (SignalType t : {SignalType::Buy1, SignalType::Sell1})
    {
      auto g = groupSurvivors_.find({static_cast<int>(t), ci});
      if (g != groupSurvivors_.end()) redo.insert(g->second.begin(), g->second.end());
    }
  for (auto it = seconds_.lower_bound(std::max(T - 2, 0)); it != seconds_.end(); ++it) redo.insert(it->first);
  auto breakoutOf = [&](int center) -> const Breakout * {
    if (center < 0 || static_cast<std::size_t>(center) >= breakouts_.size() || !breakouts_[static_cast<std::size_t>(center)])
      return nullptr;
    return &*breakouts_[static_cast<std::size_t>(center)];
  };
  for (int k : redo)
  {
    auto sIt = seconds_.find(k);
    if (sIt != seconds_.end())
    {
      Drop(sIt->second, {2, k});
      seconds_.erase(sIt);
    }
    auto fIt = survivors_.find(k);
    if (fIt == survivors_.end()) continue;
    if (std::optional<Signal> s = SecondFrom(fIt->second, p, centers, breakoutOf))
    {
      seconds_[k] = *s;
      Put(*s, {2, k});
    }
  }

  // 6) 事件：受影响键 + 确认状态可能翻转的末尾端点
  int X = std::max(std::min(oldP, P) - 2, 0);
  int fromBar = X < P ? p[static_cast<std::size_t>(X)].index : INT_MAX;
  for (auto it = byKey_.lower_bound({fromBar, INT_MIN}); it != byKey_.end(); ++it) touched_.insert(it->first);
  for (auto it = active_.lower_bound({fromBar, INT_MIN}); it != active_.end(); ++it) touched_.insert(it->first);

  std::vector<SignalEvent> appear, revoke;
  for (const SignalKey &key : touched_)
  {
    const Signal *winner = nullptr;
    auto it = byKey_.find(key);
    if (it != byKey_.end() && !it->second.empty())
    {
      const Signal &w = it->second.begin()->second;  // 同键必同类（同优先级），取来源序最先者
      if (w.pivot >= 0 && w.pivot + 1 < P) winner = &w;
    }
    auto aIt = active_.find(key);
    if (winner && aIt == active_.end())
    {
      if (emit) appear.push_back({bar, *winner, false});
      active_[key] = *winner;
    }
    else if (!winner && aIt != active_.end())
    {
      if (emit) revoke.push_back({bar, aIt->second, true});
      active_.erase(aIt);
    }
    else if (winner)
    {
      aIt->second = *winner;
    }
  }
  events.insert(events.end(), appear.begin(), appear.end());
  events.insert(events.end(), revoke.begin(), revoke.end());

  pivotCount_ = static_cast<std::size_t>(P);
  centerStarts_.resize(C);
  for (std::size_t i = 0; i < C; i++) centerStarts_[i] = centers[i].start;
  moveStarts_.resize(moves.size());
  for (std::size_t i = 0; i < moves.size(); i++) moveStarts_[i] = moves[i].start;
}

}  // namespace chan
