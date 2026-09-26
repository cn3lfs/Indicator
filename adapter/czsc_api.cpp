// 结构化快照导出：把 chan::Analyze 的结果整理为 czsc_api.h 约定的 POD 表。无全局可变状态（错误串为线程局部）。
#include "adapter/czsc_api.h"

#include "core/engine.h"
#include "core/structure.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

thread_local std::string g_error;

const uint32_t kMagic = 0x43535A43u;        // "CZSC"
const uint32_t kNestedMagic = 0x4E535A43u;  // "CZSN"
const int32_t kMaxBars = 16777216;

struct Snapshot
{
  uint32_t magic = kMagic;
  int32_t n = 0;
  uint64_t fingerprint = 0;  // 输入 H/L/C/V 指纹，区间套据此确认两快照同一数据
  std::vector<czsc_pivot> pivots;
  std::vector<czsc_center> centers;
  std::vector<czsc_movement> movements;
  std::vector<czsc_breakout> breakouts;
  std::vector<czsc_signal> signals;
  std::vector<czsc_event> events;
  std::vector<czsc_bar> bars;
};

struct Nested
{
  uint32_t magic = kNestedMagic;
  std::vector<czsc_nested> rows;
};

uint64_t Fingerprint(const czsc_input *in)
{
  uint64_t h = 1469598103934665603ULL;
  auto mix = [&](const float *p) {
    const unsigned char *b = reinterpret_cast<const unsigned char *>(p);
    for (std::size_t i = 0; i < static_cast<std::size_t>(in->n) * sizeof(float); i++)
    {
      h ^= b[i];
      h *= 1099511628211ULL;
    }
  };
  if (in->n > 0)
  {
    mix(in->high);
    mix(in->low);
    mix(in->close);
    mix(in->volume);
  }
  return h;
}

void *Fail(const std::string &message)
{
  g_error = message;
  return nullptr;
}

template <class T>
T Row()
{
  T row;
  std::memset(&row, 0, sizeof row);  // 结构体无填充，逐字节确定
  row.size = sizeof(T);
  return row;
}

int32_t SignalCode(chan::SignalType t)
{
  int v = static_cast<int>(t);
  return v < 10 ? v : -(v - 10);
}

// 构建期的表查找：按K线身份把核心对象（可能来自更早时刻）解析到最终表下标
class Resolver
{
public:
  Resolver(const chan::Analysis &a) : a_(a) {}

  int32_t Pivot(int bar) const
  {
    const std::vector<chan::Pivot> &p = a_.snapshot.pivots;
    auto it = std::lower_bound(p.begin(), p.end(), bar, [](const chan::Pivot &x, int b) { return x.index < b; });
    return (it != p.end() && it->index == bar) ? static_cast<int32_t>(it - p.begin()) : -1;
  }

  int32_t Center(int start) const
  {
    const std::vector<chan::Center> &c = a_.snapshot.centers;
    auto it = std::lower_bound(c.begin(), c.end(), start, [](const chan::Center &x, int s) { return x.start < s; });
    return (it != c.end() && it->start == start) ? static_cast<int32_t>(it - c.begin()) : -1;
  }

  int32_t MovementOf(int32_t center) const
  {
    if (center < 0) return -1;
    const std::vector<chan::Movement> &m = a_.snapshot.movements;
    for (std::size_t i = 0; i < m.size(); i++)
      if (m[i].firstCenter <= center && center <= m[i].lastCenter) return static_cast<int32_t>(i);
    return -1;
  }

  int32_t BreakoutOf(int32_t center) const
  {
    const std::vector<chan::Breakout> &b = a_.snapshot.breakouts;
    for (std::size_t i = 0; i < b.size(); i++)
      if (b[i].center == center) return static_cast<int32_t>(i);
    return -1;
  }

  czsc_divergence Divergence(const chan::Divergence &d, int semantic) const
  {
    czsc_divergence r = Row<czsc_divergence>();
    r.prevStart = d.previousStartIndex >= 0 ? Pivot(d.previousStartIndex) : -1;
    r.prevEnd = d.previousEndIndex >= 0 ? Pivot(d.previousEndIndex) : -1;
    r.curStart = d.currentStartIndex >= 0 ? Pivot(d.currentStartIndex) : -1;
    r.curEnd = d.currentEndIndex >= 0 ? Pivot(d.currentEndIndex) : -1;
    r.prevSpace = d.previous.space;
    r.prevSpeed = d.previous.speed;
    r.prevArea = d.previous.area;
    r.curSpace = d.current.space;
    r.curSpeed = d.current.speed;
    r.curArea = d.current.area;
    r.newExtreme = d.newExtreme;
    r.weakSpace = d.weakSpace;
    r.weakSpeed = d.weakSpeed;
    r.weakArea = d.weakArea;
    r.holds = d.holds;
    r.semantic = d.holds ? semantic : 0;
    return r;
  }

