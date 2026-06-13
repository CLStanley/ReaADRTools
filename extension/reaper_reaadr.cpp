#define REAPERAPI_IMPLEMENT
#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_AddCustomizableMenu
#define REAPERAPI_WANT_AddRemoveReaScript
#define REAPERAPI_WANT_ShowMessageBox

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif

namespace {

constexpr int kMainSection = 0;
constexpr const char* kReaADRMenuId = "ReaADR Tools";

reaper_plugin_info_t* g_plugin = nullptr;
REAPER_PLUGIN_HINSTANCE g_instance = nullptr;

using CreatePopupMenuFn = HMENU (*)();
using GetMenuItemCountFn = int (*)(HMENU);
using InsertMenuItemFn = void (*)(HMENU, int, BOOL, MENUITEMINFO*);

CreatePopupMenuFn g_create_popup_menu = nullptr;
GetMenuItemCountFn g_get_menu_item_count = nullptr;
InsertMenuItemFn g_insert_menu_item = nullptr;
std::string g_log_path;
bool g_top_level_menu_added = false;

struct ScriptAction {
  const char* label;
  const char* relative_path;
  int command_id = 0;
};

std::vector<ScriptAction> g_actions = {
  {"Open Manager", "ReaADRTools/scripts/ReaADR_Open_Manager.lua", 0},
  {"Quick Action 1", "ReaADRTools/scripts/ReaADR_Quick_Action_1.lua", 0},
  {"Quick Action 2", "ReaADRTools/scripts/ReaADR_Quick_Action_2.lua", 0},
  {"Quick Action 3", "ReaADRTools/scripts/ReaADR_Quick_Action_3.lua", 0},
  {"Quick Action 4", "ReaADRTools/scripts/ReaADR_Quick_Action_4.lua", 0},
};

std::vector<ScriptAction> g_legacy_actions = {
  {"Import Script", "ReaADRTools/scripts/ReaADR_Import_Script.lua", 0},
  {"Export Reports", "ReaADRTools/scripts/ReaADR_Export_Reports.lua", 0},
  {"Preferences", "ReaADRTools/scripts/ReaADR_Preferences.lua", 0},
  {"Import Cue Sheet", "ReaADRTools/scripts/ReaADR_Import_Cue_Sheet.lua", 0},
  {"Export Cue Sheet", "ReaADRTools/scripts/ReaADR_Export_Cue_Sheet.lua", 0},
  {"Next Cue", "ReaADRTools/scripts/ReaADR_Next_Cue.lua", 0},
  {"Previous Cue", "ReaADRTools/scripts/ReaADR_Previous_Cue.lua", 0},
  {"Jump To Cue", "ReaADRTools/scripts/ReaADR_Jump_To_Cue.lua", 0},
  {"Set Cue Status", "ReaADRTools/scripts/ReaADR_Set_Cue_Status.lua", 0},
  {"Character Filter", "ReaADRTools/scripts/ReaADR_Character_Filter.lua", 0},
  {"Generate Cues from Markers/Regions", "ReaADRTools/scripts/ReaADR_Generate_Cues.lua", 0},
  {"Clean Generated Cue Items", "ReaADRTools/scripts/ReaADR_Clean_Generated_Cues.lua", 0},
  {"Overlay Settings", "ReaADRTools/scripts/ReaADR_Overlay_Settings.lua", 0},
  {"Open ReaADR Menu", "ReaADRTools/scripts/ReaADR_Menu.lua", 0},
  {"Start Recording Workflow", "ReaADRTools/scripts/ReaADR_Start_Recording_Workflow.lua", 0},
  {"Monitor New Markers/Regions", "ReaADRTools/scripts/ReaADR_Monitor_Markers.lua", 0},
  {"Jump To Selected Cue", "ReaADRTools/scripts/ReaADR_Jump_To_Selected_Cue.lua", 0},
};

std::string parent_path(const std::string& path)
{
  const std::string::size_type slash = path.find_last_of("/\\");
  if (slash == std::string::npos) return ".";
  if (slash == 0) return path.substr(0, 1);
  return path.substr(0, slash);
}

std::string plugin_directory()
{
#ifdef _WIN32
  char path[MAX_PATH] = {};
  if (g_instance && GetModuleFileNameA(g_instance, path, static_cast<DWORD>(sizeof(path))) > 0) {
    return parent_path(path);
  }
  return ".";
#else
  Dl_info info = {};
  if (dladdr(reinterpret_cast<void*>(&plugin_directory), &info) && info.dli_fname) {
    return parent_path(info.dli_fname);
  }
  return ".";
#endif
}

std::string join_path(const std::string& base, const char* relative)
{
  if (base.empty() || base == ".") return relative;
  return base + "/" + relative;
}

void log_line(const std::string& message)
{
  if (g_log_path.empty()) return;

  FILE* file = std::fopen(g_log_path.c_str(), "a");
  if (!file) return;

  std::fputs(message.c_str(), file);
  std::fputc('\n', file);
  std::fclose(file);
}

void initialize_log_path()
{
  const std::string root = plugin_directory();
  const std::string bundled_log_path = join_path(root, "ReaADRTools/reaper_reaadr.log");
  FILE* file = std::fopen(bundled_log_path.c_str(), "a");
  if (file) {
    g_log_path = bundled_log_path;
    std::fclose(file);
    return;
  }

  g_log_path = join_path(root, "reaper_reaadr.log");
}

void register_scripts()
{
  const std::string root = plugin_directory();
  log_line("Registering bundled scripts from: " + root);

  for (const ScriptAction& legacy_action : g_legacy_actions) {
    const std::string script_path = join_path(root, legacy_action.relative_path);
    AddRemoveReaScript(false, kMainSection, script_path.c_str(), false);
  }

  for (std::size_t i = 0; i < g_actions.size(); ++i) {
    const bool commit = i + 1 == g_actions.size();
    const std::string script_path = join_path(root, g_actions[i].relative_path);
    g_actions[i].command_id = AddRemoveReaScript(true, kMainSection, script_path.c_str(), commit);
    log_line(std::string("Registered ") + g_actions[i].label + " command_id=" + std::to_string(g_actions[i].command_id));
  }
}

void unregister_scripts()
{
  for (std::size_t i = 0; i < g_actions.size(); ++i) {
    const bool commit = i + 1 == g_actions.size();
    const std::string script_path = join_path(plugin_directory(), g_actions[i].relative_path);
    AddRemoveReaScript(false, kMainSection, script_path.c_str(), commit);
    g_actions[i].command_id = 0;
  }
}

void add_menu_item(HMENU menu, int position, const ScriptAction& action)
{
  if (!action.command_id || !g_insert_menu_item) return;

  MENUITEMINFO item = {};
  item.cbSize = sizeof(item);
  item.fMask = MIIM_TYPE | MIIM_ID;
  item.fType = MFT_STRING;
  item.wID = static_cast<UINT>(action.command_id);
  item.dwTypeData = const_cast<char*>(action.label);
  g_insert_menu_item(menu, position, TRUE, &item);
}

void hook_custom_menu(const char* menu_id, void* menu, int flag)

{
  log_line(std::string("hookcustommenu id=") + (menu_id ? menu_id : "(null)") + " flag=" + std::to_string(flag));
  if ((flag != 0 && flag != 1) || !menu_id || !menu) return;
  if (std::strcmp(menu_id, kReaADRMenuId) != 0) return;
  if (!g_create_popup_menu || !g_get_menu_item_count || !g_insert_menu_item) {
    log_line("Menu helpers unavailable; skipping ReaADR Tools menu creation.");
    return;
  }

  int position = 0;
  if (g_top_level_menu_added) return;
  for (const ScriptAction& action : g_actions) {
    add_menu_item(static_cast<HMENU>(menu), position++, action);
  }
  g_top_level_menu_added = true;
  log_line("Added top-level ReaADR Tools menu.");
}

void load_menu_functions()
{
#ifdef _WIN32
  g_create_popup_menu = []() -> HMENU { return CreatePopupMenu(); };
  g_get_menu_item_count = [](HMENU menu) -> int { return GetMenuItemCount(menu); };
  g_insert_menu_item = [](HMENU menu, int position, BOOL by_position, MENUITEMINFO* item) {
    InsertMenuItem(menu, position, by_position, item);
  };
#endif
  log_line(std::string("CreatePopupMenu available: ") + (g_create_popup_menu ? "yes" : "no"));
  log_line(std::string("GetMenuItemCount available: ") + (g_get_menu_item_count ? "yes" : "no"));
  log_line(std::string("InsertMenuItem available: ") + (g_insert_menu_item ? "yes" : "no"));
}

bool load(reaper_plugin_info_t* plugin)
{
  g_plugin = plugin;
  initialize_log_path();
  log_line("Loading ReaADR extension.");
  if (REAPERAPI_LoadAPI(plugin->GetFunc) != 0) {
    log_line("REAPERAPI_LoadAPI failed.");
    return false;
  }
  if (!AddRemoveReaScript) {
    log_line("AddRemoveReaScript API unavailable; cannot register ReaADR scripts.");
    return false;
  }

  if (AddCustomizableMenu) {
    AddCustomizableMenu(kReaADRMenuId, kReaADRMenuId, nullptr, true);
  } else {
    log_line("AddCustomizableMenu API unavailable; actions will register without the top-level menu.");
  }
  register_scripts();
  load_menu_functions();
  if (AddCustomizableMenu) {
    plugin->Register("hookcustommenu", reinterpret_cast<void*>(hook_custom_menu));
  }
  return true;
}

void unload()
{
  log_line("Unloading ReaADR extension.");
  if (g_plugin) {
    g_plugin->Register("-hookcustommenu", reinterpret_cast<void*>(hook_custom_menu));
  }
  unregister_scripts();
  g_top_level_menu_added = false;
  g_plugin = nullptr;
}

} // namespace

extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t* plugin)
{
  if (!plugin) {
    unload();
    return 0;
  }

  g_instance = instance;
  return load(plugin) ? 1 : 0;
}

#ifndef _WIN32
extern "C" REAPER_PLUGIN_DLL_EXPORT int SWELL_dllMain(HINSTANCE, DWORD call_mode, LPVOID get_func)
{
  constexpr DWORD kProcessAttach = 1;
  if (call_mode != kProcessAttach || !get_func) return 1;

  auto api_get_func = reinterpret_cast<void* (*)(const char*)>(get_func);
  g_create_popup_menu = reinterpret_cast<CreatePopupMenuFn>(api_get_func("CreatePopupMenu"));
  g_get_menu_item_count = reinterpret_cast<GetMenuItemCountFn>(api_get_func("GetMenuItemCount"));
  g_insert_menu_item = reinterpret_cast<InsertMenuItemFn>(api_get_func("InsertMenuItem"));
  return 1;
}
#endif
