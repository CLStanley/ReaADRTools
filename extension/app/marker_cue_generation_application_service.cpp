#include "marker_cue_generation_application_service.hpp"

#include <utility>

namespace reaadr::reaper {

MarkerCueGenerationPreview MarkerCueGenerationApplicationService::preview(
  const core::MarkerCueGenerationOptions& generation_options) const
{
  MarkerCueGenerationPreview result;
  result.snapshot = snapshot_project_markers(project_, marker_api_);
  if (!result.snapshot) {
    result.error = result.snapshot.error;
    return result;
  }

  result.cues = core::build_cues_from_project_markers(result.snapshot.sources,
                                                       generation_options);
  if (result.cues.empty()) {
    result.error = "No project markers or regions matched the cue generation options.";
  }
  return result;
}

MarkerCueGenerationApplicationResult MarkerCueGenerationApplicationService::render_prepared(
  MarkerCueGenerationPreview prepared,
  const SessionRenderOptions& render_options)
{
  MarkerCueGenerationApplicationResult result;
  result.snapshot = std::move(prepared.snapshot);
  result.cues = std::move(prepared.cues);
  if (!prepared.error.empty()) {
    result.error = std::move(prepared.error);
    return result;
  }
  if (result.cues.empty()) {
    result.error = "No prepared marker or region cues are available to render.";
    return result;
  }

  result.rendered = renderer_.commit_and_render(result.cues, render_options);
  if (!result.rendered) result.error = result.rendered.error;
  return result;
}

MarkerCueGenerationApplicationResult MarkerCueGenerationApplicationService::generate(
  const core::MarkerCueGenerationOptions& generation_options,
  const SessionRenderOptions& render_options)
{
  return render_prepared(preview(generation_options), render_options);
}

} // namespace reaadr::reaper
