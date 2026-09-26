#include "core/dynamics.h"

#include <cmath>

namespace chan
{
namespace
{

// 标准 EMA：e[i] = p[i]*k + e[i-1]*(1-k)，k=2/(n+1)，e[0]=p[0]
std::vector<float> Ema(const std::vector<float> &p, int period)
{
  std::vector<float> e(p.size());
  if (p.empty()) return e;
  float k = 2.0f / static_cast<float>(period + 1);
  e[0] = p[0];
  for (std::size_t i = 1; i < p.size(); i++) e[i] = p[i] * k + e[i - 1] * (1.0f - k);
  return e;
}

const int kShortMa = 5;
const int kLongMa = 20;
const float kLipThreshold = 0.01f;  // 唇吻：短长均线相对间距 < 1%（原文未给阈值，见 docs）
const int kVolumeMa = 10;
const int kTrapLookback = 3;
const float kTrapRatio = 1.5f;

}  // namespace

EnergyTables BuildEnergyTables(const Series &s)
{
  EnergyTables t;
  int n = s.Size();
  if (n <= 0) return t;
  std::vector<float> price(static_cast<std::size_t>(n));
  for (int i = 0; i < n; i++) price[static_cast<std::size_t>(i)] = s.PriceAt(i);
  std::vector<float> fast = Ema(price, 12), slow = Ema(price, 26);
  t.dif.resize(price.size());
  for (std::size_t i = 0; i < price.size(); i++) t.dif[i] = fast[i] - slow[i];
  t.dea = Ema(t.dif, 9);
  t.cumulative.resize(price.size());
  t.red.resize(price.size());
  t.green.resize(price.size());
  float acc = 0, red = 0, green = 0;
  for (std::size_t i = 0; i < price.size(); i++)
  {
    float bar = (t.dif[i] - t.dea[i]) * 2.0f;
    acc += bar;
    if (bar > 0) red += bar; else green -= bar;
    t.cumulative[i] = acc;
    t.red[i] = red;
    t.green[i] = green;
  }
  return t;
}

void AssignEnergy(std::vector<Pivot> &pivots, const EnergyTables &t)
{
  for (Pivot &p : pivots)
  {
    if (p.index < 0 || p.index >= static_cast<int>(t.cumulative.size())) continue;
    std::size_t i = static_cast<std::size_t>(p.index);
    p.energy = t.cumulative[i];
    p.energyRed = t.red[i];
    p.energyGreen = t.green[i];
    p.dif = t.dif[i];
    p.dea = t.dea[i];
  }
}

Strength MeasureStrength(const Pivot &a, const Pivot &b)
{
  Strength s;
  s.space = std::fabs(b.Price() - a.Price());
  int span = std::abs(b.index - a.index);
  s.speed = s.space / static_cast<float>(span == 0 ? 1 : span);
  bool colored = a.energyRed != 0 || a.energyGreen != 0 || b.energyRed != 0 || b.energyGreen != 0;
  if (colored)
    s.area = b.Price() > a.Price() ? b.energyRed - a.energyRed : b.energyGreen - a.energyGreen;
  else
    s.area = b.energy - a.energy;  // 无红绿数据（手工构造的点）回落为代数差
  s.area = std::fabs(s.area);
  return s;
}

Divergence MeasureDivergence(const Pivot &ps, const Pivot &pe, const Pivot &cs, const Pivot &ce, int dir)
{
  Divergence d;
  d.previous = MeasureStrength(ps, pe);
  d.current = MeasureStrength(cs, ce);
  d.weakSpace = d.current.space < d.previous.space;
  d.weakSpeed = d.current.speed < d.previous.speed;
  d.weakArea = d.current.area > 0 && d.previous.area > 0 && d.current.area < d.previous.area;
  d.newExtreme = dir > 0 ? ce.high > pe.high : (dir < 0 ? ce.low < pe.low : false);
  d.holds = d.newExtreme && ((d.weakSpace && d.weakSpeed) || d.weakArea);
  return d;
}

std::vector<float> MovingAverage(const std::vector<float> &price, int period)
{
  std::vector<float> ma(price.size());
  float sum = 0;
  for (std::size_t i = 0; i < price.size(); i++)
  {
    sum += price[i];
    if (i >= static_cast<std::size_t>(period)) sum -= price[i - static_cast<std::size_t>(period)];
    int window = static_cast<int>(i) + 1 < period ? static_cast<int>(i) + 1 : period;
    ma[i] = sum / static_cast<float>(window);
  }
  return ma;
}

// 在短长均线间距的局部极小处判一次吻：穿越=湿吻，贴近不破=唇吻，其余=飞吻（第11课）；
// 有成交量时，湿吻当根及前 2 根均量超过量均线 1.5 倍为放量骗线嫌疑（第12课）
std::vector<Kiss> ClassifyKisses(const std::vector<float> &s, const std::vector<float> &l, const std::vector<float> &v)
{
  std::size_t n = s.size();
  std::vector<Kiss> k(n, Kiss::None);
  if (n < 3 || l.size() != n) return k;
  for (std::size_t i = 1; i + 1 < n; i++)
  {
    float prev = s[i - 1] - l[i - 1], cur = s[i] - l[i], next = s[i + 1] - l[i + 1];
    if (!(std::fabs(cur) < std::fabs(prev) && std::fabs(cur) <= std::fabs(next))) continue;
    bool cross = ((prev > 0) != (cur > 0)) || ((cur > 0) != (next > 0));
    float base = std::fabs(l[i]);
    float rel = base > 0 ? std::fabs(cur) / base : std::fabs(cur);
    k[i] = cross ? Kiss::Wet : (rel < kLipThreshold ? Kiss::Lip : Kiss::Fly);
  }
  if (v.size() != n) return k;
  std::vector<float> vma = MovingAverage(v, kVolumeMa);
  for (std::size_t i = 0; i < n; i++)
  {
    if (k[i] != Kiss::Wet) continue;
    float recent = 0;
    int look = 0;
    for (int j = static_cast<int>(i); j >= 0 && j > static_cast<int>(i) - kTrapLookback; j--)
    {
      recent += v[static_cast<std::size_t>(j)];
      look++;
    }
    recent /= look > 0 ? static_cast<float>(look) : 1.0f;
    if (vma[i] > 0 && recent > vma[i] * kTrapRatio) k[i] = Kiss::WetTrap;
  }
  return k;
}

MovingAverages BuildMovingAverages(const Series &s)
{
  MovingAverages m;
  std::vector<float> price(static_cast<std::size_t>(s.Size()));
  for (int i = 0; i < s.Size(); i++) price[static_cast<std::size_t>(i)] = s.PriceAt(i);
  m.shortMa = MovingAverage(price, kShortMa);
  m.longMa = MovingAverage(price, kLongMa);
  m.kisses = ClassifyKisses(m.shortMa, m.longMa, s.volume);
  return m;
}

}  // namespace chan
