#include "workflow_action_ids.hpp"

#include <array>
#include <cstddef>

#include <reaper_plugin.h>

namespace reaadr::reaper {
namespace {

struct ActionDefinition {
  const char* command_name;
  const char* label;
  int WorkflowActionIds::*id;
};

constexpr std::array<ActionDefinition, 13> kActions = {{
  {kValidateSessionCommandName, kValidateSessionActionLabel, &WorkflowActionIds::validate_session},
  {kRefreshOverlayCommandName, kRefreshOverlayActionLabel, &WorkflowActionIds::refresh_overlay},
  {kRefreshSessionCommandName, kRefreshSessionActionLabel, &WorkflowActionIds::refresh_session},
  {kUpdateCuesFromRegionsCommandName, kUpdateCuesFromRegionsActionLabel, &WorkflowActionIds::update_cues_from_regions},
  {kClearCharacterCuesCommandName, kClearCharacterCuesActionLabel, &WorkflowActionIds::clear_character_cues},
  {kCharacterFilterCommandName, kCharacterFilterActionLabel, &WorkflowActionIds::character_filter},
  {kNextCueCommandName, "ReaADR: Next Cue (Native)", &WorkflowActionIds::next_cue},
  {kPreviousCueCommandName, "ReaADR: Previous Cue (Native)", &WorkflowActionIds::previous_cue},
  {kJumpToCueCommandName, "ReaADR: Jump To Cue (Native)", &WorkflowActionIds::jump_to_cue},
  {kCueManagerCommandName, "ReaADR: Cue Manager (Native)", &WorkflowActionIds::cue_manager},
  {kImportCueSheetCommandName, "ReaADR: Import Cue Sheet (Native)", &WorkflowActionIds::import_cue_sheet},
  {kPreferencesCommandName, "ReaADR: Preferences (Native Preview)", &WorkflowActionIds::preferences},
  {kUiTestCommandName, "ReaADR: Native UI Test Window", &WorkflowActionIds::ui_test},
}};

WorkflowActionIds g_ids;
std::array<gaccel_register_t, kActions.size()> g_accels = {};

void clear_ids()
{
  g_ids = {};
}

} // namespace

const WorkflowActionIds& workflow_action_ids()
{
  return g_ids;
}

void unregister_migrated_workflow_actions(reaper_plugin_info_t* plugin)
{
  if (!plugin) {
    clear_ids();
    g_accels = {};
    return;
  }

  for (std::size_t index = kActions.size(); index > 0; --index) {
    const std::size_t slot = index - 1;
    if (g_ids.*(kActions[slot].id) != 0)
      plugin->Register("-gaccel", reinterpret_cast<void*>(&g_accels[slot]));
    g_ids.*(kActions[slot].id) = 0;
    g_accels[slot] = {};
  }
}

bool register_migrated_workflow_actions(reaper_plugin_info_t* plugin)
{
  if (!plugin) return false;
  if (g_ids.validate_session != 0) return true;

  for (std::size_t index = 0; index < kActions.size(); ++index) {
    const auto& action = kActions[index];
    int& command_id = g_ids.*(action.id);
    command_id = plugin->Register(
      "command_id", reinterpret_cast<void*>(const_cast<char*>(action.command_name)));
    if (!command_id) {
      unregister_migrated_workflow_actions(plugin);
      return false;
    }

    auto& accel = g_accels[index];
    accel.accel.cmd = static_cast<WORD>(command_id);
    accel.desc = action.label;
    if (!plugin->Register("gaccel", reinterpret_cast<void*>(&accel))) {
      command_id = 0;
      unregister_migrated_workflow_actions(plugin);
      return false;
    }
  }
  return true;
}

} // namespace reaadr::reaper
