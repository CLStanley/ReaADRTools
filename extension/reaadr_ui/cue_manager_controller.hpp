#pragma once

#include "reaadr_core/manager_view_model.hpp"
#include "reaadr_reaper/manager_view_application_service.hpp"

namespace reaadr::ui {

class CueManagerController {
public:
  explicit CueManagerController(reaper::ManagerViewApplicationService& service);
  bool reload();
  bool set_character_filter(const std::string& character);
  void select_index(int index);
  void select_relative(int delta);
  const core::ManagerViewModel& view() const { return view_; }

private:
  reaper::ManagerViewApplicationService& service_;
  core::CueManagerViewOptions options_;
  core::ManagerViewModel view_;
  std::string selected_key_;
};

} // namespace reaadr::ui
