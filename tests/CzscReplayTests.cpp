// 逐步重放（无未来函数）测试：重放每一步须等于只用 [0,t] 数据的完整重算。
#include "../CzscCore.h"
#include "SseIndexDaily.h"
#include <set>
#include <utility>
#include <string>
#include <vector>

namespace
{
typedef std::set<std::pair<int, float> > SignalSet;

SignalSet ConfirmedCandidates(const CzscAnalyzer &A)
{
  SignalSet S;
  for (std::size_t i = 0; i < A.Candidates.size(); i++)
  {
    const TradingSignalCandidate &C = A.Candidates[i];
    if ((C.nPoint >= 0) && ((std::size_t)C.nPoint + 1 < A.Points.size()))
    {
      S.insert(std::make_pair(C.nIndex, C.fSignal));
    }
  }
  return S;
}

bool ReplayMatchesPrefix(int nConfig)
{
  const int N = SSE_DAILY_COUNT;
  std::vector<float> H(SSE_DAILY_HIGH, SSE_DAILY_HIGH + N), L(SSE_DAILY_LOW, SSE_DAILY_LOW + N);
  CzscConfig Config = DecodeConfig((float)nConfig);
  std::vector<ReplaySignalEvent> Events = BuildReplaySignalEvents(N, &H[0], &L[0], Config);
  SignalSet Active;
  for (std::size_t e = 0; e < Events.size();)
  {
    int nBar = Events[e].nBar;
    for (; (e < Events.size()) && (Events[e].nBar == nBar); e++)
    {
      std::pair<int, float> Key(Events[e].nIndex, Events[e].fSignal);
      if (Events[e].bRevoke) Active.erase(Key); else Active.insert(Key);
    }
    CzscAnalyzer Prefix;
    BuildAnalyzerFromPrice(Prefix, nBar + 1, &H[0], &L[0], Config);
    if (ConfirmedCandidates(Prefix) != Active) return false;
  }
  // 重放结束态 = 全量结果中端点已被下一端点确认的候选
  CzscAnalyzer Full;
  BuildAnalyzerFromPrice(Full, N, &H[0], &L[0], Config);
  return ConfirmedCandidates(Full) == Active;
}

bool ReplayWindowIsSuffixOfFull()
{
  const int N = SSE_DAILY_COUNT, W = 600;
  std::vector<float> H(SSE_DAILY_HIGH, SSE_DAILY_HIGH + N), L(SSE_DAILY_LOW, SSE_DAILY_LOW + N);
  std::vector<ReplaySignalEvent> Full = BuildReplaySignalEvents(N, &H[0], &L[0], DefaultConfig());
  std::vector<ReplaySignalEvent> Win = BuildReplaySignalEvents(N, &H[0], &L[0], DefaultConfig(), W);
  std::vector<ReplaySignalEvent> Expected;
  for (std::size_t i = 0; i < Full.size(); i++)
  {
    if (Full[i].nBar >= N - W) Expected.push_back(Full[i]);
  }
  if (Win.size() != Expected.size() || Win.empty()) return false;
  for (std::size_t i = 0; i < Win.size(); i++)
  {
    if ((Win[i].nBar != Expected[i].nBar) || (Win[i].nIndex != Expected[i].nIndex) ||
        (Win[i].fSignal != Expected[i].fSignal) || (Win[i].bRevoke != Expected[i].bRevoke)) return false;
  }
  return true;
}

// SSE 上的当下失效样本：2021-01-25 一卖在 2021-02-02 出现、2021-02-19 失效，真顶 2021-02-18 于 03-01 确认
bool ReplayExposesRevokedFirstSell()
{
  const int N = SSE_DAILY_COUNT;
  std::vector<float> H(SSE_DAILY_HIGH, SSE_DAILY_HIGH + N), L(SSE_DAILY_LOW, SSE_DAILY_LOW + N);
  std::vector<ReplaySignalEvent> Events = BuildReplaySignalEvents(N, &H[0], &L[0], DefaultConfig());
  int nAppear = -1, nRevoke = -1, nTop = -1;
  for (std::size_t i = 0; i < Events.size(); i++)
  {
    const ReplaySignalEvent &E = Events[i];
    if (E.fSignal != 11.0f) continue;
    const char *pSig = SSE_DAILY_DATE[E.nIndex];
    const char *pBar = SSE_DAILY_DATE[E.nBar];
    std::string Sig(pSig), Bar(pBar);
    if (Sig == "2021-01-25" && !E.bRevoke && Bar == "2021-02-02") nAppear = (int)i;
    if (Sig == "2021-01-25" && E.bRevoke && Bar == "2021-02-19") nRevoke = (int)i;
    if (Sig == "2021-02-18" && !E.bRevoke && Bar == "2021-03-01") nTop = (int)i;
  }
  return (nAppear >= 0) && (nRevoke > nAppear) && (nTop > nRevoke);
}

bool Func30WritesReplayOutputs()
{
  const int N = SSE_DAILY_COUNT;
  std::vector<float> H(SSE_DAILY_HIGH, SSE_DAILY_HIGH + N), L(SSE_DAILY_LOW, SSE_DAILY_LOW + N);
  std::vector<float> Mode(N, -1090.0f), Out(N, -9.0f), Revoke(N, -9.0f);
  Func30(N, &Out[0], &H[0], &L[0], &Mode[0]);
  std::vector<float> Mode2(N, -1100.0f);
  Func30(N, &Revoke[0], &H[0], &L[0], &Mode2[0]);
  std::vector<ReplaySignalEvent> Events = BuildReplaySignalEvents(N, &H[0], &L[0], DefaultConfig());
  int nAppear = 0, nRevoke = 0;
  for (std::size_t i = 0; i < Events.size(); i++)
  {
    const std::vector<float> &V = Events[i].bRevoke ? Revoke : Out;
    if (V[(std::size_t)Events[i].nBar] == 0.0f) return false;
    (Events[i].bRevoke ? nRevoke : nAppear)++;
  }
  int nOut = 0, nRev = 0;
  for (int i = 0; i < N; i++)
  {
    if (Out[(std::size_t)i] != 0.0f) nOut++;
    if (Revoke[(std::size_t)i] != 0.0f) nRev++;
  }
  return (nAppear > 0) && (nRevoke > 0) && (nOut <= nAppear) && (nRev <= nRevoke) && (nOut > 0) && (nRev > 0);
}
}

bool TestReplaySuite()
{
  return ReplayMatchesPrefix(0) && ReplayMatchesPrefix(1) && ReplayMatchesPrefix(10) &&
         ReplayWindowIsSuffixOfFull() && ReplayExposesRevokedFirstSell() && Func30WritesReplayOutputs();
}