  czsc_signal Signal(const chan::Signal &s) const
  {
    czsc_signal r = Row<czsc_signal>();
    r.index = s.index;
    r.pivot = Pivot(s.index);
    r.type = SignalCode(s.type);
    r.center = s.centerStart >= 0 ? Center(s.centerStart) : -1;
    r.movement = MovementOf(r.center);
    bool third = s.type == chan::SignalType::Buy3 || s.type == chan::SignalType::Sell3;
    bool first = s.type == chan::SignalType::Buy1 || s.type == chan::SignalType::Sell1;
    r.breakout = third && r.center >= 0 ? BreakoutOf(r.center) : -1;
    r.basedOn = -1;  // 全部行建好后再解析
    r.stop = s.stop;
    r.confirmedAt = -1;
    r.revokedAt = -1;
    r.divergence = Divergence(s.divergence, first ? 1 : 2);
    r.quality = s.quality;
    r.context = s.context;
    auto pivotOrNone = [&](int bar) { return bar >= 0 ? Pivot(bar) : -1; };
    r.secondBasePivot = pivotOrNone(s.secondBaseIndex);
    r.secondTurnPivot = pivotOrNone(s.secondTurnIndex);
    r.smallTurnBasePivot = pivotOrNone(s.smallTurnBaseIndex);
    r.smallTurnLeavePivot = pivotOrNone(s.smallTurnLeaveIndex);
    r.smallTurnRetestPivot = pivotOrNone(s.smallTurnRetestIndex);
    return r;
  }

private:
  const chan::Analysis &a_;
};

using Key = std::pair<int, int>;  // (信号K线, 信号码)
Key KeyOf(const chan::Signal &s) { return {s.index, static_cast<int>(s.type)}; }

// 信号表：事后全量行在前；当下事件还原的每段生命冻结确认时内容，
// 末尾仍有效的生命并入同键的事后胜出行，其余生命追加在后
void BuildSignals(const chan::Analysis &a, const Resolver &r, bool withEvents, Snapshot &out)
{
  const std::vector<chan::Signal> &hs = a.snapshot.signals;
  std::map<Key, std::size_t> winnerRow;  // 同键首个事后行即当下的胜出者（同键同类，按来源序）
  std::vector<int> basedOnIndex;
  for (const chan::Signal &s : hs)
  {
    czsc_signal row = r.Signal(s);
    row.hindsight = 1;
    winnerRow.emplace(KeyOf(s), out.signals.size());
    out.signals.push_back(row);
    basedOnIndex.push_back(s.basedOnIndex);
  }

  struct Life
  {
    chan::Signal signal;
    int appear, revoke;
  };
  std::vector<Life> lives;
  std::map<Key, std::size_t> open;
  std::vector<std::pair<int, std::size_t>> eventLife;  // (op, life)
  for (const chan::SignalEvent &e : a.events)
  {
    Key k = KeyOf(e.signal);
    if (!e.revoked)
    {
      open[k] = lives.size();
      eventLife.push_back({+1, lives.size()});
      lives.push_back({e.signal, e.bar, -1});
    }
    else
    {
      auto it = open.find(k);
      if (it == open.end()) continue;  // 不应发生：撤销必有先前的出现
      lives[it->second].revoke = e.bar;
      eventLife.push_back({-1, it->second});
      open.erase(it);
    }
  }

  std::vector<std::size_t> lifeRow(lives.size());
  for (std::size_t i = 0; i < lives.size(); i++)
  {
    const Life &l = lives[i];
    czsc_signal row = r.Signal(l.signal);
    row.confirmedAt = l.appear;
    row.revokedAt = l.revoke;
    auto w = winnerRow.find(KeyOf(l.signal));
    if (l.revoke < 0 && w != winnerRow.end())
    {
      row.hindsight = 1;
      out.signals[w->second] = row;  // 当下有效的生命即该事后行：内容取确认时所知
      lifeRow[i] = w->second;
      basedOnIndex[w->second] = l.signal.basedOnIndex;
    }
    else
    {
      lifeRow[i] = out.signals.size();
      out.signals.push_back(row);
      basedOnIndex.push_back(l.signal.basedOnIndex);
    }
  }

  // 二类所依一类：同K线的一类行中，取确认不晚于本行确认时刻的最近一段生命；未确认行取事后行
  for (std::size_t i = 0; i < out.signals.size(); i++)
  {
    czsc_signal &row = out.signals[i];
    if (row.type != 2 && row.type != -2) continue;
    int want = row.type > 0 ? 1 : -1;
    int32_t best = -1;
    for (std::size_t j = 0; j < out.signals.size(); j++)
    {
      const czsc_signal &f = out.signals[j];
      if (f.type != want || f.index != basedOnIndex[i]) continue;
      if (row.confirmedAt < 0)
      {
        if (f.hindsight) best = static_cast<int32_t>(j);
        continue;
      }
      if (f.confirmedAt >= 0 && f.confirmedAt <= row.confirmedAt &&
          (best < 0 || f.confirmedAt >= out.signals[static_cast<std::size_t>(best)].confirmedAt))
        best = static_cast<int32_t>(j);
    }
    row.basedOn = best;
  }

  if (!withEvents) return;
  for (std::size_t i = 0; i < eventLife.size(); i++)
  {
    czsc_event ev = Row<czsc_event>();
    const Life &l = lives[eventLife[i].second];
    ev.op = eventLife[i].first;
    ev.bar = ev.op > 0 ? l.appear : l.revoke;
    ev.signal = static_cast<int32_t>(lifeRow[eventLife[i].second]);
    out.events.push_back(ev);
  }
}

