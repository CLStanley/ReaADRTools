// Migration host shell: keep the legacy monolithic host implementation intact
// while replacing its entrypoint and command hook with the persistent native
// runtime. This lets migration complete without duplicating or partially
// rewriting the 100KB legacy host; cleanup can split that host after parity.
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <reaper_plugin.h>
#undef REAPER_PLUGIN_ENTRYPOINT
#define REAPER_PLUGIN_ENTRYPOINT REAPER_PLUGIN_ENTRYPOINT_LEGACY
#define hook_native_command hook_native_command_legacy
#include "reaper_reaadr_legacy.cpp"
#undef hook_native_command
#undef REAPER_PLUGIN_ENTRYPOINT

#include "reaadr_reaper/native_runtime.hpp"

namespace {

bool g_runtime_host_hook_registered = false;

std::string persistent_export_path(const char* title)
{
  if (!GetUserInputs) return {};
  std::array<char, 4096> path = {};
  if (!GetUserInputs(title, 1, "Output CSV path", path.data(), path.size())) return {};
  std::string output(path.data());
  const auto first = output.find_first_not_of(" \t\r\n");
  const auto last = output.find_last_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  output = output.substr(first, last - first + 1);
  const std::size_t slash = output.find_last_of("/\\");
  const std::size_t dot = output.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) output += ".csv";
  return output;
}

void run_persistent_native_export_action(const std::string& action)
{
  const char* title = action == "export_cue_sheet" ? "ReaADR: Export Cue Sheet" :
                      action == "export_timing_report" ? "ReaADR: Export Timing Report" :
                      "ReaADR: Export Session Metadata";
  const std::string output_path = persistent_export_path(title);
  if (output_path.empty()) return;

  const auto loaded = reaadr::reaper::cue_manager_session_host().load_session();
  if (!loaded) {
    ShowMessageBox(reaadr::core::session_load_error_message(loaded), "ReaADR Export", 0);
    return;
  }
  std::ofstream file(output_path, std::ios::binary | std::ios::trunc);
  if (!file) {
    ShowMessageBox("Could not create the selected CSV file.", "ReaADR Export", 0);
    return;
  }
  const auto field = [](const reaadr::core::Fields& cue, const char* key) {
    const auto found = cue.find(key);
    return found == cue.end() ? std::string() : found->second;
  };

  if (action == "export_cue_sheet") {
    file << "Cue ID,Character,Start,End,Status,Type,Dialogue,Notes\n";
    for (const auto& cue : loaded.model.cues) {
      file << native_csv_escape(field(cue, "id")) << ',' << native_csv_escape(field(cue, "character")) << ','
           << native_csv_escape(field(cue, "start_time")) << ',' << native_csv_escape(field(cue, "end_time")) << ','
           << native_csv_escape(field(cue, "status")) << ',' << native_csv_escape(field(cue, "cue_type")) << ','
           << native_csv_escape(field(cue, "line")) << ',' << native_csv_escape(field(cue, "notes")) << '\n';
    }
  } else if (action == "export_timing_report") {
    file << "Cue ID,Character,Start,End,Duration,Status\n";
    for (const auto& cue : loaded.model.cues) {
      const double start = std::strtod(field(cue, "start_time").c_str(), nullptr);
      const double end = std::strtod(field(cue, "end_time").c_str(), nullptr);
      file << native_csv_escape(field(cue, "id")) << ',' << native_csv_escape(field(cue, "character")) << ','
           << start << ',' << end << ',' << (end - start) << ',' << native_csv_escape(field(cue, "status")) << '\n';
    }
  } else {
    file << "Field,Value\n";
    for (const auto& entry : loaded.model.session)
      file << native_csv_escape(entry.first) << ',' << native_csv_escape(entry.second) << '\n';
  }
  ShowMessageBox(("Exported to " + output_path + ".").c_str(), "ReaADR Export (Native)", 0);
}

