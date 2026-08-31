#include "session_mutation.hpp"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace reaadr::core {
namespace {

std::string field(const Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

std::vector<Fields> merge_records(const std::vector<Fields>& existing,
                                  const std::vector<Fields>& derived,
                                  const char* id_key)
{
  std::vector<Fields> merged = existing;
  std::map<std::string, std::size_t> indexes;
  for (std::size_t index = 0; index < merged.size(); ++index) {
    indexes[field(merged[index], id_key)] = index;
  }

  for (const Fields& record : derived) {
    const std::string id = field(record, id_key);
    const auto found = indexes.find(id);
    if (found == indexes.end()) {
      indexes[id] = merged.size();
      merged.push_back(record);
      continue;
    }

    Fields& target = merged[found->second];
    for (const auto& [key, value] : record) {
      // Derived script metadata is intentionally empty. Do not erase metadata
      // retained from an earlier import or edited by a future model version.
      if (key == "metadata" && value.empty() && target.count(key) != 0) continue;
      target[key] = value;
    }
  }
  return merged;
}

void zero_removed_record_counts(std::vector<Fields>& records,
                                const std::vector<Fields>& derived,
                                const char* id_key)
{
  std::set<std::string> active;
  for (const Fields& record : derived) active.insert(field(record, id_key));
  for (Fields& record : records) {
    if (active.count(field(record, id_key)) == 0) record["cue_count"] = "0";
  }
}

} // namespace

SessionBuildResult replace_session_cues(const SessionModel* existing,
                                        const std::vector<Fields>& cues,
                                        const CueReplacementOptions& options)
{
  SessionBuildOptions build_options = options.build;
  build_options.last_operation = options.last_operation;
  build_options.cues_modified = true;
  if (existing) build_options.session_id = existing->session_id();

  SessionBuildResult derived = build_session_model(cues, build_options);
  if (!derived || !existing) return derived;

  SessionModel replacement = *existing;
  replacement.cues = std::move(derived.model.cues);
  replacement.scripts = merge_records(existing->scripts, derived.model.scripts, "script_id");
  replacement.characters = merge_records(existing->characters, derived.model.characters, "character_id");
  zero_removed_record_counts(replacement.scripts, derived.model.scripts, "script_id");
  zero_removed_record_counts(replacement.characters, derived.model.characters, "character_id");

  // Track assignments and regions are entirely cue-derived and therefore must
  // never retain stale objects from the previous cue set.
  replacement.tracks = std::move(derived.model.tracks);
  replacement.regions = std::move(derived.model.regions);

  std::set<std::string> known_imports;
  for (const Fields& record : replacement.imports) known_imports.insert(field(record, "script_id"));
  for (const Fields& record : derived.model.imports) {
    if (known_imports.insert(field(record, "script_id")).second) replacement.imports.push_back(record);
  }

  replacement.state["last_operation"] = options.last_operation;
  replacement.dirty_flags["cues_modified"] = "true";
  if (build_options.tracks_modified) replacement.dirty_flags["tracks_modified"] = "true";
  if (build_options.regions_modified) replacement.dirty_flags["regions_modified"] = "true";
  derived.model = std::move(replacement);
  return derived;
}

} // namespace reaadr::core
