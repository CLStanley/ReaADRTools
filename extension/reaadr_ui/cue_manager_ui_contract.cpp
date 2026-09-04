#include "cue_manager_ui_contract.hpp"

namespace reaadr::core {

const std::vector<CueManagerColumn>& cue_manager_columns()
{
  static const std::vector<CueManagerColumn> columns = {
    {"id", "Cue", 44, false}, {"character", "Character", 128, true},
    {"start_time", "Start SMPTE", 98, true}, {"end_time", "End SMPTE", 98, true},
    {"status", "Status", 96, true}, {"cue_type", "Type", 72, true},
    {"line", "Line", 0, true}, {"notes", "Notes", 0, true},
  };
  return columns;
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
    "Not Recorded", "In Progress", "Recorded", "Needs Review", "Approved"};
  return choices;
}

const std::vector<std::string>& cue_manager_type_choices()
{
  static const std::vector<std::string> choices = {
    "Dialogue", "Reaction", "Effort", "Walla", "Crowd", "Announcement", "Narration", "Custom"};
  return choices;
}

bool is_cue_manager_sort_key(const std::string& key)
{
  return key == "id" || key == "character" || key == "start_time" || key == "end_time" ||
    key == "status" || key == "cue_type" || key == "line" || key == "notes";
}

} // namespace reaadr::core
