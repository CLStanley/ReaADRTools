#include "cue_take_count_adapter.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace reaadr::reaper {
namespace {

std::string field(const core::Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

bool parse_seconds(const std::string& text, double& value)
{
  if (text.empty()) return false;
  errno = 0;
  char* end = nullptr;
  const double parsed = std::strtod(text.c_str(), &end);
  if (errno != 0 || !end || end == text.c_str() || !std::isfinite(parsed)) return false;
  while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') ++end;
  if (*end != '\0') return false;
  value = parsed;
  return true;
}

std::string lowercase_ascii(std::string value)
{
  for (char& ch : value) {
    const unsigned char byte = static_cast<unsigned char>(ch);
    ch = static_cast<char>(std::tolower(byte));
  }
  return value;
}

std::string track_string(const CueTakeCountApi& api, MediaTrack* track, const char* key)
{
  if (!api.get_set_track_string || !track) return {};
  std::vector<char> buffer(4096, '\0');
  if (!api.get_set_track_string(track, key, buffer.data(), false)) return {};
  return buffer.data();
}

std::string character_from_track_key(const std::string& key)
{
  const std::size_t lane = key.rfind(".lane");
  if (lane == std::string::npos || lane + 5 >= key.size()) return key;
  for (std::size_t i = lane + 5; i < key.size(); ++i)
    if (key[i] < '0' || key[i] > '9') return key;
  return key.substr(0, lane);
}

} // namespace

CueTakeCountResult count_recorded_takes_for_cue(
  ReaProject* project,
  const CueTakeCountApi& api,
  const core::Fields& cue)
{
  CueTakeCountResult result;
  if (!api.count_tracks || !api.get_track || !api.get_set_track_string ||
      !api.count_track_media_items || !api.get_track_media_item ||
      !api.get_media_item_info_value || !api.count_takes) {
    result.error = "Required REAPER take-count APIs are unavailable.";
    return result;
  }

  double cue_start = 0.0;
  double cue_end = 0.0;
  if (!parse_seconds(field(cue, "start_time"), cue_start) ||
      !parse_seconds(field(cue, "end_time"), cue_end) || cue_end <= cue_start) {
    result.error = "Cue timing is missing or invalid while counting recorded takes.";
    return result;
  }

  const std::string character = lowercase_ascii(field(cue, "character"));
  const int track_count = (std::max)(0, api.count_tracks(project));
  for (int track_index = 0; track_index < track_count; ++track_index) {
    MediaTrack* track = api.get_track(project, track_index);
    if (!track) continue;
    if (track_string(api, track, "P_EXT:ReaADR.role") != "character") continue;

    const std::string key = track_string(api, track, "P_EXT:ReaADR.key");
    const std::string track_character = lowercase_ascii(character_from_track_key(key));
    if (!character.empty() && track_character != character) continue;

    const int item_count = (std::max)(0, api.count_track_media_items(track));
    for (int item_index = 0; item_index < item_count; ++item_index) {
      MediaItem* item = api.get_track_media_item(track, item_index);
      if (!item) continue;
      const double item_start = api.get_media_item_info_value(item, "D_POSITION");
      const double item_length = api.get_media_item_info_value(item, "D_LENGTH");
      if (!std::isfinite(item_start) || !std::isfinite(item_length)) continue;
      const double item_end = item_start + item_length;
      if (item_end <= cue_start || item_start >= cue_end) continue;
      result.take_count += static_cast<std::size_t>((std::max)(1, api.count_takes(item)));
    }
  }
  return result;
}

} // namespace reaadr::reaper