void run_persistent_native_clear_character_cues_action()
{
  if (!GetUserInputs) {
    ShowMessageBox("The character input API is unavailable.", "ReaADR Cue Cleanup", 0);
    return;
  }
  std::array<char, 1024> input = {};
  if (!GetUserInputs("ReaADR: Clear Character Cues", 1,
                     "Characters (comma-separated):", input.data(), input.size())) return;
  std::vector<std::string> characters;
  std::stringstream values(input.data());
  std::string value;
  while (std::getline(values, value, ',')) {
    const auto first = value.find_first_not_of(" \t\r\n");
    const auto last = value.find_last_not_of(" \t\r\n");
    if (first != std::string::npos) characters.push_back(value.substr(first, last - first + 1));
  }
  if (characters.empty()) {
    ShowMessageBox("Select at least one character.", "ReaADR Cue Cleanup", 0);
    return;
  }
  const std::string prompt = "Remove generated cues, regions, cue audio, and cue tracks for " +
    std::to_string(characters.size()) + " character(s)? Recording tracks and takes are preserved.";
  if (ShowMessageBox(prompt.c_str(), "ReaADR Cue Cleanup", 4) != 6) return;
  std::string error;
  const auto result = reaadr::reaper::cue_manager_session_host().clear_characters(characters, error);
  if (!result || !error.empty()) {
    ShowMessageBox((error.empty() ? result.error : error).c_str(), "ReaADR Cue Cleanup", 0);
    return;
  }
  const std::string summary = "Removed " + std::to_string(result.cues_removed) +
    " cue(s), " + std::to_string(result.regions_removed) + " region(s), and " +
    std::to_string(result.cue_audio_removed) + " cue-audio item(s).\n\nCue tracks removed: " +
    std::to_string(result.tracks_removed);
  ShowMessageBox(summary.c_str(), "ReaADR Cue Cleanup", 0);
}

std::string normalize_persistent_import_mode(std::string value)
{
  const auto first = value.find_first_not_of(" \t\r\n");
  const auto last = value.find_last_not_of(" \t\r\n");
  value = first == std::string::npos ? std::string() : value.substr(first, last - first + 1);
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (value == "1" || value == "all" || value == "import entire script" ||
      value == "import entire sheet") return "all";
  if (value == "2" || value == "selected" || value == "import selected characters" ||
      value == "add selected characters") return "selected";
  if (value == "3" || value == "update" || value == "update existing import" ||
      value == "update already imported characters") return "update";
  return value;
}

std::optional<reaadr::core::ColumnMapping> parse_persistent_import_mapping(
  const std::string& serialized,
  std::string& error)
{
  error.clear();
  if (serialized.empty()) return std::nullopt;
  reaadr::core::ColumnMapping mapping;
  std::stringstream entries(serialized);
  std::string entry;
  while (std::getline(entries, entry, ';')) {
    const std::size_t equals = entry.find('=');
    if (equals == std::string::npos) {
      error = "Mappings must use key=column pairs separated by semicolons.";
      return std::nullopt;
    }
    const auto trim = [](const std::string& text) {
      const auto first = text.find_first_not_of(" \t\r\n");
      const auto last = text.find_last_not_of(" \t\r\n");
      return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
    };
    const std::string key = trim(entry.substr(0, equals));
    const std::string column = trim(entry.substr(equals + 1));
    if (key.empty() || column.empty()) {
      error = "Mappings cannot contain empty keys or columns.";
      return std::nullopt;
    }
    mapping[key] = column;
  }
  return mapping.empty() ? std::nullopt : std::optional<reaadr::core::ColumnMapping>(mapping);
}

