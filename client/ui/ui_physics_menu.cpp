#include "ui_physics_menu.h"
#include "imgui.h"
#include "hud.h"
#include "cvardef.h"
#include "enginecallback.h"
#include "keydefs.h"
#include "physics_step_scheduler.h"

bool CImGuiPhysicsMenu::m_bVisible = false;

static void ApplyRenderingPreset(int mode)
{
    const bool legacy=mode==0,low=mode==1;
    const struct { const char *name;float value; } settings[]={
        {"r_monitor_hz",legacy ? 0.0f : low ? 15.0f : 30.0f},
        {"r_monitor_size",legacy ? 1024.0f : low ? 256.0f : 512.0f},
        {"r_reflection_scale",legacy ? 1.0f : low ? .25f : .5f},
        {"r_reflection_cache",legacy ? 0.0f : 1.0f},
        {"r_shadow_budget",legacy ? -1.0f : low ? 8.0f : 16.0f},
        {"r_shadow_subview_budget",legacy ? -1.0f : low ? 0.0f : 8.0f},
        {"r_shadow_cache",legacy ? 0.0f : 1.0f},
        {"r_shadow_particles",legacy ? 1.0f : 0.0f},
        {"r_grass_shadows",low ? 0.0f : 1.0f}
    };
    for(const auto &setting:settings) gEngfuncs.Cvar_SetValue(setting.name,setting.value);
}

void CImGuiPhysicsMenu::Initialize()
{
    gEngfuncs.pfnAddCommand("r_physics_showmenu", ShowMenu);
    gEngfuncs.pfnAddCommand("r_performance_showmenu", ShowMenu);
}
void CImGuiPhysicsMenu::Terminate() { m_bVisible = false; }
bool CImGuiPhysicsMenu::Active() { return m_bVisible; }
void CImGuiPhysicsMenu::ShowMenu() { m_bVisible = !m_bVisible; }

void CImGuiPhysicsMenu::Draw()
{
    if(ImGui::Begin("Video / Physics performance", &m_bVisible))
    {
        // Only the local server DLL registers these cvars. Do not send a
        // server command or let a remote client's preference change its host.
        cvar_t *rate = CVAR_GET_POINTER("sv_physics_hz");
        if(rate && (rate->flags & FCVAR_EXTDLL))
        {
            const int hz = physicsasset::ResolvePhysicsHz(rate->value);
            if(ImGui::RadioButton("Medium (60 Hz)", hz == 60))
                gEngfuncs.Cvar_SetValue("sv_physics_hz", 60);
            if(ImGui::RadioButton("Low (30 Hz)", hz == 30))
                gEngfuncs.Cvar_SetValue("sv_physics_hz", 30);
            if(ImGui::RadioButton("Legacy (100 Hz)", hz == 100))
                gEngfuncs.Cvar_SetValue("sv_physics_hz", 100);
            ImGui::TextWrapped("Applies to rigid bodies and ragdolls. Rendering continues every display frame.");
        }
        else ImGui::TextWrapped("Start a local game to choose physics performance. Online physics is set by the server.");
        cvar_t *monitor=CVAR_GET_POINTER("r_monitor_hz");
        if(monitor)
        {
            ImGui::TextWrapped("Rendering quality and refresh budgets");
            if(ImGui::RadioButton("Medium rendering (30 Hz monitors)",monitor->value>15)) ApplyRenderingPreset(2);
            if(ImGui::RadioButton("Low rendering (15 Hz monitors)",monitor->value>0 && monitor->value<=15)) ApplyRenderingPreset(1);
            if(ImGui::RadioButton("Legacy rendering (every frame)",monitor->value<=0)) ApplyRenderingPreset(0);
        }
    }
    ImGui::End();
}
bool CImGuiPhysicsMenu::HandleKey(bool keyDown, int keyNumber, const char *)
{
    if(keyDown && keyNumber == K_ESCAPE) m_bVisible = false;
    return false;
}
