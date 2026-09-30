// 结构化接口（adapter/czsc_api.h）验收：docs/nextjs-quant-adapter.md §7。
#include "check.h"
#include "sse_data.h"
#include "adapter/czsc_api.h"
#include "core/engine.h"
#include "tdx/exports.h"

#include <algorithm>
#include <cstddef>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{
struct Data
{
  std::vector<float> h, l, c, v;
  int n() const { return static_cast<int>(h.size()); }
};

Data Sse(int count = SSE_DAILY_COUNT)
{
  Data d;
  d.h.assign(SSE_DAILY_HIGH, SSE_DAILY_HIGH + count);
  d.l.assign(SSE_DAILY_LOW, SSE_DAILY_LOW + count);
  d.c.assign(SSE_DAILY_CLOSE, SSE_DAILY_CLOSE + count);
  d.v.assign(SSE_DAILY_VOLUME, SSE_DAILY_VOLUME + count);
  return d;
}

czsc_input Input(const Data &d, int config, int flags = CZSC_FLAG_EVENTS)
{
  czsc_input in;
  std::memset(&in, 0, sizeof in);
  in.size = sizeof in;
  in.n = d.n();
  in.high = d.h.data();
  in.low = d.l.data();
  in.close = d.c.data();
  in.volume = d.v.data();
  in.config = config;
  in.flags = flags;
  return in;
}

template <class T>
std::vector<T> Get(const T *(*f)(void *, int32_t *), void *h)
{
  int32_t n = -1;
  const T *p = f(h, &n);
  return std::vector<T>(p, p + (n > 0 ? n : 0));
}

template <class T>
bool Bytes(const std::vector<T> &a, const std::vector<T> &b)
{
  return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0);
}

struct Tables
{
  std::vector<czsc_pivot> p;
  std::vector<czsc_center> c;
  std::vector<czsc_movement> m;
  std::vector<czsc_breakout> b;
  std::vector<czsc_signal> s;
  std::vector<czsc_event> e;
  std::vector<czsc_bar> r;
};

Tables Read(void *h)
{
  return {Get(czsc_pivots, h),    Get(czsc_centers, h), Get(czsc_movements, h), Get(czsc_breakouts, h),
          Get(czsc_signals, h),   Get(czsc_events, h),  Get(czsc_bars, h)};
}

bool Same(const Tables &a, const Tables &b)
{
  return Bytes(a.p, b.p) && Bytes(a.c, b.c) && Bytes(a.m, b.m) && Bytes(a.b, b.b) && Bytes(a.s, b.s) && Bytes(a.e, b.e) &&
         Bytes(a.r, b.r);
}

Tables Build(const Data &d, int config)
{
  czsc_input in = Input(d, config);
  void *h = czsc_snapshot_build(&in);
  if (!h) return Tables();
  Tables t = Read(h);
  czsc_snapshot_free(h);
  return t;
}

int Priority(int32_t type) { int a = std::abs(type); return a == 1 ? 30 : (a == 3 ? 20 : 10); }
float TdxCode(int32_t type) { return static_cast<float>(type > 0 ? type : 10 - type); }
}  // namespace

TEST(ApiVersionAndStructSizes)
{
  CHECK(czsc_api_version() == CZSC_API_VERSION && CZSC_API_VERSION == 7);
  CHECK(czsc_build_commit() != nullptr && czsc_build_commit()[0] != 0);
  // 全部为 4 字节字段、无填充：逐字节确定
  CHECK(sizeof(czsc_pivot) == 6 * 4 && sizeof(czsc_center) == 14 * 4 && sizeof(czsc_movement) == 15 * 4);
  CHECK(sizeof(czsc_recursive_node) == 21 * 4 && sizeof(czsc_recursive_center) == 14 * 4 &&
        sizeof(czsc_recursive_connection) == 10 * 4);
  CHECK(sizeof(czsc_divergence) == 17 * 4 && sizeof(czsc_breakout) == 7 * 4 + sizeof(czsc_divergence));
  CHECK(sizeof(czsc_event) == 4 * 4 && sizeof(czsc_bar) == 10 * 4 && sizeof(czsc_nested) == 13 * 4);
  CHECK(sizeof(czsc_signal) == 12 * 4 + sizeof(czsc_divergence) + 7 * 4);
}

