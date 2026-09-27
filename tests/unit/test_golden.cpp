// 上证指数日线真实样本 golden：结构与当下事件的可读文本，算法变化会在 diff 中直接可见。
// 有意变更算法后运行 `make golden`（或 CHAN_UPDATE_GOLDEN=1 ChanTests Golden）重新生成并人工核对。
#include "check.h"
#include "sse_data.h"
#include "core/engine.h"
#include "core/structure.h"
#include "adapter/czsc_api.h"

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

// 结构化接口 v5：中阴、递归节点、递归中枢、同级别连接段（CZSC_FLAG_HIGHER，收盘价/成交量取样本真实值）
void RenderRecursion(std::ostringstream &o)
{
  char line[256];
  for (int code : {0, 2})
  {
    czsc_input in{};
    in.size = sizeof in;
    in.n = SSE_DAILY_COUNT;
    in.high = SSE_DAILY_HIGH;
    in.low = SSE_DAILY_LOW;
    in.close = SSE_DAILY_CLOSE;
    in.volume = SSE_DAILY_VOLUME;
    in.config = code;
    in.flags = CZSC_FLAG_HIGHER;
    void *h = czsc_snapshot_build(&in);
    int32_t nm = 0, nn = 0, nc = 0, nk = 0;
    const czsc_movement *m = czsc_movements(h, &nm);
    const czsc_recursive_node *nodes = czsc_recursive_nodes(h, &nn);
    const czsc_recursive_center *centers = czsc_recursive_centers(h, &nc);
    const czsc_recursive_connection *links = czsc_recursive_connections(h, &nk);
    o << "## 递归 配置 " << code << "：节点 " << nn << "，上层中枢 " << nc << "，上层连接段 " << nk << "\n";
    for (int32_t i = 0; i < nm; i++)
    {
      if (m[i].successor < 0) continue;
      std::snprintf(line, sizeof line, "中阴 走势%d %s~%s\n", i, Date(m[i].zhongyinStart), Date(m[i].successorEstablishedAt));
      o << line;
    }
    for (int32_t i = 0; i < nn; i++)
    {
      const czsc_recursive_node &n = nodes[i];
      if (n.level == 0) continue;
      std::snprintf(line, sizeof line, "节点 L%d#%d 类型 %d %s~%s 高 %.2f 低 %.2f 中枢 %d-%d 子 %d 中阴 %s~%s 定型 %s\n", n.level,
                    n.ordinal, n.type, Date(n.start), Date(n.end), n.high, n.low, n.firstCenter, n.lastCenter, n.childCount,
                    Date(n.zhongyinStart), Date(n.completed), Date(n.confirmedAt));
      o << line;
    }
    for (int32_t i = 0; i < nc; i++)
    {
      const czsc_recursive_center &c = centers[i];
      std::snprintf(line, sizeof line, "上层中枢 L%d#%d %s~%s ZG %.2f ZD %.2f GG %.2f DD %.2f 方向 %d 成员 %d+%d 成立 %s 定型 %s\n",
                    c.level, c.ordinal, Date(c.start), Date(c.end), c.zg, c.zd, c.gg, c.dd, c.direction, c.firstMember,
                    c.memberCount, Date(c.established), Date(c.confirmedAt));
      o << line;
    }
    for (int32_t i = 0; i < nk; i++)
    {
      const czsc_recursive_connection &k = links[i];
      std::snprintf(line, sizeof line, "连接段 L%d#%d %s~%s 成员 %d+%d 定型 %s\n", k.level, k.ordinal, Date(k.start), Date(k.end),
                    k.firstMember, k.memberCount, Date(k.confirmedAt));
      o << line;
    }
    o << "\n";
    czsc_snapshot_free(h);
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
  RenderRecursion(o);
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
