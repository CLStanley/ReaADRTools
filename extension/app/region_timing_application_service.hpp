#pragma once

#include "../reaadr_reaper/session_render_service.hpp"

namespace reaadr::reaper {

struct RegionTimingApplicationResult {
  core::RegionTimingSyncResult timing;
  SessionRenderResult synchronization;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};

// Application boundary for adopting user-moved generated regions. The domain
// matcher remains ownership-scoped; the render service owns snapshot, Undo,
// derived artifacts, overlay refresh, and event publication.
class RegionTimingApplicationService final {
public:
  RegionTimingApplicationService(SessionRenderService& renderer,
                                 RegionTimingRenderOptions options)
    : renderer_(renderer), options_(std::move(options)) {}

  RegionTimingApplicationResult update();

private:
  SessionRenderService& renderer_;
  RegionTimingRenderOptions options_;
};

} // namespace reaadr::reaper
