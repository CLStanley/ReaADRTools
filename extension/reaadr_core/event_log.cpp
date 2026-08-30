#include "event_log.hpp"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>

namespace reaadr::core {
namespace {

std::uint64_t parse_counter(const std::string& value)
{
  char* end = nullptr;
  const long double parsed = std::strtold(value.c_str(), &end);
  if (!end || end == value.c_str() || !std::isfinite(parsed) || parsed < 0.0L) return 0;
  while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' ||
         *end == '\f' || *end == '\v') {
    ++end;
  }
  if (*end != '\0') return 0;
  const long double maximum = static_cast<long double>(std::numeric_limits<std::uint64_t>::max());
  return parsed >= maximum ? std::numeric_limits<std::uint64_t>::max()
                           : static_cast<std::uint64_t>(parsed);
}

std::string event_id(std::uint64_t counter)
{
  std::ostringstream output;
  output << "evt_" << std::setfill('0') << std::setw(8) << counter;
  return output.str();
}

std::string storage_error(StateReadError error, const char* subject)
{
  if (error == StateReadError::value_too_large) {
    return std::string("The ") + subject + " is too large to load safely.";
  }
  return std::string("REAPER project extstate is unavailable while loading the ") + subject + ".";
}

} // namespace

std::string compact_event_payload(const Fields& payload)
{
  std::ostringstream output;
  bool first = true;
  for (const auto& [key, value] : payload) {
    if (!first) output << ';';
    first = false;
    output << key << '=' << value;
  }
  return output.str();
}

std::string serialize_event_log_line(const EventRecord& event)
{
  const std::string values[] = {
    event.event_id,
    event.timestamp,
    event.session_id,
    event.event_type,
    event.source,
    event.batch_id,
    compact_event_payload(event.payload),
  };
  std::ostringstream output;
  for (std::size_t index = 0; index < sizeof(values) / sizeof(values[0]); ++index) {
    if (index != 0) output << '|';
    output << encode_field(values[index]);
  }
  return output.str();
}

EventLogLoadResult EventLogRepository::load() const
{
  EventLogLoadResult result;
  const StateReadResult stored = store_.read(kNamespace, kLogKey);
  if (!stored) {
    if (stored.error != StateReadError::not_found) {
      result.error = storage_error(stored.error, "native event log");
    }
    return result;
  }

  std::string::size_type start = 0;
  while (start < stored.value.size()) {
    const std::string::size_type end = stored.value.find('\n', start);
    const std::string line = stored.value.substr(start, end - start);
    // Lua's ([^\n]+) reader ignores empty lines; preserving that behavior is
    // important when native code appends to logs created by older scripts.
    if (!line.empty()) result.lines.push_back(line);
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return result;
}

EventPublishResult EventLogRepository::publish(const std::string& event_type,
                                               const Fields& payload,
                                               const EventPublishOptions& options)
{
  EventPublishResult result;
  if (event_type.empty()) {
    result.error = "An event type is required.";
    return result;
  }
  if (options.utc_timestamp.empty()) {
    result.error = "A UTC timestamp is required to publish a native event.";
    return result;
  }
  if (options.log_limit == 0) {
    result.error = "The native event-log retention limit must be positive.";
    return result;
  }

  const StateReadResult stored_counter = store_.read(kNamespace, kCounterKey);
  if (!stored_counter && stored_counter.error != StateReadError::not_found) {
    result.error = storage_error(stored_counter.error, "native event counter");
    return result;
  }
  const std::uint64_t previous_counter = stored_counter ? parse_counter(stored_counter.value) : 0;
  if (previous_counter == std::numeric_limits<std::uint64_t>::max()) {
    result.error = "The native event counter cannot be incremented further.";
    return result;
  }
  result.counter = previous_counter + 1;
  result.event = {
    event_id(result.counter),
    options.utc_timestamp,
    options.session_id,
    event_type,
    options.source,
    options.batch_id,
    payload,
  };

  // Match Lua's observable order: reserve the event ID first, then append its
  // line. If the second write fails, the gap prevents accidental ID reuse.
  if (!store_.write(kNamespace, kCounterKey, std::to_string(result.counter))) {
    result.error = "Could not persist the native event counter.";
    return result;
  }
  EventLogLoadResult log = load();
  if (!log) {
    result.error = log.error;
    return result;
  }
  log.lines.push_back(serialize_event_log_line(result.event));
  if (log.lines.size() > options.log_limit) {
    log.lines.erase(log.lines.begin(),
                    log.lines.begin() + static_cast<std::ptrdiff_t>(log.lines.size() - options.log_limit));
  }

  std::ostringstream serialized;
  for (std::size_t index = 0; index < log.lines.size(); ++index) {
    if (index != 0) serialized << '\n';
    serialized << log.lines[index];
  }
  if (!store_.write(kNamespace, kLogKey, serialized.str())) {
    result.error = "Could not persist the native project event log.";
  }
  return result;
}

} // namespace reaadr::core