// §7.1：与同一引擎（golden 所用）逐项一致
TEST(ApiMatchesEngine)
{
  Data d = Sse();
  chan::Series s;
  s.high = d.h; s.low = d.l; s.close = d.c; s.volume = d.v;
  for (int code : {0, 1100})
  {
    chan::Analysis a = chan::Analyze(s, *chan::Config::Decode(code));
    Tables t = Build(d, code);
    REQUIRE(t.p.size() == a.snapshot.pivots.size() && t.c.size() == a.snapshot.centers.size() &&
            t.m.size() == a.snapshot.movements.size() && t.b.size() == a.snapshot.breakouts.size());
    for (std::size_t i = 0; i < t.p.size(); i++)
      CHECK(t.p[i].index == a.snapshot.pivots[i].index && t.p[i].price == a.snapshot.pivots[i].Price() &&
            t.p[i].size == sizeof(czsc_pivot));
    for (std::size_t i = 0; i < t.c.size(); i++)
      CHECK(t.c[i].start == a.snapshot.centers[i].start && t.c[i].zg == a.snapshot.centers[i].zg &&
            t.c[i].zd == a.snapshot.centers[i].zd && t.c[i].end == a.snapshot.centers[i].end);
    // 事后集合：hindsight 行按序与 13 号同一集合
    std::size_t k = 0;
    for (const czsc_signal &row : t.s)
    {
      if (!row.hindsight) continue;
      REQUIRE(k < a.snapshot.signals.size());
      const chan::Signal &g = a.snapshot.signals[k++];
      int code2 = static_cast<int>(g.type);
      CHECK(row.index == g.index && row.type == (code2 < 10 ? code2 : -(code2 - 10)));
    }
    CHECK(k == a.snapshot.signals.size());
    // 事件：与引擎事件一一对应，引用的行给出同一信号与失效价
    REQUIRE(t.e.size() == a.events.size());
    for (std::size_t i = 0; i < t.e.size(); i++)
    {
      const czsc_signal &row = t.s[static_cast<std::size_t>(t.e[i].signal)];
      const chan::SignalEvent &ev = a.events[i];
      CHECK(t.e[i].bar == ev.bar && t.e[i].op == (ev.revoked ? -1 : 1) && row.index == ev.signal.index);
      if (!ev.revoked) CHECK(row.stop == ev.signal.stop && row.confirmedAt == ev.bar);
      else CHECK(row.revokedAt == ev.bar);
    }
  }
}

// §7.2：因果一致性（前缀快照中已定型/已确认对象与全量逐字段相同；事件流与 5/6 号逐根等价）
TEST(ApiCausalConsistency)
{
  Data full = Sse();
  int compared = 0;
  for (int code : {0, 2, 101, 1100})
  {
    Tables all = Build(full, code);
    for (int k = 90; k < full.n(); k += 61)
    {
      Tables pre = Build(Sse(k + 1), code);
      for (std::size_t i = 0; i < pre.p.size(); i++)
        if (pre.p[i].confirmedAt >= 0) { CHECK(pre.p[i].confirmedAt <= k && std::memcmp(&pre.p[i], &all.p[i], sizeof(czsc_pivot)) == 0); compared++; }
      for (std::size_t i = 0; i < pre.c.size(); i++)
        if (pre.c[i].confirmedAt >= 0) CHECK(std::memcmp(&pre.c[i], &all.c[i], sizeof(czsc_center)) == 0);
      for (std::size_t i = 0; i < pre.m.size(); i++)
        if (pre.m[i].confirmedAt >= 0)
        {
          czsc_movement x = pre.m[i], y = all.m[i];
          x.completedBy = y.completedBy = 0;  // 信号表下标随快照解析，非因果；其余字段（含 completedByIndex）须不变
          CHECK(std::memcmp(&x, &y, sizeof x) == 0);
        }
      for (std::size_t i = 0; i < pre.b.size(); i++)
        if (pre.b[i].confirmedAt >= 0) CHECK(std::memcmp(&pre.b[i], &all.b[i], sizeof(czsc_breakout)) == 0);
      // 信号：按 (K线, 类型, 确认时刻) 找同一段生命；revokedAt 晚于 k 视为未撤销；hindsight 非因果不比
      for (const czsc_signal &x : pre.s)
      {
        if (x.confirmedAt < 0) continue;
        const czsc_signal *y = nullptr;
        for (const czsc_signal &z : all.s)
          if (z.index == x.index && z.type == x.type && z.confirmedAt == x.confirmedAt) y = &z;
        REQUIRE(y != nullptr);
        czsc_signal a = x, b = *y;
        if (b.revokedAt > k) b.revokedAt = -1;
        a.hindsight = b.hindsight = 0;
        // 引用按对象身份比较（K线/中枢起点），下标可能因未定型对象而不同
        auto pivotBar = [](const Tables &t, int32_t i) { return i < 0 ? -1 : t.p[static_cast<std::size_t>(i)].index; };
        auto centerStart = [](const Tables &t, int32_t i) { return i < 0 ? -1 : t.c[static_cast<std::size_t>(i)].start; };
        // 引用按本快照解析：信号后来被撤销、其引用对象已从当前结构中消失（-1）属契约允许
        auto same = [&](int32_t x1, int32_t y1) { return x1 == y1 || (y->revokedAt >= 0 && y1 == -1); };
        CHECK(same(pivotBar(pre, a.pivot), pivotBar(all, b.pivot)) && same(centerStart(pre, a.center), centerStart(all, b.center)));
        CHECK(same(pivotBar(pre, a.divergence.prevStart), pivotBar(all, b.divergence.prevStart)) &&
              same(pivotBar(pre, a.divergence.curEnd), pivotBar(all, b.divergence.curEnd)));
        a.pivot = b.pivot = a.center = b.center = a.movement = b.movement = a.breakout = b.breakout = a.basedOn = b.basedOn = 0;
        a.divergence.prevStart = b.divergence.prevStart = a.divergence.prevEnd = b.divergence.prevEnd = 0;
        a.divergence.curStart = b.divergence.curStart = a.divergence.curEnd = b.divergence.curEnd = 0;
        CHECK(std::memcmp(&a, &b, sizeof a) == 0);
      }
      // 事件流：前缀事件 = 全量中 bar<=k 的事件（按信号身份）
      std::vector<std::string> pe, ae;
      for (const czsc_event &e : pre.e)
      {
        const czsc_signal &s = pre.s[static_cast<std::size_t>(e.signal)];
        pe.push_back(std::to_string(e.bar) + "/" + std::to_string(e.op) + "/" + std::to_string(s.index) + "/" + std::to_string(s.type));
      }
      for (const czsc_event &e : all.e)
      {
        if (e.bar > k) continue;
        const czsc_signal &s = all.s[static_cast<std::size_t>(e.signal)];
        ae.push_back(std::to_string(e.bar) + "/" + std::to_string(e.op) + "/" + std::to_string(s.index) + "/" + std::to_string(s.type));
      }
      CHECK(pe == ae);
    }
  }
  CHECK(compared > 500);

  // 事件流与通达信 5/6 号逐根等价（两边都用真实收盘价）
  tdx::ResetForTesting();
  std::vector<float> echo(static_cast<std::size_t>(full.n()));
  tdx::RegisterCloseVolume(full.n(), echo.data(), full.c.data(), full.v.data(), nullptr);
  for (int code : {0, 1100})
  {
    Tables t = Build(full, code);
    std::vector<float> cfg(static_cast<std::size_t>(full.n()), static_cast<float>(code));
    std::vector<float> s5(cfg.size()), s6(cfg.size()), e5(cfg.size(), 0), e6(cfg.size(), 0);
    std::vector<int> p5(cfg.size(), -1), p6(cfg.size(), -1);
    tdx::Signals(full.n(), s5.data(), full.h.data(), full.l.data(), cfg.data());
    tdx::Revokes(full.n(), s6.data(), full.h.data(), full.l.data(), cfg.data());
    for (const czsc_event &e : t.e)
    {
      const czsc_signal &s = t.s[static_cast<std::size_t>(e.signal)];
      std::vector<float> &out = e.op > 0 ? e5 : e6;
      std::vector<int> &pri = e.op > 0 ? p5 : p6;
      std::size_t b = static_cast<std::size_t>(e.bar);
      if (Priority(s.type) >= pri[b]) { out[b] = TdxCode(s.type); pri[b] = Priority(s.type); }
    }
    CHECK(e5 == s5 && e6 == s6);
  }
  tdx::ResetForTesting();
}

