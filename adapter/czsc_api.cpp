// 结构化快照导出：把 chan::Analyze 的结果整理为 czsc_api.h 约定的 POD 表。无全局可变状态（错误串为线程局部）。
#include "adapter/czsc_api.h"

#include "core/engine.h"
#include "core/morphology.h"
#include "core/recursion.h"
#include "core/structure.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <array>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

// 线程局部错误串用定长数组（平凡可析构）：带析构的 thread_local 在 MinGW 静态运行时下会注册线程退出析构，
// 宿主进程（如 Node）退出时与 DLL 卸载顺序冲突而崩溃
thread_local char g_error[512];

void SetError(const std::string &message)
{
  std::size_t n = std::min(message.size(), sizeof g_error - 1);
  // 不截断在 UTF-8 多字节字符中间
  while (n > 0 && n < message.size() && (static_cast<unsigned char>(message[n]) & 0xC0) == 0x80) n--;
  std::memcpy(g_error, message.data(), n);
  g_error[n] = '\0';
}

const uint32_t kMagic = 0x43535A43u;        // "CZSC"
const int32_t kMaxBars = 16777216;

struct LevelTables
{
  std::vector<czsc_pivot> pivots;
  std::vector<czsc_center> centers;
  std::vector<czsc_movement> movements;
  std::vector<czsc_breakout> breakouts;
  std::vector<czsc_signal> signals;
  std::vector<czsc_event> events;
  std::vector<czsc_bar> bars;
  std::vector<czsc_recursive_node> nodes;
  std::vector<int32_t> children;
  std::vector<czsc_recursive_center> rcenters;
  std::vector<czsc_recursive_connection> connections;
};

struct Snapshot
{
  uint32_t magic = kMagic;
  chan::AnalysisConfig config;
  uint32_t outputs = 0;
  std::array<LevelTables,2> tables;
  std::array<std::vector<czsc_pivot>,2> projectedPivots;
  std::array<std::vector<czsc_center>,2> projectedCenters;
  std::array<std::vector<chan::Pivot>,2> sourcePivots;
  std::vector<float> high, low;
  std::vector<czsc_nested> nested;
};

