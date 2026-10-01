#include "migration/legacy_config.h"
// 社区/非原文笔口径的独立SSE回归；不改既有 tests/unit/golden。
#include "check.h"
#include "sse_data.h"
#include "core/engine.h"
#include "core/morphology.h"
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
    auto a = chan::Analyze(s, *migration::MapLegacyConfig(code));
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
  for (int place : {10000, 20000})
  {
    for (int stroke = 0; stroke <= 4; ++stroke)
    {
      int code = place + 1100 + stroke;
      auto config = *migration::MapLegacyConfig(code);
      auto a = chan::Analyze(s, config);
      out << "## 线段分界配置 " << code << " 端点 " << a.snapshot.pivots.size() << "\n";
      for (std::size_t i = 0; i < a.snapshot.pivots.size(); ++i)
      {
        const auto &p = a.snapshot.pivots[i];
        int index = chan::DisplayPivotIndex(p, config);
        std::snprintf(line, sizeof line, "端点 %zu 显示%d 极值%d %d %.2f 分型%d 定型%d\n", i, index, p.index,
                      static_cast<int>(p.kind), p.kind == chan::Kind::Top ? SSE_DAILY_HIGH[index] : SSE_DAILY_LOW[index],
                      p.fractalAt, a.pivotFinalAt[i]);
        out << line;
      }
    }
  }
  auto real = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW, SSE_DAILY_CLOSE, SSE_DAILY_VOLUME);
  for (int method : {0, 1000})
  {
    for (int unit : {0, 100})
    {
      for (int stroke = 0; stroke <= 4; ++stroke)
      {
        int code = 100000 + method + unit + stroke;
        auto a = chan::Analyze(real, *migration::MapLegacyConfig(code));
        out << "## 中枢构成配置 " << code << " 中枢 " << a.snapshot.centers.size() << " 信号 " << a.snapshot.signals.size() << " 事件 " << a.events.size() << "\n";
        for (std::size_t i = 0; i < a.snapshot.centers.size(); ++i)
        {
          const auto &c = a.snapshot.centers[i];
          std::snprintf(line, sizeof line, "中枢 %d-%d %d-%d 方向%d ZD %.2f ZG %.2f DD %.2f GG %.2f 定型%d\n",
            c.start, c.end, c.firstPivot, c.lastPivot, c.direction, c.zd, c.zg, c.dd, c.gg, a.centerFinalAt[i]);
          out << line;
        }
        for (const auto &sig : a.snapshot.signals)
          out << "信号 " << sig.index << " " << static_cast<int>(sig.type) << " " << sig.stop << "\n";
        for (const auto &e : a.events)
          out << "事件 " << e.bar << " " << e.signal.index << " " << static_cast<int>(e.signal.type) << " " << e.revoked << "\n";
      }
    }
  }
  // v10社区原始K线包络追加段，既有段落保持逐字节不变。
  for (int unit : {0, 100, 1100})
    for (int stroke = 0; stroke <= 4; ++stroke)
    {
      int code = 1000000 + unit + stroke;
      auto a = chan::Analyze(real, *migration::MapLegacyConfig(code));
      out << "## 笔内包络配置 " << code << " 端点 " << a.snapshot.pivots.size() << " 中枢 " << a.snapshot.centers.size()
          << " 信号 " << a.snapshot.signals.size() << " 事件 " << a.events.size() << "\n";
      for (const auto &p : a.snapshot.pivots)
        out << "端点 " << p.index << " " << static_cast<int>(p.kind) << " " << p.Price() << "\n";
      for (const auto &e : a.events)
        out << "事件 " << e.bar << " " << e.signal.index << " " << static_cast<int>(e.signal.type) << " " << e.revoked << "\n";
    }
  // v20新增社区缺口段，只追加，不改变既有社区段和原文golden。
  for(int gap:{1,2})for(int stroke=0;stroke<5;++stroke)
  {
    chan::AnalysisConfig config;config.stroke.rule=static_cast<chan::StrokeRule>(stroke);config.stroke.gap=static_cast<chan::GapRule>(gap);
    if(stroke==4) { out << "## 缺口 " << gap << " 分型笔 不适用\n";continue; }
    auto family=chan::AnalyzeFamily(real,config);
    for(int level=0;level<2;++level)
    {
      const auto &a=family.levels[level];const auto &n=a.snapshot;
      out << "## 缺口 " << gap << " 笔算法 " << stroke << " level " << level << " 端点 " << n.pivots.size()
          << " 中枢 " << n.centers.size() << " 信号 " << n.signals.size() << " 事件 " << a.events.size() << "\n";
      for(const auto &p:n.pivots)out << "端点 " << p.index << " " << static_cast<int>(p.kind) << " " << p.Price() << "\n";
      for(const auto &c:n.centers)out << "中枢 " << c.start << " " << c.end << " " << c.zd << " " << c.zg << "\n";
      for(const auto &e:a.events)out << "事件 " << e.bar << " " << e.signal.index << " " << static_cast<int>(e.signal.type) << " " << e.revoked << "\n";
    }
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
