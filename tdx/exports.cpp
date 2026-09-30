#include "tdx/exports.h"

#include "core/engine.h"
#include "core/structure.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

namespace tdx
{
namespace
{

using chan::Analysis;
using chan::Config;
using chan::Series;

//----------------------------------------------------------------------------
// 边界状态：Func40 注册的真实收盘价/成交量，以及分析结果缓存。只存在于适配层。
//----------------------------------------------------------------------------

struct Registered
{
  std::vector<float> close, volume;
} g_registered;

std::uint32_t Fnv(std::uint32_t h, const float *p, int n)
{
  const unsigned char *b = reinterpret_cast<const unsigned char *>(p);
  for (int i = 0; i < n * static_cast<int>(sizeof(float)); i++)
  {
    h ^= b[i];
    h *= 16777619u;
  }
  return h;
}

// 注册的收盘价只在长度一致且逐根落在 [L,H] 内时采用（Series::FromRaw 校验），否则回落 (H+L)/2
Series MakeSeries(int count, const float *high, const float *low)
{
  bool sameLength = static_cast<int>(g_registered.close.size()) == count;
  const float *close = sameLength ? g_registered.close.data() : nullptr;
  const float *volume = sameLength && static_cast<int>(g_registered.volume.size()) == count
                          ? g_registered.volume.data() : nullptr;
  return Series::FromRaw(count, high, low, close, volume);
}

// 同一图上常并用多套配置：4 槽 LRU，键为 (长度, 配置, H/L 与所用 C/V 指纹)
struct Slot
{
  int count = -1;
  int config = -1;
  std::uint32_t hash = 0;
  unsigned tick = 0;
  std::unique_ptr<Analysis> analysis;
};
Slot g_slots[4];
unsigned g_tick = 0;

const Analysis &Analyzed(int count, const float *high, const float *low, const Config &config)
{
  Series s = MakeSeries(count, high, low);
  std::uint32_t h = Fnv(Fnv(2166136261u, high, count), low, count);
  if (s.HasClose()) h = Fnv(h, s.close.data(), count);
  if (!s.volume.empty()) h = Fnv(h, s.volume.data(), count);
  int code = config.Encode();
  g_tick++;
  Slot *victim = &g_slots[0];
  for (Slot &slot : g_slots)
  {
    if (slot.analysis && slot.count == count && slot.config == code && slot.hash == h)
    {
      slot.tick = g_tick;
      return *slot.analysis;
    }
    if (!slot.analysis || (victim->analysis && slot.tick < victim->tick)) victim = &slot;
  }
  victim->analysis = std::make_unique<Analysis>(chan::Analyze(s, config));
  victim->count = count;
  victim->config = code;
  victim->hash = h;
  victim->tick = g_tick;
  return *victim->analysis;
}

//----------------------------------------------------------------------------
// 投影辅助
//----------------------------------------------------------------------------

void Clear(int count, float *out)
{
  for (int i = 0; i < count; i++) out[i] = 0;
}

// 通用入口：校验输入与配置码，非法则输出全 0
template <class Project>
void Run(int count, float *out, float *high, float *low, float *config, Project project)
{
  if (count <= 0 || out == nullptr) return;
  Clear(count, out);
  if (high == nullptr || low == nullptr) return;
  float code = config ? config[0] : 0.0f;
  if (!std::isfinite(code) || code != std::floor(code)) return;
  std::optional<Config> c = Config::Decode(static_cast<int>(code));
  if (!c) return;
  project(Analyzed(count, high, low, *c));
}

bool InRange(int i, int count) { return i >= 0 && i < count; }

// 同一根上多个信号：高优先级（一类>三类>二类）胜出，同优先级后到者胜出
template <class Value>
void WriteEvents(int count, float *out, const std::vector<chan::SignalEvent> &events, bool revoked, Value value)
{
  std::vector<int> priority(static_cast<std::size_t>(count), -1);
  for (const chan::SignalEvent &e : events)
  {
    if (e.revoked != revoked || !InRange(e.bar, count)) continue;
    std::size_t b = static_cast<std::size_t>(e.bar);
    if (e.signal.priority >= priority[b])
    {
      out[b] = value(e.signal);
      priority[b] = e.signal.priority;
    }
  }
}

float Code(const chan::Signal &s) { return static_cast<float>(static_cast<int>(s.type)); }

}  // namespace

void Pivots(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) {
    for (const chan::Pivot &p : a.snapshot.pivots)
      if (InRange(p.index, count)) out[p.index] = static_cast<float>(static_cast<int>(p.kind));
  });
}

