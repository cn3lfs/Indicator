#include "migration/legacy_config.h"
// 社区快速事件独立回归，不改既有golden。
#include "check.h"
#include "sse_data.h"
#include "core/engine.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
TEST(GoldenEarlySignalsSse)
{
  auto s = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW, SSE_DAILY_CLOSE, SSE_DAILY_VOLUME);
  chan::LevelConfig c; c.analysis.signals.publication = chan::SignalPublication::Early;
  auto a = chan::Analyze(s, c);
  std::ostringstream out;
  int count = 0, revoked = 0, sum = 0;
  std::vector<int> lags;
  for (const auto &e : a.events)
  {
    out << e.bar << " " << e.signal.index << " " << static_cast<int>(e.signal.type) << " " << e.revoked << " " << e.signal.stop << "\n";
    if (e.revoked) ++revoked;
    else { ++count; sum += e.bar - e.signal.index; lags.push_back(e.bar - e.signal.index); }
  }
  std::printf("  early SSE appearances %d revokes %d mean lag %.2f bars\n", count, revoked, count ? double(sum) / count : 0);
  const char *path = "tests/fixtures/early-signals-sse.txt";
  if (std::getenv("CHAN_UPDATE_EARLY_GOLDEN")) { std::ofstream f(path); f << out.str(); }
  std::ifstream f(path); REQUIRE(f.good());
  std::ostringstream expected; expected << f.rdbuf();
  CHECK(out.str() == expected.str());
}

TEST(GoldenDirectedEarlySignalsSse)
{
  auto s = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW, SSE_DAILY_CLOSE, SSE_DAILY_VOLUME);
  auto c = *migration::MapLegacyConfig(101000); c.analysis.signals.publication = chan::SignalPublication::Early;
  auto a = chan::Analyze(s, c);
  std::ostringstream out;
  for (const auto &center : a.snapshot.centers)
    out << "center " << center.start << " " << a.snapshot.pivots[center.firstPivot + 3].index << " "
        << center.direction << " " << center.zg << " " << center.zd << "\n";
  for (const auto &e : a.events)
    out << "event " << e.bar << " " << e.signal.index + 1 << " " << static_cast<int>(e.signal.type)
        << " " << e.revoked << " " << e.signal.stop << "\n";
  const char *path = "tests/fixtures/directed-early-signals-sse.txt";
  if (std::getenv("CHAN_UPDATE_EARLY_GOLDEN")) { std::ofstream f(path); f << out.str(); }
  std::ifstream f(path); REQUIRE(f.good());
  std::ostringstream expected; expected << f.rdbuf();
  CHECK(out.str() == expected.str());
}
