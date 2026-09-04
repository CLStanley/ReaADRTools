#pragma once

#include "../reaadr_core/model_repository.hpp"
#include "../reaadr_reaper/session_render_service.hpp"

#include <string>

namespace reaadr::reaper {

struct SessionRefreshApplicationResult {
  SessionRenderResult synchronization;
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

  SessionRefreshApplicationResult refresh();

private:
  core::SessionModelRepository& sessions_;
  SessionRenderService& renderer_;
  SessionRenderOptions render_options_;
  std::string utc_timestamp_;
};

} // namespace reaadr::reaper
