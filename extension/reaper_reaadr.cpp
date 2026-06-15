#define REAPERAPI_IMPLEMENT
#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_AddCustomizableMenu
#define REAPERAPI_WANT_AddRemoveReaScript
#define REAPERAPI_WANT_CountSelectedMediaItems
#define REAPERAPI_WANT_CreateTakeAudioAccessor
#define REAPERAPI_WANT_DestroyAudioAccessor
#define REAPERAPI_WANT_GetActiveTake
#define REAPERAPI_WANT_GetAudioAccessorEndTime
#define REAPERAPI_WANT_GetAudioAccessorSamples
#define REAPERAPI_WANT_GetAudioAccessorStartTime
#define REAPERAPI_WANT_GetExtState
#define REAPERAPI_WANT_GetMediaItemInfo_Value
#define REAPERAPI_WANT_GetSelectedMediaItem
#define REAPERAPI_WANT_ShowMessageBox

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
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
using SetMenuItemInfoFn = BOOL (*)(HMENU, UINT, BOOL, MENUITEMINFO*);

CreatePopupMenuFn g_create_popup_menu = nullptr;
GetMenuItemCountFn g_get_menu_item_count = nullptr;
InsertMenuItemFn g_insert_menu_item = nullptr;
SetMenuItemInfoFn g_set_menu_item_info = nullptr;
std::string g_log_path;
const char kDetectDialogueSegmentsDef[] =
  "bool\0"
  "double,double,double,double,int,char*,int,char*,int\0"
  "threshold_db,min_speech_seconds,min_silence_seconds,pad_seconds,sample_rate,segmentsOut,segmentsOut_sz,errorOut,errorOut_sz\0"
  "Detect dialogue segments from the first selected media item and return timeline start/end pairs as tab-delimited lines.";

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

#ifdef _WIN32
std::string module_directory_from_instance(REAPER_PLUGIN_HINSTANCE instance)
{
  char path[MAX_PATH] = {};
  if (instance && GetModuleFileNameA(instance, path, static_cast<DWORD>(sizeof(path))) > 0) {
    return parent_path(path);
  }
  return ".";
}
#endif

