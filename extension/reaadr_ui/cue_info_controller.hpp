#pragma once

#include "../app/cue_info_application_service.hpp"
#include "../app/cue_manager_application_service.hpp"
#include "../reaadr_core/manager_preferences.hpp"
#include "../reaadr_reaper/cue_navigation_service.hpp"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace reaadr::ui {

struct CueInfoControllerApi {
  int (*get_play_state)() = nullptr;
  double (*get_play_position)() = nullptr;
  double (*get_cursor_position)() = nullptr;
  double (*frame_rate)() = nullptr;
};

struct CueInfoEditValues {
  std::string cue_key;
  std::string character;
  std::string status;
  std::string cue_type;
  std::string direction;
  std::string start_time;
  std::string end_time;
  std::string dialogue;
  std::string notes;
};

struct CueInfoWindowLayout {
  int width = 1100;
  int height = 740;
  int dock = 0;
  int x = 0;
  int y = 0;
  bool has_position = false;
};

struct CueInfoLaunchOptions {
  bool open_edit = false;
  bool close_on_save = false;
};

class CueInfoController final {
public:
  CueInfoController(reaper::CueInfoApplicationService& info,
                    reaper::CueManagerMutationService& mutations,
                    core::SessionModelRepository& sessions,
                    core::CueSelectionRepository& selections,
                    core::ManagerPreferencesRepository& preferences,
                    core::ProjectStateStore& project_state,
                    reaper::CueNavigationApi navigation_api,
                    std::function<bool(std::string*)> refresh_overlay,
                    CueInfoControllerApi api)
    : info_(info), mutations_(mutations), sessions_(sessions), selections_(selections),
      preferences_(preferences), project_state_(project_state), navigation_api_(navigation_api),
      refresh_overlay_(std::move(refresh_overlay)), api_(api) {}

  bool refresh();
  bool save(const CueInfoEditValues& values);
  bool previous();
  bool next();
  bool jump_to_id(const std::string& cue_id);

  const core::CueInfoView& view() const { return current_.view; }
  const std::string& error() const { return error_; }
  CueInfoEditValues edit_values() const;
  std::vector<std::string> character_choices() const;
  CueInfoWindowLayout load_window_layout() const;
  bool save_window_layout(const CueInfoWindowLayout& layout);
  CueInfoLaunchOptions consume_launch_options();

private:
  bool navigate_relative(int delta);
  double timeline_position() const;
  double frame_rate() const;
  bool remember_window_layout() const;

  reaper::CueInfoApplicationService& info_;
  reaper::CueManagerMutationService& mutations_;
  core::SessionModelRepository& sessions_;
  core::CueSelectionRepository& selections_;
  core::ManagerPreferencesRepository& preferences_;
  core::ProjectStateStore& project_state_;
  reaper::CueNavigationApi navigation_api_;
  std::function<bool(std::string*)> refresh_overlay_;
  CueInfoControllerApi api_;
  reaper::CueInfoApplicationResult current_;
  std::string error_;
};

} // namespace reaadr::ui
