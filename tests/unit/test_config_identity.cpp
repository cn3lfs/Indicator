#include "check.h"
#include "core/config.h"

#include <string>
#include <cstring>

TEST(LayeredConfigIdentityIgnoresOutputsAndProjection)
{
  chan::LevelConfig a,b;
  b.outputs.events=false; b.outputs.levels=1; b.outputs.nested=false;
  b.projection.segmentBoundary=chan::SegmentEnd::Last;
  b.projection.centerBox=chan::CenterBox::Initial;
  b.level=chan::CenterUnit::Segment;
  CHECK(chan::AnalysisId(a.analysis)==chan::AnalysisId(b.analysis));
  b.analysis.signals.publication=chan::SignalPublication::Early;
  CHECK(chan::AnalysisId(a.analysis)!=chan::AnalysisId(b.analysis));
  a.analysis.stroke.gapThreshold=0.25f;
  CHECK(chan::AnalysisId(a.analysis)==chan::AnalysisId(chan::AnalysisConfig{}));
  a.analysis.stroke.endpoint=chan::StrokeEnd::Bounded;
  CHECK(chan::Validate(a.analysis).empty());
  CHECK(chan::AnalysisId(a.analysis).find("endpoint=bounded")!=std::string::npos);
  a.analysis.stroke.gapThreshold=0;
  CHECK(!chan::Validate(a.analysis).empty());
}

TEST(ConfigDecimalIdentityRetainsNineDigitRoundTrip)
{
  chan::AnalysisConfig config;config.stroke.gap=chan::GapRule::Large;
  std::uint32_t bits=1;
  int checked=0;
  for (int i=0;i<20000;++i)
  {
    bits=bits*1664525U+1013904223U;
    float value;std::memcpy(&value,&bits,sizeof value);
    if (!(value>0 && value<1)) continue;
    config.stroke.gapThreshold=value;
    auto id=chan::AnalysisId(config);
    chan::AnalysisConfig parsed;
    REQUIRE(chan::ParseAnalysisId(id,parsed).empty());
    CHECK(parsed.stroke.gapThreshold==value);
    char decimal[128];std::snprintf(decimal,sizeof decimal,"%.9g",static_cast<double>(value));
    CHECK(id.find(std::string("stroke.gapThreshold=")+decimal+";")!=std::string::npos);
    ++checked;
  }
  CHECK(checked>4000);
  for (const char *value:{"0.02junk","1e","--.02","nan","inf","1e1000",".02 "})
    CHECK(!chan::ApplyAnalysisField(config,"stroke.gapThreshold",value).empty());
  CHECK(chan::ApplyAnalysisField(config,"stroke.gapThreshold"," +2E-2").empty());
  CHECK(config.stroke.gapThreshold==.02f);
}
