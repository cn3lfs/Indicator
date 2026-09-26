#include "core/morphology.h"

#include <algorithm>
#include <cmath>

namespace chan
{
namespace
{

//----------------------------------------------------------------------------
// 包含处理（第62/65课）
//----------------------------------------------------------------------------

bool Included(float lh, float ll, float rh, float rl)
{
  return (rh <= lh && rl >= ll) || (rh >= lh && rl <= ll);
}

int Direction(float lh, float ll, float rh, float rl)
{
  if (rh > lh && rl > ll) return 1;
  if (rh < lh && rl < ll) return -1;
  return 0;
}

// 序列开头尚无方向时：按高低点差值较大一侧定向，等值向上（原文未定义，见 docs/chan-ambiguity-decisions.md）
int ChooseDirection(float lh, float ll, float rh, float rl, int direction)
{
  if (direction != 0) return direction;
  return std::fabs(rh - lh) >= std::fabs(rl - ll) ? 1 : -1;
}

//----------------------------------------------------------------------------
// 笔（第62/65课）
//----------------------------------------------------------------------------

bool MoreExtreme(const Fractal &base, const Fractal &f)
{
  return base.kind == Kind::Top ? f.high >= base.high : f.low <= base.low;
}

// 跨度：严格笔合并K线差 ≥4；新笔 ≥3 且原始K线差 ≥4；czsc 笔 ≥3（fx_a 首根到 fx_b 末根 ≥6 根）
bool SpanEnough(const Fractal &a, const Fractal &b, const Config &c)
{
  int merged = b.merged - a.merged;
  switch (c.stroke)
  {
    case StrokeRule::New: return merged >= 3 && (b.index - a.index) >= 4;
    case StrokeRule::Czsc: return merged >= 3;
    default: return merged >= 4;
  }
}

// 价位推进（第62课：底分型+上升K线+顶分型）
bool PriceProgress(const Fractal &start, const Fractal &end)
{
  return start.kind == Kind::Bottom ? end.high > start.high : end.low < start.low;
}

// czsc ab_include：两端分型K线区间一方包含另一方则不成笔（仅 czsc 笔）
bool Nested(const Fractal &a, const Fractal &b, const Config &c)
{
  if (c.stroke != StrokeRule::Czsc) return false;
  return (a.high > b.high && a.low < b.low) || (a.high < b.high && a.low > b.low);
}

bool ValidStroke(const Fractal &a, const Fractal &b, const Config &c)
{
  return SpanEnough(a, b, c) && PriceProgress(a, b) && !Nested(a, b, c);
}

// 收笔点细化：在 (Prev, Next) 且距 Prev 不超过 2×最小跨度的窗口内，取仍能与两侧成笔的最极端同型分型
void RefineEnds(std::vector<Fractal> &ends, const std::vector<Fractal> &fractals, const Config &c)
{
  if (c.strokeEnd != StrokeEnd::Extreme || ends.size() < 3) return;
  int minSpan = c.stroke == StrokeRule::Strict ? 4 : 3;
  for (std::size_t i = 1; i + 1 < ends.size(); i++)
  {
    const Fractal &prev = ends[i - 1];
    const Fractal &next = ends[i + 1];
    Fractal best = ends[i];
    int maxMerged = prev.merged + 2 * minSpan;
    auto it = std::upper_bound(fractals.begin(), fractals.end(), prev.index,
                               [](int idx, const Fractal &f) { return idx < f.index; });
    for (; it != fractals.end() && it->index < next.index; ++it)
    {
      const Fractal &f = *it;
      if (f.merged > maxMerged) break;
      if (f.kind != ends[i].kind) continue;
      if (!ValidStroke(prev, f, c) || !ValidStroke(f, next, c)) continue;
      if (MoreExtreme(best, f)) best = f;
    }
    ends[i] = best;
  }
}

//----------------------------------------------------------------------------
// 线段（第65/67/71课）
//----------------------------------------------------------------------------

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

bool MoreExtremePivot(const Pivot &base, const Pivot &p)
{
  return base.kind == Kind::Top ? p.high >= base.high : p.low <= base.low;
}

// 特征序列元素：以向上笔开始的线段，其向下笔为元素（反之亦然）
struct Element
{
  std::size_t inner, outer, highAt, lowAt;
  float high, low;
};

bool MakeElement(const std::vector<Pivot> &p, std::size_t start, std::size_t k, std::size_t size, Element &e)
{
  if (start + 2 + 2 * k >= size) return false;
  e.inner = start + 1 + 2 * k;
  e.outer = start + 2 + 2 * k;
  Range r = Between(p[e.inner], p[e.outer]);
  e.high = r.high;
  e.low = r.low;
  bool innerHigher = p[e.inner].Price() >= p[e.outer].Price();
  e.highAt = innerHigher ? e.inner : e.outer;
  e.lowAt = innerHigher ? e.outer : e.inner;
  return true;
}

// 标准特征序列（非包含处理，第67课）的惰性构建：只算到找到分型为止
class FeatureSequence
{
public:
  FeatureSequence(const std::vector<Pivot> &p, std::size_t start, std::size_t limit)
    : p_(p), start_(start), size_(std::min(limit, p.size())) {}

