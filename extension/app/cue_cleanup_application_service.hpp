#pragma once
#include "../reaadr_reaper/cue_cleanup_adapter.hpp"
#include "../reaadr_core/model_repository.hpp"
#include <functional>
#include <string>
#include <utility>
#include <vector>
namespace reaadr::reaper {
struct CueCleanupApplicationApi {
  std::function<core::ProjectRenderState(std::string* error)> inspect;
  std::function<CueCleanupApplyResult(const core::CueCleanupPlan&, std::string* error)> apply;
  std::string utc_timestamp;
};
struct CueCleanupApplicationResult {
  int cues_removed = 0, regions_removed = 0, cue_audio_removed = 0, tracks_removed = 0;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};
class CueCleanupApplicationService {
public:
  CueCleanupApplicationService(core::SessionModelRepository& sessions,
                               TransactionApi transaction_api,
                               CueCleanupApplicationApi api)
    : sessions_(sessions), transaction_api_(transaction_api), api_(std::move(api)) {}
  CueCleanupApplicationResult clear_characters(const std::vector<std::string>& characters);
private:
  core::SessionModelRepository& sessions_;
  TransactionApi transaction_api_;
  CueCleanupApplicationApi api_;
};
} // namespace reaadr::reaper