void run_persistent_native_manager_import(
  const std::string& mapping_override,
  bool preview_only,
  const std::string& mode,
  const std::string& characters)
{
  // Preview remains host-only for this migration slice; importantly it does not
  // mutate the session. All Manager import mutations below execute through the
  // persistent CueManagerSessionHost rather than rebuilding the legacy graph.
  if (preview_only) {
    run_native_import_cue_sheet_action(mapping_override, true, mode, characters);
    return;
  }
  if (!GetUserFileNameForRead) {
    ShowMessageBox("The native file chooser is unavailable.", "ReaADR Import", 0);
    return;
  }
  std::array<char, 4096> path = {};
  if (!GetUserFileNameForRead(path.data(), "ReaADR: Import Cue Sheet", "csv;tsv;tab;txt;xlsx")) return;
  std::ifstream file(path.data(), std::ios::binary);
  if (!file) {
    ShowMessageBox("Could not open the selected cue sheet.", "ReaADR Import", 0);
    return;
  }
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  std::string lower_path(path.data());
  std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (lower_path.size() >= 5 && lower_path.compare(lower_path.size() - 5, 5, ".xlsx") == 0) {
    std::vector<char> tsv(8 * 1024 * 1024), xlsx_error(4096);
    if (!read_xlsx_as_tsv(path.data(), tsv.data(), static_cast<int>(tsv.size()),
                          xlsx_error.data(), static_cast<int>(xlsx_error.size()))) {
      ShowMessageBox(xlsx_error.data(), "ReaADR Import", 0);
      return;
    }
    content = tsv.data();
  }

  std::string serialized_mapping = mapping_override;
  if (serialized_mapping.empty() && GetUserInputs) {
    std::array<char, 2048> input = {};
    if (GetUserInputs("ReaADR Import: Column Mapping", 1,
                      "Optional mapping key=column;... (blank=last/auto-detect)",
                      input.data(), input.size())) serialized_mapping = input.data();
  }
  if (serialized_mapping.empty() && GetProjExtState) {
    std::array<char, 4096> saved = {};
    if (GetProjExtState(nullptr, "ReaADRTools", "import_mapping_last", saved.data(), saved.size()) > 0)
      serialized_mapping = saved.data();
  }
  std::string mapping_error;
  const auto mapping = parse_persistent_import_mapping(serialized_mapping, mapping_error);
  if (!mapping_error.empty()) {
    ShowMessageBox(mapping_error.c_str(), "ReaADR Import", 0);
    return;
  }

  std::vector<std::string> selected_characters;
  if (normalize_persistent_import_mode(mode.empty() ? "all" : mode) == "selected") {
    std::stringstream values(characters);
    std::string value;
    while (std::getline(values, value, ';')) {
      const auto first = value.find_first_not_of(" \t\r\n");
      const auto last = value.find_last_not_of(" \t\r\n");
      if (first != std::string::npos) selected_characters.push_back(value.substr(first, last - first + 1));
    }
  }

  const auto result = reaadr::reaper::cue_manager_session_host().import_content(
    content, path.data(), mapping,
    normalize_persistent_import_mode(mode.empty() ? "all" : mode), selected_characters);
  if (!result) {
    ShowMessageBox(result.error.c_str(), "ReaADR Import", 0);
    return;
  }
  const std::string summary = "Imported " + std::to_string(result.imported.cues.size()) +
    " cue(s) from " + std::string(path.data()) + ".\n\nTracks created: " +
    std::to_string(result.rendered.render.tracks_and_regions.tracks_created) +
    "\nRegions created: " + std::to_string(result.rendered.render.tracks_and_regions.regions_created);
  ShowMessageBox(summary.c_str(), "ReaADR Import (Native)", 0);
}

void run_persistent_native_cue_manager_action()
{
  reaadr::reaper::CueManagerSessionConfig config;
  config.project_state_api = {GetProjExtState, SetProjExtState};
  config.global_state_api = {GetExtState, SetExtState};
  config.cleanup_api = {native_cleanup_inspect, native_cleanup_apply, native_utc_timestamp()};
  config.callbacks.trigger_import = [](
    const std::string& mapping, bool preview, const std::string& mode, const std::string& characters) {
      run_persistent_native_manager_import(mapping, preview, mode, characters);
    };
  config.callbacks.trigger_action = [](const std::string& action) {
    if (action == "sync_regions") {
      std::string error;
      if (!reaadr::reaper::cue_manager_session_host().sync_regions(error) && !error.empty())
        ShowMessageBox(error.c_str(), "ReaADR Cue Manager", 0);
    }
    else if (action == "clear_character_cues") run_persistent_native_clear_character_cues_action();
    else if (action == "export_cue_sheet" || action == "export_timing_report" ||
             action == "export_session_metadata") run_persistent_native_export_action(action);
  };
  std::string error;
  if (!reaadr::reaper::open_native_cue_manager(std::move(config), error) && !error.empty())
    ShowMessageBox(error.c_str(), "ReaADR Cue Manager", 0);
}

