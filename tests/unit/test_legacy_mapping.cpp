#include "legacy_api_bridge.h"
#include "migration/legacy_config.h"
#include "check.h"
#include "adapter/czsc_api.h"
#include "core/config.h"
#include "sse_data.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace
{
void Mix(std::uint64_t &hash, const void *data, std::size_t size)
{
  const auto *bytes = static_cast<const unsigned char *>(data);
  for (std::size_t i=0; i<size; ++i) { hash ^= bytes[i]; hash *= 1099511628211ULL; }
}
template<class T>
void HashTable(std::uint64_t &hash, const T *(*get)(void *,int32_t *), void *handle)
{
  int32_t count=0;
  const auto *rows=get(handle,&count);
  Mix(hash,&count,sizeof count);
  Mix(hash,rows,sizeof(T)*count);
}
std::uint64_t SnapshotHash(void *handle)
{
  std::uint64_t hash=14695981039346656037ULL;
  HashTable(hash,legacy_test::Pivots,handle); HashTable(hash,legacy_test::Centers,handle);
  HashTable(hash,legacy_test::Movements,handle); HashTable(hash,legacy_test::Breakouts,handle);
  HashTable(hash,legacy_test::Signals,handle); HashTable(hash,legacy_test::EventsTable,handle); HashTable(hash,legacy_test::Bars,handle);
  HashTable(hash,legacy_test::RecursiveNodes,handle); HashTable(hash,legacy_test::RecursiveChildren,handle);
  HashTable(hash,legacy_test::RecursiveCenters,handle); HashTable(hash,legacy_test::RecursiveConnections,handle);
  return hash;
}
legacy_test::Input SseInput(int code)
{
  legacy_test::Input in{};
  in.size=sizeof in; in.n=SSE_DAILY_COUNT;
  in.high=SSE_DAILY_HIGH; in.low=SSE_DAILY_LOW; in.close=SSE_DAILY_CLOSE; in.volume=SSE_DAILY_VOLUME;
  in.config=code; in.flags=3;
  return in;
}
std::string Fixture()
{
  std::string here=__FILE__;
  return here.substr(0,here.find_last_of("/\\"))+"/../fixtures/legacy-v10-sse-hashes.txt";
}
}

TEST(LegacyV10CBytesAll240Mappings)
{
  std::ifstream file(Fixture());
  REQUIRE(file.good());
  std::string line;
  int checked=0;
  std::map<int,std::uint64_t> corrected;
  auto correctedPath=Fixture()+".bounded-fixed";
  std::ifstream fixed(correctedPath);
  int fixedCode; std::uint64_t fixedHash;
  while (fixed>>fixedCode>>std::hex>>fixedHash>>std::dec) corrected[fixedCode]=fixedHash;
  bool update=std::getenv("CHAN_UPDATE_BOUNDED_HASHES")!=nullptr;
  std::ofstream output;
  if (update) output.open(correctedPath);
  std::uint64_t nestedHash=0;
  while (std::getline(file,line))
  {
    if (line.empty() || line[0]=='#') continue;
    std::istringstream in(line);
    std::string code; std::uint64_t expected=0;
    in>>code>>std::hex>>expected;
    if (code=="nested") { nestedHash=expected; continue; }
    int value=std::stoi(code);
    auto mapped=migration::MapLegacyConfig(value);
    REQUIRE(mapped.has_value());
    CHECK(migration::LegacyCode(*mapped)==value);
    CHECK(mapped->analysis.stroke.gap==chan::GapRule::None);
    CHECK(chan::Validate(mapped->analysis).empty());
    auto input=SseInput(value);
    void *handle=legacy_test::Build(&input);
    REQUIRE(handle!=nullptr);
    auto actual=SnapshotHash(handle);
    // 保留旧v10冻结文件；仅bounded错误历史使用独立的修复后基线。
    if (mapped->analysis.stroke.endpoint==chan::StrokeEnd::Bounded)
    {
      if (update) { output<<value<<" "<<std::hex<<actual<<std::dec<<"\n"; expected=actual; }
      else { REQUIRE(corrected.count(value)==1); expected=corrected.at(value); }
    }
    if (actual!=expected) std::printf("  legacy code %d bytes changed\n",value);
    CHECK(actual==expected);
    legacy_test::Free(handle);
    ++checked;
  }
  CHECK(checked==240);
  auto lo=SseInput(0),hi=SseInput(1100);
  void *low=legacy_test::Build(&lo),*high=legacy_test::Build(&hi);
  REQUIRE(low && high);
  void *nested=legacy_test::Nested(low,high);
  REQUIRE(nested!=nullptr);
  std::uint64_t actual=14695981039346656037ULL;
  HashTable(actual,legacy_test::NestedRows,nested);
  CHECK(actual==nestedHash);
  legacy_test::Free(nested); legacy_test::Free(low); legacy_test::Free(high);
  // 负对照：已知一个业务字节改变，表示级回归必须能发现。
  czsc_pivot original{}; original.index=10;
  auto mutated=original; mutated.index=11;
  std::uint64_t a=14695981039346656037ULL,b=a;
  Mix(a,&original,sizeof original); Mix(b,&mutated,sizeof mutated);
  CHECK(a!=b);
}

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
