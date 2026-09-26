#include "core/signals.h"

#include "core/dynamics.h"

#include <algorithm>
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

Divergence WithIds(Divergence d, int ps, int pe, int cs, int ce)
{
  d.previousStart = ps;
  d.previousEnd = pe;
  d.currentStart = cs;
  d.currentEnd = ce;
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
  Divergence d = WithIds(MeasureDivergence(p[ps], p[pe], p[cs], p[at], dir), static_cast<int>(ps),
                         static_cast<int>(pe), static_cast<int>(cs), static_cast<int>(at));
  if (!d.holds) return false;
  out.type = dir < 0 ? SignalType::Buy1 : SignalType::Sell1;
  out.pivot = static_cast<int>(at);
  out.index = p[at].index;
  out.center = last;
  out.priority = kPriorityFirst;
  out.stop = dir < 0 ? p[at].low : p[at].high;
  out.divergence = d;
  return true;
}

// 同一中枢区只保留价格最极端的一类点（全量口径；当下口径见引擎的出现/失效事件）
std::vector<Signal> Dedup(const std::vector<Signal> &firsts, const std::vector<Pivot> &p)
{
  std::vector<Signal> out;
  auto price = [&](const Signal &s) {
    return s.type == SignalType::Buy1 ? p[static_cast<std::size_t>(s.pivot)].low : p[static_cast<std::size_t>(s.pivot)].high;
  };
  for (std::size_t a = 0; a < firsts.size(); a++)
  {
    bool keep = true;
    for (std::size_t b = 0; b < firsts.size() && keep; b++)
    {
      if (b == a || firsts[b].type != firsts[a].type || firsts[b].center != firsts[a].center) continue;
      float fa = price(firsts[a]), fb = price(firsts[b]);
      bool moreExtreme = firsts[a].type == SignalType::Buy1 ? fb < fa : fb > fa;
      if (moreExtreme || (std::fabs(fb - fa) < 0.0001f && b < a)) keep = false;
    }
    if (keep) out.push_back(firsts[a]);
  }
  return out;
}

// 盘整背驰（第24课）：离开段相对前一同向段
Divergence Consolidation(const std::vector<Pivot> &p, std::size_t leave, int dir)
{
  Divergence none;
  if (leave == 0 || leave >= p.size() || leave < 2) return none;
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
  return WithIds(MeasureDivergence(ps, pe, cs, ce, dir), static_cast<int>(prev), static_cast<int>(prev + 1),
                 static_cast<int>(leave - 1), static_cast<int>(leave));
}

Divergence SecondDivergence(const std::vector<Pivot> &p, std::size_t first, int dir)
{
  Divergence d;
  if (first < 1 || first + 2 >= p.size()) return d;
  d.previous = MeasureStrength(p[first - 1], p[first]);
  d.current = MeasureStrength(p[first + 1], p[first + 2]);
  d = WithIds(d, static_cast<int>(first) - 1, static_cast<int>(first), static_cast<int>(first) + 1,
              static_cast<int>(first) + 2);
  d.weakSpace = d.current.space < d.previous.space;
  d.weakSpeed = d.current.speed < d.previous.speed;
  d.weakArea = d.current.area > 0 && d.previous.area > 0 && d.current.area < d.previous.area;
  d.holds = (d.weakSpace && d.weakSpeed) || d.weakArea;
  (void)dir;
  return d;
}

}  // namespace

