#pragma once
#include "core/config.h"
#include <map>
#include <string>
namespace tdx
{
struct Preset
{
  chan::LevelConfig view;
  int error=0;
  std::string reason;
};
using PresetMap=std::map<int,Preset>;
PresetMap ParsePresets(const std::string &text);
const Preset *FindPreset(int id);
void ResetPresetsForTesting();
void InstallPresetForTesting(int id,const chan::LevelConfig &view);
void PresetTextForTesting(const std::string &text);
}
