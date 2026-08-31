#include "cue_navigation_service.hpp"

#include <cmath>

namespace reaadr::reaper {
namespace {

core::CueNavigationCatalogResult load_catalog(core::SessionModelRepository& repository)
{
  const core::SessionLoadResult loaded = repository.load();
  if (!loaded) {
    core::CueNavigationCatalogResult result;
    result.error = core::session_load_error_message(loaded);
    return result;
  }
  return core::build_cue_navigation_catalog(loaded.model);
}

} // namespace

CueNavigationResult CueNavigationService::navigate_next()
{
  return navigate_relative(true);
}

CueNavigationResult CueNavigationService::navigate_previous()
{
  return navigate_relative(false);
}

CueNavigationResult CueNavigationService::navigate_relative(bool next)
{
  CueNavigationResult result;
  if (!api_.get_play_state || !api_.get_play_position || !api_.get_cursor_position ||
      !api_.set_edit_cursor_position) {
    result.error = "The REAPER cue-navigation API is incomplete.";
    return result;
  }
  const core::CueNavigationCatalogResult catalog = load_catalog(model_repository_);
  if (!catalog) {
    result.error = catalog.error;
    return result;
  }
  if (catalog.cues.empty()) {
    result.error = "The ADR session contains no cues.";
    return result;
  }

  const int play_state = api_.get_play_state();
  const double position = play_state % 2 == 1
    ? api_.get_play_position()
    : api_.get_cursor_position();
  if (!std::isfinite(position)) {
    result.error = "REAPER returned an invalid timeline position.";
    return result;
  }
  const core::CueNavigationEntry* cue = next
    ? core::find_next_cue(catalog.cues, position)
    : core::find_previous_cue(catalog.cues, position);
  if (!cue) {
    result.error = "No navigable ADR cue was found.";
    return result;
  }
  return jump_to(*cue);
}

CueNavigationResult CueNavigationService::navigate_to_id(const std::string& cue_id)
{
  CueNavigationResult result;
  if (!api_.set_edit_cursor_position) {
    result.error = "The REAPER cue-navigation API is incomplete.";
    return result;
  }
  const core::CueNavigationCatalogResult catalog = load_catalog(model_repository_);
  if (!catalog) {
    result.error = catalog.error;
    return result;
  }
  if (catalog.cues.empty()) {
    result.error = "The ADR session contains no cues.";
    return result;
  }
  const core::CueNavigationEntry* cue = core::find_cue_by_id(catalog.cues, cue_id);
  if (!cue) {
    result.error = "Cue not found: " + cue_id;
    return result;
  }
  return jump_to(*cue);
}

CueNavigationResult CueNavigationService::jump_to(const core::CueNavigationEntry& cue)
{
  CueNavigationResult result;
  result.cue = cue;
  result.selection = selection_repository_.save_selected_cue(cue.cue_key);
  if (!result.selection) {
    result.error = result.selection.error;
    return result;
  }
  api_.set_edit_cursor_position(cue.start_time, true, false);
  return result;
}

} // namespace reaadr::reaper
