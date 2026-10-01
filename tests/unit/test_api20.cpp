#include "check.h"
#include "adapter/czsc_api.h"
#include "sse_data.h"
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include <limits>
namespace
{
template<class T> std::vector<T> Schema(int32_t (*get)(T*,int32_t))
{
  int n=get(nullptr,0); std::vector<T> rows(n); CHECK(get(rows.data(),n)==n); return rows;
}
template<class T> std::vector<T> Read(const T *(*get)(void*,int32_t,int32_t*),void *h,int level)
{
  int32_t n=0;auto *p=get(h,level,&n); if(!n)return {}; return {p,p+n};
}
template<class T> bool Same(const std::vector<T> &a,const std::vector<T> &b)
{
  return a.size()==b.size() && (a.empty() || !std::memcmp(a.data(),b.data(),a.size()*sizeof(T)));
}
}
TEST(Api20ConfigSchemaAndValidation)
{
  CHECK(sizeof(czsc_config)==32); CHECK(sizeof(czsc_projection)==12);
  CHECK(sizeof(czsc_config_field)==92); CHECK(sizeof(czsc_config_choice)==396); CHECK(sizeof(czsc_config_rule)==204);
  CHECK(sizeof(czsc_input)==(sizeof(void*)==8?40:24));
  czsc_config c{}; REQUIRE(czsc_config_default(&c)==0); CHECK(c.segmentMethod==1);
  CHECK(czsc_config_validate(&c)==0);
  int n=czsc_config_id(&c,nullptr,0); REQUIRE(n>1);std::vector<char> id(n);
  CHECK(czsc_config_id(&c,id.data(),n)==n);
  czsc_config parsed{}; CHECK(czsc_config_parse(id.data(),&parsed)==0);CHECK(!std::memcmp(&c,&parsed,sizeof c));
  CHECK(czsc_config_parse("stroke.rule=4k",&parsed)==0);CHECK(parsed.strokeRule==3 && parsed.segmentMethod==1);
  CHECK(czsc_config_parse("stroke.rule=strict;stroke.rule=new",&parsed)!=0);
  CHECK(czsc_config_parse("unknown=0",&parsed)!=0);
  c.gapThreshold=.25f;std::vector<char> normalized(n);CHECK(czsc_config_id(&c,normalized.data(),n)==n);CHECK(normalized==id);
  c.gapThreshold=std::numeric_limits<float>::quiet_NaN();CHECK(czsc_config_validate(&c)!=0);CHECK(std::strlen(czsc_last_error())>0);
  c.gapThreshold=.02f;c.strokeEndpoint=3;CHECK(czsc_config_validate(&c)!=0);
  auto fields=Schema(czsc_config_fields); auto choices=Schema(czsc_config_choices); auto rules=Schema(czsc_config_rules);
  CHECK(fields.size()==13); CHECK(rules.size()>=5);
  std::set<std::string> unique, keys;
  for(const auto &f:fields){CHECK(f.size==sizeof(f));CHECK(unique.insert(f.key).second);keys.insert(f.key);}
  for(const auto &o:choices){CHECK(o.size==sizeof(o));CHECK(unique.insert(o.key).second);CHECK(keys.count(o.field)==1);CHECK(o.note[0]!=0);CHECK(o.original==0 || o.lessons[0]!=0);}
  for(const auto &r:rules){CHECK(keys.count(r.whenField)==1 && keys.count(r.field)==1);CHECK(r.reason[0]!=0);}
}
TEST(Api20FamilyProjectionAndOutputSelection)
{
  czsc_input in{sizeof(in),SSE_DAILY_COUNT,SSE_DAILY_HIGH,SSE_DAILY_LOW,SSE_DAILY_CLOSE,SSE_DAILY_VOLUME};
  czsc_config c{};czsc_config_default(&c);
  void *h=czsc_build(&in,&c,CZSC_OUTPUT_DEFAULT);REQUIRE(h!=nullptr);
  auto raw=Read(czsc_level_pivots,h,1); auto centers=Read(czsc_level_centers,h,1);
  auto events=Read(czsc_level_events,h,1); auto signals=Read(czsc_level_signals,h,1);
  int count=0;auto *nested=czsc_nested_rows(h,&count);std::vector<czsc_nested> original(nested,nested+count);
  czsc_projection p{sizeof(p),1,0};CHECK(czsc_set_projection(h,&p)==0);
  auto projected=Read(czsc_level_pivots,h,1);CHECK(projected.size()==raw.size());
  bool changed=false;for(std::size_t i=0;i<raw.size();++i){CHECK(projected[i].extremeIndex==raw[i].extremeIndex);changed|=projected[i].index!=raw[i].index;}
  CHECK(changed);CHECK(Same(events,Read(czsc_level_events,h,1)));CHECK(Same(signals,Read(czsc_level_signals,h,1)));
  nested=czsc_nested_rows(h,&count);CHECK(Same(original,std::vector<czsc_nested>(nested,nested+count)));
  p.segmentBoundary=0;p.centerBox=1;CHECK(czsc_set_projection(h,&p)==0);
  CHECK(Same(raw,Read(czsc_level_pivots,h,1)));CHECK(Same(centers,Read(czsc_level_centers,h,1)));
  p.segmentBoundary=99;CHECK(czsc_set_projection(h,&p)!=0);CHECK(Same(raw,Read(czsc_level_pivots,h,1)));
  czsc_snapshot_free(h);
  CHECK(czsc_build(&in,&c,0)==nullptr);CHECK(czsc_build(&in,&c,CZSC_OUTPUT_STROKE|CZSC_OUTPUT_NESTED)==nullptr);
  h=czsc_build(&in,&c,CZSC_OUTPUT_STROKE);REQUIRE(h);CHECK(czsc_level_pivots(h,1,&count)==nullptr && count==0);
  CHECK(czsc_level_events(h,0,&count)==nullptr && count==0);czsc_snapshot_free(h);
  c.segmentMethod=0;h=czsc_build(&in,&c,3);REQUIRE(h);p.segmentBoundary=1;CHECK(czsc_set_projection(h,&p)!=0);czsc_snapshot_free(h);
}
