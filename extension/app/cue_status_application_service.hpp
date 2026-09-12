#pragma once

#include "../reaadr_core/cue_status.hpp"
#include "../reaadr_core/event_log.hpp"
#include "../reaadr_reaper/project_transaction.hpp"

#include <functional>
#include <string>

struct ReaProject;

namespace reaadr::reaper {

struct CueStatusApplicationOptions {
  core::CueStatusCommitOptions commit;
  core::EventPublishOptions event;
  std::string undo_description = "ReaADR: update cue status";
  std::function<bool(std::string* error)> refresh_overlay;
};

struct CueStatusApplicationResult {
  core::CueStatusCommitResult status;
  core::EventPublishResult event;
  bool model_rolled_back = false;
  std::string event_warning;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Owns the complete canonical status-update boundary used by both the
// standalone Set Cue Status workflow and recording finalization. Status is
// persisted first, overlay refresh participates in the same project Undo, and
// a failed refresh restores the exact model snapshot before returning.
class CueStatusApplicationService final {
public:
  CueStatusApplicationService(core::SessionModelRepository& repository,
                              core::EventLogRepository& event_log,
                              ReaProject* project,
                              TransactionApi transaction_api)
    : repository_(repository), event_log_(event_log), project_(project),
      transaction_api_(transaction_api) {}

  CueStatusApplicationResult apply(const std::string& cue_key,
                                   const std::string& status,
                                   const CueStatusApplicationOptions& options);

private:
  core::SessionModelRepository& repository_;
  core::EventLogRepository& event_log_;
  ReaProject* project_ = nullptr;
  TransactionApi transaction_api_;
};

} // namespace reaadr::reaper
