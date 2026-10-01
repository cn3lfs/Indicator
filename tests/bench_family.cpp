// 配置重构性能工具：原始数据只从调用方本地二进制读取，不入库。
// 构建：g++ -std=c++17 -O2 -I. tests/bench_family.cpp core/*.o adapter/*.o -o build/bench-family
// 二进制：int32 n，然后float H[n],L[n],C[n],V[n]；不传路径使用SSE真实C/V。
#include "adapter/czsc_api.h"
#include "core/engine.h"
#include "tests/unit/sse_data.h"
#include <chrono>
#include <cstdio>
#include <vector>
#include <string>
int main(int argc,char **argv)
{
  int n=SSE_DAILY_COUNT;std::vector<float> h,l,c,v;
  const float *hp=SSE_DAILY_HIGH,*lp=SSE_DAILY_LOW,*cp=SSE_DAILY_CLOSE,*vp=SSE_DAILY_VOLUME;
  bool stats=argc>1 && std::string(argv[1])=="--stats";
  if(argc>1 && !stats)
  {
    FILE *f=std::fopen(argv[1],"rb");if(!f)return 1;
    if(std::fread(&n,4,1,f)!=1 || n<0 || n>16777216){std::fclose(f);return 2;}
    h.resize(n);l.resize(n);c.resize(n);v.resize(n);
    for(auto *p:{&h,&l,&c,&v})if(std::fread(p->data(),4,n,f)!=static_cast<std::size_t>(n)){std::fclose(f);return 3;}
    std::fclose(f);hp=h.data();lp=l.data();cp=c.data();vp=v.data();
  }
  if(stats)
  {
    auto source=chan::Series::FromRaw(n,hp,lp,cp,vp);
    for(int rule=0;rule<5;++rule)for(int gap=0;gap<3;++gap)
    {
      chan::AnalysisConfig config;config.stroke.rule=static_cast<chan::StrokeRule>(rule);config.stroke.gap=static_cast<chan::GapRule>(gap);
      if(!chan::Validate(config).empty())continue;
      auto family=chan::AnalyzeFamily(source,config);
      for(int level=0;level<2;++level)
      {
        const auto &a=family.levels[level];const auto &s=a.snapshot;
        std::printf("| %d | %d | %d | %zu | %zu | %zu | %zu |\n",rule,gap,level,s.pivots.size(),s.centers.size(),s.signals.size(),a.events.size());
      }
    }
    return 0;
  }
  czsc_input in{sizeof(in),n,hp,lp,cp,vp};czsc_config config{};czsc_config_default(&config);
  auto build=[&](){void *h=czsc_build(&in,&config,CZSC_OUTPUT_DEFAULT);if(!h){std::fprintf(stderr,"%s\n",czsc_last_error());return false;}czsc_snapshot_free(h);return true;};
  for(int i=0;i<5;++i)if(!build())return 4;
  int reps=n>10000?20:100;auto start=std::chrono::steady_clock::now();for(int i=0;i<reps;++i)if(!build())return 5;
  double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/reps;
  std::printf("new single family+nested: n%d reps%d %.3f us\n",n,reps,us);
}