  // 使 seq[k] 定型（其后已有非包含新元素或元素耗尽）；返回 seq[k] 是否存在
  bool Settle(std::size_t k)
  {
    while (seq.size() <= k + 1)
    {
      Element cur;
      if (!MakeElement(p_, start_, next_, size_, cur)) break;
      next_++;
      if (seq.empty())
      {
        seq.push_back(cur);
        continue;
      }
      Element &last = seq.back();
      if (Included(last.high, last.low, cur.high, cur.low))
      {
        int d = ChooseDirection(last.high, last.low, cur.high, cur.low, direction_);
        if (d >= 0)
        {
          if (cur.high >= last.high) { last.high = cur.high; last.highAt = cur.highAt; }
          if (cur.low >= last.low) { last.low = cur.low; last.lowAt = cur.lowAt; }
        }
        else
        {
          if (cur.high <= last.high) { last.high = cur.high; last.highAt = cur.highAt; }
          if (cur.low <= last.low) { last.low = cur.low; last.lowAt = cur.lowAt; }
        }
        last.outer = cur.outer;
        if (direction_ == 0) direction_ = d;
        continue;
      }
      int d = Direction(last.high, last.low, cur.high, cur.low);
      if (d != 0) direction_ = d;
      seq.push_back(cur);
    }
    return seq.size() > k;
  }

