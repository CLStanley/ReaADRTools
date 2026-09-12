#pragma once

#include "../reaadr_core/marker_cue_generation.hpp"
#include "../reaadr_reaper/marker_snapshot_adapter.hpp"
#include "../reaadr_reaper/session_render_service.hpp"

#include <string>
#include <vector>

struct ReaProject;

namespace reaadr::reaper {

struct MarkerCueGenerationPreview {
  MarkerSnapshotResult snapshot;
  std::vector<core::Fields> cues;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct MarkerCueGenerationApplicationResult {
  MarkerSnapshotResult snapshot;
  std::vector<core::Fields> cues;
  SessionRenderResult rendered;
  std::string error;

  explicit operator bool() const { return error.empty() && static_cast<bool>(rendered); }
};

// Native Generate Cues orchestration. Preview keeps REAPER enumeration and cue
// conversion read-only so the host can show replacement/full-session
// confirmation before any canonical model or project mutation occurs.
class MarkerCueGenerationApplicationService final {
public:
  MarkerCueGenerationApplicationService(SessionRenderService& renderer,
                                        ReaProject* project,
                                        MarkerSnapshotApi marker_api)
    : renderer_(renderer), project_(project), marker_api_(marker_api) {}

  MarkerCueGenerationPreview preview(
    const core::MarkerCueGenerationOptions& generation_options) const;

  MarkerCueGenerationApplicationResult render_prepared(
    MarkerCueGenerationPreview prepared,
    const SessionRenderOptions& render_options);

  MarkerCueGenerationApplicationResult generate(
    const core::MarkerCueGenerationOptions& generation_options,
    const SessionRenderOptions& render_options);

private:
  SessionRenderService& renderer_;
  ReaProject* project_ = nullptr;
  MarkerSnapshotApi marker_api_;
};

} // namespace reaadr::reaper
