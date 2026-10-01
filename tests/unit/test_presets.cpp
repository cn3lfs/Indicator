// 预设加载和publication/cache边界独立验证，不走旧整数迁移桥。
#include "check.h"
#include "sse_data.h"
#include "tdx/presets.h"
#include "tdx/exports.h"
#include <vector>
namespace
{
std::vector<float> Raw(void (*f)(int,float*,float*,float*,float*),float preset)
{
  std::vector<float> h(SSE_DAILY_HIGH,SSE_DAILY_HIGH+SSE_DAILY_COUNT),l(SSE_DAILY_LOW,SSE_DAILY_LOW+SSE_DAILY_COUNT);
  std::vector<float> out(SSE_DAILY_COUNT);f(SSE_DAILY_COUNT,out.data(),h.data(),l.data(),&preset);return out;
}
bool Zero(const std::vector<float> &v){for(float x:v)if(x!=0)return false;return true;}
}
TEST(PresetsParseFieldsDependenciesAndDiagnostics)
{
  auto presets=tdx::ParsePresets("[3]\nstroke.rule=4k\noutputs.levels=segment\nprojection.segmentBoundary=last\n[4]\nsegment.method=heuristic\nprojection.segmentBoundary=first\n[5]\nunknown=1\n[6]\nstroke.rule=new\nstroke.rule=strict\n");
  CHECK(presets.at(3).error==0);CHECK(presets.at(3).view.analysis.stroke.rule==chan::StrokeRule::FourK);
  CHECK(presets.at(3).view.level==chan::CenterUnit::Segment);
  CHECK(presets.at(4).error==4 && presets.at(5).error==4 && presets.at(6).error==4);
  tdx::ResetForTesting();tdx::PresetTextForTesting("[3]\nunknown=1\n");
  CHECK(Zero(Raw(tdx::Pivots,3)));CHECK(Raw(tdx::PresetDiagnostic,3).back()==4);
  CHECK(Zero(Raw(tdx::Gaps,3)));CHECK(Raw(tdx::PresetDiagnostic,9999).back()==2);
  CHECK(Raw(tdx::PresetDiagnostic,16777216).back()==1);CHECK(Zero(Raw(tdx::Pivots,16777216)));
  CHECK(Raw(tdx::PresetDiagnostic,.5f).back()==1);
}
TEST(PresetsShareFamilyAcrossLevelAndProjectionAndExplicitPublication)
{
  tdx::ResetForTesting();
  auto stroke=Raw(tdx::Pivots,0),segment=Raw(tdx::Pivots,1);
  CHECK(stroke!=segment);CHECK(tdx::AnalysisBuildsForTesting()==1);
  tdx::PresetTextForTesting("[3]\noutputs.levels=segment\nprojection.segmentBoundary=first\n[4]\noutputs.levels=stroke\nsignals.publication=early\n[5]\noutputs.events=off\n");
  CHECK(Raw(tdx::Pivots,3)!=segment);CHECK(tdx::AnalysisBuildsForTesting()==1);
  auto standard=Raw(tdx::Signals,0),early=Raw(tdx::Signals,4);
  CHECK(standard!=early);CHECK(Raw(tdx::EarlySignals,0)==standard);
  CHECK(Raw(tdx::EarlySignals,4)==early);CHECK(tdx::AnalysisBuildsForTesting()==2);
  CHECK(Zero(Raw(tdx::Signals,5)) && Zero(Raw(tdx::EarlySignals,5)));
  CHECK(Raw(tdx::Signals,0)==standard);CHECK(tdx::AnalysisBuildsForTesting()==2);
}