// §7.3：可重入——交错构建不同输入/配置、释放后结果逐字节不变，未释放句柄互不影响
TEST(ApiReentrant)
{
  Data a = Sse(), b = Sse(900);
  czsc_input ia = Input(a, 0), ib = Input(b, 1100), ic = Input(a, 2);
  void *h1 = czsc_snapshot_build(&ia);
  Tables t1 = Read(h1);
  void *h2 = czsc_snapshot_build(&ib);
  void *h3 = czsc_snapshot_build(&ic);
  Tables t2 = Read(h2);
  czsc_snapshot_free(h2);
  void *h4 = czsc_snapshot_build(&ia);
  CHECK(Same(Read(h1), t1) && Same(Read(h4), t1));
  czsc_snapshot_free(h1);
  CHECK(Same(Read(h4), t1));
  void *h5 = czsc_snapshot_build(&ib);
  CHECK(Same(Read(h5), t2));
  czsc_snapshot_free(h3);
  czsc_snapshot_free(h4);
  czsc_snapshot_free(h5);
  czsc_snapshot_free(nullptr);  // 允许
}

// §7.4：边界
TEST(ApiEdgeCases)
{
  CHECK(czsc_snapshot_build(nullptr) == nullptr && std::strlen(czsc_last_error()) > 0);
  for (int n : {0, 1, 2})
  {
    Data d = Sse(n);
    czsc_input in = Input(d, 0);
    void *h = czsc_snapshot_build(&in);
    REQUIRE(h != nullptr);
    Tables t = Read(h);
    CHECK(t.p.empty() && t.c.empty() && t.s.empty() && t.e.empty() && static_cast<int>(t.r.size()) == n);
    czsc_snapshot_free(h);
  }
  Data flat;
  flat.h.assign(50, 10.0f); flat.l.assign(50, 10.0f); flat.c.assign(50, 10.0f); flat.v.assign(50, 1.0f);
  czsc_input fin = Input(flat, 1100);
  void *hf = czsc_snapshot_build(&fin);
  REQUIRE(hf != nullptr);
  CHECK(Read(hf).p.empty() && Read(hf).r.size() == 50);
  czsc_snapshot_free(hf);

  auto rejects = [](czsc_input in) {
    void *h = czsc_snapshot_build(&in);
    bool ok = h == nullptr && std::strlen(czsc_last_error()) > 0;
    czsc_snapshot_free(h);
    return ok;
  };
  Data d = Sse(100);
  czsc_input in = Input(d, 0);
  for (int bad : {5, 20, 200, 2000, 9999, -1}) { czsc_input x = in; x.config = bad; CHECK(rejects(x)); }
  { czsc_input x = in; x.size = 8; CHECK(rejects(x)); }
  { czsc_input x = in; x.n = -1; CHECK(rejects(x)); }
  { czsc_input x = in; x.n = 16777217; CHECK(rejects(x)); }
  { czsc_input x = in; x.close = nullptr; CHECK(rejects(x)); }
  { czsc_input x = in; x.flags = 0x10; CHECK(rejects(x)); }
  Data nan = d; nan.h[40] = std::numeric_limits<float>::quiet_NaN();
  CHECK(rejects(Input(nan, 0)));
  Data inv = d; inv.l[10] = inv.h[10] + 1;
  CHECK(rejects(Input(inv, 0)));
  Data outc = d; outc.c[10] = outc.h[10] + 5;
  CHECK(rejects(Input(outc, 0)));
  int32_t count = 7;
  CHECK(czsc_pivots(nullptr, &count) == nullptr && count == 0);
  CHECK(czsc_bars(nullptr, nullptr) == nullptr);
}