void CenterHigh(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) {
    for (const chan::Center &c : a.snapshot.centers)
    {
      // 显示口径：只画最初三笔／三段（第17/18课成枢构件），延伸仍由结构层计算。
      int end = a.snapshot.pivots[static_cast<std::size_t>(c.firstPivot + 3)].index;
      for (int i = std::max(c.start, 0); i <= end && i < count; i++) out[i] = c.zg;
    }
  });
}

void CenterLow(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) {
    for (const chan::Center &c : a.snapshot.centers)
    {
      int end = a.snapshot.pivots[static_cast<std::size_t>(c.firstPivot + 3)].index;
      for (int i = std::max(c.start, 0); i <= end && i < count; i++) out[i] = c.zd;
    }
  });
}

void CenterRelation(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) {
    const std::vector<chan::Center> &c = a.snapshot.centers;
    for (std::size_t i = 1; i < c.size(); i++)
      if (InRange(c[i].start, count)) out[c[i].start] = static_cast<float>(static_cast<int>(chan::Relate(c[i - 1], c[i])));
  });
}

void Signals(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) { WriteEvents(count, out, a.events, false, Code); });
}

void Revokes(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) { WriteEvents(count, out, a.events, true, Code); });
}

void Stops(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) {
    WriteEvents(count, out, a.events, false, [](const chan::Signal &s) { return s.stop; });
  });
}

void Divergence(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) {
    WriteEvents(count, out, a.events, false, [](const chan::Signal &s) {
      const chan::Divergence &d = s.divergence;
      return d.previous.area > 0 ? d.current.area / d.previous.area * 100.0f : 0.0f;
    });
  });
}

void Movements(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) {
    for (const chan::Movement &m : a.snapshot.movements)
      for (int i = std::max(m.start, 0); i <= m.end && i < count; i++) out[i] = static_cast<float>(static_cast<int>(m.type));
  });
}

void Kisses(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) {
    for (int i = 0; i < count && i < static_cast<int>(a.ma.kisses.size()); i++)
      out[i] = static_cast<float>(static_cast<int>(a.ma.kisses[static_cast<std::size_t>(i)]));
  });
}

// 缺口（借鉴 czsc check_gap_info，只输出当下可知部分；“是否回补”需未来K线，不输出）
void Gaps(int count, float *out, float *high, float *low, float *config)
{
  (void)config;
  if (count <= 0 || out == nullptr) return;
  Clear(count, out);
  if (high == nullptr || low == nullptr) return;
  std::vector<int8_t> g = chan::Gaps(Series::FromRaw(count, high, low));
  for (int i = 0; i < count; i++) out[i] = static_cast<float>(g[static_cast<std::size_t>(i)]);
}

void FractalStrength(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) {
    std::vector<int8_t> f = chan::FractalStrengths(MakeSeries(count, high, low), a.bars, a.fractals);
    for (int i = 0; i < count; i++) out[i] = static_cast<float>(f[static_cast<std::size_t>(i)]);
  });
}

void HindsightSignals(int count, float *out, float *high, float *low, float *config)
{
  Run(count, out, high, low, config, [&](const Analysis &a) {
    std::vector<int> priority(static_cast<std::size_t>(count), -1);
    for (const chan::Signal &s : a.snapshot.signals)
    {
      if (!InRange(s.index, count)) continue;
      std::size_t b = static_cast<std::size_t>(s.index);
      if (s.priority >= priority[b])
      {
        out[b] = Code(s);
        priority[b] = s.priority;
      }
    }
  });
}

void RegisterCloseVolume(int count, float *out, float *close, float *volume, float *unused)
{
  (void)unused;
  if (count <= 0 || close == nullptr)
  {
    g_registered = Registered();
  }
  else
  {
    g_registered.close = chan::Sanitize(count, close);
    g_registered.volume = volume ? chan::Sanitize(count, volume) : std::vector<float>();
  }
  if (count <= 0 || out == nullptr) return;
  for (int i = 0; i < count; i++) out[i] = close ? close[i] : 0.0f;  // 透传收盘价，便于公式写成 XC:=TDXDLL1(40,C,V,0)
}

void ResetForTesting()
{
  g_registered = Registered();
  for (Slot &slot : g_slots) slot = Slot();
  g_tick = 0;
}

}  // namespace tdx
