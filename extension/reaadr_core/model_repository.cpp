#include "model_repository.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

namespace reaadr::core {
namespace {

std::uint64_t parse_revision(const std::string& value)
{
  char* end = nullptr;
  const long double parsed = std::strtold(value.c_str(), &end);
  if (!end || end == value.c_str() || !std::isfinite(parsed) || parsed < 0.0L) return 0;
  while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\f' || *end == '\v') ++end;
  if (*end != '\0') return 0;
  const long double maximum = static_cast<long double>(std::numeric_limits<std::uint64_t>::max());
  return parsed >= maximum ? std::numeric_limits<std::uint64_t>::max() : static_cast<std::uint64_t>(parsed);
}

bool read_optional(ProjectStateStore& store, const char* key, std::string& value, std::string& error)
{
  const StateReadResult result = store.read(SessionModelRepository::kNamespace, key);
  if (result) {
    value = result.value;
    return true;
  }
  if (result.error == StateReadError::not_found) {
    value.clear();
    return true;
  }
  error = result.error == StateReadError::value_too_large
    ? "A project extstate value is too large to snapshot safely."
    : "REAPER project extstate is unavailable.";
  return false;
}

} // namespace

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

RevisionResult SessionModelRepository::revision() const
{
  RevisionResult result;
  const StateReadResult stored = store_.read(kNamespace, kRevisionKey);
  if (stored) {
    result.revision = parse_revision(stored.value);
    return result;
  }
  if (stored.error == StateReadError::not_found) return result;
  result.error = stored.error == StateReadError::value_too_large
    ? "The session revision value is too large to load safely."
    : "REAPER project extstate is unavailable.";
  return result;
}

RevisionResult SessionModelRepository::bump_revision()
{
  RevisionResult result = revision();
  if (!result) return result;
  if (result.revision == std::numeric_limits<std::uint64_t>::max()) {
    result.error = "The session revision counter cannot be incremented further.";
    return result;
  }
  ++result.revision;
  if (!store_.write(kNamespace, kRevisionKey, std::to_string(result.revision))) {
    result.error = "Could not persist the updated session revision.";
  }
  return result;
}

SnapshotResult SessionModelRepository::create_snapshot(const std::string& label,
                                                       const std::string& utc_timestamp)
{
  SnapshotResult result;
  result.snapshot.label = label;
  result.snapshot.timestamp = utc_timestamp;
  if (!read_optional(store_, kModelKey, result.snapshot.model_blob, result.error)) return result;
  if (!read_optional(store_, kRevisionKey, result.snapshot.revision, result.error)) return result;
  if (!read_optional(store_, kSessionIdKey, result.snapshot.session_id, result.error)) return result;

  const bool saved =
    store_.write(kNamespace, kSnapshotLabelKey, result.snapshot.label) &&
    store_.write(kNamespace, kSnapshotTimestampKey, result.snapshot.timestamp) &&
    store_.write(kNamespace, kSnapshotModelKey, result.snapshot.model_blob) &&
    store_.write(kNamespace, kSnapshotRevisionKey, result.snapshot.revision);
  if (!saved) result.error = "Could not persist the session safety snapshot.";
  return result;
}

RevisionResult SessionModelRepository::restore_snapshot(const SessionSnapshot& snapshot)
{
  RevisionResult current = revision();
  if (!current) return current;
  const std::uint64_t restored_revision = parse_revision(snapshot.revision);
  const std::uint64_t base = (std::max)(restored_revision, current.revision);
  if (base == std::numeric_limits<std::uint64_t>::max()) {
    current.error = "The restored session revision cannot be incremented further.";
    return current;
  }

  current.revision = base + 1;
  if (!store_.write(kNamespace, kModelKey, snapshot.model_blob)) {
    current.error = "Could not restore the session model snapshot.";
    return current;
  }
  if (!store_.write(kNamespace, kSessionIdKey, snapshot.session_id)) {
    current.error = "The model was restored, but its session ID index could not be synchronized.";
    return current;
  }
  if (!store_.write(kNamespace, kRevisionKey, std::to_string(current.revision))) {
    current.error = "The model was restored, but its new session revision could not be persisted.";
  }
  return current;
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
