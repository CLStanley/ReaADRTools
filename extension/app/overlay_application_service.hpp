#pragma once

#include "../reaadr_core/character_filter.hpp"
#include "../reaadr_core/overlay_refresh.hpp"
#include "../reaadr_core/overlay_settings.hpp"
#include "../reaadr_core/model_repository.hpp"
#include "../reaadr_core/cue_navigation.hpp"

#include <cstddef>
#include <string>

namespace reaadr::reaper {

struct OverlaySelectionInput {
  std::string selected_region_cue_key;
  std::string selected_item_cue_key;
};

struct OverlayApplicationApi {
  double (*frame_rate)() = nullptr;
  OverlaySelectionInput (*selection)() = nullptr;
  bool (*refresh_overlay)(const core::OverlayRefreshOptions&, std::string* error) = nullptr;
};

struct OverlayApplicationResult {
  core::OverlayRefreshOptions refresh;
  std::size_t displayed_cue_count = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Coordinates native model/settings/selection inputs and delegates the final
// host mutation to the transactional overlay adapter. No REAPER SDK types
// enter the domain generator or the persisted preference repositories.
class OverlayApplicationService {
public:
  OverlayApplicationService(core::SessionModelRepository& sessions,
                            core::OverlaySettingsRepository& settings,
                            core::CueSelectionRepository& selections,
                            core::CharacterFilterRepository& filters,
                            OverlayApplicationApi api)
    : sessions_(sessions), settings_(settings), selections_(selections),
      filters_(filters), api_(api) {}

  OverlayApplicationResult refresh();

private:
  core::SessionModelRepository& sessions_;
  core::OverlaySettingsRepository& settings_;
  core::CueSelectionRepository& selections_;
  core::CharacterFilterRepository& filters_;
  OverlayApplicationApi api_;
};

} // namespace reaadr::reaper
