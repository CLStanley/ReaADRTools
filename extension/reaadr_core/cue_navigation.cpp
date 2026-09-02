#include "cue_navigation.hpp"

#include "render_plan.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

namespace reaadr::core {
namespace {

std::string field(const Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

std::string trim_ascii(const std::string& value)
{
  const auto is_space = [](unsigned char byte) {
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' ||
      byte == '\f' || byte == '\v';
  };
  std::size_t first = 0;
  while (first < value.size() && is_space(static_cast<unsigned char>(value[first]))) ++first;
  std::size_t last = value.size();
  while (last > first && is_space(static_cast<unsigned char>(value[last - 1]))) --last;
  return value.substr(first, last - first);
}

std::string lowercase_ascii(std::string value)
{
  for (char& byte : value) {
    if (byte >= 'A' && byte <= 'Z') byte = static_cast<char>(byte - 'A' + 'a');
  }
  return value;
}

bool parse_number(const std::string& value, double& output)
{
  const std::string cleaned = trim_ascii(value);
  char* end = nullptr;
  output = std::strtod(cleaned.c_str(), &end);
  return !cleaned.empty() && end && end != cleaned.c_str() && *end == '\0' &&
    std::isfinite(output);
}

bool valid_position_options(double position, double epsilon)
{
  return std::isfinite(position) && std::isfinite(epsilon) && epsilon >= 0.0;
}

bool read_optional(ProjectStateStore& store,
                   const char* key,
                   std::string& value,
                   std::string& error)
{
  const StateReadResult stored = store.read(CueSelectionRepository::kNamespace, key);
  if (stored) {
    value = stored.value;
    return true;
  }
  if (stored.error == StateReadError::not_found) {
    value.clear();
    return true;
  }
  error = stored.error == StateReadError::value_too_large
    ? "A cue-selection value is too large to load safely."
    : "REAPER project extstate is unavailable while loading cue selection.";
  return false;
}

} // namespace

CueNavigationCatalogResult build_cue_navigation_catalog(const SessionModel& model)
{
  CueNavigationCatalogResult result;
  if (model.session_id().empty()) {
    result.error = "A canonical session ID is required before navigating cues.";
    return result;
  }
  result.cues.reserve(model.cues.size());
  std::set<std::string> cue_keys;
  for (std::size_t index = 0; index < model.cues.size(); ++index) {
    const Fields& cue = model.cues[index];
    CueNavigationEntry entry;
    entry.cue = cue;
    entry.model_index = index;
    entry.cue_key = render_cue_key(cue);
    entry.cue_id = trim_ascii(field(cue, "id"));
    if (entry.cue_key.empty()) {
      result.error = "Cue " + entry.cue_id + " has no stable navigation key.";
      return result;
    }
    if (!cue_keys.insert(entry.cue_key).second) {
      result.error = "Multiple cues resolve to the navigation key: " + entry.cue_key;
      return result;
    }
    if (!parse_number(field(cue, "start_time"), entry.start_time) ||
        !parse_number(field(cue, "end_time"), entry.end_time) ||
        entry.end_time < entry.start_time) {
      result.error = "Cue " + entry.cue_id + " has invalid navigation timing.";
      return result;
    }
    result.cues.push_back(std::move(entry));
  }

  std::stable_sort(result.cues.begin(), result.cues.end(),
    [](const CueNavigationEntry& left, const CueNavigationEntry& right) {
      if (left.start_time != right.start_time) return left.start_time < right.start_time;
      // navigation_cues compares tostring(cue.id) before lookup later trims
      // user input, so retain the persisted ID for exact tie-break parity.
      return field(left.cue, "id") < field(right.cue, "id");
    });
  return result;
}

const CueNavigationEntry* find_next_cue(const std::vector<CueNavigationEntry>& cues,
                                        double position,
                                        double epsilon)
{
  if (cues.empty() || !valid_position_options(position, epsilon)) return nullptr;
  const auto found = std::find_if(cues.begin(), cues.end(), [&](const CueNavigationEntry& cue) {
    return cue.start_time > position + epsilon;
  });
  return found == cues.end() ? &cues.front() : &*found;
}

const CueNavigationEntry* find_previous_cue(const std::vector<CueNavigationEntry>& cues,
                                            double position,
                                            double epsilon)
{
  if (cues.empty() || !valid_position_options(position, epsilon)) return nullptr;
  const CueNavigationEntry* previous = nullptr;
  for (const CueNavigationEntry& cue : cues) {
    if (cue.start_time < position - epsilon) previous = &cue;
    else break;
  }
  return previous ? previous : &cues.back();
}

const CueNavigationEntry* find_cue_by_id(const std::vector<CueNavigationEntry>& cues,
                                         const std::string& cue_id)
{
  const std::string wanted = trim_ascii(cue_id);
  if (wanted.empty()) return nullptr;
  for (const CueNavigationEntry& cue : cues) {
    if (cue.cue_id == wanted) return &cue;
  }
  const std::string lowered = lowercase_ascii(wanted);
  for (const CueNavigationEntry& cue : cues) {
    if (lowercase_ascii(cue.cue_id).find(lowered) != std::string::npos) return &cue;
  }
  return nullptr;
}

const CueNavigationEntry* find_cue_at_position(const std::vector<CueNavigationEntry>& cues,
                                               double position)
{
  if (!std::isfinite(position)) return nullptr;
  const auto found = std::find_if(cues.begin(), cues.end(), [&](const CueNavigationEntry& cue) {
    return position >= cue.start_time && position <= cue.end_time;
  });
  return found == cues.end() ? nullptr : &*found;
}

CueSelectionLoadResult CueSelectionRepository::load() const
{
  CueSelectionLoadResult result;
  if (!read_optional(store_, kManagerSelectionKey,
                     result.state.manager_selected_cue_key, result.error)) {
    return result;
  }
  read_optional(store_, kActiveOverlayKey, result.state.active_overlay_cue_key, result.error);
  return result;
}

CueSelectionSaveResult CueSelectionRepository::save_selected_cue(const std::string& cue_key)
{
  return save_state({cue_key, cue_key});
}

CueSelectionSaveResult CueSelectionRepository::save_state(const CueSelectionState& state)
{
  CueSelectionSaveResult result;
  const CueSelectionLoadResult previous = load();
  if (!previous) {
    result.error = previous.error;
    return result;
  }
  result.state = previous.state;
  if (previous.state.manager_selected_cue_key == state.manager_selected_cue_key &&
      previous.state.active_overlay_cue_key == state.active_overlay_cue_key) {
    return result;
  }

  const bool manager_changed =
    previous.state.manager_selected_cue_key != state.manager_selected_cue_key;
  if (manager_changed &&
      !store_.write(kNamespace, kManagerSelectionKey, state.manager_selected_cue_key)) {
    result.error = "Could not persist the manager cue selection.";
    return result;
  }
  if (previous.state.active_overlay_cue_key != state.active_overlay_cue_key &&
      !store_.write(kNamespace, kActiveOverlayKey, state.active_overlay_cue_key)) {
    result.error = "Could not persist the active overlay cue selection.";
    if (manager_changed) {
      result.rolled_back = store_.write(
        kNamespace, kManagerSelectionKey, previous.state.manager_selected_cue_key);
      if (!result.rolled_back) result.error += " The manager selection rollback also failed.";
    }
    return result;
  }

  result.state = state;
  result.changed = true;
  return result;
}

} // namespace reaadr::core