std::vector<Breakout> BuildBreakouts(const std::vector<Pivot> &p, const std::vector<Center> &centers)
{
  std::vector<Breakout> out;
  if (p.size() < 2) return out;
  auto make = [&](std::size_t ci, std::size_t leave, std::size_t retest, int dir) {
    const Center &c = centers[ci];
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
  for (std::size_t ci = 0; ci < centers.size(); ci++)
  {
    const Center &c = centers[ci];
    std::size_t first = static_cast<std::size_t>(c.lastPivot) + 1;  // 首个下标晚于中枢终点的端点
    for (std::size_t j = std::max<std::size_t>(first, 1); j < p.size(); j++)
    {
      const Pivot &s = p[j - 1], &e = p[j];
      int dir = MoveDirection(s, e);
      if (s.kind == Kind::Bottom && s.low < c.zd)
      {
        if (dir < 0) continue;
        out.push_back(make(ci, j - 1, j, -1));
        break;
      }
      if (s.kind == Kind::Top && s.high > c.zg)
      {
        if (dir > 0) continue;
        out.push_back(make(ci, j - 1, j, 1));
        break;
      }
      bool up = dir > 0 && e.kind == Kind::Top && e.high > c.zg;
      bool down = dir < 0 && e.kind == Kind::Bottom && e.low < c.zd;
      if (!up && !down) continue;
      int bd = up ? 1 : -1;
      for (std::size_t k = j + 1; k < p.size(); k++)
      {
        if ((bd > 0 && p[k].kind == Kind::Bottom) || (bd < 0 && p[k].kind == Kind::Top))
        {
          out.push_back(make(ci, j, k, bd));
          break;
        }
      }
      break;  // 每个中枢只取第一次离开+回试（第20课“必须是第一次”）
    }
  }
  return out;
}

std::vector<Signal> BuildSignals(const std::vector<Pivot> &p, const std::vector<Center> &centers,
                                 const std::vector<Movement> &moves, const std::vector<Breakout> &breakouts)
{
  std::vector<Signal> firsts;
  for (std::size_t i = 0; i < p.size(); i++)
  {
    Signal s;
    if (FirstClass(p, centers, moves, i, -1, s) || FirstClass(p, centers, moves, i, 1, s)) firsts.push_back(s);
  }
  firsts = Dedup(firsts, p);

  std::vector<Signal> out;
  // 二类（第21课）：一类后第二段次级别走势不创新低/新高（恰等于不算，等值口径见 docs）
  for (const Signal &f : firsts)
  {
    std::size_t at = static_cast<std::size_t>(f.pivot);
    if (at + 2 >= p.size()) continue;
    const Pivot &first = p[at], &turn = p[at + 1], &second = p[at + 2];
    bool buy = f.type == SignalType::Buy1;
    if (buy ? !(turn.kind == Kind::Top && second.kind == Kind::Bottom && second.low >= first.low)
            : !(turn.kind == Kind::Bottom && second.kind == Kind::Top && second.high <= first.high))
      continue;
    int dir = buy ? 1 : -1;
    const Breakout *overlapped = nullptr;
    for (const Breakout &b : breakouts)
      if (b.third && b.center == f.center && b.direction == dir && b.retestPivot == static_cast<int>(at) + 2)
      {
        overlapped = &b;
        break;
      }
    Signal s;
    s.type = buy ? SignalType::Buy2 : SignalType::Sell2;
    s.pivot = static_cast<int>(at) + 2;
    s.index = second.index;
    s.center = CenterAt(centers, second.index);
    s.priority = kPrioritySecond;
    s.stop = buy ? second.low : second.high;
    s.divergence = overlapped ? overlapped->divergence : SecondDivergence(p, at, dir);
    out.push_back(s);
  }
  // 三类（第20课）：首次离开后回试不回 [ZD,ZG]
  for (const Breakout &b : breakouts)
  {
    if (!b.third || b.direction == 0) continue;
    const Center &c = centers[static_cast<std::size_t>(b.center)];
    Signal s;
    s.type = b.direction > 0 ? SignalType::Buy3 : SignalType::Sell3;
    s.pivot = b.retestPivot;
    s.index = p[static_cast<std::size_t>(b.retestPivot)].index;
    s.center = b.center;
    s.priority = kPriorityThird;
    s.stop = b.direction > 0 ? c.zg : c.zd;
    s.divergence = b.divergence;
    out.push_back(s);
  }
  out.insert(out.end(), firsts.begin(), firsts.end());
  return out;
}

}  // namespace chan
