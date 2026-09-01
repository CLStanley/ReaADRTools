#include "character_filter_application_service.hpp"
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
  for (const auto& character : characters) next.active_tokens.insert(core::character_filter_key(character));
  next.encoded_selection = core::encode_character_filter_tokens(characters);
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
