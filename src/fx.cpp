// DarkPicturesFX: a ReShade add-on that turns the game's camera effects on or off
// (depth of field, motion blur, lens flares, chromatic aberration, bloom, light shafts,
// and, experimentally, film grain with vignette) by forcing Unreal Engine 4.26 console
// variables (cvars.hpp; the same code DarkPicturesDLSS uses to force its render scale).
//
// Each effect is one int cvar. "On" leaves the game's own value alone; "off" writes the
// effect's off value into both copies (game and render thread) every frame, so the game's
// graphics menu or scalability changes can't bring it back. Switching it on again puts the
// game's last own value back. No rendering hooks: works on DirectX 11 and 12, with or
// without DarkPicturesDLSS.
//
// Settings: ReShade.ini, section [DarkPicturesFX], one key per effect (1 = on, 0 = off).
// Log: DarkPicturesFX.log next to the game executable.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define ImTextureID ImU64
#include <imgui.h>
#include <reshade.hpp>

#include <cstdarg>
#include <cstdio>
#include <string>

#include "cvars.hpp"
#include "version.h"

extern "C" __declspec(dllexport) const char* NAME = QDLSS_PRODUCT;
extern "C" __declspec(dllexport) const char* DESCRIPTION =
    "Turns camera effects (depth of field, motion blur, lens flares, chromatic aberration, bloom, light "
    "shafts, film grain and vignette) on or off in " QDLSS_GAMES ".";

namespace {

constexpr const char* kSection = QDLSS_PRODUCT;

struct Effect {
    const wchar_t* cvar;
    const char* key;      // ReShade.ini
    const char* label;    // overlay checkbox
    const char* tooltip;
    int32_t off;          // value that disables the effect
    bool enabled = true;  // the user's setting: true leaves the game's value alone
    bool looked = false;
    int32_t* data = nullptr;  // TConsoleVariableData<int32>::ShadowedValue[2]
    int32_t gameValue = 0;    // the game's own value, kept while we aren't forcing
    bool forcing = false;
};

Effect g_effects[] = {
    {L"r.DepthOfFieldQuality", "DepthOfField", "Depth of field",
     "Background and foreground blur, mostly in cutscenes (r.DepthOfFieldQuality 0).", 0},
    {L"r.MotionBlurQuality", "MotionBlur", "Motion blur", "Blur on fast camera and object motion (r.MotionBlurQuality 0).",
     0},
    {L"r.LensFlareQuality", "LensFlares", "Lens flares", "Flares and ghosts from bright lights (r.LensFlareQuality 0).",
     0},
    {L"r.SceneColorFringeQuality", "ChromaticAberration", "Chromatic aberration",
     "Colour fringing towards the screen edges (r.SceneColorFringeQuality 0).", 0},
    {L"r.BloomQuality", "Bloom", "Bloom",
     "Glow around bright areas, including lens dirt (r.BloomQuality 0). Lights look flatter without it.", 0},
    {L"r.LightShaftQuality", "LightShafts", "Light shafts", "God rays from the sun and bright lights (r.LightShaftQuality 0).",
     0},
    {L"r.Tonemapper.Quality", "GrainVignette", "Film grain and vignette (experimental)",
     "Uses the simplest tonemapper (r.Tonemapper.Quality 0), which leaves out grain and vignette.\n"
     "Experimental: grain the game adds in its own post-process pass may stay.",
     0},
};

enum class State { untried, waiting, ready, failed };
State g_state = State::untried;
void** g_singleton = nullptr;
FILE* g_log = nullptr;
std::wstring g_dir;

void Log(const char* fmt, ...) {
    if (!g_log) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "%02u:%02u:%02u.%03u ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    va_end(args);
    fputc('\n', g_log);
    fflush(g_log);
}

void OpenLog() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    g_dir = path;
    g_dir = g_dir.substr(0, g_dir.find_last_of(L'\\') + 1);
    g_log = _wfopen((g_dir + L"" QDLSS_PRODUCT ".log").c_str(), L"w");
    char exe[MAX_PATH * 3] = {};
    WideCharToMultiByte(CP_UTF8, 0, path, -1, exe, sizeof(exe), nullptr, nullptr);
    Log("%s %s", QDLSS_PRODUCT, QDLSS_VERSION);
    Log("exe: %s", exe);
}