// §2.8：SSE 两套配置一次完整构建（含事件）< 50ms
TEST(ApiPerformance)
{
  Data d = Sse();
  auto t0 = std::chrono::steady_clock::now();
  for (int code : {0, 1100})
  {
    czsc_input in = Input(d, code);
    czsc_snapshot_free(czsc_snapshot_build(&in));
  }
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  std::printf("  SSE 2038 bars x 2 configs: %.2f ms\n", ms);
  CHECK(ms < 50.0);
}

namespace
{
Data X12(int count)
{
  Data d;
  for (const SseSample &s : SseSamples())
    if (std::string(s.name) == "sse-x12")
    {
      d.h.assign(s.high.begin(), s.high.begin() + count);
      d.l.assign(s.low.begin(), s.low.begin() + count);
    }
  d.c.resize(d.h.size());
  d.v.assign(d.h.size(), 1.0f);
  for (std::size_t i = 0; i < d.h.size(); i++) d.c[i] = (d.h[i] + d.l[i]) * 0.5f;
  return d;
}
}  // namespace

// P1：研判语义字段
TEST(ApiResearchFields)
{
  Data d = X12(24000);
  chan::Series s;
  s.high = d.h; s.low = d.l; s.close = d.c; s.volume = d.v;
  chan::Analysis a = chan::Analyze(s, chan::Config{});
  Tables t = Build(d, 0);
  int seconds = 0, strong = 0;
  for (const czsc_signal &x : t.s)
  {
    CHECK(x.quality == 1 || x.quality == 2);
    strong += x.quality == 2;
    if (std::abs(x.type) == 2)
    {
      seconds++;
      if (x.basedOn >= 0 && x.secondBasePivot >= 0)
        CHECK(t.s[static_cast<std::size_t>(x.basedOn)].pivot == x.secondBasePivot);
      if (x.secondBasePivot >= 0 && x.pivot >= 0) CHECK(x.secondTurnPivot == x.secondBasePivot + 1 && x.pivot == x.secondBasePivot + 2);
    }
    else
    {
      CHECK(x.secondBasePivot == -1 && x.secondTurnPivot == -1);
    }
    if (std::abs(x.type) == 3) CHECK((x.context & CZSC_CTX_FIRST_RETEST) != 0);
    if (x.context & CZSC_CTX_STANDARD) CHECK((x.context & CZSC_CTX_ZERO_PULLBACK) && x.divergence.weakArea);
    if (!(x.context & CZSC_CTX_SMALL_TURN)) CHECK(x.smallTurnBasePivot == -1);
  }
  CHECK(seconds > 0 && strong > 0);
  for (std::size_t i = 0; i < t.c.size(); i++)
  {
    if (i == 0) { CHECK(t.c[i].lifecycle == -1); continue; }
    int rel = t.c[i].relationToPrev, lc = t.c[i].lifecycle;
    CHECK(lc == 0 || (rel == 2 && lc == 1) || (rel == 1 && lc == 2) || (rel == -1 && lc == 3));
  }
  REQUIRE(t.r.size() == a.instantWarning.size());
  int warn = 0;
  for (std::size_t i = 0; i < t.r.size(); i++)
  {
    CHECK(t.r[i].instantDivergence == a.instantWarning[i]);
    warn += t.r[i].instantDivergence != 0;
  }
  CHECK(warn > 0);
}

// 逐轮放大 1.8 倍拼接的上证序列：线段级中枢层层上移，形成线段级趋势（线段级一类信号才可能出现）
Data Rising()
{
  Data d;
  float scale = 1.0f;
  for (int r = 0; r < 6; r++, scale *= 1.8f)
    for (int i = 0; i < SSE_DAILY_COUNT; i++)
    {
      d.h.push_back(SSE_DAILY_HIGH[i] * scale);
      d.l.push_back(SSE_DAILY_LOW[i] * scale);
      d.c.push_back(SSE_DAILY_CLOSE[i] * scale);
      d.v.push_back(SSE_DAILY_VOLUME[i]);
    }
  return d;
}