void *Fail(const std::string &message)
{
  SetError(message);
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
void BuildSignals(const chan::Analysis &a, const Resolver &r, bool withEvents, LevelTables &out)
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

// 走势完成证据（第17/29课）：连接段、后继、后继成立时刻；趋势以最后中枢上的反向一类点为末端背驰证据
void CompleteMovements(const chan::Analysis &a, LevelTables &out)
{
  const std::vector<chan::Movement> &m = a.snapshot.movements;
  const std::vector<chan::Center> &c = a.snapshot.centers;
  std::vector<int> bound = chan::MovementBoundaries(a.snapshot.pivots, c, m);
  for (std::size_t i = 0; i < m.size(); i++)
  {
    czsc_movement &row = out.movements[i];
    // 中阴开始（第89课）：连接极值点之后的下一端点分型成立
    row.zhongyinStart = i + 1 < m.size() ? a.snapshot.pivots[static_cast<std::size_t>(bound[i + 1]) + 1].fractalAt : -1;
    row.connectionStart = c[static_cast<std::size_t>(m[i].lastCenter)].lastPivot;
    row.connectionEnd = row.successor = row.successorEstablishedAt = row.completedAt = -1;
    row.completedByIndex = row.completedBy = -1;
    if (i + 1 < m.size())
    {
      const chan::Center &next = c[static_cast<std::size_t>(m[i + 1].firstCenter)];
      row.connectionEnd = next.firstPivot;
      row.successor = static_cast<int32_t>(i + 1);
      row.successorEstablishedAt = a.snapshot.pivots[static_cast<std::size_t>(next.firstPivot) + 3].fractalAt;
      row.completedAt = row.successorEstablishedAt;
    }
    if (m[i].type == chan::MovementType::Consolidation) continue;
    int32_t want = m[i].type == chan::MovementType::Up ? -1 : 1;
    for (std::size_t j = 0; j < out.signals.size(); j++)
    {
      const czsc_signal &s = out.signals[j];
      if (s.hindsight && s.type == want && s.center == m[i].lastCenter)
      {
        row.completedByIndex = s.index;
        row.completedBy = static_cast<int32_t>(j);
        break;
      }
    }
  }
}

int Later(int a, int b) { return (a < 0 || b < 0) ? -1 : std::max(a, b); }

// 递归走势节点（第17课）：逐层给出走势的起止连接点、中枢范围、成立/完成时刻、后继与子节点
// 下一层节点 [lo, hi) 中起止落在 [start, end] 内的连续一段；定型取成员中最晚者（任一未定型则 -1）
void Members(const LevelTables &out, std::size_t lo, std::size_t hi, int start, int end, int32_t &first, int32_t &count,
             int &finalAt)
{
  first = -1;
  count = 0;
  for (std::size_t c = lo; c < hi; c++)
  {
    const czsc_recursive_node &n = out.nodes[c];
    if (n.start < start || n.end > end) continue;
    if (first < 0) first = static_cast<int32_t>(c);
    count++;
    finalAt = Later(finalAt, n.confirmedAt);
  }
}

void BuildRecursiveNodes(const chan::Analysis &a, const chan::Series &s, LevelTables &out)
{
  std::vector<chan::RecursiveLevel> levels = chan::BuildRecursion(a);
  std::vector<std::size_t> levelBase;
  for (const chan::RecursiveLevel &l : levels)
  {
    std::size_t L = levelBase.size();
    levelBase.push_back(out.nodes.size());
    std::size_t nm = l.movements.size();
    // 本层中枢（level >= 1）：成员为下一层节点（第17课）
    int32_t centerBase = static_cast<int32_t>(out.rcenters.size());
    if (L > 0)
      for (std::size_t c = 0; c < l.centers.size(); c++)
      {
        const chan::Center &k = l.centers[c];
        czsc_recursive_center row = Row<czsc_recursive_center>();
        row.level = static_cast<int32_t>(L);
        row.ordinal = static_cast<int32_t>(c);
        row.start = k.start;
        row.end = k.end;
        row.zg = k.zg;
        row.zd = k.zd;
        row.gg = k.gg;
        row.dd = k.dd;
        row.direction = k.direction;
        row.established = l.pivotFinalAt[static_cast<std::size_t>(k.firstPivot) + 3];
        int f = l.centerFinalAt[c];
        Members(out, levelBase[L - 1], levelBase[L], k.start, k.end, row.firstMember, row.memberCount, f);
        row.confirmedAt = f;
        out.rcenters.push_back(row);
      }
    for (std::size_t m = 0; m < nm; m++)
    {
      const chan::Movement &mv = l.movements[m];
      const chan::Center &first = l.centers[static_cast<std::size_t>(mv.firstCenter)];
      czsc_recursive_node row = Row<czsc_recursive_node>();
      row.level = static_cast<int32_t>(levelBase.size() - 1);
      row.ordinal = static_cast<int32_t>(m);
      row.type = static_cast<int32_t>(mv.type);
      row.start = l.pivots[static_cast<std::size_t>(l.boundaries[m])].index;
      row.end = l.pivots[static_cast<std::size_t>(l.boundaries[m + 1])].index;
      row.centerStart = mv.start;
      row.centerEnd = mv.end;
      row.centerCount = mv.lastCenter - mv.firstCenter + 1;
      std::size_t third = static_cast<std::size_t>(first.firstPivot) + 3;
      row.established = row.level == 0 ? l.pivots[third].fractalAt : l.pivotFinalAt[third];
      row.connection = m + 1 < nm ? row.end : -1;
      row.completed = -1;
      row.successor = m + 1 < nm ? static_cast<int32_t>(out.nodes.size() + 1) : -1;
      row.firstChild = -1;
      row.childCount = 0;
      // 起点依赖前一走势（首个走势依赖进入端点），终点依赖后一走势：三者都定型才定型
      int startFinal = m == 0 ? l.pivotFinalAt[static_cast<std::size_t>(l.boundaries[0])] : l.movementFinalAt[m - 1];
      int endFinal = m + 1 < nm ? l.movementFinalAt[m + 1] : -1;
      row.confirmedAt = Later(Later(startFinal, l.movementFinalAt[m]), endFinal);
      row.firstCenter = (L > 0 ? centerBase : 0) + mv.firstCenter;
      row.lastCenter = (L > 0 ? centerBase : 0) + mv.lastCenter;
      row.high = s.high[static_cast<std::size_t>(row.start)];
      row.low = s.low[static_cast<std::size_t>(row.start)];
      for (int i = row.start; i <= row.end; i++)
      {
        row.high = std::max(row.high, s.high[static_cast<std::size_t>(i)]);
        row.low = std::min(row.low, s.low[static_cast<std::size_t>(i)]);
      }
      row.zhongyinStart = -1;
      if (m + 1 < nm)
      {
        std::size_t after = static_cast<std::size_t>(l.boundaries[m + 1]) + 1;  // 连接极值点之后的下一端点
        row.zhongyinStart = L == 0 ? l.pivots[after].fractalAt : l.pivotFinalAt[after];
      }
      out.nodes.push_back(row);
    }
    for (std::size_t m = levelBase.back(); m + 1 < out.nodes.size(); m++)
      out.nodes[m].completed = out.nodes[m + 1].established;
    // 同级别连接段（level >= 1）：前走势最后中枢末端点 → 后走势首中枢首端点，成员为下一层节点链
    if (L > 0)
      for (std::size_t m = 0; m + 1 < nm; m++)
      {
        czsc_recursive_connection row = Row<czsc_recursive_connection>();
        row.level = static_cast<int32_t>(L);
        row.ordinal = static_cast<int32_t>(m);
        row.left = static_cast<int32_t>(levelBase[L] + m);
        row.right = row.left + 1;
        row.start = l.centers[static_cast<std::size_t>(l.movements[m].lastCenter)].end;
        row.end = l.centers[static_cast<std::size_t>(l.movements[m + 1].firstCenter)].start;
        int f = l.movementFinalAt[m];  // 两端由后继首中枢截止，其定型即本走势分组定型
        Members(out, levelBase[L - 1], levelBase[L], row.start, row.end, row.firstMember, row.memberCount, f);
        row.confirmedAt = f;
        out.connections.push_back(row);
      }
  }
  // 子节点：下一层中起止落在本节点起止之内的节点（上层端点即下层连接点，故为连续的一段）
  for (std::size_t L = 1; L < levelBase.size(); L++)
  {
    std::size_t lo = levelBase[L - 1], hi = levelBase[L];
    for (std::size_t n = levelBase[L]; n < (L + 1 < levelBase.size() ? levelBase[L + 1] : out.nodes.size()); n++)
    {
      czsc_recursive_node &node = out.nodes[n];
      for (std::size_t c = lo; c < hi; c++)
      {
        if (out.nodes[c].start < node.start || out.nodes[c].end > node.end) continue;
        if (node.firstChild < 0) node.firstChild = static_cast<int32_t>(out.children.size());
        out.children.push_back(static_cast<int32_t>(c));
        node.childCount++;
      }
    }
  }
}

std::string CheckInput(const czsc_input *in)
{
  if (in == nullptr) return "input 为 NULL";
  if (in->size < sizeof(czsc_input)) return "input->size 小于 sizeof(czsc_input)，请按当前头文件构造";
  if (in->n < 0 || in->n > kMaxBars) return "n 须在 0..16777216";
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

LevelTables ProjectLevel(const czsc_input *in, const chan::Series &s, const chan::Analysis &a,
                         uint32_t outputs, const std::vector<int8_t> &gaps, const std::vector<int8_t> &strengths)
{
  const auto &config = a.config;
  std::size_t n = static_cast<std::size_t>(in->n);
  Resolver r(a);
  LevelTables table;
  LevelTables *out = &table;
  for (std::size_t i = 0; i < a.snapshot.pivots.size(); i++)
  {
    const chan::Pivot &p = a.snapshot.pivots[i];
    czsc_pivot row = Row<czsc_pivot>();
    row.index = chan::DisplayPivotIndex(p, config);
    row.extremeIndex = p.index;
    row.kind = static_cast<int32_t>(p.kind);
    row.price = row.index == p.index ? p.Price()
        : (p.kind == chan::Kind::Top ? in->high[row.index] : in->low[row.index]);
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
    row.established = a.snapshot.pivots[static_cast<std::size_t>(c.firstPivot) + 3].fractalAt;
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
  BuildSignals(a, r, (outputs & CZSC_OUTPUT_EVENTS) != 0, *out);
  CompleteMovements(a, *out);
  if (outputs & CZSC_OUTPUT_RECURSION) BuildRecursiveNodes(a, s, *out);

  for (std::size_t i = 0; i < n; i++)
  {
    czsc_bar row = Row<czsc_bar>();
    row.dif = a.inputs->energy.dif[i];
    row.dea = a.inputs->energy.dea[i];
    row.macd = (a.inputs->energy.dif[i] - a.inputs->energy.dea[i]) * 2.0f;
    row.kiss = static_cast<int32_t>(a.inputs->ma.kisses[i]);
    row.maShort = a.inputs->ma.shortMa[i];
    row.maLong = a.inputs->ma.longMa[i];
    row.gap = gaps[i];
    row.fractalStrength = strengths[i];
    row.instantDivergence = a.instantWarning[i];
    out->bars.push_back(row);
  }
  return table;
}

Snapshot *Handle(void *h)
{
  auto *s = static_cast<Snapshot *>(h);
  if (!s || s->magic != kMagic) { SetError("snapshot句柄无效"); return nullptr; }
  return s;
}

// 区间套（第27/61课）：每个低级别一类信号，在高级别快照中找同向且背驰段 c 包含其K线的一类信号；
// 同时给出包含它的高级别段；段方向一致而无高级别背驰时标小转大候选（第43课）
std::vector<czsc_nested> BuildNested(const LevelTables &low, const LevelTables &high)
{
  std::vector<czsc_nested> out;
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
      int cStart = hp[static_cast<std::size_t>(h.divergence.curStart)].extremeIndex;
      if (cStart > l.index || l.index > h.index) continue;
      const czsc_signal *best = row.highSignal >= 0 ? &high.signals[static_cast<std::size_t>(row.highSignal)] : nullptr;
      if (!best || h.index < best->index || (h.index == best->index && h.confirmedAt >= 0 && best->confirmedAt < 0))
        row.highSignal = static_cast<int32_t>(j);  // 取包含它的最近（最早结束）的高级别背驰段
    }
    auto it = std::upper_bound(hp.begin(), hp.end(), l.index, [](int b, const czsc_pivot &p) { return b < p.extremeIndex; });
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
    row.highPrevStartLow = row.highPrevEndLow = row.highCurStartLow = row.highCurEndLow = -1;
    if (h)
    {
      auto lowPivot = [&](int32_t highPivot) -> int32_t {
        if (highPivot < 0) return -1;
        int bar = hp[static_cast<std::size_t>(highPivot)].extremeIndex;
        auto it = std::lower_bound(low.pivots.begin(), low.pivots.end(), bar,
                                   [](const czsc_pivot &p, int b) { return p.extremeIndex < b; });
        return (it != low.pivots.end() && it->extremeIndex == bar) ? static_cast<int32_t>(it - low.pivots.begin()) : -1;
      };
      row.highPrevStartLow = lowPivot(h->divergence.prevStart);
      row.highPrevEndLow = lowPivot(h->divergence.prevEnd);
      row.highCurStartLow = lowPivot(h->divergence.curStart);
      row.highCurEndLow = lowPivot(h->divergence.curEnd);
    }
    out.push_back(row);
  }
  return out;
}

std::string ReadConfig(const czsc_config *c, chan::AnalysisConfig &out)
{
  if (!c || c->size < sizeof(*c)) return "config为空或size不足";
  out.stroke.rule=static_cast<chan::StrokeRule>(c->strokeRule);
  out.stroke.endpoint=static_cast<chan::StrokeEnd>(c->strokeEndpoint);
  out.stroke.gap=static_cast<chan::GapRule>(c->strokeGap);
  out.stroke.gapThreshold=c->gapThreshold;
  out.segment.method=static_cast<chan::SegmentMethod>(c->segmentMethod);
  out.center.strokeFormation=static_cast<chan::CenterFormation>(c->centerStrokeFormation);
  out.signals.publication=static_cast<chan::SignalPublication>(c->signalsPublication);
  return chan::Validate(out);
}

czsc_config ExportConfig(const chan::AnalysisConfig &a)
{
  return {sizeof(czsc_config),static_cast<int>(a.stroke.rule),static_cast<int>(a.stroke.endpoint),
    static_cast<int>(a.stroke.gap),a.stroke.gapThreshold,static_cast<int>(a.segment.method),
    static_cast<int>(a.center.strokeFormation),static_cast<int>(a.signals.publication)};
}

std::string Project(Snapshot &s, const czsc_projection *p)
{
  if (!p || p->size < sizeof(*p)) return "projection为空或size不足";
  if (p->segmentBoundary < 0 || p->segmentBoundary > 2 || p->centerBox < 0 || p->centerBox > 1)
    return "显示投影枚举无效";
  if (s.config.segment.method==chan::SegmentMethod::Heuristic && p->segmentBoundary!=0)
    return "启发式线段仅支持极值分界";
  // 先构造临时表再交换，分配失败时保持旧投影完整。
  std::array<std::vector<czsc_pivot>,2> pivots;
  std::array<std::vector<czsc_center>,2> centers;
  for (int level=0; level<2; ++level)
  {
    pivots[level]=s.tables[level].pivots; centers[level]=s.tables[level].centers;
    chan::LevelConfig view; view.analysis=s.config; view.level=static_cast<chan::CenterUnit>(level);
    view.projection.segmentBoundary=static_cast<chan::SegmentEnd>(p->segmentBoundary);
    for (std::size_t i=0; i<pivots[level].size(); ++i)
    {
      auto &row=pivots[level][i]; row.index=chan::DisplayPivotIndex(s.sourcePivots[level][i],view);
      if (row.index!=row.extremeIndex) row.price=row.kind==1?s.high[row.index]:s.low[row.index];
    }
    for (auto &row:centers[level])
    {
      row.start=s.tables[level].pivots[row.firstPivot].extremeIndex;
      row.end=s.tables[level].pivots[p->centerBox==0?row.firstPivot+3:row.lastPivot].extremeIndex;
    }
  }
  s.projectedPivots.swap(pivots); s.projectedCenters.swap(centers);
  return {};
}

Snapshot *Build(const czsc_input *in, const czsc_config *c, uint32_t outputs)
{
  chan::AnalysisConfig config;
  auto error=CheckInput(in); if(error.empty()) error=ReadConfig(c,config);
  if (error.empty() && ((outputs & ~CZSC_OUTPUT_DEFAULT) || !(outputs & 3))) error="outputs需选择合法级别且无未知位";
  if (error.empty() && (outputs & CZSC_OUTPUT_NESTED) && (outputs & 3)!=3) error="区间套必须同时选择笔级与线段级";
  if (!error.empty()) { Fail(error); return nullptr; }
  chan::Series source;
  if (in->n>0)
  {
    source.high.assign(in->high,in->high+in->n); source.low.assign(in->low,in->low+in->n);
    source.close.assign(in->close,in->close+in->n); source.volume.assign(in->volume,in->volume+in->n);
  }
  auto family=chan::AnalyzeFamily(source,config);
  auto out=std::make_unique<Snapshot>(); out->config=chan::Normalize(config); out->outputs=outputs;
  out->high=source.high; out->low=source.low;
  auto gaps=chan::Gaps(source);
  auto strengths=chan::FractalStrengths(source,family.levels[0].inputs->bars,family.levels[0].inputs->fractals);
  for (int level=0; level<2; ++level) if(outputs & (1u<<level))
  {
    out->tables[level]=ProjectLevel(in,source,family.levels[level],outputs,gaps,strengths);
    out->sourcePivots[level]=family.levels[level].snapshot.pivots;
  }
  if(outputs & CZSC_OUTPUT_NESTED) out->nested=BuildNested(out->tables[0],out->tables[1]);
  czsc_projection p{sizeof(p),0,1}; Project(*out,&p);
  return out.release();
}

template<class T>
const T *Table(void *h, int32_t level, int32_t *count, std::vector<T> LevelTables::*member)
{
  if(count)*count=0;
  auto *s=Handle(h); if(!s)return nullptr;
  if(level<0 || level>1 || !(s->outputs & (1u<<level))) { SetError("level无效或未请求此级别"); return nullptr; }
  const auto &v=s->tables[level].*member;
  if(count)*count=static_cast<int32_t>(v.size());
  return v.data();
}

} // namespace

