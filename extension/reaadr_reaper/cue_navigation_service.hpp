#pragma once

#include "reaadr_core/cue_navigation.hpp"

#include <string>
#include <functional>
#include <utility>

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

// Persists paired selection before refreshing owned overlay FX. The callback
// compensates failed FX mutations; this helper restores the previous keys.
core::CueSelectionSaveResult save_cue_selection_and_refresh(
  core::CueSelectionRepository& selections, const std::string& cue_key,
  const std::function<bool(std::string*)>& refresh_overlay);

// Application boundary for Next Cue, Previous Cue, and Jump To Cue. It reads
// only the canonical session model, persists the paired UI selection keys, and
// refreshes the overlay before moving the REAPER cursor. Selection creates no
// model revision; the supplied overlay adapter owns any FX Undo transaction.
class CueNavigationService {
public:
  CueNavigationService(core::SessionModelRepository& model_repository,
                       core::CueSelectionRepository& selection_repository,
                       CueNavigationApi api,
                       std::function<bool(std::string*)> refresh_overlay = {})
    : model_repository_(model_repository),
      selection_repository_(selection_repository),
      api_(api), refresh_overlay_(std::move(refresh_overlay))
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
  std::function<bool(std::string*)> refresh_overlay_;
};

} // namespace reaadr::reaper