// P1：区间套
TEST(ApiNested)
{
  Data d = Rising();
  czsc_input lo = Input(d, 0), hi = Input(d, 1100);
  void *hl = czsc_snapshot_build(&lo), *hh = czsc_snapshot_build(&hi);
  REQUIRE(hl && hh);
  Data other = Sse();
  czsc_input oi = Input(other, 1100);
  void *ho = czsc_snapshot_build(&oi);
  CHECK(czsc_nested_build(hl, ho) == nullptr && std::strlen(czsc_last_error()) > 0);  // 不同数据
  CHECK(czsc_nested_build(hl, nullptr) == nullptr);
  void *hn = czsc_nested_build(hl, hh);
  REQUIRE(hn != nullptr);
  Tables L = Read(hl), H = Read(hh);
  std::vector<czsc_nested> rows = Get(czsc_nested_rows, hn);
  int lowFirst = 0, inside = 0, turn = 0;
  for (const czsc_signal &x : L.s) lowFirst += std::abs(x.type) == 1;
  CHECK(static_cast<int>(rows.size()) == lowFirst);
  for (const czsc_nested &r : rows)
  {
    const czsc_signal &l = L.s[static_cast<std::size_t>(r.lowSignal)];
    CHECK(std::abs(l.type) == 1 && r.size == sizeof(czsc_nested));
    if (r.highSignal >= 0)
    {
      const czsc_signal &h = H.s[static_cast<std::size_t>(r.highSignal)];
      CHECK(h.type == l.type && H.p[static_cast<std::size_t>(h.divergence.curStart)].index <= l.index && l.index <= h.index);
      inside++;
    }
    CHECK(r.insideHighSegment == (r.highSignal >= 0));
    if (r.confirmed) CHECK(r.insideHighSegment);
    if (r.smallTurn) { CHECK(!r.insideHighSegment); turn++; }
    if (r.highSegmentStart >= 0) CHECK(H.p[static_cast<std::size_t>(r.highSegmentStart)].index <= l.index);
    if (r.highSegmentEnd >= 0) CHECK(H.p[static_cast<std::size_t>(r.highSegmentEnd)].index > l.index);
  }
  std::printf("  nested: %zu low first-class, %d inside high divergence, %d small-turn candidates\n", rows.size(), inside, turn);
  CHECK(inside > 0 && turn > 0);
  czsc_snapshot_free(hn);
  czsc_snapshot_free(hl);
  czsc_snapshot_free(hh);
  czsc_snapshot_free(ho);
  CHECK(czsc_nested_rows(nullptr, nullptr) == nullptr);
}

// v3：走势完成证据与区间套低级别映射
TEST(ApiMovementCompletion)
{
  Data d = Rising();
  for (int code : {0, 1100})
  {
    Tables t = Build(d, code);
    int trends = 0, ended = 0;
    for (std::size_t i = 0; i < t.m.size(); i++)
    {
      const czsc_movement &m = t.m[i];
      CHECK(m.connectionStart == t.c[static_cast<std::size_t>(m.lastCenter)].lastPivot);
      if (i + 1 < t.m.size())
      {
        const czsc_center &next = t.c[static_cast<std::size_t>(t.m[i + 1].firstCenter)];
        CHECK(m.successor == static_cast<int32_t>(i + 1) && m.connectionEnd == next.firstPivot);
        CHECK(m.successorEstablishedAt == t.p[static_cast<std::size_t>(next.firstPivot) + 3].fractalAt && m.completedAt == m.successorEstablishedAt);
        CHECK(m.successorEstablishedAt >= next.start);
      }
      else
      {
        CHECK(m.successor == -1 && m.completedAt == -1);
      }
      if (m.type != 0) trends++;
      if (m.completedBy >= 0)
      {
        const czsc_signal &s = t.s[static_cast<std::size_t>(m.completedBy)];
        CHECK(m.type != 0 && s.type == (m.type > 0 ? -1 : 1) && s.center == m.lastCenter && s.index == m.completedByIndex);
        ended++;
      }
    }
    if (code == 0) CHECK(trends > 0 && ended > 0);
  }
  czsc_input lo = Input(d, 0), hi = Input(d, 1100);
  void *hl = czsc_snapshot_build(&lo), *hh = czsc_snapshot_build(&hi), *hn = czsc_nested_build(hl, hh);
  REQUIRE(hn != nullptr);
  Tables L = Read(hl), H = Read(hh);
  int mapped = 0;
  for (const czsc_nested &r : Get(czsc_nested_rows, hn))
  {
    if (r.highSignal < 0) { CHECK(r.highCurStartLow == -1 && r.highPrevStartLow == -1); continue; }
    const czsc_signal &h = H.s[static_cast<std::size_t>(r.highSignal)];
    if (r.highCurStartLow >= 0) CHECK(L.p[static_cast<std::size_t>(r.highCurStartLow)].index == H.p[static_cast<std::size_t>(h.divergence.curStart)].index);
    if (r.highCurEndLow >= 0) CHECK(L.p[static_cast<std::size_t>(r.highCurEndLow)].index == h.index);
    mapped += r.highCurStartLow >= 0 && r.highCurEndLow >= 0;
  }
  CHECK(mapped > 0);
  czsc_snapshot_free(hn);
  czsc_snapshot_free(hl);
  czsc_snapshot_free(hh);
}

