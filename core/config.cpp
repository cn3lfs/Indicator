#include "config.h"

namespace chan
{

int Config::Encode() const
{
  return static_cast<int>(stroke) + static_cast<int>(strokeEnd) * 10 +
         static_cast<int>(unit) * 100 + static_cast<int>(segment) * 1000 + static_cast<int>(segmentEnd) * 10000 + static_cast<int>(centerFormation) * 100000 + (innerBounded ? 1000000 : 0);
}

std::optional<Config> Config::Decode(int code)
{
  if (code < 0 || code > 1129999)
  {
    return std::nullopt;
  }
  int d0 = code % 10, d1 = (code / 10) % 10, d2 = (code / 100) % 10, d3 = (code / 1000) % 10;
  int d4 = (code / 10000) % 10, d5 = (code / 100000) % 10, d6 = code / 1000000;
  if (d6 > 1 || (d6 == 1 && d1 == 1) || d5 > 1 || d4 > 2 || (d3 == 0 && d4 != 0) || d0 > 4 || d1 > 1 || d2 > 1 || d3 > 1)
  {
    return std::nullopt;
  }
  Config c;
  c.stroke = static_cast<StrokeRule>(d0);
  c.strokeEnd = static_cast<StrokeEnd>(d1);
  c.unit = static_cast<CenterUnit>(d2);
  c.segment = static_cast<SegmentMethod>(d3);
  c.segmentEnd = static_cast<SegmentEnd>(d4);
  c.centerFormation = static_cast<CenterFormation>(d5);
  c.innerBounded = d6 == 1;
  return c;
}

}  // namespace chan
