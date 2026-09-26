// 买卖点（第20/21/24/27/29/37课）：一类=趋势背驰，二类=一类后第二段不创新低/新高，三类=离开中枢后首次回试不回。
#pragma once

#include "core/model.h"
#include "core/morphology.h"

#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace chan
{

struct EnergyTables;

// 中枢首次离开与回试（第20课）；third=回试不回 [ZD,ZG]，构成三类买卖点
struct Breakout
{
  int center = -1;
  int direction = 0;
  int leavePivot = -1;
  int retestPivot = -1;
  bool third = false;
  Divergence divergence;  // 离开段相对前一同向段的盘整背驰（第24课）
};

// 单个中枢的首次离开+回试；h 记录读取视界（扫描到数据尽头仍未找到则无界）
std::optional<Breakout> BreakoutFor(const std::vector<Pivot> &pivots, const std::vector<Center> &centers,
                                    std::size_t center, Horizon &h);
std::vector<Breakout> BuildBreakouts(const std::vector<Pivot> &pivots, const std::vector<Center> &centers);

// 全部候选（批量）：顺序为 二类、三类、一类（与同根取胜规则配合：一类 30 > 三类 20 > 二类 10）
// tables 用于黄白线相关上下文（回零、黄白线弱），为空时这些标志位不置
std::vector<Signal> BuildSignals(const std::vector<Pivot> &pivots, const std::vector<Center> &centers,
                                 const std::vector<Movement> &movements, const std::vector<Breakout> &breakouts,
                                 const EnergyTables *tables = nullptr);

// 买卖点流：随端点/中枢/走势的局部变化只重算受影响部分，并对“端点已被下一端点确认”的信号
// 做出现/失效差分。与每步批量重算后差分的结果逐事件一致。
class SignalStream
{
public:
  // dirtyPivot/dirtyCenter/dirtyMove：各层首个变化下标（-1 表示该层无变化）
  void Update(const std::vector<Pivot> &pivots, const std::vector<Center> &centers,
              const std::vector<Movement> &movements, int dirtyPivot, int dirtyCenter, int dirtyMove, int bar,
              bool emit, std::vector<SignalEvent> &events);
  void SetTables(const EnergyTables *tables) { tables_ = tables; }

  // 中枢 ci 的首次离开+回试已找到且扫描视界落在前 pivotFinal 个已定型端点内
  bool BreakoutFinal(std::size_t ci, std::size_t pivotFinal) const
  {
    return ci < breakouts_.size() && breakouts_[ci].has_value() && breakoutHorizons_[ci].Before(pivotFinal);
  }

private:
  using SignalKey = std::pair<int, int>;  // (信号K线, 信号码)
  using Source = std::pair<int, int>;     // (类别 1/2/3, 来源：一类端点/二类所依一类端点/三类中枢)
  static SignalKey KeyOf(const Signal &s) { return {s.index, static_cast<int>(s.type)}; }
  void Put(const Signal &s, Source source);
  void Drop(const Signal &s, Source source);

  std::vector<std::optional<Signal>> rawFirst_;              // 逐端点：去重前的一类候选
  std::map<std::pair<int, int>, std::set<int>> members_;      // (类型, 中枢) → 一类候选端点
  std::map<std::pair<int, int>, std::vector<int>> groupSurvivors_;
  std::map<int, Signal> survivors_;                           // 一类幸存者（按端点）
  std::map<int, Signal> seconds_;                             // 二类（按所依一类端点）
  std::map<int, Signal> thirds_;                              // 三类（按中枢）
  std::vector<std::optional<Breakout>> breakouts_;
  std::vector<Horizon> breakoutHorizons_;
  std::vector<char> breakoutIndexed_;                         // 该中枢的视界是否已登记在索引中
  std::set<int> openBreakouts_;                               // 视界无界（尚未定型）的中枢
  std::multimap<std::size_t, int> breakoutByHorizon_;         // 视界上界 → 中枢
  std::map<SignalKey, std::map<Source, Signal>> byKey_;
  std::map<SignalKey, Signal> active_;                        // 当前已确认且有效的信号
  std::set<SignalKey> touched_;
  std::size_t pivotCount_ = 0;
  std::vector<int> centerStarts_, moveStarts_;
  const EnergyTables *tables_ = nullptr;
};

}  // namespace chan
