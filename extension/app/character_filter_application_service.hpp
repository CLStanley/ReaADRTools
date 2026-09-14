#pragma once
#include "../reaadr_reaper/character_filter_adapter.hpp"
#include "../reaadr_core/model_repository.hpp"
#include <string>
#include <vector>
namespace reaadr::reaper {
struct CharacterFilterApplicationApi {
  CharacterFilterInspectionResult (*inspect)(std::string* error) = nullptr;
  CharacterFilterApplyResult (*apply)(const core::CharacterFilterPlan&, std::string* error) = nullptr;
};
struct CharacterFilterApplicationResult {
  CharacterFilterApplyResult applied;
  std::string error;
  explicit operator bool() const { return error.empty(); }
};
class CharacterFilterApplicationService {
public:
  CharacterFilterApplicationService(core::SessionModelRepository& sessions,
                                    core::CharacterFilterRepository& filters,
                                    TransactionApi transaction_api,
                                    CharacterFilterApplicationApi api)
    : sessions_(sessions), filters_(filters), transaction_api_(transaction_api), api_(api) {}

  // Convenience path used by the standalone character filter command. Each
  // character activates all of that character's lanes.
  CharacterFilterApplicationResult apply(const std::vector<std::string>& characters,
                                         bool hide_regions);

  // Native Manager path. Tokens may be either character keys (all lanes) or
  // exact character.laneN target keys, matching CharacterFilterState.
  CharacterFilterApplicationResult apply_tokens(const std::vector<std::string>& tokens,
                                                bool hide_regions);
private:
  core::SessionModelRepository& sessions_;
  core::CharacterFilterRepository& filters_;
  TransactionApi transaction_api_;
  CharacterFilterApplicationApi api_;
};
} // namespace reaadr::reaper
