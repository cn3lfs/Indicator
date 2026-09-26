// 行情序列：H/L 必需，C/V 可选（缺失时 MACD 用 (H+L)/2 代理）。
// 构造时清洗通达信无效数 0xF8F8F8F8：向前填充；开头的无效值用首个有效值（无可用数据的K线本无意义）。
#pragma once

#include <vector>

namespace chan
{

struct Series
{
  std::vector<float> high;
  std::vector<float> low;
  std::vector<float> close;   // 为空表示无真实收盘价
  std::vector<float> volume;  // 为空表示无成交量

  int Size() const { return static_cast<int>(high.size()); }
  bool HasClose() const { return !close.empty(); }

  // MACD/均线使用的价格：有收盘价用收盘价，否则 (H+L)/2
  float PriceAt(int i) const
  {
    return HasClose() ? close[static_cast<std::size_t>(i)]
                      : (high[static_cast<std::size_t>(i)] + low[static_cast<std::size_t>(i)]) * 0.5f;
  }

  // 由通达信原始指针构造；close/volume 可为 nullptr。收盘价须每根落在 [L,H]（容差 0.1%）才被采用，
  // 否则视为错配数据丢弃（与旧版 Func40 旁路校验一致）。
  static Series FromRaw(int count, const float *high, const float *low,
                        const float *close = nullptr, const float *volume = nullptr);
};

bool IsTdxInvalid(float value);
std::vector<float> Sanitize(int count, const float *values);

}  // namespace chan