extern "C" {
int32_t czsc_api_version(void) { return CZSC_API_VERSION; }
#ifndef CZSC_BUILD_COMMIT
#define CZSC_BUILD_COMMIT "unknown"
#endif
const char *czsc_build_commit(void) { return CZSC_BUILD_COMMIT; }
const char *czsc_last_error(void) { return g_error; }
int32_t czsc_config_default(czsc_config *out)
{
  if(!out) { SetError("config输出为空"); return -1; }
  *out=ExportConfig(chan::AnalysisConfig{}); return 0;
}
int32_t czsc_projection_default(czsc_projection *out)
{
  if(!out) { SetError("projection输出为空"); return -1; }
  *out={sizeof(*out),0,1}; return 0;
}
int32_t czsc_config_validate(const czsc_config *c)
{
  chan::AnalysisConfig a; auto error=ReadConfig(c,a); SetError(error); return error.empty()?0:-1;
}
int32_t czsc_config_id(const czsc_config *c, char *out, int32_t cap)
{
  try
  {
    chan::AnalysisConfig a; auto error=ReadConfig(c,a);
    if(cap<0 || (!out && cap!=0)) error="身份输出容量无效";
    if(!error.empty()) { SetError(error); return -1; }
    auto id=chan::AnalysisId(a); int32_t size=static_cast<int32_t>(id.size()+1);
    if(out && cap>=size) std::memcpy(out,id.c_str(),size);
    return size;
  } catch(...) { SetError("配置身份生成失败"); return -1; }
}
int32_t czsc_config_parse(const char *id, czsc_config *out)
{
  try
  {
    if(!id || !out) { SetError("身份或config输出为空"); return -1; }
    chan::AnalysisConfig a; auto error=chan::ParseAnalysisId(id,a);
    if(!error.empty()) { SetError(error); return -1; }
    *out=ExportConfig(a); return 0;
  } catch(...) { SetError("配置身份解析失败"); return -1; }
}
void *czsc_build(const czsc_input *in, const czsc_config *c, uint32_t outputs)
{
  g_error[0]='\0';
  try { return Build(in,c,outputs); }
  catch(const std::bad_alloc &) { return Fail("内存不足"); }
  catch(...) { return Fail("内部错误"); }
}
void czsc_snapshot_free(void *h)
{
  if(!h)return; auto *s=Handle(h); if(s) { s->magic=0; delete s; }
}
int32_t czsc_set_projection(void *h, const czsc_projection *p)
{
  try
  {
    auto *s=Handle(h); if(!s)return -1;
    auto error=Project(*s,p); SetError(error); return error.empty()?0:-1;
  } catch(...) { SetError("显示投影生成失败"); return -1; }
}
const czsc_nested *czsc_nested_rows(void *h, int32_t *count)
{
  if(count)*count=0; auto *s=Handle(h); if(!s)return nullptr;
  if(!(s->outputs & CZSC_OUTPUT_NESTED)) { SetError("未请求区间套"); return nullptr; }
  if(count)*count=static_cast<int32_t>(s->nested.size()); return s->nested.data();
}
const czsc_pivot *czsc_level_pivots(void *h, int32_t level, int32_t *count)
{
  Table(h,level,count,&LevelTables::pivots);
  auto *s=Handle(h); if(!s || level<0 || level>1 || !(s->outputs & (1u<<level)))return nullptr;
  return s->projectedPivots[level].data();
}
const czsc_center *czsc_level_centers(void *h, int32_t level, int32_t *count)
{
  Table(h,level,count,&LevelTables::centers);
  auto *s=Handle(h); if(!s || level<0 || level>1 || !(s->outputs & (1u<<level)))return nullptr;
  return s->projectedCenters[level].data();
}
const czsc_movement *czsc_level_movements(void *h, int32_t level, int32_t *count) { return Table(h,level,count,&LevelTables::movements); }
const czsc_breakout *czsc_level_breakouts(void *h, int32_t level, int32_t *count) { return Table(h,level,count,&LevelTables::breakouts); }
const czsc_signal *czsc_level_signals(void *h, int32_t level, int32_t *count) { return Table(h,level,count,&LevelTables::signals); }
const czsc_event *czsc_level_events(void *h, int32_t level, int32_t *count) { return Table(h,level,count,&LevelTables::events); }
const czsc_bar *czsc_level_bars(void *h, int32_t level, int32_t *count) { return Table(h,level,count,&LevelTables::bars); }
const czsc_recursive_node *czsc_level_recursive_nodes(void *h, int32_t level, int32_t *count) { return Table(h,level,count,&LevelTables::nodes); }
const int32_t *czsc_level_recursive_children(void *h, int32_t level, int32_t *count) { return Table(h,level,count,&LevelTables::children); }
const czsc_recursive_center *czsc_level_recursive_centers(void *h, int32_t level, int32_t *count) { return Table(h,level,count,&LevelTables::rcenters); }
const czsc_recursive_connection *czsc_level_recursive_connections(void *h, int32_t level, int32_t *count) { return Table(h,level,count,&LevelTables::connections); }
} // extern C
