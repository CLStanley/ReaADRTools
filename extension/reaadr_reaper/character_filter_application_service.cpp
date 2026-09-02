#include "character_filter_application_service.hpp"
#include <utility>
namespace reaadr::reaper {
CharacterFilterApplicationResult CharacterFilterApplicationService::apply(
  const std::vector<std::string>& characters, bool hide_regions)
{
  CharacterFilterApplicationResult result;
  if (!api_.inspect || !api_.apply) { result.error = "The native character-filter application API is incomplete."; return result; }
  const auto loaded = sessions_.load();
  if (!loaded) { result.error = core::session_load_error_message(loaded); return result; }
  const auto inspected = api_.inspect(&result.error);
  if (!result.error.empty()) return result;
  core::CharacterFilterState next;
  next.active_tokens = {};
  std::vector<std::string> normalized_tokens;
  normalized_tokens.reserve(characters.size());
  for (const auto& character : characters) {
    const std::string token = core::character_filter_key(character);
    next.active_tokens.insert(token);
    normalized_tokens.push_back(token);
  }
  next.encoded_selection = core::encode_character_filter_tokens(std::move(normalized_tokens));
  next.hide_inactive_regions = hide_regions;
  const auto planned = core::build_character_filter_plan(loaded.model, next, inspected.state);
  if (!planned) { result.error = planned.error; return result; }
  if (planned.plan.empty()) {
    if (!filters_.save(next)) result.error = "Could not persist the character filter state.";
    return result;
  }
  ProjectTransaction transaction(nullptr, transaction_api_, "ReaADR: apply character filter");
  UiRefreshScope refresh(transaction_api_.prevent_ui_refresh);
  result.applied = api_.apply(planned.plan, &result.error);
  if (!result.applied) { transaction.mark_failed(); return result; }
  if (!filters_.save(next)) {
    result.error = "Could not persist the character filter state.";
    transaction.mark_failed();
    return result;
  }
  return result;
}
} // namespace reaadr::reaper
