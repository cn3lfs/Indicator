// 通达信适配层：把 core 的分析结果投影为逐根序列。签名固定为 (数据个数, 输出, 输入a, 输入b, 输入c)。
// 公式写法：TDXDLL1(编号, H, L, 配置码)；配置码 个位笔(0严格/1新/2czsc)、十位笔结束、百位中枢构件、千位线段法。
// 非法配置码输出全 0。“当下”输出只用截至该根的数据（无未来函数）；“当前结构”随新K线可能改变最后一段。
#pragma once

namespace tdx
{

void Pivots(int count, float *out, float *high, float *low, float *config);          // 1  端点 顶+1/底-1
void CenterHigh(int count, float *out, float *high, float *low, float *config);      // 2  中枢 ZG
void CenterLow(int count, float *out, float *high, float *low, float *config);       // 3  中枢 ZD
void CenterRelation(int count, float *out, float *high, float *low, float *config);  // 4  1上涨/-1下跌/2扩展（后中枢起点）
void Signals(int count, float *out, float *high, float *low, float *config);         // 5  当下确认买卖点
void Revokes(int count, float *out, float *high, float *low, float *config);         // 6  信号失效
void Stops(int count, float *out, float *high, float *low, float *config);           // 7  失效价（止损参考）
void Divergence(int count, float *out, float *high, float *low, float *config);      // 8  c/b 同色 MACD 面积比 %
void Movements(int count, float *out, float *high, float *low, float *config);       // 9  走势类型 0盘整/1上涨/-1下跌
void Kisses(int count, float *out, float *high, float *low, float *config);          // 10 均线吻 1飞/2唇/3湿/4放量湿
void Gaps(int count, float *out, float *high, float *low, float *config);            // 11 缺口 ±1
void FractalStrength(int count, float *out, float *high, float *low, float *config); // 12 分型强弱 ±1/±2
void HindsightSignals(int count, float *out, float *high, float *low, float *config);// 13 事后买卖点（复盘用）
void RegisterCloseVolume(int count, float *out, float *close, float *volume, float *unused);  // 40

// 41/42/43 社区快速提示、结构失效、端点分型极值失效价；分型确认即提示，不等下一笔。
void EarlySignals(int count, float *out, float *high, float *low, float *config);
void EarlyRevokes(int count, float *out, float *high, float *low, float *config);
void EarlyStops(int count, float *out, float *high, float *low, float *config);

// 测试用：清空旁路注册与缓存
void ResetForTesting();

}  // namespace tdx