std::string plugin_directory()
{
#ifdef _WIN32
  return module_directory_from_instance(g_instance);
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

#ifdef _WIN32
void log_windows_dll_load(REAPER_PLUGIN_HINSTANCE instance)
{
  const std::string path = join_path(module_directory_from_instance(instance), "reaper_reaadr_dll_load.log");
  FILE* file = std::fopen(path.c_str(), "a");
  if (!file) return;

  std::fputs("DllMain process attach reached.\n", file);
  std::fclose(file);
}
#endif

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

std::string quick_action_label(int slot)
{
  if (!GetExtState) return "Quick Action " + std::to_string(slot);

  const std::string key = "quick_action_" + std::to_string(slot);
  const char* value = GetExtState("ReaADRTools", key.c_str());
  const std::string action_key = value ? value : "";

  if (action_key == "import" || (action_key.empty() && slot == 1)) return "Quick Action " + std::to_string(slot) + ": Import Cue Sheet";
  if (action_key == "cue_manager" || (action_key.empty() && slot == 2)) return "Quick Action " + std::to_string(slot) + ": Open Cue Manager";
  if (action_key == "export_reports" || (action_key.empty() && slot == 3)) return "Quick Action " + std::to_string(slot) + ": Export Reports";
  if (action_key == "overlay_settings" || (action_key.empty() && slot == 4)) return "Quick Action " + std::to_string(slot) + ": Video Overlays Tab";
  if (action_key == "character_filter") return "Quick Action " + std::to_string(slot) + ": Character Filter";
  if (action_key == "refresh_overlay") return "Quick Action " + std::to_string(slot) + ": Refresh Video Overlay";
  if (action_key == "validate") return "Quick Action " + std::to_string(slot) + ": Validate Session";
  return "Quick Action " + std::to_string(slot);
}

void copy_to_buffer(const std::string& value, char* buffer, int buffer_size)
{
  if (!buffer || buffer_size <= 0) return;
  std::snprintf(buffer, static_cast<std::size_t>(buffer_size), "%s", value.c_str());
}

bool detect_dialogue_segments(double threshold_db,
                              double min_speech_seconds,
                              double min_silence_seconds,
                              double pad_seconds,
                              int sample_rate,
                              char* segments_out,
                              int segments_out_sz,
                              char* error_out,
                              int error_out_sz)
{
  copy_to_buffer("", segments_out, segments_out_sz);
  copy_to_buffer("", error_out, error_out_sz);

  if (!CountSelectedMediaItems || !GetSelectedMediaItem || !GetActiveTake || !CreateTakeAudioAccessor ||
      !DestroyAudioAccessor || !GetAudioAccessorStartTime || !GetAudioAccessorEndTime ||
      !GetAudioAccessorSamples || !GetMediaItemInfo_Value) {
    copy_to_buffer("Required REAPER audio accessor APIs are unavailable.", error_out, error_out_sz);
    return false;
  }

  if (CountSelectedMediaItems(nullptr) <= 0) {
    copy_to_buffer("Select one audio or video media item to analyze.", error_out, error_out_sz);
    return false;
  }

  MediaItem* item = GetSelectedMediaItem(nullptr, 0);
  MediaItem_Take* take = item ? GetActiveTake(item) : nullptr;
  if (!item || !take) {
    copy_to_buffer("The selected media item does not have an active take.", error_out, error_out_sz);
    return false;
  }

  AudioAccessor* accessor = CreateTakeAudioAccessor(take);
  if (!accessor) {
    copy_to_buffer("Could not create an audio accessor for the selected media.", error_out, error_out_sz);
    return false;
  }

  struct Segment {
    double start_time = 0.0;
    double end_time = 0.0;
  };

  const int safe_sample_rate = sample_rate > 0 ? sample_rate : 12000;
  const int channels = 1;
  const int block_samples = std::max(64, static_cast<int>(safe_sample_rate * 0.025 + 0.5));
  const double block_duration = static_cast<double>(block_samples) / static_cast<double>(safe_sample_rate);
  const double threshold = std::pow(10.0, threshold_db / 20.0);
  const double min_speech = std::max(0.0, min_speech_seconds);
  const double min_silence = std::max(0.0, min_silence_seconds);
  const double pad = std::max(0.0, pad_seconds);
  const double item_position = GetMediaItemInfo_Value(item, "D_POSITION");
  const double item_length = std::max(0.0, GetMediaItemInfo_Value(item, "D_LENGTH"));
  const double item_end = item_position + item_length;
  const double start_time = GetAudioAccessorStartTime(accessor);
  const double end_time = std::min(GetAudioAccessorEndTime(accessor), start_time + item_length);

  if (end_time <= start_time) {
    DestroyAudioAccessor(accessor);
    copy_to_buffer("The selected media item has no readable audio in the placed item range.", error_out, error_out_sz);
    return false;
  }

  std::vector<double> buffer(static_cast<std::size_t>(block_samples) * static_cast<std::size_t>(channels), 0.0);
  std::vector<Segment> segments;
  double active_start = -1.0;
  double last_loud_end = -1.0;
  double t = start_time;

  auto timeline_time = [item_position, start_time](double accessor_time) {
    return item_position + std::max(0.0, accessor_time - start_time);
  };

  auto flush_segment = [&]() {
    if (active_start >= 0.0 && last_loud_end >= 0.0 && (last_loud_end - active_start) >= min_speech) {
      Segment segment;
      segment.start_time = std::max(item_position, timeline_time(active_start - pad));
      segment.end_time = std::min(item_end, timeline_time(last_loud_end + pad));
      if (segment.end_time > segment.start_time) {
        segments.push_back(segment);
      }
    }
    active_start = -1.0;
    last_loud_end = -1.0;
  };

  while (t < end_time) {
    std::fill(buffer.begin(), buffer.end(), 0.0);
    const double block_end = std::min(end_time, t + block_duration);
    const int got = GetAudioAccessorSamples(accessor, safe_sample_rate, channels, t, block_samples, buffer.data());
    bool loud = false;

    if (got < 0) {
      DestroyAudioAccessor(accessor);
      copy_to_buffer("REAPER returned an error while reading the selected media.", error_out, error_out_sz);
      return false;
    }

    if (got == 1) {
      double sum = 0.0;
      for (double sample : buffer) {
        sum += sample * sample;
      }
      const double rms = std::sqrt(sum / static_cast<double>(block_samples));
      loud = rms >= threshold;
    }

    if (loud) {
      if (active_start < 0.0) active_start = t;
      last_loud_end = block_end;
    } else if (active_start >= 0.0 && last_loud_end >= 0.0 && (t - last_loud_end) >= min_silence) {
      flush_segment();
    }

    t = block_end;
  }

  flush_segment();
  DestroyAudioAccessor(accessor);

  std::ostringstream output;
  output.setf(std::ios::fixed);
  output.precision(9);
  for (const Segment& segment : segments) {
    output << segment.start_time << '\t' << segment.end_time << '\n';
  }

  const std::string text = output.str();
  if (static_cast<int>(text.size()) >= segments_out_sz) {
    copy_to_buffer("Detected segment output exceeded the provided buffer size.", error_out, error_out_sz);
    return false;
  }

  copy_to_buffer(text, segments_out, segments_out_sz);
  return true;
}

void add_menu_item_with_label(HMENU menu, int position, const ScriptAction& action, const std::string& label)
{
  if (!action.command_id || !g_insert_menu_item) return;

  MENUITEMINFO item = {};
  item.cbSize = sizeof(item);
  item.fMask = MIIM_TYPE | MIIM_ID;
  item.fType = MFT_STRING;
  item.wID = static_cast<UINT>(action.command_id);
  item.dwTypeData = const_cast<char*>(label.c_str());
  g_insert_menu_item(menu, position, TRUE, &item);
}

void update_menu_item_label(HMENU menu, int position, const ScriptAction& action, const std::string& label)
{
  if (!action.command_id || !g_set_menu_item_info) return;

  MENUITEMINFO item = {};
  item.cbSize = sizeof(item);
  item.fMask = MIIM_TYPE | MIIM_ID;
  item.fType = MFT_STRING;
  item.wID = static_cast<UINT>(action.command_id);
  item.dwTypeData = const_cast<char*>(label.c_str());
  g_set_menu_item_info(menu, static_cast<UINT>(position), TRUE, &item);
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

  HMENU hmenu = static_cast<HMENU>(menu);
  int position = 0;
  const int existing_items = g_get_menu_item_count ? g_get_menu_item_count(hmenu) : 0;

  if (existing_items <= 0) {
    for (std::size_t i = 0; i < g_actions.size(); ++i) {
      if (i == 0) {
        add_menu_item(hmenu, position++, g_actions[i]);
      } else {
        add_menu_item_with_label(hmenu, position++, g_actions[i], quick_action_label(static_cast<int>(i)));
      }
    }
    log_line("Added top-level ReaADR Tools menu.");
    return;
  }

  for (std::size_t i = 1; i < g_actions.size(); ++i) {
    if (static_cast<int>(i) < existing_items) {
      update_menu_item_label(hmenu, static_cast<int>(i), g_actions[i], quick_action_label(static_cast<int>(i)));
    } else {
      add_menu_item_with_label(hmenu, position++, g_actions[i], quick_action_label(static_cast<int>(i)));
    }
  }
  log_line("Updated top-level ReaADR quick-action labels.");
}

void load_menu_functions()
{
#ifdef _WIN32
  g_create_popup_menu = []() -> HMENU { return CreatePopupMenu(); };
  g_get_menu_item_count = [](HMENU menu) -> int { return GetMenuItemCount(menu); };
  g_insert_menu_item = [](HMENU menu, int position, BOOL by_position, MENUITEMINFO* item) {
    InsertMenuItem(menu, position, by_position, item);
  };
  g_set_menu_item_info = [](HMENU menu, UINT item, BOOL by_position, MENUITEMINFO* info) -> BOOL {
    return SetMenuItemInfoA(menu, item, by_position, info);
  };
#endif
  log_line(std::string("CreatePopupMenu available: ") + (g_create_popup_menu ? "yes" : "no"));
  log_line(std::string("GetMenuItemCount available: ") + (g_get_menu_item_count ? "yes" : "no"));
  log_line(std::string("InsertMenuItem available: ") + (g_insert_menu_item ? "yes" : "no"));
  log_line(std::string("SetMenuItemInfo available: ") + (g_set_menu_item_info ? "yes" : "no"));
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
  plugin->Register("API_ReaADR_DetectDialogueSegments", reinterpret_cast<void*>(detect_dialogue_segments));
  plugin->Register("APIdef_ReaADR_DetectDialogueSegments", reinterpret_cast<void*>(const_cast<char*>(kDetectDialogueSegmentsDef)));
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
    g_plugin->Register("-API_ReaADR_DetectDialogueSegments", reinterpret_cast<void*>(detect_dialogue_segments));
    g_plugin->Register("-APIdef_ReaADR_DetectDialogueSegments", reinterpret_cast<void*>(const_cast<char*>(kDetectDialogueSegmentsDef)));
  }
  unregister_scripts();
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

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
  if (reason == DLL_PROCESS_ATTACH) {
    g_instance = instance;
    log_windows_dll_load(instance);
  }
  return TRUE;
}
#endif

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