bool promote_native_quick_actions(reaper_plugin_info_t* plugin)
{
  if (!plugin || !plugin->GetFunc) return false;
  using NamedCommandLookupFn = int (*)(const char*);
  auto named_command_lookup = reinterpret_cast<NamedCommandLookupFn>(plugin->GetFunc("NamedCommandLookup"));
  if (!named_command_lookup) return false;
  constexpr std::array<const char*, 4> command_names = {{
    "_ReaADRQuickAction1Native", "_ReaADRQuickAction2Native",
    "_ReaADRQuickAction3Native", "_ReaADRQuickAction4Native",
  }};
  std::array<int, 4> command_ids = {};
  for (std::size_t index = 0; index < command_names.size(); ++index) {
    command_ids[index] = named_command_lookup(command_names[index]);
    if (!command_ids[index]) { log_line(std::string("Native Quick Action command unavailable: ") + command_names[index]); return false; }
  }
  for (std::size_t index = 0; index < command_ids.size(); ++index) g_actions[index + 1].command_id = command_ids[index];
  log_line("Promoted all four Quick Actions to native command registrations.");
  return true;
}

bool runtime_host_hook(int command, int flag)
{
  if (command == g_cue_manager_command_id && command != 0) { run_persistent_native_cue_manager_action(); return true; }
  return hook_native_command_legacy(command, flag);
}

bool activate_native_runtime(reaper_plugin_info_t* plugin)
{
  if (g_native_command_hook_registered) {
    plugin->Register("-hookcommand", reinterpret_cast<void*>(hook_native_command_legacy));
    g_native_command_hook_registered = false;
  }
  if (!plugin->Register("hookcommand", reinterpret_cast<void*>(runtime_host_hook))) {
    log_line("Could not install persistent native runtime command hook."); return false;
  }
  g_runtime_host_hook_registered = true;
  std::string error;
  if (!reaadr::reaper::initialize_native_runtime(plugin, &error)) {
    if (!error.empty()) log_line(error);
    plugin->Register("-hookcommand", reinterpret_cast<void*>(runtime_host_hook));
    g_runtime_host_hook_registered = false; return false;
  }
  if (!promote_native_quick_actions(plugin)) {
    log_line("Could not promote Quick Actions to native registrations.");
    reaadr::reaper::shutdown_native_runtime(plugin, nullptr);
    plugin->Register("-hookcommand", reinterpret_cast<void*>(runtime_host_hook));
    g_runtime_host_hook_registered = false; return false;
  }
  log_line("Persistent native workflow runtime activated."); return true;
}

void deactivate_native_runtime(reaper_plugin_info_t* plugin)
{
  std::string error;
  if (!reaadr::reaper::shutdown_native_runtime(plugin, &error) && !error.empty()) log_line(error);
  if (plugin && g_runtime_host_hook_registered) {
    plugin->Register("-hookcommand", reinterpret_cast<void*>(runtime_host_hook));
    g_runtime_host_hook_registered = false;
  }
}

} // namespace

extern "C" REAPER_PLUGIN_DLL_EXPORT int ReaperPluginEntry(REAPER_PLUGIN_HINSTANCE instance, reaper_plugin_info_t* plugin)
{
  if (!plugin) { deactivate_native_runtime(g_plugin); return REAPER_PLUGIN_ENTRYPOINT_LEGACY(instance, nullptr); }
  const int loaded = REAPER_PLUGIN_ENTRYPOINT_LEGACY(instance, plugin);
  if (!loaded) return 0;
  if (activate_native_runtime(plugin)) return 1;
  REAPER_PLUGIN_ENTRYPOINT_LEGACY(instance, nullptr); return 0;
}