// v4：递归走势节点
namespace
{
struct Rec
{
  std::vector<czsc_recursive_node> nodes;
  std::vector<int32_t> children;
  std::vector<czsc_movement> movements;
  std::vector<czsc_recursive_center> centers;
  std::vector<czsc_recursive_connection> connections;
};

Rec BuildRec(const Data &d, int config, int flags = CZSC_FLAG_HIGHER)
{
  czsc_input in = Input(d, config, flags);
  void *h = czsc_snapshot_build(&in);
  Rec r;
  if (!h) return r;
  r.nodes = Get(czsc_recursive_nodes, h);
  r.children = Get(czsc_recursive_children, h);
  r.movements = Get(czsc_movements, h);
  r.centers = Get(czsc_recursive_centers, h);
  r.connections = Get(czsc_recursive_connections, h);
  czsc_snapshot_free(h);
  return r;
}

const czsc_recursive_node *Find(const Rec &r, int level, int ordinal)
{
  for (const czsc_recursive_node &n : r.nodes)
    if (n.level == level && n.ordinal == ordinal) return &n;
  return nullptr;
}
}  // namespace

TEST(ApiRecursiveNodes)
{
  Data d = Rising();
  CHECK(BuildRec(d, 0, 0).nodes.empty());  // 未置位1不生成
  Rec r = BuildRec(d, 0);
  int maxLevel = 0;
  std::size_t level0 = 0;
  for (std::size_t i = 0; i < r.nodes.size(); i++)
  {
    const czsc_recursive_node &n = r.nodes[i];
    CHECK(n.size == sizeof(czsc_recursive_node) && n.start <= n.centerStart && n.centerEnd <= n.end + 1);
    maxLevel = std::max(maxLevel, n.level);
    if (n.level == 0)
    {
      const czsc_movement &m = r.movements[static_cast<std::size_t>(n.ordinal)];
      CHECK(n.type == m.type && n.centerStart == m.start && n.centerEnd == m.end && n.childCount == 0);
      level0++;
    }
    else
    {
      REQUIRE(n.childCount > 0);
      int prevEnd = -1;
      for (int k = 0; k < n.childCount; k++)
      {
        const czsc_recursive_node &c = r.nodes[static_cast<std::size_t>(r.children[static_cast<std::size_t>(n.firstChild + k)])];
        CHECK(c.level == n.level - 1 && c.start >= n.start && c.end <= n.end && (prevEnd < 0 || c.start == prevEnd));
        prevEnd = c.end;
      }
      CHECK(n.centerCount >= 1 && (n.type == 0 || n.centerCount >= 2));
    }
    if (n.successor >= 0)
    {
      const czsc_recursive_node &s = r.nodes[static_cast<std::size_t>(n.successor)];
      CHECK(s.level == n.level && s.ordinal == n.ordinal + 1 && s.start == n.end && n.completed == s.established);
    }
  }
  CHECK(level0 == r.movements.size() && maxLevel >= 1);

  // 因果：前缀中已定型的节点与全量中同层同序号的节点逐字段相同
  int compared = 0;
  for (int k = 3000; k < d.n(); k += 1111)
  {
    Data p = d;
    p.h.resize(static_cast<std::size_t>(k) + 1); p.l.resize(p.h.size()); p.c.resize(p.h.size()); p.v.resize(p.h.size());
    Rec pr = BuildRec(p, 0);
    for (const czsc_recursive_node &x : pr.nodes)
    {
      if (x.confirmedAt < 0) continue;
      const czsc_recursive_node *y = Find(r, x.level, x.ordinal);
      REQUIRE(y != nullptr);
      CHECK(x.type == y->type && x.start == y->start && x.end == y->end && x.centerStart == y->centerStart &&
            x.centerEnd == y->centerEnd && x.centerCount == y->centerCount && x.established == y->established &&
            x.connection == y->connection && x.completed == y->completed && x.confirmedAt == y->confirmedAt &&
            x.childCount == y->childCount);
      compared++;
    }
  }
  CHECK(compared > 50);
}

// v5：递归中枢、同级别连接段、中阴
namespace
{
// 成员链：同为 level-1、首尾相接、覆盖 [start, end]
bool Chain(const Rec &r, int level, int first, int count, int start, int end)
{
  if (first < 0 || count <= 0) return false;
  int prevEnd = start;
  for (int k = 0; k < count; k++)
  {
    const czsc_recursive_node &c = r.nodes[static_cast<std::size_t>(first + k)];
    if (c.level != level - 1 || c.start != prevEnd) return false;
    prevEnd = c.end;
  }
  return prevEnd == end;
}

// 下标 → (level, ordinal)，跨前缀比对用
std::pair<int, int> Key(const Rec &r, int index)
{
  if (index < 0) return {-1, -1};
  const czsc_recursive_node &n = r.nodes[static_cast<std::size_t>(index)];
  return {n.level, n.ordinal};
}
}  // namespace

