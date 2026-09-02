#include "overlay_refresh_adapter.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace reaadr::reaper {
namespace {

constexpr int kInitialConfigBuffer = 4096;
constexpr int kMaximumConfigBuffer = 4 * 1024 * 1024;

bool inspection_api_complete(const OverlayRefreshApi& api)
{
  return api.count_tracks && api.get_track && api.validate_track &&
    api.get_set_track_string && api.track_fx_get_count &&
    api.track_fx_get_named_config && api.track_fx_get_enabled;
}

bool mutation_api_complete(const OverlayRefreshApi& api)
{
  return inspection_api_complete(api) && api.track_fx_add_by_name &&
    api.track_fx_delete && api.track_fx_set_named_config &&
    api.track_fx_set_enabled;
}

std::string track_string(const OverlayRefreshApi& api,
                         MediaTrack* track,
                         const char* parameter)
{
  std::array<char, 4096> buffer = {};
  if (!api.get_set_track_string(track, parameter, buffer.data(), false)) return {};
  return buffer.data();
}

bool read_optional_fx_config(const OverlayRefreshApi& api,
                             MediaTrack* track,
                             int fx_index,
                             const char* parameter,
                             std::string& value,
                             std::string& error)
{
  for (int size = kInitialConfigBuffer; size <= kMaximumConfigBuffer; size *= 2) {
    std::vector<char> buffer(static_cast<std::size_t>(size), '\x7f');
    if (!api.track_fx_get_named_config(
          track, fx_index, parameter, buffer.data(), size)) {
      value.clear();
      return true;
    }
    const auto terminator = std::find(buffer.begin(), buffer.end(), '\0');
    const std::size_t length =
      static_cast<std::size_t>(terminator - buffer.begin());
    if (length + 1 < buffer.size()) {
      value.assign(buffer.data(), length);
      return true;
    }
  }
  error = std::string("The overlay FX ") + parameter +
    " value is too large to inspect safely.";
  return false;
}

bool inspect_effect(const OverlayRefreshApi& api,
                    MediaTrack* track,
                    int fx_index,
                    core::ExistingOverlayFx& effect,
                    std::string& error)
{
  effect.fx_index = fx_index;
  if (!read_optional_fx_config(
        api, track, fx_index, "renamed_name", effect.renamed_name, error)) {
    return false;
  }
  if (!read_optional_fx_config(
        api, track, fx_index, "VIDEO_CODE", effect.video_code, error)) {
    return false;
  }
  effect.enabled = api.track_fx_get_enabled(track, fx_index);
  return true;
}

bool effect_matches(const core::ExistingOverlayFx& actual,
                    const core::ExistingOverlayFx& expected)
{
  return actual.fx_index == expected.fx_index &&
    actual.renamed_name == expected.renamed_name &&
    actual.video_code == expected.video_code && actual.enabled == expected.enabled;
}

bool configure_effect(const OverlayRefreshApi& api,
                      MediaTrack* track,
                      int fx_index,
                      const std::string& renamed_name,
                      const std::string& video_code,
                      bool enabled)
{
  if (!api.track_fx_set_named_config(
        track, fx_index, "renamed_name", renamed_name.c_str()) ||
      !api.track_fx_set_named_config(
        track, fx_index, "VIDEO_CODE", video_code.c_str()) ||
      !api.track_fx_set_named_config(track, fx_index, "DONE", "")) {
    return false;
  }
  api.track_fx_set_enabled(track, fx_index, enabled);

  core::ExistingOverlayFx verified;
  std::string error;
  return inspect_effect(api, track, fx_index, verified, error) &&
    verified.renamed_name == renamed_name && verified.video_code == video_code &&
    verified.enabled == enabled;
}

MediaTrack* revalidate_target(ReaProject* project,
                              const OverlayRefreshApi& api,
                              const core::OverlayRefreshPlan& plan,
                              std::string& error)
{
  MediaTrack* track = api.get_track(
    project, static_cast<int>(plan.track_project_index));
  if (!track || !api.validate_track(project, track) ||
      track_string(api, track, "P_EXT:ReaADR.role") != plan.expected_track_role ||
      track_string(api, track, "P_EXT:ReaADR.key") != plan.expected_track_key) {
    error = "The owned source-video track changed before overlay refresh.";
    return nullptr;
  }
  return track;
}

} // namespace

OverlayRefreshInspectionResult inspect_overlay_project(
  ReaProject* project,
  OverlayRefreshApi api)
{
  OverlayRefreshInspectionResult result;
  if (!inspection_api_complete(api)) {
    result.error = "The REAPER overlay inspection API is incomplete.";
    return result;
  }
  const int track_count = api.count_tracks(project);
  if (track_count < 0) {
    result.error = "REAPER returned an invalid track count while inspecting overlays.";
    return result;
  }
  result.tracks.reserve(static_cast<std::size_t>(track_count));
  for (int track_index = 0; track_index < track_count; ++track_index) {
    MediaTrack* track = api.get_track(project, track_index);
    if (!track || !api.validate_track(project, track)) {
      result.error = "REAPER could not resolve a track while inspecting overlays.";
      return result;
    }
    core::ExistingOverlayTrack inspected;
    inspected.project_index = static_cast<std::size_t>(track_index);
    inspected.role = track_string(api, track, "P_EXT:ReaADR.role");
    inspected.key = track_string(api, track, "P_EXT:ReaADR.key");
    const int fx_count = api.track_fx_get_count(track);
    if (fx_count < 0) {
      result.error = "REAPER returned an invalid FX count while inspecting overlays.";
      return result;
    }
    inspected.effects.reserve(static_cast<std::size_t>(fx_count));
    for (int fx_index = 0; fx_index < fx_count; ++fx_index) {
      core::ExistingOverlayFx effect;
      if (!inspect_effect(api, track, fx_index, effect, result.error)) return result;
      inspected.effects.push_back(std::move(effect));
    }
    result.tracks.push_back(std::move(inspected));
  }
  return result;
}

