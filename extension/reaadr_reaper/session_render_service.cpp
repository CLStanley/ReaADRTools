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

void append_event_warning(SessionRenderResult& result, const std::string& warning)
{
  if (warning.empty()) return;
  if (!result.event_warning.empty()) result.event_warning += " ";
  result.event_warning += warning;
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
  core::CharacterFilterState filter_state;
  if (options.apply_character_filter) {
    const core::CharacterFilterLoadResult loaded_filter = character_filter_.load();
    if (!loaded_filter) {
      result.error = loaded_filter.error;
      return result;
    }
    filter_state = loaded_filter.state;
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
    // Keep the full model/render/filter sequence visually atomic. Narrow
    // adapters may add nested suppression scopes, which safely join this one.
    UiRefreshScope refresh(transaction_api_.prevent_ui_refresh);
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
        } else {
          bool filter_applied = true;
          if (options.apply_character_filter) {
            const CharacterFilterInspectionResult filter_inspection =
              inspect_character_filter_project(project_, track_region_api_, ruler_lane_api_);
            if (!filter_inspection) {
              result.error = filter_inspection.error;
              filter_applied = false;
            } else {
              const core::CharacterFilterPlanResult filter_plan =
                core::build_character_filter_plan(
                  result.commit.model, filter_state, filter_inspection.state,
                  options.commit.replacement.build.preroll_seconds);
              if (!filter_plan) {
                result.error = filter_plan.error;
                filter_applied = false;
              } else {
                result.character_filter_plan = filter_plan.plan;
                result.character_filter = apply_character_filter_plan_transactionally(
                  project_, track_region_api_, ruler_lane_api_, transaction_api_,
                  result.character_filter_plan, options.undo_description);
                if (!result.character_filter) {
                  result.error = result.character_filter.error;
                  filter_applied = false;
                }
              }
            }
            if (!filter_applied) {
              restore_model_after_project_rollback = true;
              transaction.mark_failed();
            }
          }

          if (filter_applied && options.publish_events) {
            core::EventPublishOptions event_options = options.event;
            if (event_options.utc_timestamp.empty()) {
              event_options.utc_timestamp = options.commit.utc_timestamp;
            }
            event_options.session_id = result.commit.model.session_id();
            if (event_options.source.empty()) event_options.source = "native_session_render";

            const core::Fields saved_payload = {
              {"cue_count", std::to_string(result.commit.model.cues.size())},
              {"operation", options.commit.replacement.last_operation},
              {"revision", std::to_string(result.commit.revision)},
            };
            result.events.push_back(event_log_.publish(options.commit_event_type, saved_payload, event_options));
            if (!result.events.back()) {
              append_event_warning(result, options.commit_event_type + " event publication failed: " +
                result.events.back().error);
            }

            const core::Fields sync_payload = {
              {"cue_audio_created", std::to_string(result.render.cue_audio.items_created)},
              {"cue_audio_updated", std::to_string(result.render.cue_audio.items_updated)},
              {"cue_count", std::to_string(result.commit.model.cues.size())},
              {"regions_created", std::to_string(result.render.tracks_and_regions.regions_created)},
              {"regions_updated", std::to_string(result.render.tracks_and_regions.regions_updated)},
              {"tracks_created", std::to_string(result.render.tracks_and_regions.tracks_created)},
            };
            result.events.push_back(event_log_.publish("SyncFull", sync_payload, event_options));
            if (!result.events.back()) {
              append_event_warning(result, "SyncFull event publication failed: " +
                result.events.back().error);
            }
          }
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

RegionTimingRenderResult SessionRenderService::sync_region_timings_and_render(
  const RegionTimingRenderOptions& options)
{
  RegionTimingRenderResult result;
  const core::SessionLoadResult loaded = repository_.load();
  if (!loaded) {
    result.error = core::session_load_error_message(loaded);
    return result;
  }

  const ProjectInspectionResult inspected =
    TrackRegionAdapter(project_, track_region_api_).inspect();
  if (!inspected) {
    result.error = inspected.error;
    return result;
  }
  result.timing = core::sync_cue_timings_from_regions(
    loaded.model, inspected.state.regions, options.timing);
  if (!result.timing) {
    result.error = result.timing.error;
    return result;
  }
  if (result.timing.changed_cues == 0) return result;

  SessionRenderOptions render_options = options.session;
  render_options.commit.replacement.last_operation = "update_cues_from_regions";
  render_options.commit.replacement.build.regions_modified = true;
  render_options.commit.snapshot_label = "Update Cues From Regions";
  render_options.undo_description = "ReaADR: update cues from regions";
  render_options.commit_event_type = "CueTimingUpdated";
  result.render = commit_and_render(result.timing.cues, render_options);
  if (!result.render) result.error = result.render.error;
  return result;
}

} // namespace reaadr::reaper
