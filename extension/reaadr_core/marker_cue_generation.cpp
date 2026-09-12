#include "marker_cue_generation.hpp"

#include <algorithm>
#include <iomanip>
#include <regex>
#include <sstream>

namespace reaadr::core {
namespace {

std::string trim(const std::string& value)
{
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string number_string(double value)
{
  std::ostringstream output;
  output << std::setprecision(14) << value;
  return output.str();
}

struct OwnedCueLabel {
  std::string cue_id;
  std::string character;
};

OwnedCueLabel parse_owned_cue_label(const std::string& label)
{
  // Lua compatibility: ^[ReaADR]:id=<key> ADR Cue ... - <character>
  static const std::regex pattern(
    R"(^\[ReaADR\]:id=([^\s]+)\s+ADR Cue\s+.*\s+-\s+(.+)$)");
  std::smatch match;
  if (!std::regex_match(label, match, pattern) || match.size() != 3) return {};
  return {match[1].str(), trim(match[2].str())};
}

} // namespace

std::vector<Fields> build_cues_from_project_markers(
  const std::vector<ProjectMarkerCueSource>& sources,
  MarkerCueGenerationOptions options)
{
  options.default_duration = std::max(0.1, options.default_duration);
  if (options.character.empty()) options.character = "ADR";

  std::vector<Fields> cues;
  cues.reserve(sources.size());
  for (const auto& source : sources) {
    if ((source.is_region && !options.include_regions) ||
        (!source.is_region && !options.include_markers)) continue;

    std::string label = trim(source.name);
    if (label.empty()) {
      label = source.is_region ? "Region " + std::to_string(source.marker_id)
                               : "Marker " + std::to_string(source.marker_id);
    }

    const OwnedCueLabel owned = parse_owned_cue_label(label);
    const double end_time = source.is_region && source.end_time > source.start_time
      ? source.end_time : source.start_time + options.default_duration;

    const bool use_owned_export_fields = options.flexible_export && !owned.cue_id.empty();
    Fields cue = {
      {"id", owned.cue_id.empty() ? std::to_string(source.marker_id) : owned.cue_id},
      {"character", use_owned_export_fields && !owned.character.empty() ? owned.character : options.character},
      {"start_time", number_string(source.start_time)},
      {"end_time", number_string(end_time)},
      {"line", use_owned_export_fields ? std::string() : label},
      {"notes", ""},
      {"direction", ""},
      {"cue_type", use_owned_export_fields ? "Dialogue" : (source.is_region ? "Region" : "Marker")},
      {"status", "Not Recorded"},
      {"source_line", std::to_string(cues.size() + 1)},
    };
    cues.push_back(std::move(cue));
  }
  return cues;
}

} // namespace reaadr::core
