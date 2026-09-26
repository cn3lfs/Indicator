// 迁移期 oracle 对照用数据：SSE 日线原样 + 翻转/缩放拼接的放大序列。
#pragma once

#include "tests/SseIndexDaily.h"

#include <vector>

struct OracleSample
{
  const char *name;
  std::vector<float> high, low;
};

inline std::vector<OracleSample> OracleSamples()
{
  std::vector<OracleSample> out;
  out.push_back({"sse", std::vector<float>(SSE_DAILY_HIGH, SSE_DAILY_HIGH + SSE_DAILY_COUNT),
                 std::vector<float>(SSE_DAILY_LOW, SSE_DAILY_LOW + SSE_DAILY_COUNT)});
  const int rep = 12;
  OracleSample big{"sse-x12", {}, {}};
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

inline const int kOracleConfigs[] = {0, 1, 2, 10, 11, 12, 100, 101, 102, 1100, 1101, 1102, 1110};
