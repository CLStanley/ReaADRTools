#include "session_render_service.hpp"

#include <cmath>
#include <cstdlib>

namespace reaadr::reaper {
namespace {

double positive_number_or(const std::string& value, double fallback)
{
  char* end = nullptr;
  const double parsed = std::strtod(value.c_str(), &end);
  if (!end || end == value.c_str()) return fallback;
  while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' ||
         *end == '\f' || *end == '\v') {
    ++end;
  }
  return *end == '\0' && std::isfinite(parsed) && parsed > 0.0 ? parsed : fallback;
}

} // namespace

SessionRenderResult SessionRenderService::commit_and_render(
  const std::vector<core::Fields>& cues,
  const SessionRenderOptions& options)
{
  SessionRenderResult result;
  if (options.cue_audio_path.empty()) {
    result.error = "A project cue-audio path is required to render session cues.";
    return result;
  }

  core::SessionLoadResult loaded = repository_.load();
  core::SessionModel previous_model;
  const core::SessionModel* previous_model_pointer = nullptr;
  if (loaded) {
    previous_model = loaded.model;
    previous_model_pointer = &previous_model;
  } else if (loaded.error != core::SessionLoadError::missing) {
    result.error = core::session_load_error_message(loaded);
    return result;
  }

  // Generate and validate the deterministic media asset before changing the
  // model or project. Existing sessions retain their canonical timecode during
  // cue replacement, while new sessions use the incoming build frame rate.
  core::CueWavOptions cue_wav_options = options.cue_wav;
  std::string canonical_frame_rate = options.commit.replacement.build.frame_rate;
  if (previous_model_pointer) {
    const auto stored_frame_rate = previous_model.timecode.find("frame_rate");
    if (stored_frame_rate != previous_model.timecode.end()) {
      canonical_frame_rate = stored_frame_rate->second;
    }
  }
  cue_wav_options.frame_rate = positive_number_or(canonical_frame_rate, 24.0);
  result.cue_wav = core::build_cue_wav(cue_wav_options);
  if (!result.cue_wav) {
    result.error = result.cue_wav.error;
    return result;
  }
  if (!core::write_cue_wav_file(options.cue_audio_path, result.cue_wav.bytes, result.error)) {
    return result;
  }

  // Ask REAPER to open the completed file rather than trusting calculated
  // duration. The measured value also protects apply-time source replacement.
  const CueAudioSourceResult cue_source =
    CueAudioAdapter(project_, cue_audio_api_).inspect_source(options.cue_audio_path);
  if (!cue_source) {
    result.error = cue_source.error;
    return result;
  }
  const CompleteRenderInspectionResult inspected = inspect_complete_render_state(
    project_, track_region_api_, ruler_lane_api_, cue_audio_api_);
  if (!inspected) {
    result.error = inspected.error;
    return result;
  }

  bool restore_model_after_project_rollback = false;
  {
    ProjectTransaction transaction(project_, transaction_api_, options.undo_description);
    result.commit = core::commit_session_cues(repository_, cues, options.commit);
    if (!result.commit) {
      result.error = result.commit.error;
      transaction.mark_failed();
    } else {
      core::RenderPlanOptions render_options = options.render;
      render_options.preroll_seconds = options.commit.replacement.build.preroll_seconds;
      render_options.cue_audio_path = options.cue_audio_path;
      render_options.cue_audio_duration = cue_source.duration;
      const core::RenderPlanResult planned = core::build_render_plan(
        result.commit.model, previous_model_pointer, inspected.state, render_options);
      if (!planned) {
        result.error = planned.error;
        restore_model_after_project_rollback = true;
        transaction.mark_failed();
      } else {
        result.plan = planned.plan;
        result.render = apply_complete_render_plan_transactionally(
          project_, track_region_api_, ruler_lane_api_, cue_audio_api_, transaction_api_,
          result.plan, options.undo_description);
        if (!result.render) {
          result.error = result.render.error;
          restore_model_after_project_rollback = true;
          transaction.mark_failed();
        }
      }
    }
  }

  if (restore_model_after_project_rollback) {
    // The outer transaction destructor has now restored the visible REAPER
    // project. Publish the snapshot afterward so observers never see the old
    // project paired with the newly committed canonical model.
    const core::RevisionResult restored = repository_.restore_snapshot(result.commit.snapshot);
    result.model_rolled_back = static_cast<bool>(restored);
    if (!restored) result.error += " Model snapshot rollback also failed: " + restored.error;
  }
  return result;
}

} // namespace reaadr::reaper
