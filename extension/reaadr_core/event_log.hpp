#pragma once

#include "model_repository.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace reaadr::core {

struct EventRecord {
  std::string event_id;
  std::string timestamp;
  std::string session_id;
  std::string event_type;
  std::string source;
  std::string batch_id;
  Fields payload;
};

struct EventPublishOptions {
  // Time is injected by the application boundary to keep the core independent
  // of wall-clock and timezone APIs. Use the same UTC format as Lua:
  // YYYY-MM-DDTHH:MM:SSZ.
  std::string utc_timestamp;
  std::string session_id;
  std::string source;
  std::string batch_id;
  std::size_t log_limit = 200;
};

struct EventLogLoadResult {
  std::vector<std::string> lines;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

struct EventPublishResult {
  EventRecord event;
  std::uint64_t counter = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Scalar native payloads use the same sorted key=value representation as
// compact_event_payload in ReaADR_Core.lua. Nested-table payloads remain a Lua
// concern until a typed native payload model is introduced.
std::string compact_event_payload(const Fields& payload);
std::string serialize_event_log_line(const EventRecord& event);

// Persists the compatibility event counter and bounded event_log_v1 history.
// This deliberately depends only on ProjectStateStore, allowing native UI and
// Lua consumers to observe one shared project event stream during migration.
class EventLogRepository {
public:
  explicit EventLogRepository(ProjectStateStore& store) : store_(store) {}

  EventLogLoadResult load() const;
  EventPublishResult publish(const std::string& event_type,
                             const Fields& payload,
                             const EventPublishOptions& options);

  static constexpr const char* kNamespace = SessionModelRepository::kNamespace;
  static constexpr const char* kCounterKey = "event_counter";
  static constexpr const char* kLogKey = "event_log_v1";

private:
  ProjectStateStore& store_;
};

} // namespace reaadr::core