  std::vector<Element> seq;

private:
  const std::vector<Pivot> &p_;
  std::size_t start_, size_, next_ = 0;
  int direction_ = 0;
};

// 以向上笔开始的线段只考察特征序列顶分型，向下只考察底分型（第67课）
bool FeatureFractal(const Element &l, const Element &m, const Element &r, int dir)
{
  if (dir > 0) return m.high > l.high && m.high > r.high && m.low > l.low && m.low > r.low;
  return m.low < l.low && m.low < r.low && m.high < l.high && m.high < r.high;
}

bool AnyFeatureFractal(const std::vector<Pivot> &p, std::size_t start, int dir, std::size_t limit)
{
  FeatureSequence fs(p, start, limit);
  for (std::size_t i = 1; fs.Settle(i + 1); i++)
  {
    if (FeatureFractal(fs.seq[i - 1], fs.seq[i], fs.seq[i + 1], dir)) return true;
  }
  return false;
}

// 线段终点（第67课）：无缺口在分型极值结束；有缺口须反向特征序列分型确认，且须在原线段
// 再创新极值之前出现（第67/71课；与极值持平不算突破）
int FindSegmentEnd(const std::vector<Pivot> &p, std::size_t start, int dir)
{
  FeatureSequence fs(p, start, p.size());
  for (std::size_t i = 1; fs.Settle(i + 1); i++)
  {
    const std::vector<Element> &s = fs.seq;
    if (!FeatureFractal(s[i - 1], s[i], s[i + 1], dir)) continue;
    std::size_t end = dir > 0 ? s[i].highAt : s[i].lowAt;
    if (Overlap(s[i - 1].low, s[i - 1].high, s[i].low, s[i].high)) return static_cast<int>(end);
    std::size_t limit = end + 2;
    while (limit < p.size() && !(MoreExtremePivot(p[end], p[limit]) && p[limit].Price() != p[end].Price()))
    {
      limit += 2;
    }
    if (AnyFeatureFractal(p, end, -dir, limit)) return static_cast<int>(end);
  }
  return -1;
}

// 线段前提：前三笔有重叠（第65课）
bool FirstThreeOverlap(const std::vector<Pivot> &p, std::size_t start)
{
  if (start + 3 >= p.size()) return false;
  Range a = Between(p[start], p[start + 1]);
  Range b = Between(p[start + 1], p[start + 2]);
  Range c = Between(p[start + 2], p[start + 3]);
  return std::max({a.low, b.low, c.low}) <= std::min({a.high, b.high, c.high});
}

// 起点在 (start, limit) 内被更极端的同型端点突破 → 前线段延续（第71课）；返回最极端者
std::size_t StartBreak(const std::vector<Pivot> &p, std::size_t start, std::size_t limit)
{
  std::size_t best = start;
  for (std::size_t i = start + 2; i < limit && i < p.size(); i += 2)
  {
    if (p[i].kind == p[best].kind && MoreExtremePivot(p[best], p[i]) && p[i].Price() != p[best].Price())
    {
      best = i;
    }
  }
  return best;
}

bool BrokenByProtect(int dir, const Pivot &protect, const Pivot &p)
{
  if (dir > 0 && p.kind == Kind::Bottom) return p.low < protect.low;
  if (dir < 0 && p.kind == Kind::Top) return p.high > protect.high;
  return false;
}

Pivot ToPivot(const Fractal &f)
{
  Pivot p;
  p.kind = f.kind;
  p.index = f.index;
  p.high = f.high;
  p.low = f.low;
  p.confirmedAt = f.confirmedAt;
  return p;
}

}  // namespace

//----------------------------------------------------------------------------

bool BarMerger::Push(int index, float high, float low)
{
  MergedBar bar{index, index, index, index, high, low};
  if (bars_.empty())
  {
    bars_.push_back(bar);
    return true;
  }
  MergedBar &last = bars_.back();
  if (Included(last.high, last.low, high, low))
  {
    // 向上取高高/低取高，向下取低低/高取低（第62课）
    int d = ChooseDirection(last.high, last.low, high, low, direction_);
    if (d >= 0)
    {
      if (high >= last.high) { last.high = high; last.highAt = index; }
      if (low >= last.low) { last.low = low; last.lowAt = index; }
    }
    else
    {
      if (high <= last.high) { last.high = high; last.highAt = index; }
      if (low <= last.low) { last.low = low; last.lowAt = index; }
    }
    last.last = index;
    if (direction_ == 0) direction_ = d;
    return false;
  }
  int d = Direction(last.high, last.low, high, low);
  if (d != 0) direction_ = d;
  bars_.push_back(bar);
  return true;
}

std::vector<MergedBar> MergeBars(const Series &s)
{
  BarMerger m;
  for (int i = 0; i < s.Size(); i++)
  {
    m.Push(i, s.high[static_cast<std::size_t>(i)], s.low[static_cast<std::size_t>(i)]);
  }
  return m.Bars();
}

std::vector<Fractal> DetectFractals(const std::vector<MergedBar> &bars)
{
  std::vector<Fractal> out;
  for (std::size_t i = 1; i + 1 < bars.size(); i++)
  {
    const MergedBar &l = bars[i - 1], &m = bars[i], &r = bars[i + 1];
    Kind kind;
    if (m.high > l.high && m.high > r.high && m.low > l.low && m.low > r.low)
      kind = Kind::Top;
    else if (m.low < l.low && m.low < r.low && m.high < l.high && m.high < r.high)
      kind = Kind::Bottom;
    else
      continue;
    Fractal f;
    f.kind = kind;
    f.index = kind == Kind::Top ? m.highAt : m.lowAt;
    f.merged = static_cast<int>(i);
    f.high = m.high;
    f.low = m.low;
    f.confirmedAt = r.first;
    // 无包含K线下顶底必交替；保留同型合并仅作防御
    if (!out.empty() && out.back().kind == kind)
    {
      if (MoreExtreme(out.back(), f)) out.back() = f;
      continue;
    }
    out.push_back(f);
  }
  return out;
}

std::vector<Fractal> BuildStrokeEnds(const std::vector<Fractal> &fractals, const Config &c)
{
  std::vector<Fractal> ends;
  if (fractals.empty()) return ends;
  ends.push_back(fractals[0]);
  for (std::size_t i = 1; i < fractals.size(); i++)
  {
    const Fractal &f = fractals[i];
    const Fractal &last = ends.back();
    if (f.kind == last.kind)
    {
      if (c.strokeEnd == StrokeEnd::Extreme && MoreExtreme(last, f)) ends.back() = f;  // 延伸（中继）
    }
    else if (ValidStroke(last, f, c))
    {
      ends.push_back(f);  // 新端点；不达标的反向分型忽略，不弹出已成笔端点（第65课）
    }
  }
  RefineEnds(ends, fractals, c);
  return ends;
}

std::vector<Pivot> StrokePivots(const std::vector<Fractal> &ends)
{
  std::vector<Pivot> out;
  for (const Fractal &f : ends)
  {
    if (out.empty() || out.back().index != f.index) out.push_back(ToPivot(f));
  }
  // 单个端点不构成笔
  if (out.size() < 2) out.clear();
  return out;
}

std::vector<Pivot> SegmentPivotsHeuristic(const std::vector<Pivot> &s)
{
  std::vector<Pivot> out;
  if (s.size() < 4) return out;
  out.push_back(s[0]);
  std::size_t start = 0, i = 1, candidate = 0, protect = 0;
  bool has = false;
  while (i < s.size())
  {
    int dir = s[start].kind == Kind::Bottom ? 1 : -1;
    if (!has)
    {
      if (i - start >= 3 && s[i].kind != s[start].kind)
      {
        candidate = i;
        protect = i - 1;
        has = true;
      }
      i++;
      continue;
    }
    if (s[i].kind == s[candidate].kind && MoreExtremePivot(s[candidate], s[i]))
    {
      candidate = i;
      protect = i - 1;
      i++;
      continue;
    }
    if (BrokenByProtect(dir, s[protect], s[i]))
    {
      if (out.back().index != s[candidate].index) out.push_back(s[candidate]);
      start = candidate;
      has = false;
      i = start + 1;
      continue;
    }
    i++;
  }
  if (has && out.back().index != s[candidate].index) out.push_back(s[candidate]);
  return out;
}

std::vector<Pivot> SegmentPivotsFeature(const std::vector<Pivot> &s)
{
  std::vector<Pivot> out;
  if (s.size() < 4) return out;
  std::size_t start = 0;
  while (start + 3 < s.size() && !FirstThreeOverlap(s, start)) start++;
  if (start + 3 >= s.size()) return out;
  out.push_back(s[start]);
  while (start + 3 < s.size())
  {
    int dir = s[start].kind == Kind::Bottom ? 1 : -1;
    int end = FindSegmentEnd(s, start, dir);
    std::size_t limit = end < 0 ? s.size() : static_cast<std::size_t>(end) + 1;
    std::size_t extreme = StartBreak(s, start, limit);
    if (extreme != start)
    {
      out.back() = s[extreme];
      start = extreme;
      continue;
    }
    if (end < 0 || static_cast<std::size_t>(end) <= start || !FirstThreeOverlap(s, start)) break;
    if (out.back().index != s[static_cast<std::size_t>(end)].index) out.push_back(s[static_cast<std::size_t>(end)]);
    start = static_cast<std::size_t>(end);
  }
  return out;
}

std::vector<Pivot> BuildPivots(const std::vector<Fractal> &fractals, const Config &c)
{
  std::vector<Pivot> strokes = StrokePivots(BuildStrokeEnds(fractals, c));
  if (c.unit == CenterUnit::Stroke) return strokes;
  return c.segment == SegmentMethod::Feature ? SegmentPivotsFeature(strokes) : SegmentPivotsHeuristic(strokes);
}

}  // namespace chan
