#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace reaadr::reaper {

// Register and retire extension API exports in a single, symmetric location.
// The caller owns the callbacks and definitions; this helper owns the
// registration sequence and rollback of any successfully registered entries.
struct HostApiExport {
  const char* registration_key = nullptr;
  void* value = nullptr;
};

template <std::size_t N, typename Register>
bool register_host_api_exports(const std::array<HostApiExport, N>& exports,
                               Register&& register_callback)
{
  std::size_t registered = 0;
  for (; registered < exports.size(); ++registered) {
    if (!exports[registered].registration_key ||
        !register_callback(exports[registered].registration_key,
                           exports[registered].value)) {
      while (registered > 0) {
        --registered;
        // REAPER unregisters an API export using its prefixed negative key.
        // No registration key is generated from user input.
        const char* key = exports[registered].registration_key;
        std::string negative_key = "-";
        negative_key += key;
        register_callback(negative_key.c_str(), exports[registered].value);
      }
      return false;
    }
  }
  return true;
}

template <std::size_t N, typename Register>
void unregister_host_api_exports(const std::array<HostApiExport, N>& exports,
                                 Register&& register_callback)
{
  for (std::size_t index = exports.size(); index > 0; --index) {
    const auto& entry = exports[index - 1];
    if (!entry.registration_key) continue;
    std::string negative_key = "-";
    negative_key += entry.registration_key;
    register_callback(negative_key.c_str(), entry.value);
  }
}

} // namespace reaadr::reaper
