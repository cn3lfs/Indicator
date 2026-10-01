// 一次性v10整数记录迁移工具；不作为DLL接口，v20生产核心不依赖它。
#pragma once
#include "core/config.h"
#include <optional>

namespace migration
{
inline std::optional<chan::LevelConfig> MapLegacyConfig(int code)
{
  if (code<0 || code>1129999) return std::nullopt;
  int d0=code%10,d1=(code/10)%10,d2=(code/100)%10,d3=(code/1000)%10;
  int d4=(code/10000)%10,d5=(code/100000)%10,d6=code/1000000;
  if (d6>1 || (d6==1 && d1==1) || d5>1 || d4>2 || (d3==0 && d4!=0) || d0>4 || d1>1 || d2>1 || d3>1)
    return std::nullopt;
  chan::LevelConfig c;
  c.analysis.stroke.rule=static_cast<chan::StrokeRule>(d0);
  c.analysis.stroke.endpoint=d6 ? chan::StrokeEnd::Bounded : static_cast<chan::StrokeEnd>(d1);
  c.level=static_cast<chan::CenterUnit>(d2);
  c.analysis.segment.method=static_cast<chan::SegmentMethod>(d3);
  c.projection.segmentBoundary=static_cast<chan::SegmentEnd>(d4);
  c.analysis.center.strokeFormation=static_cast<chan::CenterFormation>(d5);
  return c;
}
// 仅回归工具核对映射，不支持新缺口配置。
inline int LegacyCode(const chan::LevelConfig &c)
{
  if (c.analysis.stroke.gap!=chan::GapRule::None) return -1;
  return static_cast<int>(c.analysis.stroke.rule)+(c.analysis.stroke.endpoint==chan::StrokeEnd::First ? 10 : 0)+
    static_cast<int>(c.level)*100+static_cast<int>(c.analysis.segment.method)*1000+
    static_cast<int>(c.projection.segmentBoundary)*10000+static_cast<int>(c.analysis.center.strokeFormation)*100000+
    (c.analysis.stroke.endpoint==chan::StrokeEnd::Bounded ? 1000000 : 0);
}
}
