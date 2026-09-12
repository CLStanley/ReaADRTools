#include "dialogue_cue_generation_application_service.hpp"

namespace reaadr::reaper {

DialogueCueGenerationApplicationResult DialogueCueGenerationApplicationService::generate(
  const std::vector<core::DialogueSegment>& segments,
  const core::DialogueCueGenerationOptions& generation_options,
  const SessionRenderOptions& render_options)
{
  DialogueCueGenerationApplicationResult result;
  result.cues = core::build_dialogue_cues(segments, generation_options);
  if (result.cues.empty()) {
    result.error = "No valid dialogue segments were detected with the current settings.";
    return result;
  }

  result.rendered = renderer_.commit_and_render(result.cues, render_options);
  if (!result.rendered) result.error = result.rendered.error;
  return result;
}

} // namespace reaadr::reaper
