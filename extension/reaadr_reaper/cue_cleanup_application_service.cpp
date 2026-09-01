#include "cue_cleanup_application_service.hpp"
#include "project_transaction.hpp"
#include "../reaadr_core/cue_cleanup.hpp"
namespace reaadr::reaper {
CueCleanupApplicationResult CueCleanupApplicationService::clear_characters(
  const std::vector<std::string>& characters)
{
  CueCleanupApplicationResult result;
  if (!api_.inspect || !api_.apply) { result.error = "The native cue-cleanup application API is incomplete."; return result; }
  const core::SessionLoadResult loaded = sessions_.load();
  if (!loaded) { result.error = core::session_load_error_message(loaded); return result; }
  const core::ProjectRenderState existing = api_.inspect(&result.error);
  if (!result.error.empty()) return result;
  const core::CueCleanupPlanResult planned = core::build_cue_cleanup_plan(loaded.model, existing, characters);
  if (!planned) { result.error = planned.error; return result; }
  if (planned.plan.empty()) return result;
  const auto snapshot = sessions_.create_snapshot("clear cues", api_.utc_timestamp);
  if (!snapshot) { result.error = snapshot.error; return result; }
  ProjectTransaction transaction(nullptr, transaction_api_, "ReaADR: clear cues");
  UiRefreshScope refresh(transaction_api_.prevent_ui_refresh);
  const CueCleanupApplyResult applied = api_.apply(planned.plan, &result.error);
  if (!applied) { transaction.mark_failed(); sessions_.restore_snapshot(snapshot.snapshot); return result; }
  core::SessionModel updated = loaded.model;
  updated.cues = planned.remaining_cues;
  if (!sessions_.save(updated)) {
    result.error = "Could not persist the session after clearing cues.";
    transaction.mark_failed(); sessions_.restore_snapshot(snapshot.snapshot); return result;
  }
  result.cues_removed = applied.cues_removed;
  result.regions_removed = applied.regions_removed;
  result.cue_audio_removed = applied.cue_audio_removed;
  result.tracks_removed = applied.tracks_removed;
  return result;
}
} // namespace reaadr::reaper
