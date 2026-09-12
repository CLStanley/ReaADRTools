#pragma once

#include "../reaadr_core/dialogue_cue_generation.hpp"
#include "../reaadr_reaper/session_render_service.hpp"

#include <string>
#include <vector>

namespace reaadr::reaper {

struct DialogueCueGenerationApplicationResult {
  std::vector<core::Fields> cues;
  SessionRenderResult rendered;
  std::string error;

  explicit operator bool() const { return error.empty() && static_cast<bool>(rendered); }
};

// Completes the native half of dialogue detection after the host audio scanner
// returns timeline segments. It builds canonical cues and sends them through
// the same transactional session renderer used by import and marker generation.
class DialogueCueGenerationApplicationService final {
public:
  explicit DialogueCueGenerationApplicationService(SessionRenderService& renderer)
    : renderer_(renderer) {}

  DialogueCueGenerationApplicationResult generate(
    const std::vector<core::DialogueSegment>& segments,
    const core::DialogueCueGenerationOptions& generation_options,
    const SessionRenderOptions& render_options);

private:
  SessionRenderService& renderer_;
};

} // namespace reaadr::reaper
