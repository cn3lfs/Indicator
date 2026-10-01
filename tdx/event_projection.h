// 当下信号关联投影；只撤销原(type,index)所指向的信号。
#pragma once
#include "core/model.h"
#include <vector>
namespace tdx
{
// 返回最近window根内仍有效、且实际显示过的最新买/卖点码；无信号为0。
std::vector<float> RecentLiveSignals(const std::vector<chan::SignalEvent> &events, int count, bool buy, int window = 3);
}
