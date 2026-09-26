#include "config.h"

namespace chan
{

int Config::Encode() const
{
  return static_cast<int>(stroke) + static_cast<int>(strokeEnd) * 10 +
         static_cast<int>(unit) * 100 + static_cast<int>(segment) * 1000;
}

std::optional<Config> Config::Decode(int code)
{
  if (code < 0 || code > 9999)
  {
    return std::nullopt;
  }
  int d0 = code % 10, d1 = (code / 10) % 10, d2 = (code / 100) % 10, d3 = (code / 1000) % 10;
  if (d0 > 2 || d1 > 1 || d2 > 1 || d3 > 1)
  {
    return std::nullopt;
  }
  Config c;
  c.stroke = static_cast<StrokeRule>(d0);
  c.strokeEnd = static_cast<StrokeEnd>(d1);
  c.unit = static_cast<CenterUnit>(d2);
  c.segment = static_cast<SegmentMethod>(d3);
  return c;
}

}  // namespace chan
