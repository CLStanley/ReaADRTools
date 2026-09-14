#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_GetProjExtState
#define REAPERAPI_WANT_SetProjExtState

#include "character_filter_command.hpp"

#include "character_filter_adapter.hpp"
#include "native_host_services.hpp"
#include "project_state.hpp"
#include "../reaadr_core/model_repository.hpp"

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>

namespace reaadr::reaper {
namespace {

CharacterFilterInspectionResult inspect_filter(std::string* error)
{
  const auto inspected = inspect_character_filter_project(
    nullptr, native_track_region_api(), native_ruler_lane_api());
  if (!inspected && error) *error = inspected.error;
  return inspected;
}

CharacterFilterApplyResult apply_filter(const core::CharacterFilterPlan& plan,
                                        std::string* error)
{
  const auto applied = apply_character_filter_plan_transactionally(
    nullptr, native_track_region_api(), native_ruler_lane_api(),
    native_transaction_api(), plan, "ReaADR: apply character filter");
  if (!applied && error) *error = applied.error;
  return applied;
}

} // namespace

core::CharacterFilterCatalogResult load_native_character_filter_catalog()
{
  core::CharacterFilterCatalogResult result;
  ProjectStateStore project_state(nullptr, {GetProjExtState, SetProjExtState});
  core::SessionModelRepository sessions(project_state);
  core::CharacterFilterRepository filters(project_state);

  const auto loaded_session = sessions.load();
  if (!loaded_session) {
    result.error = core::session_load_error_message(loaded_session);
    return result;
  }
  const auto loaded_filter = filters.load();
  if (!loaded_filter) {
    result.error = loaded_filter.error;
    return result;
  }
  return core::build_character_filter_catalog(
    loaded_session.model, loaded_filter.state);
}

CharacterFilterApplicationResult apply_native_character_filter_tokens(
  const std::vector<std::string>& tokens,
  bool hide_inactive_regions)
{
  ProjectStateStore project_state(nullptr, {GetProjExtState, SetProjExtState});
  core::SessionModelRepository sessions(project_state);
  core::CharacterFilterRepository filters(project_state);
  CharacterFilterApplicationService service(
    sessions, filters, native_transaction_api(), {inspect_filter, apply_filter});
  return service.apply_tokens(tokens, hide_inactive_regions);
}

} // namespace reaadr::reaper
