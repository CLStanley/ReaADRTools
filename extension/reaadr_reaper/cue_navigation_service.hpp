#pragma once

#include "reaadr_core/cue_navigation.hpp"

#include <string>

namespace reaadr::reaper {

struct CueNavigationApi {
  int (*get_play_state)() = nullptr;
  double (*get_play_position)() = nullptr;
  double (*get_cursor_position)() = nullptr;
  void (*set_edit_cursor_position)(double, bool, bool) = nullptr;
};

struct CueNavigationResult {
  core::CueNavigationEntry cue;
  core::CueSelectionSaveResult selection;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Application boundary for Next Cue, Previous Cue, and Jump To Cue. It reads
// only the canonical session model, persists the paired UI selection keys, and
// moves the REAPER cursor without creating a model revision or Undo point.
class CueNavigationService {
public:
  CueNavigationService(core::SessionModelRepository& model_repository,
                       core::CueSelectionRepository& selection_repository,
                       CueNavigationApi api)
    : model_repository_(model_repository),
      selection_repository_(selection_repository),
      api_(api)
  {
  }

  CueNavigationResult navigate_next();
  CueNavigationResult navigate_previous();
  CueNavigationResult navigate_to_id(const std::string& cue_id);

private:
  CueNavigationResult navigate_relative(bool next);
  CueNavigationResult jump_to(const core::CueNavigationEntry& cue);

  core::SessionModelRepository& model_repository_;
  core::CueSelectionRepository& selection_repository_;
  CueNavigationApi api_;
};

} // namespace reaadr::reaper
