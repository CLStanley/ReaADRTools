#include "model_repository.hpp"

#include <utility>

namespace reaadr::core {

SessionLoadResult SessionModelRepository::load() const
{
  SessionLoadResult result;
  const StateReadResult stored = store_.read(kNamespace, kModelKey);
  if (!stored) {
    switch (stored.error) {
      case StateReadError::not_found:
        result.error = SessionLoadError::missing;
        break;
      case StateReadError::value_too_large:
        result.error = SessionLoadError::value_too_large;
        break;
      case StateReadError::unavailable:
      case StateReadError::none:
        result.error = SessionLoadError::storage_unavailable;
        break;
    }
    return result;
  }

  ParseResult parsed = parse_session_model(stored.value);
  if (!parsed) {
    result.error = SessionLoadError::invalid_model;
    result.parse_error = parsed.error;
    return result;
  }

  result.model = std::move(parsed.model);
  return result;
}

bool SessionModelRepository::save(const SessionModel& model)
{
  // Write the canonical model first. adr_session_id is a compatibility index;
  // readers can always recover it from a successfully written model.
  if (model.session_id().empty()) return false;
  if (!store_.write(kNamespace, kModelKey, serialize_session_model(model))) return false;
  return store_.write(kNamespace, kSessionIdKey, model.session_id());
}

const char* session_load_error_message(const SessionLoadResult& result)
{
  switch (result.error) {
    case SessionLoadError::none: return "";
    case SessionLoadError::missing: return "No ADR session model was found. Import or generate cues first.";
    case SessionLoadError::storage_unavailable: return "REAPER project extstate is unavailable.";
    case SessionLoadError::value_too_large: return "The ADR session model is too large to load safely.";
    case SessionLoadError::invalid_model: return parse_error_message(result.parse_error);
  }
  return "The ADR session model could not be loaded.";
}

} // namespace reaadr::core
