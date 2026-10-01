#include "check.h"
#include "adapter/czsc_api.h"
#include "core/config.h"
#include "sse_data.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

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
  HashTable(hash,czsc_pivots,handle); HashTable(hash,czsc_centers,handle);
  HashTable(hash,czsc_movements,handle); HashTable(hash,czsc_breakouts,handle);
  HashTable(hash,czsc_signals,handle); HashTable(hash,czsc_events,handle); HashTable(hash,czsc_bars,handle);
  HashTable(hash,czsc_recursive_nodes,handle); HashTable(hash,czsc_recursive_children,handle);
  HashTable(hash,czsc_recursive_centers,handle); HashTable(hash,czsc_recursive_connections,handle);
  return hash;
}
czsc_input SseInput(int code)
{
  czsc_input in{};
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
  std::uint64_t nestedHash=0;
  while (std::getline(file,line))
  {
    if (line.empty() || line[0]=='#') continue;
    std::istringstream in(line);
    std::string code; std::uint64_t expected=0;
    in>>code>>std::hex>>expected;
    if (code=="nested") { nestedHash=expected; continue; }
    int value=std::stoi(code);
    auto mapped=chan::Config::Decode(value);
    REQUIRE(mapped.has_value());
    CHECK(mapped->Encode()==value);
    CHECK(mapped->analysis.stroke.gap==chan::GapRule::None);
    CHECK(chan::Validate(mapped->analysis).empty());
    auto input=SseInput(value);
    void *handle=czsc_snapshot_build(&input);
    REQUIRE(handle!=nullptr);
    auto actual=SnapshotHash(handle);
    if (actual!=expected) std::printf("  legacy code %d bytes changed\n",value);
    CHECK(actual==expected);
    czsc_snapshot_free(handle);
    ++checked;
  }
  CHECK(checked==240);
  auto lo=SseInput(0),hi=SseInput(1100);
  void *low=czsc_snapshot_build(&lo),*high=czsc_snapshot_build(&hi);
  REQUIRE(low && high);
  void *nested=czsc_nested_build(low,high);
  REQUIRE(nested!=nullptr);
  std::uint64_t actual=14695981039346656037ULL;
  HashTable(actual,czsc_nested_rows,nested);
  CHECK(actual==nestedHash);
  czsc_snapshot_free(nested); czsc_snapshot_free(low); czsc_snapshot_free(high);
  // 负对照：已知一个业务字节改变，表示级回归必须能发现。
  czsc_pivot original{}; original.index=10;
  auto mutated=original; mutated.index=11;
  std::uint64_t a=14695981039346656037ULL,b=a;
  Mix(a,&original,sizeof original); Mix(b,&mutated,sizeof mutated);
  CHECK(a!=b);
}

TEST(LayeredConfigIdentityIgnoresOutputsAndProjection)
{
  chan::Config a,b;
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
