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
