#include "cue_manager_ui_contract.hpp"
#include <algorithm>

namespace reaadr::core {

const std::vector<CueManagerColumn>& cue_manager_columns()
{
  static const std::vector<CueManagerColumn> columns = {
    {"id", "Cue", 48, false}, {"character", "Character", 150, true},
    {"start_time", "Start SMPTE", 112, true}, {"end_time", "End SMPTE", 112, true},
    {"status", "Status", 112, true}, {"cue_type", "Type", 84, true},
    {"line", "Line", 420, true}, {"notes", "Notes", 340, true},
  };
  return columns;
}

int adjust_cue_manager_column_width(int current, bool increase)
{
  // Widen before arithmetic so even a malformed host width cannot overflow.
  const long long adjusted = std::max(44LL, static_cast<long long>(current)) + (increase ? 24 : -24);
  return static_cast<int>(std::clamp(adjusted, 44LL, 900LL));
}

const std::vector<CueManagerAction>& cue_manager_actions()
{
  static const std::vector<CueManagerAction> actions = {
    {"jump", "Jump...", "Type a cue number and jump to its region start."},
    {"prev", "Previous", "Select the previous cue in the list."},
    {"next", "Next", "Select the next cue in the list."},
    {"record", "Record Current Cue", "Arm the current cue and start the dedicated record workflow."},
    {"add", "Add Cue", "Create a cue at the current timeline position."},
    {"remove", "Remove Cue", "Delete the selected cue and rebuild generated session artifacts."},
    {"filter", "Character Filter", "Enable or disable character tracks for focused recording passes."},
    {"sync", "Refresh Session", "Repair generated tracks, regions, cue audio, filters, and overlays."},
    {"info", "Info Panel", "Open the large cue information panel for the selected cue."},
  };
  return actions;
}

const std::vector<std::string>& cue_manager_status_choices()
{
  static const std::vector<std::string> choices = {
    "Not Recorded", "In Progress", "Recorded", "Needs Review", "Approved", "Needs Retake"};
  return choices;
}

const std::vector<std::string>& cue_manager_type_choices()
{
  static const std::vector<std::string> choices = {
    "Dialogue", "Reaction", "Effort", "Walla", "Crowd", "Announcement", "Narration", "Custom"};
  return choices;
}

std::vector<CueManagerCharacterFilterItem> cue_manager_character_filter_items(
  const CharacterFilterCatalogResult& catalog)
{
  std::vector<CueManagerCharacterFilterItem> items;
  if (!catalog) return items;

  items.push_back({CueManagerCharacterFilterItem::Kind::show_all,
                   "Show All Character Cues", {}, {}, catalog.show_all, false});
  for (const auto& group : catalog.groups) {
    items.push_back({CueManagerCharacterFilterItem::Kind::character,
                     group.character, group.character, {}, group.all_active,
                     group.partially_active});
    if (group.targets.size() <= 1) continue;
    for (const auto& target : group.targets) {
      items.push_back({CueManagerCharacterFilterItem::Kind::lane,
                       "Lane " + std::to_string(target.lane), target.character,
                       target.key, target.active, false});
    }
  }
  return items;
}

bool is_cue_manager_sort_key(const std::string& key)
{
  return key == "id" || key == "character" || key == "start_time" || key == "end_time" ||
    key == "status" || key == "cue_type" || key == "line" || key == "notes";
}

} // namespace reaadr::core
