// 结构层（第17/18/20课）：中枢、相邻中枢关系、走势类型。输入为端点序列。
#pragma once

#include "core/model.h"
#include "core/morphology.h"

#include <vector>

namespace chan
{

// 中枢：由进入段之后的连续三段重叠成枢（第17课），[ZD,ZG] 成枢即固定（第20课），与之重叠的段延伸；
// 离开段后回抽不回 [ZD,ZG] 即破坏，离开段归入下一中枢的进入段（第18课定理一/三）。
std::vector<Center> BuildCenters(const std::vector<Pivot> &pivots);

// 中枢流：Update(pivots, dirty) 表示端点从 dirty 起可能变化；返回首个变化的中枢下标（无变化 -1）
class CenterStream
{
public:
  int Update(const std::vector<Pivot> &pivots, std::size_t dirty);
  const std::vector<Center> &Centers() const { return out_; }
  // 已定型中枢数：端点前 pivotFinal 个不再改变时，视界落在其内的最后检查点之前的中枢不再改变
  std::size_t FinalCount(std::size_t pivotFinal) const;
  // 第 center 个中枢确定时的累计读取视界（其后首个检查点记录的视界）；中枢尚未收尾返回 false
  bool HorizonAfter(std::size_t center, Horizon &out) const;

private:
  struct Checkpoint
  {
    std::size_t i, outSize;
    Horizon horizon;
  };
  void Run(const std::vector<Pivot> &p, std::size_t i);
  static bool Same(const Center &a, const Center &b);

  std::vector<Center> out_;
  std::vector<Checkpoint> checkpoints_;
  Horizon horizon_;
};

// 中心定理二（第20课）：后DD>前GG 上涨，后GG<前DD 下跌，否则扩展（形成高级别中枢）
CenterRelation Relate(const Center &previous, const Center &next);

// 走势类型（第17课）：连续同向关系的中枢合并为趋势，其余单中枢为盘整
std::vector<Movement> BuildMovements(const std::vector<Center> &centers);

// 中枢从 dirtyCenter 起变化后就地更新走势；返回首个变化的走势下标（无变化 -1）
int UpdateMovements(const std::vector<Center> &centers, std::vector<Movement> &moves, int dirtyCenter);

}  // namespace chan