std::string CheckInput(const czsc_input *in, chan::Config &config)
{
  if (in == nullptr) return "input 为 NULL";
  if (in->size < sizeof(czsc_input)) return "input->size 小于 sizeof(czsc_input)，请按当前头文件构造";
  if (in->n < 0 || in->n > kMaxBars) return "n 须在 0..16777216";
  std::optional<chan::Config> c = chan::Config::Decode(in->config);
  if (!c) return "非法配置码 " + std::to_string(in->config) + "：个位 0..2，其余位 0..1，最大 1112";
  config = *c;
  if (in->flags & ~static_cast<int32_t>(CZSC_FLAG_EVENTS | CZSC_FLAG_HIGHER)) return "flags 含未定义的位";
  if (in->flags & CZSC_FLAG_HIGHER) return "CZSC_FLAG_HIGHER（高级别/递归结构，P2）尚未实现";
  if (in->n == 0) return "";
  if (!in->high || !in->low || !in->close || !in->volume) return "high/low/close/volume 均须非空";
  for (int32_t i = 0; i < in->n; i++)
  {
    float h = in->high[i], l = in->low[i], c = in->close[i], v = in->volume[i];
    std::string at = "第 " + std::to_string(i) + " 根";
    if (!std::isfinite(h) || !std::isfinite(l) || !std::isfinite(c) || !std::isfinite(v)) return at + "含 NaN/Inf";
    if (h < l) return at + "最高价低于最低价";
    if (c < l || c > h) return at + "收盘价不在 [最低价, 最高价] 内";
    if (v < 0) return at + "成交量为负";
  }
  return "";
}

