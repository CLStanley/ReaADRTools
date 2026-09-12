#include "manager_navigation.hpp"

#include <algorithm>
#include <charconv>

namespace reaadr::core {

const std::vector<ManagerModule>& manager_modules()
{
  static const std::vector<ManagerModule> modules = {
    {"import", "Import"}, {"cues", "Cue Management"},
    {"session", "Session Tools"}, {"reports", "Reports"},
    {"overlay", "Video Overlays"}, {"preferences", "Preferences"},
    {"help", "Help"},
  };
  return modules;
}

bool is_manager_tab(const std::string& key)
{
  for (const auto& module : manager_modules()) if (module.key == key) return true;
  return false;
}

std::string normalize_manager_tab(const std::string& requested)
{
  return is_manager_tab(requested) ? requested : manager_modules().front().key;
}

const std::vector<ManagerAction>& manager_actions()
{
  static const std::vector<ManagerAction> actions = {
    {"import", "import_cue_sheet", "Import Cue Sheet", "Import CSV or TSV script data and build the ADR session."},
    {"import", "detect_dialogue", "Detect Dialogue From Selected Media", "Analyze selected media and create editable ADR cues."},
    {"import", "generate_cues", "Generate Cues from Markers/Regions", "Create ADR cues from existing markers or regions."},
    {"cues", "cue_manager", "Open Cue Manager", "Browse, edit, navigate, and refresh the active cue session."},
    {"session", "validate_session", "Check Session", "Check timing, fields, metadata, and generated session items."},
    {"session", "refresh_session", "Refresh Session", "Repair generated tracks, regions, cue audio, and overlays."},
    {"session", "sync_regions", "Update Cues From Regions", "Save region timing back to the canonical cue session."},
    {"session", "clear_character_cues", "Clear Character Cues", "Remove owned generated character cues while preserving takes."},
    {"session", "character_filter", "Character Filter", "Mute inactive character lanes and optionally hide their regions."},
    {"reports", "export_cue_sheet", "Export Cue Sheet CSV", "Export regions and cues to a flexible CSV."},
    {"overlay", "refresh_overlay", "Refresh Video Overlay", "Rebuild video overlay effects from canonical cue data."},
    {"preferences", "preferences", "Open Preferences", "Inspect and configure overlay and Manager preferences."},
    {"help", "search_help", "Search Help", "Search the built-in guide by action or workflow."},
    {"help", "help_import", "Import Help", "Show import, mapping, metadata, and session guidance."},
    {"help", "help_cues", "Cue Management Help", "Show navigation, status, filtering, and cue guidance."},
    {"help", "help_overlay", "Overlay Help", "Show video overlay and metadata guidance."},
    {"help", "help_reports", "Reports Help", "Show export and report workflow guidance."},
    {"help", "help_quick_actions", "Quick Actions Help", "Explain configurable top-menu quick actions."},
  };
  return actions;
}

bool manager_action_is_native(const std::string& key)
{
  // These commands bind directly to native application services. Remaining
  // Manager actions stay explicit compatibility routes until their UI and host
  // wiring are cut over and smoke-tested inside REAPER.
  return key == "detect_dialogue" || key == "generate_cues" ||
    key == "validate_session" || key == "refresh_session" || key == "sync_regions" ||
    key == "clear_character_cues" || key == "character_filter" || key == "preferences" || key == "refresh_overlay" ||
    key == "export_cue_sheet" ||
    key == "next_cue" || key == "previous_cue" || key == "jump_to_cue";
}

ManagerWindowLayout default_manager_window_layout()
{
  return {};
}

namespace {
constexpr const char* kNamespace = "ReaADRTools";

bool parse_int(const StateReadResult& value, int& output)
{
  if (!value || value.value.empty()) return false;
  const char* begin = value.value.data();
  const char* end = begin + value.value.size();
  auto parsed = std::from_chars(begin, end, output);
  return parsed.ec == std::errc{} && parsed.ptr == end;
}
}

ManagerWindowLayoutResult ManagerWindowLayoutRepository::load(bool remember_layout) const
{
  ManagerWindowLayoutResult result;
  result.layout = default_manager_window_layout();
  if (!remember_layout) return result;

  int value = 0;
  if (parse_int(store_.read(kNamespace, "ui.window.manager.width"), value))
    result.layout.width = std::max(result.layout.min_width, value);
  if (parse_int(store_.read(kNamespace, "ui.window.manager.height"), value))
    result.layout.height = std::max(result.layout.min_height, value);
  if (parse_int(store_.read(kNamespace, "ui.window.manager.dock"), value))
    result.layout.dock = std::max(0, value);
  if (parse_int(store_.read(kNamespace, "ui.window.manager.x"), value)) result.layout.x = value;
  if (parse_int(store_.read(kNamespace, "ui.window.manager.y"), value)) result.layout.y = value;
  return result;
}

bool ManagerWindowLayoutRepository::save(const ManagerWindowLayout& layout)
{
  const auto width = std::to_string(std::max(layout.min_width, layout.width));
  const auto height = std::to_string(std::max(layout.min_height, layout.height));
  const auto dock = std::to_string(std::max(0, layout.dock));
  return store_.write(kNamespace, "ui.window.manager.width", width) &&
    store_.write(kNamespace, "ui.window.manager.height", height) &&
    store_.write(kNamespace, "ui.window.manager.dock", dock) &&
    store_.write(kNamespace, "ui.window.manager.x", std::to_string(layout.x)) &&
    store_.write(kNamespace, "ui.window.manager.y", std::to_string(layout.y));
}

} // namespace reaadr::core
