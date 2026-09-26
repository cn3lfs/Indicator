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

// 收笔点细化：在 (prev, next) 且距 prev 不超过 2×最小跨度的窗口内，取仍能与两侧成笔的最极端同型分型。
// 只读取下标早于 next 的分型，故对分型前缀是因果的。
Fractal RefineOne(const Fractal &prev, const Fractal &cur, const Fractal &next, const std::vector<Fractal> &fractals,
                  const Config &c)
{
  int minSpan = c.stroke == StrokeRule::Strict ? 4 : 3;
  Fractal best = cur;
  int maxMerged = prev.merged + 2 * minSpan;
  auto it = std::upper_bound(fractals.begin(), fractals.end(), prev.index,
                             [](int idx, const Fractal &f) { return idx < f.index; });
  for (; it != fractals.end() && it->index < next.index; ++it)
  {
    const Fractal &f = *it;
    if (f.merged > maxMerged) break;
    if (f.kind != cur.kind) continue;
    if (!ValidStroke(prev, f, c) || !ValidStroke(f, next, c)) continue;
    if (MoreExtreme(best, f)) best = f;
  }
  return best;
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
  FeatureSequence(const std::vector<Pivot> &p, std::size_t start, std::size_t limit, Horizon &h)
    : p_(p), start_(start), size_(std::min(limit, p.size())), h_(h) {}

  // 使 seq[k] 定型（其后已有非包含新元素或元素耗尽）；返回 seq[k] 是否存在
  bool Settle(std::size_t k)
  {
    while (seq.size() <= k + 1)
    {
      Element cur;
      if (!MakeElement(p_, start_, next_, size_, cur))
      {
        if (size_ == p_.size()) h_.Bound();  // 因端点耗尽而止：结果依赖数据尽头
        break;
      }
      h_.Read(cur.outer);
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
  Horizon &h_;
  int direction_ = 0;
};

// 以向上笔开始的线段只考察特征序列顶分型，向下只考察底分型（第67课）
bool FeatureFractal(const Element &l, const Element &m, const Element &r, int dir)
{
  if (dir > 0) return m.high > l.high && m.high > r.high && m.low > l.low && m.low > r.low;
  return m.low < l.low && m.low < r.low && m.high < l.high && m.high < r.high;
}

bool AnyFeatureFractal(const std::vector<Pivot> &p, std::size_t start, int dir, std::size_t limit, Horizon &h)
{
  FeatureSequence fs(p, start, limit, h);
  for (std::size_t i = 1; fs.Settle(i + 1); i++)
  {
    if (FeatureFractal(fs.seq[i - 1], fs.seq[i], fs.seq[i + 1], dir)) return true;
  }
  return false;
}

// 线段终点（第67课）：无缺口在分型极值结束；有缺口须反向特征序列分型确认，且须在原线段
// 再创新极值之前出现（第67/71课；与极值持平不算突破）
int FindSegmentEnd(const std::vector<Pivot> &p, std::size_t start, int dir, Horizon &h)
{
  FeatureSequence fs(p, start, p.size(), h);
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
    // 视界：反向分型若在 r 处找到，结论只依赖到 r 为止的数据（r 之后的新极值不会早于该分型）；
    // 未找到时依赖到 limit（再创新极值处）为止，扫描到数据尽头则无界
    Horizon reverse;
    if (AnyFeatureFractal(p, end, -dir, limit, reverse))
    {
      h.Read(reverse.max);
      return static_cast<int>(end);
    }
    if (limit < p.size()) h.Read(limit); else h.Bound();
    h.Read(reverse.max);
    if (reverse.unbounded) h.Bound();
  }
  h.Bound();
  return -1;
}

// 线段前提：前三笔有重叠（第65课）
bool FirstThreeOverlap(const std::vector<Pivot> &p, std::size_t start, Horizon &h)
{
  if (start + 3 >= p.size())
  {
    h.Bound();
    return false;
  }
  h.Read(start + 3);
  Range a = Between(p[start], p[start + 1]);
  Range b = Between(p[start + 1], p[start + 2]);
  Range c = Between(p[start + 2], p[start + 3]);
  return std::max({a.low, b.low, c.low}) <= std::min({a.high, b.high, c.high});
}

// 起点在 (start, limit) 内被更极端的同型端点突破 → 前线段延续（第71课）；返回最极端者
std::size_t StartBreak(const std::vector<Pivot> &p, std::size_t start, std::size_t limit, Horizon &h)
{
  std::size_t best = start;
  if (limit > p.size()) h.Bound();
  for (std::size_t i = start + 2; i < limit && i < p.size(); i += 2)
  {
    h.Read(i);
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

int StrokeStream::Add(std::size_t k)
{
  const Fractal &f = (*fractals_)[k];
  std::size_t changed;
  if (raw_.empty())
  {
    raw_.push_back(f);
    ends_.push_back(f);
    return 0;
  }
  const Fractal &last = raw_.back();
  if (f.kind == last.kind)
  {
    // 同型：严格收笔取更极端者延伸端点（中继）
    if (!(config_.strokeEnd == StrokeEnd::Extreme && MoreExtreme(last, f))) return -1;
    raw_.back() = f;
    ends_.back() = f;
  }
  else if (ValidStroke(last, f, config_))
  {
    raw_.push_back(f);  // 新端点；不达标的反向分型忽略，不弹出已成笔端点（第65课）
    ends_.push_back(f);
  }
  else
  {
    return -1;
  }
  // 末端点永不细化；其前一端点的“下一端点”变了，须按已定型的前前端点重新细化
  changed = raw_.size() - 1;
  if (config_.strokeEnd == StrokeEnd::Extreme && raw_.size() >= 3)
  {
    std::size_t i = raw_.size() - 2;
    Fractal refined = RefineOne(ends_[i - 1], raw_[i], raw_[i + 1], *fractals_, config_);
    if (refined.index != ends_[i].index || refined.kind != ends_[i].kind) changed = i;
    ends_[i] = refined;
  }
  return static_cast<int>(changed);
}

std::vector<Fractal> BuildStrokeEnds(const std::vector<Fractal> &fractals, const Config &c)
{
  StrokeStream s(fractals, c);
  for (std::size_t k = 0; k < fractals.size(); k++) s.Add(k);
  return s.Ends();
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

// 线段流：每划出一段前记录检查点（状态 + 至此的读取视界）。输入从 dirty 起变化时，
// 从视界早于 dirty 的最后检查点续算；批量划分即空状态下 Update(strokes, 0)。
int SegmentStream::Update(const std::vector<Pivot> &s, std::size_t dirty)
{
  while (!checkpoints_.empty() && !checkpoints_.back().horizon.Before(dirty)) checkpoints_.pop_back();
  // 续算只改写检查点处的末元素及其后；只保留并比较这段尾部
  std::size_t keep = checkpoints_.empty() ? 0 : (checkpoints_.back().outSize > 0 ? checkpoints_.back().outSize - 1 : 0);
  std::vector<Pivot> oldTail(out_.begin() + static_cast<std::ptrdiff_t>(std::min(keep, out_.size())), out_.end());
  if (checkpoints_.empty())
  {
    out_.clear();
    horizon_ = Horizon();
    if (method_ == SegmentMethod::Feature) RunFeature(s, true); else RunHeuristic(s, true);
  }
  else
  {
    Checkpoint cp = checkpoints_.back();
    checkpoints_.pop_back();  // 续算会重新记录它
    out_.resize(cp.outSize);
    if (!out_.empty()) out_.back() = cp.outBack;
    horizon_ = cp.horizon;
    start_ = cp.start;
    i_ = cp.i;
    has_ = cp.has;
    candidate_ = cp.candidate;
    protect_ = cp.protect;
    if (method_ == SegmentMethod::Feature) RunFeature(s, false); else RunHeuristic(s, false);
  }
  std::size_t k = 0;
  while (k < oldTail.size() && keep + k < out_.size() && oldTail[k].index == out_[keep + k].index &&
         oldTail[k].kind == out_[keep + k].kind)
    k++;
  return (k == oldTail.size() && keep + k == out_.size()) ? -1 : static_cast<int>(keep + k);
}

void SegmentStream::Save()
{
  Checkpoint cp;
  cp.start = start_;
  cp.i = i_;
  cp.has = has_;
  cp.candidate = candidate_;
  cp.protect = protect_;
  cp.outSize = out_.size();
  if (!out_.empty()) cp.outBack = out_.back();
  cp.horizon = horizon_;
  checkpoints_.push_back(cp);
}

// 特征序列法（第65/67/71课）：前三笔有重叠的起点开始，依次定位线段终点；新段确立前起点被破则顺延
void SegmentStream::RunFeature(const std::vector<Pivot> &s, bool fresh)
{
  if (fresh)
  {
    if (s.size() < 4)
    {
      horizon_.Bound();
      return;
    }
    start_ = 0;
    while (start_ + 3 < s.size() && !FirstThreeOverlap(s, start_, horizon_)) start_++;
    if (start_ + 3 >= s.size())
    {
      horizon_.Bound();
      return;
    }
    out_.push_back(s[start_]);
  }
  while (start_ + 3 < s.size())
  {
    Save();
    int dir = s[start_].kind == Kind::Bottom ? 1 : -1;
    int end = FindSegmentEnd(s, start_, dir, horizon_);
    std::size_t limit = end < 0 ? s.size() : static_cast<std::size_t>(end) + 1;
    std::size_t extreme = StartBreak(s, start_, limit, horizon_);
    if (extreme != start_)
    {
      out_.back() = s[extreme];
      start_ = extreme;
      continue;
    }
    if (end < 0 || static_cast<std::size_t>(end) <= start_ || !FirstThreeOverlap(s, start_, horizon_)) return;
    if (out_.back().index != s[static_cast<std::size_t>(end)].index) out_.push_back(s[static_cast<std::size_t>(end)]);
    start_ = static_cast<std::size_t>(end);
  }
}

// 保护点启发式：至少三笔后出现反向候选点，其后被保护点反向突破即确认转折
void SegmentStream::RunHeuristic(const std::vector<Pivot> &s, bool fresh)
{
  if (fresh)
  {
    if (s.size() < 4)
    {
      horizon_.Bound();
      return;
    }
    out_.push_back(s[0]);
    start_ = 0;
    i_ = 1;
    has_ = false;
    candidate_ = protect_ = 0;
    Save();
  }
  while (i_ < s.size())
  {
    horizon_.Read(i_);
    int dir = s[start_].kind == Kind::Bottom ? 1 : -1;
    // 第71课：新段确立前先破了起点（向上段出现更低的底 / 向下段出现更高的顶），起点不是分界点，
    // 前一段延续 → 起点顺延到该端点后重新考察（与极值持平不算突破）
    if (s[i_].kind == s[start_].kind && MoreExtremePivot(s[start_], s[i_]) && s[i_].Price() != s[start_].Price())
    {
      out_.back() = s[i_];
      start_ = i_;
      has_ = false;
      i_ = start_ + 1;
      continue;
    }
    if (!has_)
    {
      if (i_ - start_ >= 3 && s[i_].kind != s[start_].kind)
      {
        candidate_ = i_;
        protect_ = i_ - 1;
        has_ = true;
      }
      i_++;
      continue;
    }
    if (s[i_].kind == s[candidate_].kind && MoreExtremePivot(s[candidate_], s[i_]))
    {
      candidate_ = i_;
      protect_ = i_ - 1;
      i_++;
      continue;
    }
    if (BrokenByProtect(dir, s[protect_], s[i_]))
    {
      if (out_.back().index != s[candidate_].index) out_.push_back(s[candidate_]);
      start_ = candidate_;
      has_ = false;
      i_ = start_ + 1;
      Save();
      continue;
    }
    i_++;
  }
  horizon_.Bound();
  if (has_ && out_.back().index != s[candidate_].index) out_.push_back(s[candidate_]);
}

std::vector<Pivot> SegmentPivotsHeuristic(const std::vector<Pivot> &s)
{
  SegmentStream st(SegmentMethod::Heuristic);
  st.Update(s, 0);
  return st.Pivots();
}

std::vector<Pivot> SegmentPivotsFeature(const std::vector<Pivot> &s)
{
  SegmentStream st(SegmentMethod::Feature);
  st.Update(s, 0);
  return st.Pivots();
}

std::vector<Pivot> BuildPivots(const std::vector<Fractal> &fractals, const Config &c)
{
  std::vector<Pivot> strokes = StrokePivots(BuildStrokeEnds(fractals, c));
  if (c.unit == CenterUnit::Stroke) return strokes;
  return c.segment == SegmentMethod::Feature ? SegmentPivotsFeature(strokes) : SegmentPivotsHeuristic(strokes);
}

}  // namespace chan
