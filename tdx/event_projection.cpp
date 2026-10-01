#include "tdx/event_projection.h"
#include <map>
#include <utility>
namespace tdx
{
std::vector<float> RecentLiveSignals(const std::vector<chan::SignalEvent> &events, int count, bool buy, int window)
{
  if (count <= 0) return {};
  std::vector<float> out(static_cast<std::size_t>(count), 0);
  if (window <= 0) return out;
  using Key = std::pair<int, int>;  // 原极值下标与类型，不能按撤销当天或信号类型单独关联。
  std::map<Key, const chan::SignalEvent *> active;
  std::size_t next = 0;
  for (int bar = 0; bar < count; ++bar)
  {
    std::size_t first = next;
    while (next < events.size() && events[next].bar <= bar) ++next;
    const chan::SignalEvent *winner = nullptr;
    for (std::size_t k = first; k < next; ++k)
    {
      const auto &e = events[k];
      if (e.bar == bar && !e.revoked && (!winner || e.signal.priority >= winner->signal.priority)) winner = &e;
    }
    if (winner) active[{winner->signal.index, static_cast<int>(winner->signal.type)}] = winner;
    for (std::size_t k = first; k < next; ++k)
    {
      const auto &e = events[k];
      if (e.revoked) active.erase({e.signal.index, static_cast<int>(e.signal.type)});
    }
    const chan::SignalEvent *latest = nullptr;
    for (auto it = active.begin(); it != active.end();)
    {
      const auto *e = it->second;
      if (bar - e->bar >= window) { it = active.erase(it); continue; }
      bool isBuy = static_cast<int>(e->signal.type) <= 3;
      if (isBuy == buy && (!latest || e->bar > latest->bar)) latest = e;
      ++it;
    }
    if (latest) out[bar] = static_cast<int>(latest->signal.type);
  }
  return out;
}
}
