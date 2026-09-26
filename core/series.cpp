#include "series.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace chan
{

bool IsTdxInvalid(float value)
{
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof bits);
  return bits == 0xF8F8F8F8u;
}

std::vector<float> Sanitize(int count, const float *values)
{
  std::vector<float> out;
  if (count <= 0 || values == nullptr)
  {
    return out;
  }
  out.resize(static_cast<std::size_t>(count));
  float last = 0;
  bool any = false;
  for (int i = 0; i < count && !any; i++)
  {
    if (!IsTdxInvalid(values[i]))
    {
      last = values[i];
      any = true;
    }
  }
  for (int i = 0; i < count; i++)
  {
    if (!IsTdxInvalid(values[i]))
    {
      last = values[i];
    }
    out[static_cast<std::size_t>(i)] = any ? last : 0;
  }
  return out;
}

Series Series::FromRaw(int count, const float *high, const float *low, const float *close, const float *volume)
{
  Series s;
  if (count <= 0 || high == nullptr || low == nullptr)
  {
    return s;
  }
  s.high = Sanitize(count, high);
  s.low = Sanitize(count, low);
  if (close != nullptr)
  {
    std::vector<float> c = Sanitize(count, close);
    bool valid = true;
    for (int i = 0; i < count && valid; i++)
    {
      if (IsTdxInvalid(high[i]) || IsTdxInvalid(low[i]))
      {
        continue;
      }
      float tol = (std::fabs(high[i]) + 1.0f) * 0.001f;
      float v = c[static_cast<std::size_t>(i)];
      valid = (v >= low[i] - tol) && (v <= high[i] + tol);
    }
    if (valid)
    {
      s.close = std::move(c);
      if (volume != nullptr)
      {
        s.volume = Sanitize(count, volume);
      }
    }
  }
  return s;
}

}  // namespace chan
