#include "marker_cue_generation_application_service.hpp"

namespace reaadr::reaper {

MarkerCueGenerationApplicationResult MarkerCueGenerationApplicationService::generate(
  const core::MarkerCueGenerationOptions& generation_options,
  const SessionRenderOptions& render_options)
{
  MarkerCueGenerationApplicationResult result;
  result.snapshot = snapshot_project_markers(project_, marker_api_);
  if (!result.snapshot) {
    result.error = result.snapshot.error;
    return result;
  }

  result.cues = core::build_cues_from_project_markers(result.snapshot.sources,
                                                       generation_options);
  if (result.cues.empty()) {
    result.error = "No project markers or regions matched the cue generation options.";
    return result;
  }

  result.rendered = renderer_.commit_and_render(result.cues, render_options);
  if (!result.rendered) result.error = result.rendered.error;
  return result;
}

} // namespace reaadr::reaper
