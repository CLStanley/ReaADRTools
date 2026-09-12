#pragma once

#include "reaadr_core/cue_manager_model.hpp"
#include "reaadr_core/cue_navigation.hpp"
#include "reaadr_core/overlay_settings.hpp"
#include "reaadr_reaper/session_render_service.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace reaadr::reaper {

// Selects an exact canonical cue without moving the transport or revising the
// session. The overlay callback owns transactional FX compensation on failure;
// this boundary restores the prior paired selection if that refresh fails.
bool select_manager_cue(core::ProjectStateStore& state, const std::string& cue_key,
                        const std::function<bool(std::string*)>& refresh_overlay,
                        std::string& error);

struct CueManagerApplicationResult {
  core::CueManagerEditResult edit;
  core::CueManagerMutationResult mutation;
  SessionRenderResult synchronization;
  std::uint64_t revision = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Narrow UI-facing mutation boundary. Tests can replace it without exposing
// REAPER handles to the controller, while production always uses the
// transactional render-backed implementation below.
class CueManagerMutationService {
public:
  virtual ~CueManagerMutationService() = default;
  virtual CueManagerApplicationResult edit(
    const core::CueManagerEditOptions& options) = 0;
  virtual CueManagerApplicationResult add(
    const core::CueManagerAddOptions& options) = 0;
  virtual CueManagerApplicationResult remove(const std::string& cue_key) = 0;
};

struct CueManagerApplicationApi {
  std::string (*utc_timestamp)() = nullptr;
  // Read at submission so project frame-rate changes apply to subsequent edits.
  double (*frame_rate)() = nullptr;
};

// Validates a cue edit against the canonical model, then commits the resulting
// cue set through SessionRenderService so extstate and visible REAPER artifacts
// remain one Undo-backed operation.
class CueManagerApplicationService final : public CueManagerMutationService {
public:
  CueManagerApplicationService(core::SessionModelRepository& sessions,
                               core::OverlaySettingsRepository& overlay_settings,
                               core::CueSelectionRepository& selections,
                               SessionRenderService& renderer,
                               SessionRenderOptions render_options,
                               CueManagerApplicationApi api = {})
    : sessions_(sessions),
      overlay_settings_(overlay_settings),
      selections_(selections),
      renderer_(renderer),
      render_options_(std::move(render_options)),
      api_(api)
  {
  }

  CueManagerApplicationResult edit(
    const core::CueManagerEditOptions& options) override;
  CueManagerApplicationResult add(
    const core::CueManagerAddOptions& options) override;
  CueManagerApplicationResult remove(const std::string& cue_key) override;

private:
  bool synchronize(const std::vector<core::Fields>& cues,
                   const char* last_operation,
                   const char* snapshot_label,
                   const char* undo_description,
                   const char* event_type,
                   const std::string& selected_cue_key,
                   CueManagerApplicationResult& result);
  core::SessionModelRepository& sessions_;
  core::OverlaySettingsRepository& overlay_settings_;
  core::CueSelectionRepository& selections_;
  SessionRenderService& renderer_;
  SessionRenderOptions render_options_;
  CueManagerApplicationApi api_;
};

} // namespace reaadr::reaper