TEST(ApiRecursiveCentersAndConnections)
{
  Data d = Rising();
  CHECK(BuildRec(d, 0, 0).centers.empty() && BuildRec(d, 0, 0).connections.empty());
  std::size_t connections = 0;
  for (int cfg : {0, 2})
  {
    Rec r = BuildRec(d, cfg);
    REQUIRE(!r.centers.empty());
    connections += r.connections.size();
    for (std::size_t i = 0; i < r.centers.size(); i++)
    {
      const czsc_recursive_center &c = r.centers[i];
      CHECK(c.size == sizeof c && c.level >= 1 && c.zd <= c.zg && c.dd <= c.zd && c.zg <= c.gg && c.start < c.end);
      CHECK(c.memberCount >= 3 && Chain(r, c.level, c.firstMember, c.memberCount, c.start, c.end));
      CHECK(c.confirmedAt < 0 || (c.established >= 0 && c.confirmedAt >= c.established));
      if (i > 0 && r.centers[i - 1].level == c.level) CHECK(r.centers[i - 1].ordinal + 1 == c.ordinal);
    }
    int zy = 0;
    for (std::size_t i = 0; i < r.nodes.size(); i++)
    {
      const czsc_recursive_node &n = r.nodes[i];
      CHECK(n.low <= n.high && n.firstCenter >= 0 && n.lastCenter - n.firstCenter + 1 == n.centerCount);
      if (n.level > 0)
      {
        const czsc_recursive_center &f = r.centers[static_cast<std::size_t>(n.firstCenter)];
        const czsc_recursive_center &l = r.centers[static_cast<std::size_t>(n.lastCenter)];
        CHECK(f.level == n.level && f.start == n.centerStart && l.end == n.centerEnd);
        for (int k = 0; k < n.childCount; k++)
        {
          const czsc_recursive_node &c = r.nodes[static_cast<std::size_t>(r.children[static_cast<std::size_t>(n.firstChild + k)])];
          CHECK(c.high <= n.high && c.low >= n.low);
        }
      }
      else
      {
        const czsc_movement &m = r.movements[static_cast<std::size_t>(n.ordinal)];
        CHECK(n.firstCenter == m.firstCenter && n.lastCenter == m.lastCenter && n.zhongyinStart == m.zhongyinStart);
      }
      // 中阴：连接点之后开始，不晚于后继成立（= 完成）结束
      if (n.successor >= 0)
      {
        CHECK(n.zhongyinStart > n.end && (n.completed < 0 || n.zhongyinStart <= n.completed));
        zy += n.completed >= 0;
      }
      else CHECK(n.zhongyinStart == -1);
    }
    CHECK(zy > 0);
    for (const czsc_recursive_connection &c : r.connections)
    {
      const czsc_recursive_node &a = r.nodes[static_cast<std::size_t>(c.left)], &b = r.nodes[static_cast<std::size_t>(c.right)];
      CHECK(c.size == sizeof c && a.level == c.level && b.level == c.level && a.ordinal == c.ordinal && b.ordinal == c.ordinal + 1);
      CHECK(c.start == a.centerEnd && c.end == b.centerStart && c.start < c.end);
      CHECK(Chain(r, c.level, c.firstMember, c.memberCount, c.start, c.end));
    }

    // 因果：前缀中已定型的中枢/连接段与全量中同层同序号者逐字段相同（成员以 (level, ordinal) 比对）
    int compared = 0;
    for (int k = 3000; k < d.n(); k += 1111)
    {
      Data p = d;
      p.h.resize(static_cast<std::size_t>(k) + 1); p.l.resize(p.h.size()); p.c.resize(p.h.size()); p.v.resize(p.h.size());
      Rec pr = BuildRec(p, cfg);
      for (const czsc_recursive_center &x : pr.centers)
      {
        if (x.confirmedAt < 0) continue;
        const czsc_recursive_center *y = nullptr;
        for (const czsc_recursive_center &c : r.centers)
          if (c.level == x.level && c.ordinal == x.ordinal) y = &c;
        REQUIRE(y != nullptr);
        CHECK(x.start == y->start && x.end == y->end && x.zg == y->zg && x.zd == y->zd && x.gg == y->gg && x.dd == y->dd &&
              x.direction == y->direction && x.memberCount == y->memberCount && x.established == y->established &&
              x.confirmedAt == y->confirmedAt && Key(pr, x.firstMember) == Key(r, y->firstMember));
        compared++;
      }
      for (const czsc_recursive_connection &x : pr.connections)
      {
        if (x.confirmedAt < 0) continue;
        const czsc_recursive_connection *y = nullptr;
        for (const czsc_recursive_connection &c : r.connections)
          if (c.level == x.level && c.ordinal == x.ordinal) y = &c;
        REQUIRE(y != nullptr);
        CHECK(x.start == y->start && x.end == y->end && x.memberCount == y->memberCount && x.confirmedAt == y->confirmedAt &&
              Key(pr, x.firstMember) == Key(r, y->firstMember) && Key(pr, x.left) == Key(r, y->left));
        compared++;
      }
      for (const czsc_recursive_node &x : pr.nodes)
      {
        if (x.confirmedAt < 0) continue;
        const czsc_recursive_node *y = Find(r, x.level, x.ordinal);
        REQUIRE(y != nullptr);
        CHECK(x.high == y->high && x.low == y->low && x.zhongyinStart == y->zhongyinStart);
      }
      for (std::size_t i = 0; i < pr.movements.size(); i++)
        if (pr.movements[i].confirmedAt >= 0) CHECK(pr.movements[i].zhongyinStart == r.movements[i].zhongyinStart);
    }
    CHECK(compared > 5);
  }
  CHECK(connections > 0);
}