OverlayRefreshApplyResult apply_overlay_refresh_plan_transactionally(
  ReaProject* project,
  OverlayRefreshApi api,
  TransactionApi transaction_api,
  const core::OverlayRefreshPlan& plan,
  const std::string& description)
{
  OverlayRefreshApplyResult result;
  if (plan.mutation == core::OverlayMutationKind::none) return result;
  if (!mutation_api_complete(api)) {
    result.error = "The REAPER overlay mutation API is incomplete.";
    return result;
  }

  ProjectTransaction transaction(project, transaction_api, description);
  UiRefreshScope refresh(transaction_api.prevent_ui_refresh);
  MediaTrack* track = revalidate_target(project, api, plan, result.error);
  if (!track) {
    transaction.mark_failed();
    return result;
  }

  if (plan.mutation == core::OverlayMutationKind::create) {
    const int fx_count = api.track_fx_get_count(track);
    if (fx_count < 0) {
      result.error = "REAPER returned an invalid FX count before overlay creation.";
      transaction.mark_failed();
      return result;
    }
    for (int fx_index = 0; fx_index < fx_count; ++fx_index) {
      core::ExistingOverlayFx effect;
      if (!inspect_effect(api, track, fx_index, effect, result.error)) {
        transaction.mark_failed();
        return result;
      }
      if (core::is_owned_overlay_fx(effect)) {
        result.error = "The overlay creation plan became stale before mutation.";
        transaction.mark_failed();
        return result;
      }
    }
    const int created = api.track_fx_add_by_name(track, "Video processor", false, -1);
    if (created < 0) {
      result.error = "REAPER could not create the generated video overlay effect.";
      transaction.mark_failed();
      return result;
    }
    if (!configure_effect(
          api, track, created, core::kOverlayFxName, plan.video_code, true)) {
      result.error = "REAPER could not configure the generated video overlay effect.";
      result.restored_after_failure = api.track_fx_delete(track, created);
      if (!result.restored_after_failure) {
        result.error += " The incomplete effect could not be removed.";
      }
      transaction.mark_failed();
      return result;
    }
    ++result.effects_created;
  } else {
    const int fx_count = api.track_fx_get_count(track);
    if (plan.existing.fx_index < 0 || plan.existing.fx_index >= fx_count) {
      result.error = "The generated overlay effect changed before mutation.";
      transaction.mark_failed();
      return result;
    }
    core::ExistingOverlayFx actual;
    if (!inspect_effect(api, track, plan.existing.fx_index, actual, result.error) ||
        !effect_matches(actual, plan.existing) || !core::is_owned_overlay_fx(actual)) {
      if (result.error.empty()) {
        result.error = "The generated overlay effect changed before mutation.";
      }
      transaction.mark_failed();
      return result;
    }

    if (plan.mutation == core::OverlayMutationKind::remove) {
      if (!api.track_fx_delete(track, plan.existing.fx_index)) {
        result.error = "REAPER could not remove the generated video overlay effect.";
        transaction.mark_failed();
        return result;
      }
      ++result.effects_removed;
    } else {
      if (!configure_effect(
            api, track, plan.existing.fx_index,
            core::kOverlayFxName, plan.video_code, true)) {
        result.error = "REAPER could not update the generated video overlay effect.";
        result.restored_after_failure = configure_effect(
          api, track, plan.existing.fx_index,
          plan.existing.renamed_name, plan.existing.video_code,
          plan.existing.enabled);
        if (!result.restored_after_failure) {
          result.error += " The prior overlay configuration could not be restored.";
        }
        transaction.mark_failed();
        return result;
      }
      ++result.effects_updated;
    }
  }

  if (api.adjust_track_windows) api.adjust_track_windows(false);
  if (api.update_arrange) api.update_arrange();
  return result;
}

OverlayRefreshApplyResult refresh_generated_overlay_transactionally(
  ReaProject* project,
  OverlayRefreshApi api,
  TransactionApi transaction_api,
  const core::OverlayRefreshOptions& options,
  const std::string& description)
{
  OverlayRefreshApplyResult result;
  const OverlayRefreshInspectionResult inspected = inspect_overlay_project(project, api);
  if (!inspected) {
    result.error = inspected.error;
    return result;
  }
  const core::OverlayRefreshPlanResult planned =
    core::build_overlay_refresh_plan(inspected.tracks, options);
  if (!planned) {
    result.error = planned.error;
    return result;
  }
  return apply_overlay_refresh_plan_transactionally(
    project, api, transaction_api, planned.plan, description);
}

} // namespace reaadr::reaper
