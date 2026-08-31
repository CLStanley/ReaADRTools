#include "region_timing_sync.hpp"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace reaadr::core {
namespace {

std::string field(const Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

bool parse_number(const std::string& value, double& output)
{
  char* end = nullptr;
  output = std::strtod(value.c_str(), &end);
  if (!end || end == value.c_str() || !std::isfinite(output)) return false;
  while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' ||
         *end == '\f' || *end == '\v') {
    ++end;
  }
  return *end == '\0';
}

std::string number_string(double value)
{
  std::ostringstream output;
  output << std::setprecision(14) << value;
  return output.str();
}

} // namespace

RegionTimingSyncResult sync_cue_timings_from_regions(
  const SessionModel& model,
  const std::vector<ExistingRegion>& project_regions,
  const RegionTimingSyncOptions& options)
{
  RegionTimingSyncResult result;
  result.cues = model.cues;
  if (!std::isfinite(options.timing_epsilon) || options.timing_epsilon < 0.0) {
    result.error = "The region timing tolerance must be a finite, non-negative number.";
    return result;
  }

  std::map<std::string, const ExistingRegion*> regions_by_name;
  std::set<std::string> duplicate_region_names;
  for (const ExistingRegion& region : project_regions) {
    const auto inserted = regions_by_name.emplace(region.name, &region);
    if (!inserted.second) duplicate_region_names.insert(region.name);
  }

  std::set<std::string> desired_region_names;
  for (std::size_t index = 0; index < result.cues.size(); ++index) {
    Fields& cue = result.cues[index];
    const std::string cue_key = render_cue_key(cue);
    if (cue_key.empty()) {
      result.error = "Cue " + field(cue, "id") + " has no stable generated-region identity.";
      return result;
    }
    const std::string region_name = render_region_name(cue);
    if (!desired_region_names.insert(region_name).second) {
      result.error = "Multiple cues resolve to the generated region name: " + region_name;
      return result;
    }

    const auto found = regions_by_name.find(region_name);
    if (found == regions_by_name.end()) {
      ++result.missing_regions;
      continue;
    }
    if (duplicate_region_names.count(region_name) != 0) {
      result.error = "Multiple project regions match the generated region name: " + region_name;
      return result;
    }

    double model_start = 0.0;
    double model_end = 0.0;
    if (!parse_number(field(cue, "start_time"), model_start) ||
        !parse_number(field(cue, "end_time"), model_end)) {
      result.error = "Cue " + field(cue, "id") + " has invalid canonical timing.";
      return result;
    }
    const ExistingRegion& region = *found->second;
    if (!std::isfinite(region.start_time) || !std::isfinite(region.end_time)) {
      result.error = "Project region timing is invalid for " + region_name + ".";
      return result;
    }

    const double region_end = region.end_time < region.start_time
      ? region.start_time
      : region.end_time;
    if (std::abs(region.start_time - model_start) <= options.timing_epsilon &&
        std::abs(region_end - model_end) <= options.timing_epsilon) {
      continue;
    }
    cue["start_time"] = number_string(region.start_time);
    cue["end_time"] = number_string(region_end);
    ++result.changed_cues;
  }
  return result;
}

} // namespace reaadr::core
