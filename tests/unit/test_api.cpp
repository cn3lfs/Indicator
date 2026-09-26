// 结构化接口（adapter/czsc_api.h）验收：docs/nextjs-quant-adapter.md §7。
#include "check.h"
#include "sse_data.h"
#include "adapter/czsc_api.h"
#include "core/engine.h"
#include "tdx/exports.h"

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
  CHECK(czsc_api_version() == CZSC_API_VERSION);
  // 全部为 4 字节字段、无填充：逐字节确定
  CHECK(sizeof(czsc_pivot) == 6 * 4 && sizeof(czsc_center) == 13 * 4 && sizeof(czsc_movement) == 7 * 4);
  CHECK(sizeof(czsc_divergence) == 17 * 4 && sizeof(czsc_breakout) == 7 * 4 + sizeof(czsc_divergence));
  CHECK(sizeof(czsc_event) == 4 * 4 && sizeof(czsc_bar) == 8 * 4 && sizeof(czsc_nested) == 9 * 4);
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
        if (pre.m[i].confirmedAt >= 0) CHECK(std::memcmp(&pre.m[i], &all.m[i], sizeof(czsc_movement)) == 0);
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
  for (int bad : {3, 20, 200, 2000, 9999, -1}) { czsc_input x = in; x.config = bad; CHECK(rejects(x)); }
  { czsc_input x = in; x.size = 8; CHECK(rejects(x)); }
  { czsc_input x = in; x.n = -1; CHECK(rejects(x)); }
  { czsc_input x = in; x.n = 16777217; CHECK(rejects(x)); }
  { czsc_input x = in; x.close = nullptr; CHECK(rejects(x)); }
  { czsc_input x = in; x.flags = CZSC_FLAG_HIGHER; CHECK(rejects(x)); }
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
