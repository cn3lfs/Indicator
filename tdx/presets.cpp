// 工程预设：编号只作查表，分析身份由结构化字段确定，INI按UTF-8读取。
#include "presets.h"
#include <cstdio>
#include <cwchar>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif
namespace tdx
{
namespace
{
PresetMap presets;
bool loaded=false;
std::string Trim(const std::string &s)
{
  auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");
  return a==std::string::npos?std::string{}:s.substr(a,b-a+1);
}
PresetMap Builtins()
{
  PresetMap result;
  result[0].view.projection.centerBox=chan::CenterBox::Initial;
  result[1]=result[0];result[1].view.level=chan::CenterUnit::Segment;
  result[2]=result[0];result[2].view.analysis.center.strokeFormation=chan::CenterFormation::Segment;
  result[2].view.analysis.signals.publication=chan::SignalPublication::Early;
  return result;
}
std::string SetField(chan::LevelConfig &c,const std::string &key,const std::string &value)
{
  if(key=="outputs.levels")
  {
    if(value=="stroke")c.level=chan::CenterUnit::Stroke;
    else if(value=="segment")c.level=chan::CenterUnit::Segment;
    else return "通达信预设视图须选择stroke或segment";
  }
  else if(key=="projection.segmentBoundary")
  {
    if(value=="extreme")c.projection.segmentBoundary=chan::SegmentEnd::Extreme;
    else if(value=="first")c.projection.segmentBoundary=chan::SegmentEnd::First;
    else if(value=="last")c.projection.segmentBoundary=chan::SegmentEnd::Last;
    else return "分界投影取值无效";
  }
  else if(key=="projection.centerBox")
  {
    if(value=="initial")c.projection.centerBox=chan::CenterBox::Initial;
    else if(value=="extended")c.projection.centerBox=chan::CenterBox::Extended;
    else return "中枢框取值无效";
  }
  else if(key=="outputs.events" || key=="outputs.recursion" || key=="outputs.nested")
  {
    if(value!="on" && value!="off")return "输出开关取值无效";
    bool on=value=="on";
    if(key=="outputs.events")c.outputs.events=on;
    else if(key=="outputs.recursion")c.outputs.recursion=on;
    else c.outputs.nested=on;
  }
  else return chan::ApplyAnalysisField(c.analysis,key,value);
  return {};
}
void Load()
{
  if(loaded)return;loaded=true;presets=Builtins();
#ifdef _WIN32
  // 从本DLL内静态对象地址定位模块，不取宿主EXE目录，不依赖当前工作目录。
  HMODULE module=nullptr;
  if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(&loaded),&module))return;
  wchar_t path[32768];DWORD n=GetModuleFileNameW(module,path,32768);
  if(n==0 || n>=32768)return;
  wchar_t *separator=std::wcsrchr(path,L'\\');
  if(!separator)return;
  const wchar_t name[]=L"czsc-presets.ini";
  if(static_cast<std::size_t>(separator-path)+1+sizeof(name)/sizeof(*name)>32768)return;
  std::wcscpy(separator+1,name);
  FILE *file=_wfopen(path,L"rb");
  if(!file)
  {
    DWORD attributes=GetFileAttributesW(path),error=GetLastError();
    if(attributes==INVALID_FILE_ATTRIBUTES && (error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND))return;
    for(auto &item:presets) { item.second.error=3;item.second.reason="预设文件无法读取"; }
    return;
  }
  std::string text;char buffer[4096];std::size_t bytes;
  while((bytes=std::fread(buffer,1,sizeof buffer,file))!=0)text.append(buffer,bytes);
  bool failed=std::ferror(file)!=0;
  if(std::fclose(file)!=0)failed=true;
  if(failed)
  {
    for(auto &item:presets) { item.second.error=3;item.second.reason="预设文件无法读取"; }
    return;
  }
  presets=ParsePresets(text);
#endif
}
}
PresetMap ParsePresets(const std::string &input)
{
  auto result=Builtins();std::string text=input;
  if(text.compare(0,3,"\xEF\xBB\xBF")==0)text.erase(0,3);
  std::size_t start=0;int current=-1;
  std::map<int,std::set<std::string>> keys;
  std::set<int> sections;
  while(start<text.size())
  {
    auto end=text.find('\n',start);
    auto line=Trim(text.substr(start,end==std::string::npos ? end : end-start));
    start=end==std::string::npos ? text.size() : end+1;if(line.empty() || line[0]=='#' || line[0]==';')continue;
    if(line.front()=='[' && line.back()==']')
    {
      auto value=line.substr(1,line.size()-2);
      current=-1;
      if(value.empty() || value.size()>4 || value.find_first_not_of("0123456789")!=std::string::npos)continue;
      current=0;for(char digit:value)current=current*10+digit-'0';
      if(!sections.insert(current).second){result[current].error=4;result[current].reason="预设节重复";}
      if(!result.count(current)) { result[current]=Preset{};result[current].view.projection.centerBox=chan::CenterBox::Initial; }
      continue;
    }
    if(current<0)continue;
    auto &p=result[current];auto at=line.find('=');
    std::string error;
    if(at==std::string::npos)error="预设字段缺少等号";
    else
    {
      auto key=Trim(line.substr(0,at)),value=Trim(line.substr(at+1));
      if(!keys[current].insert(key).second)error="预设字段重复";
      else error=SetField(p.view,key,value);
    }
    if(!error.empty()){p.error=4;p.reason=error;}
  }
  for(auto &item:result)
  {
    auto &p=item.second;if(p.error)continue;
    auto error=chan::Validate(p.view.analysis);
    if(error.empty() && p.view.analysis.segment.method==chan::SegmentMethod::Heuristic &&
        p.view.projection.segmentBoundary!=chan::SegmentEnd::Extreme)error="启发式线段只能显示极值分界";
    if(!error.empty()){p.error=4;p.reason=error;}
    else p.view.analysis=chan::Normalize(p.view.analysis);
  }
  return result;
}
const Preset *FindPreset(int id)
{
  Load();auto it=presets.find(id);return it==presets.end()?nullptr:&it->second;
}
void ResetPresetsForTesting(){presets.clear();loaded=false;}
void InstallPresetForTesting(int id,const chan::LevelConfig &view){Load();presets[id]=Preset{view,0,{}};}
void PresetTextForTesting(const std::string &text){presets=ParsePresets(text);loaded=true;}
}