void LoadSettings() {
    for (auto& e : g_effects) reshade::get_config_value(nullptr, kSection, e.key, e.enabled);
    std::string line;
    for (const auto& e : g_effects) line += std::string(" ") + e.key + "=" + (e.enabled ? "on" : "off");
    Log("settings:%s", line.c_str());
}

void SaveSettings() {
    for (const auto& e : g_effects) reshade::set_config_value(nullptr, kSection, e.key, e.enabled);
}

// The console manager exists only once the engine is up: retried every frame until then.
void FindCvars() {
    if (g_state == State::untried) {
        g_singleton = cvars::FindConsoleManagerSingleton(reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr)));
        if (!g_singleton) {
            g_state = State::failed;
            Log("cvars: IConsoleManager::Singleton not found; not an Unreal Engine 4.26 game?");
            return;
        }
        g_state = State::waiting;
    }
    void* manager = *g_singleton;
    if (!manager) return;
    int found = 0;
    for (auto& e : g_effects) {
        e.looked = true;
        auto data = static_cast<int32_t*>(cvars::FindData(manager, e.cvar, false));
        // Sanity check before ever writing: these are all small quality levels.
        if (data && data[0] >= 0 && data[0] <= 16 && data[1] >= 0 && data[1] <= 16) {
            e.data = data;
            e.gameValue = data[0];
            ++found;
            Log("cvars: %ls = %d / %d (game / render thread)", e.cvar, data[0], data[1]);
        } else {
            Log("cvars: %ls not found or implausible", e.cvar);
        }
    }
    g_state = found ? State::ready : State::failed;
}

void Apply() {
    if (g_state == State::untried || g_state == State::waiting) FindCvars();
    if (g_state != State::ready) return;
    for (auto& e : g_effects) {
        if (!e.data) continue;
        if (!e.enabled) {
            if (!e.forcing) {
                e.forcing = true;
                Log("%s off: forcing %ls %d (game's value %d)", e.label, e.cvar, e.off, e.gameValue);
            }
            e.data[0] = e.data[1] = e.off;
        } else if (e.forcing) {
            e.forcing = false;
            e.data[0] = e.data[1] = e.gameValue;
            Log("%s on: restored %ls %d", e.label, e.cvar, e.gameValue);
        } else if (e.data[0] != e.gameValue) {
            e.gameValue = e.data[0];  // the game changed it (graphics menu): that's its value now
        }
    }
}

void OnReshadePresent(reshade::api::effect_runtime*) { Apply(); }

void DrawOverlay(reshade::api::effect_runtime*) {
    switch (g_state) {
        case State::ready: break;
        case State::failed:
            ImGui::TextWrapped("The game's settings weren't found (see %s.log). This add-on works with Unreal "
                               "Engine 4.26 games such as The Dark Pictures Anthology.",
                               QDLSS_PRODUCT);
            return;
        default: ImGui::TextUnformatted("Waiting for the game engine..."); return;
    }
    ImGui::TextUnformatted("Camera effects (untick to turn off):");
    bool changed = false;
    for (auto& e : g_effects) {
        if (!e.data) {
            ImGui::BeginDisabled();
            bool unavailable = true;
            ImGui::Checkbox(e.label, &unavailable);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Not available in this game.");
            continue;
        }
        changed |= ImGui::Checkbox(e.label, &e.enabled);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e.tooltip);
    }
    if (changed) SaveSettings();
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            if (!reshade::register_addon(module)) return FALSE;
            OpenLog();
            LoadSettings();
            reshade::register_event<reshade::addon_event::reshade_present>(&OnReshadePresent);
            reshade::register_overlay(QDLSS_PRODUCT, &DrawOverlay);
            break;
        case DLL_PROCESS_DETACH:
            reshade::unregister_overlay(QDLSS_PRODUCT, &DrawOverlay);
            reshade::unregister_addon(module);
            if (g_log) {
                Log("unloading");
                fclose(g_log);
                g_log = nullptr;
            }
            break;
    }
    return TRUE;
}
