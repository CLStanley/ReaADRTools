#pragma once

#include "../reaadr_core/model_repository.hpp"
#include "../reaadr_reaper/session_render_service.hpp"

#include <string>
#include <functional>

namespace reaadr::reaper {

struct SessionRefreshApplicationResult {
  SessionRenderResult synchronization;
  bool cancelled = false;
  std::size_t modified_regions = 0;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};

// Rebuilds all ReaADR-owned session artifacts from the canonical cue model.
// The renderer owns the snapshot, Undo, derived model, filters, overlay, and
// event boundary; this service only supplies the current model intent.
class SessionRefreshApplicationService final {
public:
  SessionRefreshApplicationService(core::SessionModelRepository& sessions,
                                   SessionRenderService& renderer,
                                   SessionRenderOptions render_options,
                                   std::string utc_timestamp = {})
    : sessions_(sessions), renderer_(renderer),
      render_options_(std::move(render_options)), utc_timestamp_(std::move(utc_timestamp)) {}

  // Interactive callers inspect before opening any transaction. A declined review
  // is a cancellation, with no snapshot, revision, artifact, or event writes.
  SessionRefreshApplicationResult refresh(
    std::function<ProjectInspectionResult()> inspect = {},
    std::function<bool(std::size_t)> confirm_overwrite = {});

private:
  core::SessionModelRepository& sessions_;
  SessionRenderService& renderer_;
  SessionRenderOptions render_options_;
  std::string utc_timestamp_;
};

} // namespace reaadr::reaper
