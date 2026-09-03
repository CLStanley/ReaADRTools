#include "overlay_application_service.hpp"

#include "../reaadr_core/overlay_eel.hpp"

#include <cmath>

namespace reaadr::reaper {

OverlayApplicationResult OverlayApplicationService::refresh()
{
  OverlayApplicationResult result;
  if (!api_.refresh_overlay) {
    result.error = "The native overlay refresh callback is unavailable.";
    return result;
  }

  const core::SessionLoadResult session = sessions_.load();
  if (!session) {
    result.error = core::session_load_error_message(session);
    return result;
  }
  const core::OverlaySettingsLoadResult settings = settings_.load();
  if (!settings) {
    result.error = settings.error;
    return result;
  }
  const core::CueSelectionLoadResult selection = selections_.load();
  if (!selection) {
    result.error = selection.error;
    return result;
  }
  const core::CharacterFilterLoadResult filter = filters_.load();
  if (!filter) {
    result.error = filter.error;
    return result;
  }

  result.refresh.enabled = settings.settings.enabled;
  if (settings.settings.enabled) {
    core::OverlayEelOptions options;
    options.settings = settings.settings;
    options.active_overlay_cue_key = selection.state.active_overlay_cue_key;
    if (api_.selection) {
      const OverlaySelectionInput host_selection = api_.selection();
      options.selected_region_cue_key = host_selection.selected_region_cue_key;
      options.selected_item_cue_key = host_selection.selected_item_cue_key;
    }
    options.character_filter = filter.state;
    options.frame_rate = api_.frame_rate ? api_.frame_rate() : 24.0;
    const core::OverlayEelResult generated = core::build_overlay_eel(session.model, options);
    if (!generated) {
      result.error = generated.error;
      return result;
    }
    result.refresh.video_code = generated.video_code;
    result.displayed_cue_count = generated.displayed_cue_count;
  }

  if (!api_.refresh_overlay(result.refresh, &result.error)) {
    if (result.error.empty()) result.error = "The native overlay refresh failed.";
    return result;
  }
  return result;
}

} // namespace reaadr::reaper
