#pragma once

#include "manager_navigation.hpp"

#include <string>
#include <vector>

namespace reaadr::core {

struct ManagerUiRect { int x = 0; int y = 0; int width = 0; int height = 0; };

struct ManagerUiSection {
  std::string key;
  std::string title;
  ManagerUiRect rect;
};

struct ManagerUiContract {
  ManagerWindowLayout window;
  ManagerUiRect header;
  ManagerUiRect tab_bar;
  ManagerUiRect quick_actions;
  ManagerUiRect content;
  ManagerUiRect footer;
  std::vector<ManagerUiSection> sections;
};

// Geometry and labels mirrored from ReaADR_App.lua. This is presentation
// metadata only; state and action routing remain in the native view model.
const ManagerUiContract& manager_ui_contract();

} // namespace reaadr::core
