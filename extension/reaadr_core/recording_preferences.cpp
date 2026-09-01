#include "recording_preferences.hpp"

namespace reaadr::core {
namespace {

bool lua_compatible_boolean(const std::string& value)
{
  return value == "1" || value == "true" || value == "yes";
}

std::string storage_error(StateReadError error)
{
  return error == StateReadError::value_too_large
    ? "The recording preference is too large to load safely."
    : "REAPER project extstate is unavailable while loading recording preferences.";
}

} // namespace

RecordingPreferenceLoadResult RecordingPreferenceRepository::load() const
{
  RecordingPreferenceLoadResult result;
  const StateReadResult stored = store_.read(kNamespace, kIncludePrerollKey);
  if (stored) {
    result.include_preroll_each_loop = lua_compatible_boolean(stored.value);
  } else if (stored.error != StateReadError::not_found) {
    result.error = storage_error(stored.error);
  }
  return result;
}

RecordingPreferenceSaveResult
RecordingPreferenceRepository::save_include_preroll_each_loop(bool enabled)
{
  RecordingPreferenceSaveResult result;
  result.include_preroll_each_loop = enabled;
  const RecordingPreferenceLoadResult previous = load();
  if (!previous) {
    result.error = previous.error;
    return result;
  }
  if (previous.include_preroll_each_loop == enabled) return result;
  if (!store_.write(kNamespace, kIncludePrerollKey, enabled ? "1" : "0")) {
    result.error = "Could not persist the per-loop recording preroll preference.";
    return result;
  }
  result.changed = true;
  return result;
}

} // namespace reaadr::core
