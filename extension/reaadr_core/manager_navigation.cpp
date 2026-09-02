#include "manager_navigation.hpp"

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
    {"reports", "export_cue_sheet", "Export Cue Sheet CSV", "Export regions and cues to a flexible CSV."},
    {"overlay", "refresh_overlay", "Refresh Video Overlay", "Rebuild video overlay effects from canonical cue data."},
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
  // These commands already bind to native application services. All other
  // Manager actions remain explicit compatibility routes until their UI and
  // host wiring are cut over.
  return key == "validate_session" || key == "refresh_overlay" ||
    key == "next_cue" || key == "previous_cue" || key == "jump_to_cue";
}

ManagerWindowLayout default_manager_window_layout()
{
  return {};
}

} // namespace reaadr::core
