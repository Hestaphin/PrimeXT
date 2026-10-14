#include "hud.h"
#include "cvardef.h"
#include "ui_physics_menu.h"
#include "keydefs.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

cl_enginefunc_t gEngfuncs{};
extern "C" const char *__asan_default_options() { return "detect_leaks=0:halt_on_error=1"; }
static cvar_t rate={const_cast<char *>("sv_physics_hz"),const_cast<char *>("60"),FCVAR_EXTDLL|FCVAR_ARCHIVE,60,nullptr};
static cvar_t *available=&rate;
static void (*openMenu)()=nullptr;
static std::string clicked;
static bool expanded=true;
static int writes=0,ends=0,buttons=0;
namespace ImGui
{
bool Begin(const char *, bool *) { return expanded; }
void End() { ++ends; }
bool RadioButton(const char *label, bool) { ++buttons;return clicked==label; }
void TextWrapped(const char *, ...) {}
}
int main()
{
    gEngfuncs.pfnAddCommand=[](const char *name,void (*command)()){assert(std::strcmp(name,"r_physics_showmenu")==0 || std::strcmp(name,"r_performance_showmenu")==0);openMenu=command;return 1;};
    gEngfuncs.pfnGetCvarPointer=[](const char *name){return std::strcmp(name,"sv_physics_hz")==0 ? available : nullptr;};
    gEngfuncs.Cvar_SetValue=[](const char *name,float value){assert(std::strcmp(name,"sv_physics_hz")==0);++writes;rate.value=value;};
    CImGuiPhysicsMenu menu;menu.Initialize();assert(openMenu && !menu.Active());openMenu();assert(menu.Active());
    for(const char *label:{"Low (30 Hz)","Medium (60 Hz)","Legacy (100 Hz)"})
    {
        clicked=label;menu.Draw();assert(menu.Active());
        assert(rate.value==(clicked=="Low (30 Hz)"?30:clicked=="Medium (60 Hz)"?60:100));
    }
    assert(writes==3 && ends==3 && buttons==9);
    expanded=false;menu.Draw();assert(writes==3 && buttons==9 && ends==4);expanded=true;
    available=nullptr;clicked="Low (30 Hz)";menu.Draw();assert(writes==3 && buttons==9);
    available=&rate;rate.flags=FCVAR_SERVER;menu.Draw();assert(writes==3 && buttons==9); // remote/engine-only cvar
    menu.HandleKey(false,K_ESCAPE,nullptr);assert(menu.Active());
    assert(!menu.HandleKey(true,K_ESCAPE,nullptr) && !menu.Active());openMenu();assert(menu.Active());
    menu.Terminate();assert(!menu.Active());
    std::cout<<"PASS actual client physics menu: local 30/60/100 selection, archived server cvar writes, collapsed window, missing/remote cvar, command and Escape lifecycle (recording ImGui)\n";
}
