#include "hud.h"
#include "cvardef.h"
#include "ui_physics_menu.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
cl_enginefunc_t gEngfuncs{};
extern "C" const char *__asan_default_options(){return "detect_leaks=0:halt_on_error=1";}
static cvar_t monitor={};
static std::map<std::string,float> values;
static std::string clicked;
static int writes=0,commands=0;
namespace ImGui
{
bool Begin(const char *,bool *){return true;}void End(){}
bool RadioButton(const char *name,bool){return clicked==name;}
void TextWrapped(const char *,...){}
}
int main()
{
    monitor.value=30;
    gEngfuncs.pfnAddCommand=[](const char *name,void (*)()){assert(!std::strcmp(name,"r_physics_showmenu") || !std::strcmp(name,"r_performance_showmenu"));++commands;return 1;};
    gEngfuncs.pfnGetCvarPointer=[](const char *name)->cvar_t*{return !std::strcmp(name,"r_monitor_hz")?&monitor:nullptr;};
    gEngfuncs.Cvar_SetValue=[](const char *name,float value){assert(std::strcmp(name,"sv_physics_hz"));values[name]=value;if(!std::strcmp(name,"r_monitor_hz"))monitor.value=value;++writes;};
    CImGuiPhysicsMenu menu;menu.Initialize();assert(commands==2);
    clicked="Low rendering (15 Hz monitors)";menu.Draw();assert(writes==9);
    assert(values["r_monitor_hz"]==15 && values["r_monitor_size"]==256 && values["r_reflection_scale"]==.25f);
    assert(values["r_shadow_budget"]==8 && values["r_shadow_subview_budget"]==0 && values["r_grass_shadows"]==0);
    clicked="Medium rendering (30 Hz monitors)";menu.Draw();assert(writes==18);
    assert(values["r_monitor_hz"]==30 && values["r_monitor_size"]==512 && values["r_reflection_scale"]==.5f);
    assert(values["r_shadow_budget"]==16 && values["r_shadow_subview_budget"]==8 && values["r_grass_shadows"]==1);
    clicked="Legacy rendering (every frame)";menu.Draw();assert(writes==27);
    assert(values["r_monitor_hz"]==0 && values["r_monitor_size"]==1024 && values["r_reflection_scale"]==1);
    assert(values["r_shadow_budget"]==-1 && values["r_shadow_subview_budget"]==-1 && values["r_shadow_cache"]==0 && values["r_shadow_particles"]==1);
    std::cout<<"PASS actual performance menu: both command aliases, Low/Medium/Legacy rendering presets and all nine settings; remote physics untouched\n";
}
