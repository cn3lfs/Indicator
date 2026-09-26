// 上证指数日线真实样本 golden：结构与当下事件的可读文本，算法变化会在 diff 中直接可见。
// 有意变更算法后运行 `make golden`（或 CHAN_UPDATE_GOLDEN=1 ChanTests Golden）重新生成并人工核对。
#include "check.h"
#include "sse_data.h"
#include "core/engine.h"
#include "core/structure.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
const char *Date(int i) { return (i >= 0 && i < SSE_DAILY_COUNT) ? SSE_DAILY_DATE[i] : "?"; }

const char *Name(chan::SignalType t)
{
  switch (t)
  {
    case chan::SignalType::Buy1: return "一买";
    case chan::SignalType::Buy2: return "二买";
    case chan::SignalType::Buy3: return "三买";
    case chan::SignalType::Sell1: return "一卖";
    case chan::SignalType::Sell2: return "二卖";
    default: return "三卖";
  }
}

std::string Render()
{
  std::ostringstream o;
  char line[256];
  chan::Series s = chan::Series::FromRaw(SSE_DAILY_COUNT, SSE_DAILY_HIGH, SSE_DAILY_LOW);
  for (int code : {0, 2, 1100})
  {
    chan::Analysis a = chan::Analyze(s, *chan::Config::Decode(code));
    const chan::Snapshot &n = a.snapshot;
    o << "## 配置 " << code << "：端点 " << n.pivots.size() << "，中枢 " << n.centers.size() << "，走势 "
      << n.movements.size() << "，事后信号 " << n.signals.size() << "，当下事件 " << a.events.size() << "\n";
    for (std::size_t i = 0; i < n.centers.size(); i++)
    {
      const chan::Center &c = n.centers[i];
      std::snprintf(line, sizeof line, "中枢 %s~%s ZG %.2f ZD %.2f GG %.2f DD %.2f 方向 %d%s\n", Date(c.start), Date(c.end),
                    c.zg, c.zd, c.gg, c.dd, c.direction,
                    i == 0 ? "" : (chan::Relate(n.centers[i - 1], c) == chan::CenterRelation::Up     ? " 上涨"
                                   : chan::Relate(n.centers[i - 1], c) == chan::CenterRelation::Down ? " 下跌"
                                                                                                     : " 扩展"));
      o << line;
    }
    for (const chan::Movement &m : n.movements)
    {
      if (m.type == chan::MovementType::Consolidation) continue;
      std::snprintf(line, sizeof line, "趋势 %s %s~%s 中枢 %d-%d\n", m.type == chan::MovementType::Up ? "上涨" : "下跌",
                    Date(m.start), Date(m.end), m.firstCenter, m.lastCenter);
      o << line;
    }
    for (const chan::Signal &g : n.signals)
    {
      std::snprintf(line, sizeof line, "事后 %s %s\n", Name(g.type), Date(g.index));
      o << line;
    }
    for (const chan::SignalEvent &e : a.events)
    {
      std::snprintf(line, sizeof line, "当下 %s %s %s 于 %s 失效价 %.2f\n", e.revoked ? "失效" : "出现", Name(e.signal.type),
                    Date(e.signal.index), Date(e.bar), e.signal.stop);
      o << line;
    }
    o << "\n";
  }
  return o.str();
}

std::string GoldenPath()
{
  std::string here = __FILE__;
  std::size_t slash = here.find_last_of("/\\");
  return (slash == std::string::npos ? std::string(".") : here.substr(0, slash)) + "/golden/sse.txt";
}
}  // namespace

TEST(GoldenSse)
{
  std::string now = Render();
  if (std::getenv("CHAN_UPDATE_GOLDEN"))
  {
    std::ofstream(GoldenPath(), std::ios::binary) << now;
    std::printf("  golden updated: %s\n", GoldenPath().c_str());
    return;
  }
  std::ifstream in(GoldenPath(), std::ios::binary);
  std::stringstream saved;
  saved << in.rdbuf();
  if (saved.str() != now) std::printf("  golden differs; review and run `make golden` if intended\n");
  CHECK(saved.str() == now);
}
