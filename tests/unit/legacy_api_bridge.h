#pragma once
#include "adapter/czsc_api.h"
#include "migration/legacy_config.h"
#include <cstring>
#include <memory>
namespace legacy_test
{
constexpr int Events=1, Higher=2;
struct Input
{
  uint32_t size; int32_t n; const float *high,*low,*close,*volume;
  int32_t config,flags;
};
struct Handle { void *family=nullptr; int level=0; Input input{}; bool nested=false; };
inline void *Build(const Input *in)
{
  if(!in) return czsc_build(nullptr,nullptr,0);
  auto mapped=migration::MapLegacyConfig(in->config);
  if(!mapped || in->size<sizeof(Input) || (in->flags & ~3)) return czsc_build(nullptr,nullptr,0);
  const auto &a=mapped->analysis;
  czsc_config c{sizeof(c),static_cast<int>(a.stroke.rule),static_cast<int>(a.stroke.endpoint),0,.02f,
    static_cast<int>(a.segment.method),static_cast<int>(a.center.strokeFormation),static_cast<int>(a.signals.publication)};
  czsc_input raw{sizeof(raw),in->n,in->high,in->low,in->close,in->volume};
  unsigned outputs=3|CZSC_OUTPUT_NESTED;
  if(in->flags & Events)outputs|=CZSC_OUTPUT_EVENTS;
  if(in->flags & Higher)outputs|=CZSC_OUTPUT_RECURSION;
  void *family=czsc_build(&raw,&c,outputs); if(!family)return nullptr;
  czsc_projection p{sizeof(p),static_cast<int>(mapped->projection.segmentBoundary),1};
  if(czsc_set_projection(family,&p)!=0) { czsc_snapshot_free(family); return nullptr; }
  return new Handle{family,static_cast<int>(mapped->level),*in,false};
}
inline void Free(void *h) { if(h) { auto *s=static_cast<Handle *>(h);czsc_snapshot_free(s->family);delete s; } }
inline void *Nested(void *lo,void *hi)
{
  auto *l=static_cast<Handle *>(lo),*h=static_cast<Handle *>(hi);
  if(!l || !h)return czsc_build(nullptr,nullptr,0);
  const auto &a=l->input,&b=h->input;
  auto bytes=static_cast<std::size_t>(a.n)*sizeof(float);
  if(a.n!=b.n || (bytes && (std::memcmp(a.high,b.high,bytes)||std::memcmp(a.low,b.low,bytes)||
    std::memcmp(a.close,b.close,bytes)||std::memcmp(a.volume,b.volume,bytes)))) return czsc_build(nullptr,nullptr,0);
  auto low=migration::MapLegacyConfig(a.config), high=migration::MapLegacyConfig(b.config);
  if(!low || !high)return czsc_build(nullptr,nullptr,0);
  auto c=*low;c.analysis.segment.method=high->analysis.segment.method;c.projection.segmentBoundary=chan::SegmentEnd::Extreme;
  Input in=a;in.config=migration::LegacyCode(c);in.flags=a.flags|b.flags;
  auto *out=static_cast<Handle *>(Build(&in));if(out)out->nested=true;return out;
}
inline const czsc_nested *NestedRows(void *h,int32_t *n)
{
  if(!h){if(n)*n=0;return czsc_nested_rows(nullptr,n);}
  return czsc_nested_rows(static_cast<Handle *>(h)->family,n);
}
inline const czsc_pivot *Pivots(void *h,int32_t *n)
{
  if(!h)return czsc_level_pivots(nullptr,0,n);
  auto *s=static_cast<Handle *>(h);return czsc_level_pivots(s->family,s->level,n);
}
inline const czsc_center *Centers(void *h,int32_t *n)
{
  if(!h)return czsc_level_centers(nullptr,0,n);
  auto *s=static_cast<Handle *>(h);return czsc_level_centers(s->family,s->level,n);
}
inline const czsc_movement *Movements(void *h,int32_t *n)
{
  if(!h)return czsc_level_movements(nullptr,0,n);
  auto *s=static_cast<Handle *>(h);return czsc_level_movements(s->family,s->level,n);
}
inline const czsc_breakout *Breakouts(void *h,int32_t *n)
{
  if(!h)return czsc_level_breakouts(nullptr,0,n);
  auto *s=static_cast<Handle *>(h);return czsc_level_breakouts(s->family,s->level,n);
}
inline const czsc_signal *Signals(void *h,int32_t *n)
{
  if(!h)return czsc_level_signals(nullptr,0,n);
  auto *s=static_cast<Handle *>(h);return czsc_level_signals(s->family,s->level,n);
}
inline const czsc_event *EventsTable(void *h,int32_t *n)
{
  if(!h)return czsc_level_events(nullptr,0,n);
  auto *s=static_cast<Handle *>(h);return czsc_level_events(s->family,s->level,n);
}
inline const czsc_bar *Bars(void *h,int32_t *n)
{
  if(!h)return czsc_level_bars(nullptr,0,n);
  auto *s=static_cast<Handle *>(h);return czsc_level_bars(s->family,s->level,n);
}
inline const czsc_recursive_node *RecursiveNodes(void *h,int32_t *n)
{
  if(!h)return czsc_level_recursive_nodes(nullptr,0,n);
  auto *s=static_cast<Handle *>(h);return czsc_level_recursive_nodes(s->family,s->level,n);
}
inline const int32_t *RecursiveChildren(void *h,int32_t *n)
{
  if(!h)return czsc_level_recursive_children(nullptr,0,n);
  auto *s=static_cast<Handle *>(h);return czsc_level_recursive_children(s->family,s->level,n);
}
inline const czsc_recursive_center *RecursiveCenters(void *h,int32_t *n)
{
  if(!h)return czsc_level_recursive_centers(nullptr,0,n);
  auto *s=static_cast<Handle *>(h);return czsc_level_recursive_centers(s->family,s->level,n);
}
inline const czsc_recursive_connection *RecursiveConnections(void *h,int32_t *n)
{
  if(!h)return czsc_level_recursive_connections(nullptr,0,n);
  auto *s=static_cast<Handle *>(h);return czsc_level_recursive_connections(s->family,s->level,n);
}
}
