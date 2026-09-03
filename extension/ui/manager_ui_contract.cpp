#include "manager_ui_contract.hpp"

namespace reaadr::core {

const ManagerUiContract& manager_ui_contract()
{
  static const ManagerUiContract contract = {
    default_manager_window_layout(),
    {24, 20, 992, 78},
    {24, 108, 992, 42},
    {24, 162, 420, 402},
    {464, 162, 552, 632},
    {24, 810, 992, 42},
    {{24, 192, 420, 30}, {24, 228, 420, 30}, {24, 264, 420, 30}, {24, 300, 420, 30}},
    {
      {"remember_window_layout", "Remember ReaADR window layout per project", {24, 450, 420, 26}},
      {"cue_hover_preview", "Show cue text preview on hover", {24, 486, 420, 26}},
      {"tooltips_enabled", "Show delayed tooltips on hover", {24, 522, 420, 26}},
      {"navigation_wrap_enabled", "Wrap cue navigation at ends", {24, 558, 420, 26}},
      {"cue_manager_auto_dock", "Open Cue Manager docked", {24, 594, 420, 26}},
    },
    {
      {"import", "Import", {24, 162, 420, 120}},
      {"cues", "Cue Management", {24, 162, 420, 120}},
      {"session", "Session Tools", {24, 162, 420, 120}},
      {"reports", "Reports", {24, 162, 420, 120}},
      {"overlay", "Video Overlays", {24, 162, 420, 402}},
      {"preferences", "Preferences", {24, 162, 420, 402}},
      {"help", "Help", {24, 162, 420, 402}},
    },
  };
  return contract;
}

} // namespace reaadr::core
