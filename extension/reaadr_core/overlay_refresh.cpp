#include "overlay_refresh.hpp"

namespace reaadr::core {

bool is_owned_overlay_fx(const ExistingOverlayFx& effect)
{
  return effect.renamed_name == kOverlayFxName ||
    effect.video_code.rfind(kOverlayCodeMarker, 0) == 0;
}

OverlayRefreshPlanResult build_overlay_refresh_plan(
  const std::vector<ExistingOverlayTrack>& tracks,
  const OverlayRefreshOptions& options)
{
  OverlayRefreshPlanResult result;
  const ExistingOverlayTrack* target = nullptr;
  for (const ExistingOverlayTrack& track : tracks) {
    if (track.role != "source_video" || track.key != "source_video") continue;
    if (target) {
      result.error = "Multiple owned source-video tracks are available for the overlay.";
      return result;
    }
    target = &track;
  }
  if (!target) {
    result.error =
      "The owned ADR source-video track is unavailable. Render the session first.";
    return result;
  }
  if (options.enabled && options.video_code.rfind(kOverlayCodeMarker, 0) != 0) {
    result.error = "Generated overlay code must begin with the ReaADR ownership marker.";
    return result;
  }

  result.plan.track_project_index = target->project_index;
  result.plan.video_code = options.video_code;
  const ExistingOverlayFx* owned = nullptr;
  for (const ExistingOverlayFx& effect : target->effects) {
    if (!is_owned_overlay_fx(effect)) continue;
    if (owned) {
      result.error = "Multiple generated ReaADR overlay effects require manual cleanup.";
      return result;
    }
    owned = &effect;
  }

  if (!options.enabled) {
    if (owned) {
      result.plan.mutation = OverlayMutationKind::remove;
      result.plan.existing = *owned;
    }
    return result;
  }
  if (!owned) {
    result.plan.mutation = OverlayMutationKind::create;
    return result;
  }
  result.plan.existing = *owned;
  if (owned->renamed_name == kOverlayFxName &&
      owned->video_code == options.video_code && owned->enabled) {
    return result;
  }
  result.plan.mutation = OverlayMutationKind::update;
  return result;
}

} // namespace reaadr::core
