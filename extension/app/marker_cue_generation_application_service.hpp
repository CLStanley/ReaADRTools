#pragma once

#include "../reaadr_core/marker_cue_generation.hpp"
#include "../reaadr_reaper/marker_snapshot_adapter.hpp"
#include "../reaadr_reaper/session_render_service.hpp"

#include <string>

struct ReaProject;

namespace reaadr::reaper {

struct MarkerCueGenerationApplicationResult {
  MarkerSnapshotResult snapshot;
  std::vector<core::Fields> cues;
  SessionRenderResult rendered;
  std::string error;

  explicit operator bool() const { return error.empty() && static_cast<bool>(rendered); }
};

// Native Generate Cues orchestration: snapshot REAPER markers/regions, convert
// them through the host-independent parity layer, then use the canonical
// session renderer for model commit, generated regions/tracks, cue audio, and
// other derived artifacts.
class MarkerCueGenerationApplicationService final {
public:
  MarkerCueGenerationApplicationService(SessionRenderService& renderer,
                                        ReaProject* project,
                                        MarkerSnapshotApi marker_api)
    : renderer_(renderer), project_(project), marker_api_(marker_api) {}

  MarkerCueGenerationApplicationResult generate(
    const core::MarkerCueGenerationOptions& generation_options,
    const SessionRenderOptions& render_options);

private:
  SessionRenderService& renderer_;
  ReaProject* project_ = nullptr;
  MarkerSnapshotApi marker_api_;
};

} // namespace reaadr::reaper
