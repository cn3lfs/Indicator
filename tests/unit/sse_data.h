// 上证指数日线样本：原样 + 翻转/缩放拼接的 12 倍放大序列（覆盖更多形态）；CommonConfigs 为常用配置全集。
#pragma once

#include "tests/SseIndexDaily.h"

#include <vector>
#include <initializer_list>
#include "migration/legacy_config.h"

struct SseSample
{
  const char *name;
  std::vector<float> high, low;
};

inline std::vector<SseSample> SseSamples()
{
  std::vector<SseSample> out;
  out.push_back({"sse", std::vector<float>(SSE_DAILY_HIGH, SSE_DAILY_HIGH + SSE_DAILY_COUNT),
                 std::vector<float>(SSE_DAILY_LOW, SSE_DAILY_LOW + SSE_DAILY_COUNT)});
  const int rep = 12;
  SseSample big{"sse-x12", {}, {}};
  for (int r = 0; r < rep; r++)
  {
    float scale = 1.0f + 0.013f * static_cast<float>(r % 7);
    for (int i = 0; i < SSE_DAILY_COUNT; i++)
    {
      int k = (r % 2) ? SSE_DAILY_COUNT - 1 - i : i;
      big.high.push_back(SSE_DAILY_HIGH[k] * scale);
      big.low.push_back(SSE_DAILY_LOW[k] * scale);
    }
  }
  out.push_back(big);
  return out;
}

// 新端点策略显式构造，不借旧first整数迁移。
inline std::vector<chan::LevelConfig> AnalysisConfigs(std::initializer_list<int> legacy,
                                                     std::initializer_list<int> second = {})
{
  std::vector<chan::LevelConfig> out;
  for (int code : legacy) out.push_back(migration::MapLegacyConfig(code).value());
  for (int code : second)
  {
    auto c = migration::MapLegacyConfig(code).value();
    c.analysis.stroke.endpoint = chan::StrokeEnd::Second;
    out.push_back(c);
  }
  return out;
}
inline std::vector<chan::LevelConfig> CommonConfigs()
{
  return AnalysisConfigs({0,1,2,100,101,102,1100,1101,1102},{0,1,2,1100});
}
