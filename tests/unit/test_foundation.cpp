#include "check.h"
#include "core/config.h"
#include "core/series.h"

#include <cstring>

using namespace chan;

TEST(ConfigRoundTrip)
{
  for (int code : {0, 1, 2, 10, 11, 100, 1100, 1101, 1102, 1111})
  {
    auto c = Config::Decode(code);
    REQUIRE(c.has_value());
    CHECK(c->Encode() == code);
  }
  CHECK(!Config::Decode(5).has_value());     // 个位笔类型只允许 0/1/2/3/4
  CHECK(!Config::Decode(20).has_value());
  CHECK(!Config::Decode(-1).has_value());
  CHECK(!Config::Decode(10000).has_value());
  CHECK(Config::Decode(0)->analysis.stroke.rule == StrokeRule::Strict);
  CHECK(Config::Decode(2)->analysis.stroke.rule == StrokeRule::Czsc);
}

TEST(SeriesSanitizesTdxInvalid)
{
  float bad;
  std::uint32_t bits = 0xF8F8F8F8u;
  std::memcpy(&bad, &bits, sizeof bad);
  float h[5] = {bad, 10, bad, 12, 11};
  float l[5] = {bad, 8, 9, bad, 10};
  Series s = Series::FromRaw(5, h, l);
  CHECK(s.Size() == 5);
  CHECK(s.high[0] == 10 && s.high[2] == 10 && s.high[3] == 12);  // 开头用首个有效值，其后向前填充
  CHECK(s.low[3] == 9);
  CHECK(!s.HasClose());
  CHECK(s.PriceAt(1) == 9.0f);
}

TEST(SeriesRejectsMismatchedClose)
{
  float h[3] = {10, 11, 12}, l[3] = {9, 10, 11};
  float good[3] = {9.5f, 10.5f, 11.5f}, bad[3] = {9.5f, 20.0f, 11.5f};
  CHECK(Series::FromRaw(3, h, l, good).HasClose());
  CHECK(!Series::FromRaw(3, h, l, bad).HasClose());  // 越界收盘价视为错配，回落 (H+L)/2
}

TEST(SegmentEndConfigRoundTrip)
{
  int count = 0;
  for (int code = 0; code < 30000; ++code)
  {
    auto c = Config::Decode(code);
    if (!c) continue;
    CHECK(c->Encode() == code);
    CHECK(c->projection.segmentBoundary == SegmentEnd::Extreme || c->analysis.segment.method == SegmentMethod::Feature);
    ++count;
  }
  CHECK(count == 80);
  CHECK(!Config::Decode(10000) && !Config::Decode(20100) && !Config::Decode(31000));
  CHECK(Config::Decode(11100) && Config::Decode(21100));
  CHECK(!(*Config::Decode(1100) == *Config::Decode(11100)));
}

TEST(CenterFormationConfigRoundTrip)
{
  int count = 0;
  for (int code = 0; code < 130000; ++code)
  {
    auto c = Config::Decode(code);
    if (!c) continue;
    CHECK(c->Encode() == code);
    ++count;
  }
  CHECK(count == 160 && !Config::Decode(200000));
  CHECK(Config::Decode(100000)->analysis.center.strokeFormation == CenterFormation::Segment);
}