// v6：中枢成立时刻与均线导出
TEST(ApiCenterEstablishedAndMovingAverages)
{
  Data d = Sse();
  for (int code : {0, 2, 1100})
  {
    Tables t = Build(d, code);
    for (const czsc_center &c : t.c)
    {
      CHECK(c.established == t.p[static_cast<std::size_t>(c.firstPivot) + 3].fractalAt);
      CHECK(c.established >= t.p[static_cast<std::size_t>(c.firstPivot) + 3].index && (c.confirmedAt < 0 || c.confirmedAt >= c.established));
    }
    for (const czsc_movement &m : t.m)
      if (m.successor >= 0) CHECK(m.successorEstablishedAt == t.c[static_cast<std::size_t>(t.m[static_cast<std::size_t>(m.successor)].firstCenter)].established);
  }
  Tables t = Build(d, 0);
  REQUIRE(t.r.size() == static_cast<std::size_t>(d.n()));
  // 与 MovingAverage 同口径：收盘价的 5/20 日均值，前 N-1 根为已有K线均值
  for (int i = 0; i < d.n(); i++)
  {
    double s5 = 0, s20 = 0;
    int w5 = 0, w20 = 0;
    for (int j = i; j >= 0 && j > i - 20; j--)
    {
      if (j > i - 5) { s5 += d.c[static_cast<std::size_t>(j)]; w5++; }
      s20 += d.c[static_cast<std::size_t>(j)];
      w20++;
    }
    const czsc_bar &b = t.r[static_cast<std::size_t>(i)];
    CHECK(std::fabs(b.maShort - s5 / w5) <= 1e-3 * std::fabs(s5 / w5) && std::fabs(b.maLong - s20 / w20) <= 1e-3 * std::fabs(s20 / w20));
  }
}

TEST(ApiConfigOptionsAndAllLegalSse)
{
  CHECK(sizeof(czsc_config_option) == 116 && alignof(czsc_config_option) == 1);
  CHECK(offsetof(czsc_config_option, key) == 20 && offsetof(czsc_config_option, label) == 52 && offsetof(czsc_config_option, lessons) == 84);
  int count = czsc_config_options(nullptr, 0);
  REQUIRE(count > 0);
  std::vector<czsc_config_option> options(static_cast<std::size_t>(count) + 1);
  options.back().size = 123;
  CHECK(czsc_config_options(options.data(), count) == count && options.back().size == 123);
  CHECK(czsc_config_options(options.data(), 1) == 1);
  CHECK(czsc_config_options(nullptr, 1) == 0 && czsc_config_options(options.data(), -1) == 0);
  std::vector<int> codes{0};
  for (int place : {1, 10, 100, 1000})
  {
    std::vector<int> next;
    int defaults = 0;
    for (int i = 0; i < count; ++i)
    {
      const auto &o = options[static_cast<std::size_t>(i)];
      CHECK(o.size == sizeof o && o.key[31] == 0 && o.label[31] == 0 && o.lessons[31] == 0);
      if (!o.original) CHECK(o.lessons[0] == 0);
      if (o.place != place) continue;
      defaults += o.isDefault;
      for (int code : codes) next.push_back(code + place * o.value);
    }
    CHECK(defaults == 1);
    codes = next;
  }
  Data d = Sse();
  for (int code = -1; code <= 10000; ++code)
  {
    bool listed = std::find(codes.begin(), codes.end(), code) != codes.end();
    CHECK(czsc_config_valid(code) == static_cast<int>(listed));
    CHECK(czsc_config_valid(code) == static_cast<int>(chan::Config::Decode(code).has_value()));
    if (!listed) continue;
    czsc_input in = Input(d, code, CZSC_FLAG_EVENTS | CZSC_FLAG_HIGHER);
    void *h = czsc_snapshot_build(&in);
    REQUIRE(h != nullptr);
    Tables t = Read(h);
    CHECK(t.r.size() == d.h.size());
    for (std::size_t i = 1; i < t.p.size(); ++i)
      CHECK(t.p[i].kind == -t.p[i-1].kind && t.p[i].index > t.p[i-1].index);
    for (const auto &c : t.c)
    {
      REQUIRE(c.firstPivot >= 0 && static_cast<std::size_t>(c.firstPivot) + 3 < t.p.size());
      CHECK(c.zd < c.zg && c.established == t.p[static_cast<std::size_t>(c.firstPivot) + 3].fractalAt);
    }
    CHECK(Get(czsc_recursive_nodes, h).size() >= t.m.size());
    for (const auto &c : Get(czsc_recursive_centers, h)) CHECK(c.zd < c.zg);
    Get(czsc_recursive_children, h);
    Get(czsc_recursive_connections, h);
    czsc_snapshot_free(h);
  }
}
