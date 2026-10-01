// v20配置自描述，界面使用稳定字段key与全局唯一选项key；非原文与工程选择original=0。
#include "czsc_api.h"
#include <algorithm>
namespace
{
template<class T, unsigned N> int32_t Copy(T *out, int32_t cap, const T (&rows)[N])
{
  if(cap<0 || (!out && cap>0)) { czsc_config_validate(nullptr); return 0; }
  if(!out)return N;
  int32_t n=std::min(cap,static_cast<int32_t>(N));
  for(int32_t i=0;i<n;++i)out[i]=rows[i];
  return n;
}
}
extern "C" int32_t czsc_config_fields(czsc_config_field *out, int32_t cap)
{
  static const czsc_config_field rows[] = {
    {sizeof(czsc_config_field),"stroke.rule","笔算法",0,0,0,0,0,0},
    {sizeof(czsc_config_field),"stroke.endpoint","笔端点",0,0,0,0,0,0},
    {sizeof(czsc_config_field),"stroke.gap","笔中缺口",0,0,0,0,0,0},
    {sizeof(czsc_config_field),"stroke.gapThreshold","大缺口阈值",0,1,0,0.02f,0,1},
    {sizeof(czsc_config_field),"segment.method","线段算法",0,0,1,0,0,0},
    {sizeof(czsc_config_field),"center.strokeFormation","笔中枢构成",0,0,0,0,0,0},
    {sizeof(czsc_config_field),"signals.publication","信号发布",0,0,0,0,0,0},
    {sizeof(czsc_config_field),"outputs.levels","输出级别",1,0,3,0,0,0},
    {sizeof(czsc_config_field),"outputs.events","事件流",1,0,1,0,0,0},
    {sizeof(czsc_config_field),"outputs.recursion","递归表",1,0,1,0,0,0},
    {sizeof(czsc_config_field),"outputs.nested","区间套",1,0,1,0,0,0},
    {sizeof(czsc_config_field),"projection.segmentBoundary","线段分界点",2,0,0,0,0,0},
    {sizeof(czsc_config_field),"projection.centerBox","中枢框",2,0,1,0,0,0},
  };
  return Copy(out,cap,rows);
}
extern "C" int32_t czsc_config_choices(czsc_config_choice *out, int32_t cap)
{
  static const czsc_config_choice rows[] = {
    {sizeof(czsc_config_choice),"stroke.rule",0,"stroke.rule.strict","严格笔","62/65",1,"包含处理后两端分型间至少一根独立K线。"},
    {sizeof(czsc_config_choice),"stroke.rule",1,"stroke.rule.new","新笔","",0,"社区口径：合并跨度不少于3且原始跨度不少于4。"},
    {sizeof(czsc_config_choice),"stroke.rule",2,"stroke.rule.czsc","czsc笔","",0,"社区口径：采用czsc的分型互不包含约束。"},
    {sizeof(czsc_config_choice),"stroke.rule",3,"stroke.rule.4k","4K笔","",0,"社区口径：原始极值下标差至少3，分型不共用合并K线。"},
    {sizeof(czsc_config_choice),"stroke.rule",4,"stroke.rule.fractal","分型笔","",0,"社区口径：相邻异型分型直接连接。"},
    {sizeof(czsc_config_choice),"stroke.endpoint",0,"stroke.endpoint.extreme","极值延伸","65",1,"同型分型向更极端者延伸。"},
    {sizeof(czsc_config_choice),"stroke.endpoint",1,"stroke.endpoint.first","次高次低","",0,"社区口径：保留首个同型分型。"},
    {sizeof(czsc_config_choice),"stroke.endpoint",2,"stroke.endpoint.bounded","区间包络","",0,"社区口径：合并K线包络，原始影线只作未确认末端延伸；端点链可回退。"},
    {sizeof(czsc_config_choice),"stroke.gap",0,"stroke.gap.none","不处理","",0,"缺口不改变成笔跨度，保持旧分析。"},
    {sizeof(czsc_config_choice),"stroke.gap",1,"stroke.gap.asbar","缺口计作1根","",0,"社区口径：端点闭区间内每个同向原始K线缺口，同时给合并与原始跨度加1。"},
    {sizeof(czsc_config_choice),"stroke.gap",2,"stroke.gap.large","大缺口成笔","",0,"社区口径：包括小缺口计数，比例严格大于gapThreshold（默认2%）时跳过跨度；其他成笔约束仍适用。"},
    {sizeof(czsc_config_choice),"segment.method",0,"segment.method.heuristic","启发式","",0,"保护点启发式，非第67/71课的特征序列规则。"},
    {sizeof(czsc_config_choice),"segment.method",1,"segment.method.feature","特征序列","67/71",1,"按特征序列分型、起点被破及有缺口新极值判定。"},
    {sizeof(czsc_config_choice),"center.strokeFormation",0,"centerFormation.entry","按进入段","",0,"社区笔中枢抽象；首三构件在进入段之后。"},
    {sizeof(czsc_config_choice),"center.strokeFormation",1,"centerFormation.segment","服从所属线段","",0,"社区口径：按真实极值归属父线段，三构件与延伸不得越界。线段中枢仍按进入段。"},
    {sizeof(czsc_config_choice),"signals.publication",0,"signals.publication.standard","标准","",0,"工程发布策略：等待端点确认；允许后续撤销。"},
    {sizeof(czsc_config_choice),"signals.publication",1,"signals.publication.early","快速","",0,"工程发布策略：已成立分型上发布概率信号；影线候选不发布，可失败撤销。"},
    {sizeof(czsc_config_choice),"outputs.levels",1,"outputs.levels.stroke","笔级","",0,"仅投出笔级表；结构算法不变。"},
    {sizeof(czsc_config_choice),"outputs.levels",2,"outputs.levels.segment","线段级","",0,"仅投出线段级表；结构算法不变。"},
    {sizeof(czsc_config_choice),"outputs.levels",3,"outputs.levels.both","两级","",0,"同一次分析共享笔层，两级表分别读取。"},
    {sizeof(czsc_config_choice),"projection.segmentBoundary",0,"boundary.extreme","极值笔","67",1,"真实极值作为显示坐标。"},
    {sizeof(czsc_config_choice),"projection.segmentBoundary",1,"boundary.first","合并首笔","",0,"社区显示投影：只改端点index/price；中枢框仍用极值，分析仍按extremeIndex。"},
    {sizeof(czsc_config_choice),"projection.segmentBoundary",2,"boundary.last","合并末笔","",0,"社区显示投影：取合并特征元素末笔；分析不变。"},
    {sizeof(czsc_config_choice),"projection.centerBox",0,"projection.centerBox.initial","前三构件","",0,"只画成枢前三构件，延伸与背驰分析保持。"},
    {sizeof(czsc_config_choice),"projection.centerBox",1,"projection.centerBox.extended","含延伸","",0,"画到最后成员，保留旧C表坐标。"},
    {sizeof(czsc_config_choice),"outputs.events",0,"outputs.events.off","关闭","",0,"输出选择，不改变结构或信号判断。"},
    {sizeof(czsc_config_choice),"outputs.events",1,"outputs.events.on","开启","",0,"输出选择，不改变结构或信号判断。"},
    {sizeof(czsc_config_choice),"outputs.recursion",0,"outputs.recursion.off","关闭","",0,"输出选择，不改变结构或信号判断。"},
    {sizeof(czsc_config_choice),"outputs.recursion",1,"outputs.recursion.on","开启","",0,"输出选择，不改变结构或信号判断。"},
    {sizeof(czsc_config_choice),"outputs.nested",0,"outputs.nested.off","关闭","",0,"输出选择，不改变结构或信号判断。"},
    {sizeof(czsc_config_choice),"outputs.nested",1,"outputs.nested.on","开启","",0,"输出选择，不改变结构或信号判断。"},
  };
  return Copy(out,cap,rows);
}
extern "C" int32_t czsc_config_rules(czsc_config_rule *out, int32_t cap)
{
  static const czsc_config_rule rows[] = {
    {sizeof(czsc_config_rule),"stroke.gap",0,"stroke.gapThreshold",-1,"不处理缺口时阈值不适用。"},
    {sizeof(czsc_config_rule),"stroke.gap",1,"stroke.gapThreshold",-1,"计作1根时阈值不适用。"},
    {sizeof(czsc_config_rule),"stroke.rule",4,"stroke.gap",0,"分型笔没有跨度门槛，缺口选项必须为不处理。"},
    {sizeof(czsc_config_rule),"segment.method",0,"projection.segmentBoundary",0,"启发式线段没有合并特征元素，只能显示极值。"},
    {sizeof(czsc_config_rule),"outputs.nested",1,"outputs.levels",3,"区间套必须同时选择笔级和线段级。"},
  };
  return Copy(out,cap,rows);
}
