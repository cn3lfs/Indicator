// Windows真实DLL ABI冒烟：支持32/64位，各自用匹配MinGW编译器编译。参数 DLL路径 输出二进制。
#include "adapter/czsc_api.h"
#include "FxIndicator.h"
#include "tests/unit/sse_data.h"
#include <cstdio>
#include <vector>
#include <cstring>
#include <windows.h>
#define LOAD(symbol) auto symbol##Fn=reinterpret_cast<decltype(&symbol)>(GetProcAddress(dll,#symbol)); if(!symbol##Fn)return 10
#define REQUIRE_OK(x) do { if(!(x)){std::fprintf(stderr,"failed %s:%d: %s\n",__FILE__,__LINE__,#x);return 20;} } while(0)
template<class T> void Dump(FILE *file,const T *(*get)(void*,int32_t,int32_t*),void *h,int level)
{
  int32_t n=0;auto *p=get(h,level,&n);std::fwrite(&n,4,1,file);if(n)std::fwrite(p,sizeof(T),n,file);
}
int main(int argc,char **argv)
{
  if(argc!=3)return 1;HMODULE dll=LoadLibraryA(argv[1]);if(!dll){std::fprintf(stderr,"load error %lu\n",GetLastError());return 2;}
  LOAD(czsc_api_version);LOAD(czsc_build_commit);LOAD(czsc_config_default);LOAD(czsc_config_validate);LOAD(czsc_build);LOAD(czsc_snapshot_free);
  LOAD(czsc_level_pivots);LOAD(czsc_level_centers);LOAD(czsc_level_movements);LOAD(czsc_level_breakouts);LOAD(czsc_level_signals);LOAD(czsc_level_events);LOAD(czsc_level_bars);
  LOAD(czsc_level_recursive_nodes);LOAD(czsc_level_recursive_children);LOAD(czsc_level_recursive_centers);LOAD(czsc_level_recursive_connections);LOAD(czsc_nested_rows);
  LOAD(czsc_set_projection);LOAD(czsc_config_fields);LOAD(czsc_config_choices);LOAD(czsc_config_rules);
  REQUIRE_OK(czsc_api_versionFn()==20);REQUIRE_OK(sizeof(czsc_input)==(sizeof(void*)==8?40:24));
  for(const char *old:{"czsc_snapshot_build","czsc_config_valid","czsc_config_options","czsc_nested_build","czsc_pivots"}) REQUIRE_OK(!GetProcAddress(dll,old));
  REQUIRE_OK(czsc_config_fieldsFn(nullptr,0)==13);REQUIRE_OK(czsc_config_choicesFn(nullptr,0)==31);REQUIRE_OK(czsc_config_rulesFn(nullptr,0)==5);
  czsc_config config{};REQUIRE_OK(czsc_config_defaultFn(&config)==0);REQUIRE_OK(czsc_config_validateFn(&config)==0);
  czsc_input input{sizeof(input),SSE_DAILY_COUNT,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME};
  void *h=czsc_buildFn(&input,&config,CZSC_OUTPUT_DEFAULT);REQUIRE_OK(h);
  FILE *file=std::fopen(argv[2],"wb");REQUIRE_OK(file);
  for(int level:{0,1})
  {
    Dump(file,czsc_level_pivotsFn,h,level);Dump(file,czsc_level_centersFn,h,level);Dump(file,czsc_level_movementsFn,h,level);
    Dump(file,czsc_level_breakoutsFn,h,level);Dump(file,czsc_level_signalsFn,h,level);Dump(file,czsc_level_eventsFn,h,level);Dump(file,czsc_level_barsFn,h,level);
    Dump(file,czsc_level_recursive_nodesFn,h,level);Dump(file,czsc_level_recursive_childrenFn,h,level);Dump(file,czsc_level_recursive_centersFn,h,level);Dump(file,czsc_level_recursive_connectionsFn,h,level);
  }
  std::fclose(file);int count=0;czsc_nested_rowsFn(h,&count);REQUIRE_OK(count>0);
  czsc_projection projection{sizeof(projection),2,0};REQUIRE_OK(czsc_set_projectionFn(h,&projection)==0);czsc_snapshot_freeFn(h);
  config.strokeGap=2;h=czsc_buildFn(&input,&config,CZSC_OUTPUT_DEFAULT);REQUIRE_OK(h);czsc_level_pivotsFn(h,0,&count);REQUIRE_OK(count==166);czsc_snapshot_freeFn(h);
  auto reg=reinterpret_cast<BOOL (*)(PluginTCalcFuncInfo**)>(GetProcAddress(dll,"RegisterTdxFunc"));REQUIRE_OK(reg);
  PluginTCalcFuncInfo *table=nullptr;REQUIRE_OK(reg(&table));
  auto function=[&](int mark){for(auto *p=table;p->nFuncMark;++p)if(p->nFuncMark==mark)return p->pCallFunc;return static_cast<pPluginFUNC>(nullptr);};
  auto pivots=function(1),signals=function(5),early=function(41),diagnostic=function(46),registerCV=function(40);REQUIRE_OK(pivots && signals && early && diagnostic && registerCV);
  std::vector<float> high(SSE_DAILY_HIGH,SSE_DAILY_HIGH+SSE_DAILY_COUNT),low(SSE_DAILY_LOW,SSE_DAILY_LOW+SSE_DAILY_COUNT);
  std::vector<float> close(SSE_DAILY_CLOSE,SSE_DAILY_CLOSE+SSE_DAILY_COUNT),volume(SSE_DAILY_VOLUME,SSE_DAILY_VOLUME+SSE_DAILY_COUNT),out(SSE_DAILY_COUNT),other(out.size());
  registerCV(input.n,out.data(),close.data(),volume.data(),nullptr);
  float preset=0;signals(input.n,out.data(),high.data(),low.data(),&preset);early(input.n,other.data(),high.data(),low.data(),&preset);REQUIRE_OK(out==other);
  preset=3;diagnostic(input.n,out.data(),nullptr,nullptr,&preset);REQUIRE_OK(out.back()==0);
  pivots(input.n,out.data(),high.data(),low.data(),&preset);count=0;for(float value:out)count+=value!=0;REQUIRE_OK(count==166);
  preset=4;diagnostic(input.n,out.data(),nullptr,nullptr,&preset);REQUIRE_OK(out.back()==4);pivots(input.n,out.data(),high.data(),low.data(),&preset);for(float value:out)REQUIRE_OK(value==0);
  std::printf("api20 pointer%zu commit=%s default tables dumped; DLL-adjacent INI and gap validated\n",sizeof(void*),czsc_build_commitFn());
  FreeLibrary(dll);return 0;
}
