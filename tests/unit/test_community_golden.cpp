// 社区/非原文笔口径的独立SSE回归；不改既有 tests/unit/golden。
#include "check.h"
#include "sse_data.h"
#include "core/engine.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>

TEST(GoldenCommunityStrokesSse)
{
  std::ostringstream out;
  chan::Series s = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW);
  char line[256];
  for (int code : {0, 1, 2, 3, 4})
  {
    auto a = chan::Analyze(s, *chan::Config::Decode(code));
    const auto &n = a.snapshot;
    out << "## 配置 " << code << " 端点 " << n.pivots.size() << " 中枢 " << n.centers.size()
        << " 走势 " << n.movements.size() << " 信号 " << n.signals.size() << " 事件 " << a.events.size() << "\n";
    for (std::size_t i = 0; i < n.pivots.size(); ++i)
    {
      const auto &p = n.pivots[i];
      std::snprintf(line, sizeof line, "端点 %zu %d %s %d %.2f 分型%d 定型%d\n", i, p.index,
                    SSE_DAILY_DATE[p.index], static_cast<int>(p.kind), p.Price(), p.fractalAt, a.pivotFinalAt[i]);
      out << line;
    }
    for (const auto &c : n.centers)
    {
      std::snprintf(line, sizeof line, "中枢 %d-%d %d-%d ZD %.2f ZG %.2f DD %.2f GG %.2f\n",
                    c.start, c.end, c.firstPivot, c.lastPivot, c.zd, c.zg, c.dd, c.gg);
      out << line;
    }
    for (const auto &e : a.events)
      out << "事件 " << e.bar << " " << e.signal.index << " " << static_cast<int>(e.signal.type) << " " << e.revoked << "\n";
  }
  std::string here = __FILE__;
  std::size_t slash = here.find_last_of("/\\");
  std::string path = (slash == std::string::npos ? "." : here.substr(0, slash)) + "/../fixtures/sse-community.txt";
  if (std::getenv("CHAN_UPDATE_COMMUNITY_GOLDEN"))
  {
    std::ofstream(path, std::ios::binary) << out.str();
    std::printf("  community golden updated: %s\n", path.c_str());
    return;
  }
  std::ifstream in(path, std::ios::binary);
  std::stringstream saved;
  saved << in.rdbuf();
  CHECK(saved.str() == out.str());
}