Snapshot *Build(const czsc_input *in)
{
  chan::Config config;
  std::string error = CheckInput(in, config);
  if (!error.empty())
  {
    Fail(error);
    return nullptr;
  }
  chan::Series s;
  std::size_t n = static_cast<std::size_t>(in->n);
  if (n > 0)
  {
    s.high.assign(in->high, in->high + n);
    s.low.assign(in->low, in->low + n);
    s.close.assign(in->close, in->close + n);
    s.volume.assign(in->volume, in->volume + n);
  }
  chan::Analysis a = chan::Analyze(s, config);
  Resolver r(a);
  Snapshot *out = new Snapshot();
  out->n = in->n;
  out->fingerprint = Fingerprint(in);

  for (std::size_t i = 0; i < a.snapshot.pivots.size(); i++)
  {
    const chan::Pivot &p = a.snapshot.pivots[i];
    czsc_pivot row = Row<czsc_pivot>();
    row.index = p.index;
    row.kind = static_cast<int32_t>(p.kind);
    row.price = p.Price();
    row.fractalAt = p.fractalAt;
    row.confirmedAt = a.pivotFinalAt[i];
    out->pivots.push_back(row);
  }
  for (std::size_t i = 0; i < a.snapshot.centers.size(); i++)
  {
    const chan::Center &c = a.snapshot.centers[i];
    czsc_center row = Row<czsc_center>();
    row.start = c.start;
    row.end = c.end;
    row.firstPivot = c.firstPivot;
    row.lastPivot = c.lastPivot;
    row.zg = c.zg;
    row.zd = c.zd;
    row.gg = c.gg;
    row.dd = c.dd;
    row.direction = c.direction;
    row.confirmedAt = a.centerFinalAt[i];
    row.relationToPrev = i == 0 ? 0 : static_cast<int32_t>(chan::Relate(a.snapshot.centers[i - 1], c));
    row.lifecycle = -1;
    if (i > 0)
    {
      const chan::Center &prev = a.snapshot.centers[i - 1];
      // 第18/20课：[ZD,ZG] 重叠为同级延伸；否则按中心定理二，GG/DD 重叠为扩展，不重叠为同向新生
      if (prev.zd <= c.zg && c.zd <= prev.zg) row.lifecycle = 0;
      else if (row.relationToPrev == 2) row.lifecycle = 1;
      else row.lifecycle = row.relationToPrev == 1 ? 2 : 3;
    }
    out->centers.push_back(row);
  }
  for (std::size_t i = 0; i < a.snapshot.movements.size(); i++)
  {
    const chan::Movement &m = a.snapshot.movements[i];
    czsc_movement row = Row<czsc_movement>();
    row.type = static_cast<int32_t>(m.type);
    row.firstCenter = m.firstCenter;
    row.lastCenter = m.lastCenter;
    row.start = m.start;
    row.end = m.end;
    row.confirmedAt = a.movementFinalAt[i];
    out->movements.push_back(row);
  }
  for (const chan::Breakout &b : a.snapshot.breakouts)
  {
    czsc_breakout row = Row<czsc_breakout>();
    row.center = b.center;
    row.direction = b.direction;
    row.leavePivot = b.leavePivot;
    row.retestPivot = b.retestPivot;
    row.third = b.third;
    row.confirmedAt = a.breakoutFinalAt[static_cast<std::size_t>(b.center)];
    row.divergence = r.Divergence(b.divergence, 2);
    out->breakouts.push_back(row);
  }
  BuildSignals(a, r, (in->flags & CZSC_FLAG_EVENTS) != 0, *out);

  std::vector<int8_t> gaps = chan::Gaps(s);
  std::vector<int8_t> strengths = chan::FractalStrengths(s, a.bars, a.fractals);
  for (std::size_t i = 0; i < n; i++)
  {
    czsc_bar row = Row<czsc_bar>();
    row.dif = a.energy.dif[i];
    row.dea = a.energy.dea[i];
    row.macd = (a.energy.dif[i] - a.energy.dea[i]) * 2.0f;
    row.kiss = static_cast<int32_t>(a.ma.kisses[i]);
    row.gap = gaps[i];
    row.fractalStrength = strengths[i];
    row.instantDivergence = a.instantWarning[i];
    out->bars.push_back(row);
  }
  return out;
}

Snapshot *Handle(void *h)
{
  Snapshot *s = static_cast<Snapshot *>(h);
  if (s == nullptr || s->magic != kMagic)
  {
    g_error = "snapshot 句柄无效";
    return nullptr;
  }
  return s;
}

