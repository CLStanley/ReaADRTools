#include "project_state.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace reaadr::reaper {

core::StateReadResult ProjectStateStore::read(const char* name_space, const char* key) const
{
  if (!api_.get) return {{}, core::StateReadError::unavailable};

  std::size_t capacity = kInitialCapacity;
  while (capacity <= kMaximumCapacity && capacity <= static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    // Zero initialization guarantees a terminator even if an older REAPER
    // build does not write one when returning an empty value.
    std::vector<char> buffer(capacity, '\0');
    const int result = api_.get(project_, name_space, key, buffer.data(), static_cast<int>(buffer.size()));
    if (result <= 0) return {{}, core::StateReadError::not_found};

    const auto terminator = std::find(buffer.begin(), buffer.end(), '\0');
    const std::size_t length = static_cast<std::size_t>(terminator - buffer.begin());
    if (terminator != buffer.end() && length + 1 < capacity) {
      return {std::string(buffer.data(), length), core::StateReadError::none};
    }

    // A full buffer may mean the value was truncated. Retrying also handles
    // the rare legitimate case where its length exactly matches the boundary.
    if (capacity == kMaximumCapacity) break;
    capacity = (std::min)(capacity * 2U, kMaximumCapacity);
  }
  return {{}, core::StateReadError::value_too_large};
}

bool ProjectStateStore::write(const char* name_space, const char* key, const std::string& value)
{
  if (!api_.set) return false;
  return api_.set(project_, name_space, key, value.c_str()) > 0;
}

} // namespace reaadr::reaper
