// 通达信适配层：投影与引擎结果一致、非法输入、旁路 C/V、缓存。
#include "check.h"
#include "sse_data.h"
#include "core/engine.h"
#include "tdx/exports.h"

#include <vector>

namespace
{
struct Sse
{
  std::vector<float> h{SSE_DAILY_HIGH, SSE_DAILY_HIGH + SSE_DAILY_COUNT};
  std::vector<float> l{SSE_DAILY_LOW, SSE_DAILY_LOW + SSE_DAILY_COUNT};
  int n = SSE_DAILY_COUNT;
};

std::vector<float> Call(void (*f)(int, float *, float *, float *, float *), Sse &s, float code)
{
  std::vector<float> out(static_cast<std::size_t>(s.n), -9.0f), cfg(static_cast<std::size_t>(s.n), code);
  f(s.n, out.data(), s.h.data(), s.l.data(), cfg.data());
  return out;
}

int NonZero(const std::vector<float> &v)
{
  int n = 0;
  for (float x : v) n += x != 0.0f;
  return n;
}
}  // namespace

TEST(TdxRejectsInvalidConfig)
{
  tdx::ResetForTesting();
  Sse s;
  for (float bad : {3.0f, 20.0f, -1.0f, 0.5f, 99999.0f})
    CHECK(NonZero(Call(tdx::Pivots, s, bad)) == 0);
  std::vector<float> out(4, 7.0f);
  tdx::Pivots(4, out.data(), nullptr, nullptr, nullptr);
  CHECK(out[0] == 0.0f);  // 缺 H/L：清零
}

TEST(TdxProjectionsMatchEngine)
{
  tdx::ResetForTesting();
  Sse s;
  for (float code : {0.0f, 2.0f, 1100.0f})
  {
    chan::Analysis a = chan::Analyze(chan::Series::FromRaw(s.n, s.h.data(), s.l.data()), *chan::Config::Decode(static_cast<int>(code)));
    std::vector<float> pivots = Call(tdx::Pivots, s, code);
    CHECK(NonZero(pivots) == static_cast<int>(a.snapshot.pivots.size()));
    for (const chan::Pivot &p : a.snapshot.pivots) CHECK(pivots[static_cast<std::size_t>(p.index)] == static_cast<float>(static_cast<int>(p.kind)));

    std::vector<float> sig = Call(tdx::Signals, s, code), rev = Call(tdx::Revokes, s, code), stop = Call(tdx::Stops, s, code);
    for (const chan::SignalEvent &e : a.events)
    {
      std::size_t b = static_cast<std::size_t>(e.bar);
      CHECK((e.revoked ? rev[b] : sig[b]) != 0.0f);
      if (!e.revoked) CHECK(stop[b] != 0.0f);
    }
    std::vector<float> zg = Call(tdx::CenterHigh, s, code), zd = Call(tdx::CenterLow, s, code);
    for (const chan::Center &c : a.snapshot.centers)
      CHECK(zg[static_cast<std::size_t>(c.start)] == c.zg && zd[static_cast<std::size_t>(c.end)] == c.zd);
  }
  // SSE 笔级：2021-01-25 一卖当下出现后失效
  std::vector<float> rev = Call(tdx::Revokes, s, 0.0f);
  CHECK(NonZero(rev) >= 3);
}

TEST(TdxCacheKeepsConfigsApart)
{
  tdx::ResetForTesting();
  Sse s;
  std::vector<float> a1 = Call(tdx::Pivots, s, 0.0f), b1 = Call(tdx::Pivots, s, 1100.0f);
  std::vector<float> a2 = Call(tdx::Pivots, s, 0.0f), b2 = Call(tdx::Pivots, s, 1100.0f);
  CHECK(a1 == a2 && b1 == b2 && NonZero(a1) > NonZero(b1));
}

TEST(TdxRegisteredCloseChangesDynamicsOnly)
{
  tdx::ResetForTesting();
  Sse s;
  std::vector<float> close(SSE_DAILY_CLOSE, SSE_DAILY_CLOSE + SSE_DAILY_COUNT), vol(SSE_DAILY_VOLUME, SSE_DAILY_VOLUME + SSE_DAILY_COUNT);
  std::vector<float> proxyPivots = Call(tdx::Pivots, s, 0.0f), proxyDiv = Call(tdx::Divergence, s, 0.0f);
  std::vector<float> echo(static_cast<std::size_t>(s.n));
  tdx::RegisterCloseVolume(s.n, echo.data(), close.data(), vol.data(), nullptr);
  CHECK(echo == close);
  std::vector<float> realPivots = Call(tdx::Pivots, s, 0.0f), realDiv = Call(tdx::Divergence, s, 0.0f);
  CHECK(realPivots == proxyPivots);  // 形态只看 H/L
  CHECK(realDiv != proxyDiv);        // MACD 改用真实收盘价
  tdx::RegisterCloseVolume(0, nullptr, nullptr, nullptr, nullptr);  // 撤销
  CHECK(Call(tdx::Divergence, s, 0.0f) == proxyDiv);
  tdx::ResetForTesting();
}

// 缺口/分型强弱取值，且前缀输出与全量逐位一致（无未来函数）
TEST(TdxGapsAndFractalStrength)
{
  tdx::ResetForTesting();
  float h[6] = {5, 9, 6, 5, 2, 3}, l[6] = {4, 6, 3, 4, 1, 2}, g[6], f[6], zero[6] = {0};
  tdx::Gaps(6, g, h, l, zero);
  tdx::FractalStrength(6, f, h, l, zero);
  CHECK(g[1] == 1 && g[4] == -1 && g[2] == 0 && f[2] == 2);

  Sse s;
  std::vector<float> fullG = Call(tdx::Gaps, s, 0.0f), fullF = Call(tdx::FractalStrength, s, 0.0f);
  CHECK(NonZero(fullG) > 0 && NonZero(fullF) > 0);
  for (int t = 60; t <= s.n; t += 111)
  {
    std::vector<float> pg(static_cast<std::size_t>(t)), pf(static_cast<std::size_t>(t)), cfg(static_cast<std::size_t>(t), 0.0f);
    tdx::Gaps(t, pg.data(), s.h.data(), s.l.data(), cfg.data());
    tdx::FractalStrength(t, pf.data(), s.h.data(), s.l.data(), cfg.data());
    for (int i = 0; i < t; i++) CHECK(pg[static_cast<std::size_t>(i)] == fullG[static_cast<std::size_t>(i)] && pf[static_cast<std::size_t>(i)] == fullF[static_cast<std::size_t>(i)]);
  }
}