// 区间套（第27/61课）：每个低级别一类信号，在高级别快照中找同向且背驰段 c 包含其K线的一类信号；
// 同时给出包含它的高级别段；段方向一致而无高级别背驰时标小转大候选（第43课）
Nested *BuildNested(const Snapshot &low, const Snapshot &high)
{
  Nested *out = new Nested();
  const std::vector<czsc_pivot> &hp = high.pivots;
  for (std::size_t i = 0; i < low.signals.size(); i++)
  {
    const czsc_signal &l = low.signals[i];
    if (l.type != 1 && l.type != -1) continue;
    czsc_nested row = Row<czsc_nested>();
    row.lowSignal = static_cast<int32_t>(i);
    row.highSignal = -1;
    for (std::size_t j = 0; j < high.signals.size(); j++)
    {
      const czsc_signal &h = high.signals[j];
      if (h.type != l.type || h.divergence.curStart < 0) continue;
      int cStart = hp[static_cast<std::size_t>(h.divergence.curStart)].index;
      if (cStart > l.index || l.index > h.index) continue;
      const czsc_signal *best = row.highSignal >= 0 ? &high.signals[static_cast<std::size_t>(row.highSignal)] : nullptr;
      if (!best || h.index < best->index || (h.index == best->index && h.confirmedAt >= 0 && best->confirmedAt < 0))
        row.highSignal = static_cast<int32_t>(j);  // 取包含它的最近（最早结束）的高级别背驰段
    }
    auto it = std::upper_bound(hp.begin(), hp.end(), l.index, [](int b, const czsc_pivot &p) { return b < p.index; });
    row.highSegmentStart = it == hp.begin() ? -1 : static_cast<int32_t>(it - hp.begin()) - 1;
    row.highSegmentEnd = it == hp.end() ? -1 : static_cast<int32_t>(it - hp.begin());
    row.insideHighSegment = row.highSignal >= 0;
    const czsc_signal *h = row.highSignal >= 0 ? &high.signals[static_cast<std::size_t>(row.highSignal)] : nullptr;
    row.confirmed = h && l.confirmedAt >= 0 && h->confirmedAt >= 0;
    row.newExtreme = l.divergence.newExtreme;
    // 一买（l.type=1）落在高级别向下段（起点为顶）中，一卖落在向上段中，方向一致
    bool sameDirection = row.highSegmentStart >= 0 &&
                         hp[static_cast<std::size_t>(row.highSegmentStart)].kind == (l.type > 0 ? 1 : -1);
    row.smallTurn = !row.insideHighSegment && sameDirection;
    out->rows.push_back(row);
  }
  return out;
}

template <class T>
const T *Table(void *h, int32_t *count, std::vector<T> Snapshot::*member)
{
  if (count) *count = 0;
  Snapshot *s = Handle(h);
  if (!s) return nullptr;
  const std::vector<T> &v = s->*member;
  if (count) *count = static_cast<int32_t>(v.size());
  return v.data();
}

}  // namespace

extern "C" {

int32_t czsc_api_version(void) { return CZSC_API_VERSION; }

const char *czsc_last_error(void) { return g_error.c_str(); }

void *czsc_snapshot_build(const czsc_input *input)
{
  g_error.clear();
  try
  {
    return Build(input);
  }
  catch (const std::bad_alloc &)
  {
    return Fail("内存不足");
  }
  catch (...)
  {
    return Fail("内部错误");
  }
}

void czsc_snapshot_free(void *snapshot)
{
  if (snapshot == nullptr) return;
  uint32_t magic = *static_cast<uint32_t *>(snapshot);
  if (magic == kMagic)
  {
    Snapshot *s = static_cast<Snapshot *>(snapshot);
    s->magic = 0;
    delete s;
  }
  else if (magic == kNestedMagic)
  {
    Nested *s = static_cast<Nested *>(snapshot);
    s->magic = 0;
    delete s;
  }
}

void *czsc_nested_build(void *low, void *high)
{
  g_error.clear();
  Snapshot *l = Handle(low), *h = Handle(high);
  if (!l || !h) return Fail("low/high 须为 czsc_snapshot_build 返回的有效句柄");
  if (l->n != h->n || l->fingerprint != h->fingerprint) return Fail("low 与 high 不是同一输入数据构建的快照");
  try
  {
    return BuildNested(*l, *h);
  }
  catch (...)
  {
    return Fail("内存不足或内部错误");
  }
}

const czsc_nested *czsc_nested_rows(void *nested, int32_t *count)
{
  if (count) *count = 0;
  Nested *s = static_cast<Nested *>(nested);
  if (s == nullptr || s->magic != kNestedMagic)
  {
    g_error = "nested 句柄无效";
    return nullptr;
  }
  if (count) *count = static_cast<int32_t>(s->rows.size());
  return s->rows.data();
}

const czsc_pivot *czsc_pivots(void *h, int32_t *count) { return Table(h, count, &Snapshot::pivots); }
const czsc_center *czsc_centers(void *h, int32_t *count) { return Table(h, count, &Snapshot::centers); }
const czsc_movement *czsc_movements(void *h, int32_t *count) { return Table(h, count, &Snapshot::movements); }
const czsc_breakout *czsc_breakouts(void *h, int32_t *count) { return Table(h, count, &Snapshot::breakouts); }
const czsc_signal *czsc_signals(void *h, int32_t *count) { return Table(h, count, &Snapshot::signals); }
const czsc_event *czsc_events(void *h, int32_t *count) { return Table(h, count, &Snapshot::events); }
const czsc_bar *czsc_bars(void *h, int32_t *count) { return Table(h, count, &Snapshot::bars); }

}  // extern "C"
