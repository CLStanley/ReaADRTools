#include "cue_import_workflow.hpp"

#include "reaadr_reaper/xlsx_import.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace reaadr::reaper {
namespace {

bool has_xlsx_extension(std::string path)
{
  std::transform(path.begin(), path.end(), path.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return path.size() >= 5 && path.compare(path.size() - 5, 5, ".xlsx") == 0;
}

std::string serialize_mapping(const std::optional<core::ColumnMapping>& mapping)
{
  if (!mapping) return {};
  std::ostringstream out;
  bool first = true;
  for (const auto& entry : *mapping) {
    if (!first) out << ';';
    first = false;
    out << entry.first << '=' << entry.second;
  }
  return out.str();
}

std::string mode_label(const std::string& mode)
{
  if (mode == "selected") return "selected characters";
  if (mode == "update") return "update existing";
  return "entire sheet";
}

} // namespace

CueImportSourceResult load_cue_import_source(const std::string& source_path)
{
  CueImportSourceResult result;
  result.source_path = source_path;
  if (source_path.empty()) {
    result.error = "No cue sheet path was provided.";
    return result;
  }

  if (has_xlsx_extension(source_path)) {
    const auto xlsx = read_xlsx_first_sheet_as_tsv(source_path);
    if (!xlsx) {
      result.error = xlsx.error;
      return result;
    }
    result.content = xlsx.tsv;
    return result;
  }

  std::ifstream file(source_path, std::ios::binary);
  if (!file) {
    result.error = "Could not open the selected cue sheet.";
    return result;
  }
  result.content.assign(std::istreambuf_iterator<char>(file), {});
  return result;
}

std::string format_cue_import_preview_summary(
  const CueImportPreviewResult& preview,
  const std::optional<core::ColumnMapping>& requested_mapping,
  const CueImportPreviewSummaryOptions& options)
{
  if (!preview) return preview.error;

  const auto mapping = requested_mapping
    ? *requested_mapping : core::default_column_mapping(preview.parsed.table.headers);
  const std::string normalized_mode = normalize_cue_import_mode(options.mode);

  std::size_t selected_count = preview.imported.cues.size();
  if (normalized_mode == "selected" || normalized_mode == "update") {
    selected_count = 0;
    for (const auto& cue : preview.imported.cues) {
      const auto found = cue.find("character");
      if (found != cue.end() &&
          std::find(options.characters.begin(), options.characters.end(), found->second) !=
            options.characters.end()) {
        ++selected_count;
      }
    }
  }

  std::ostringstream summary;
  summary << "Detected " << preview.parsed.table.delimiter_name << " with "
          << preview.parsed.table.headers.size() << " column(s) and "
          << preview.parsed.table.rows.size() << " data row(s).\n\nHeaders:\n";
  for (std::size_t index = 0; index < preview.parsed.table.headers.size(); ++index) {
    if (index) summary << ", ";
    summary << preview.parsed.table.headers[index];
  }
  summary << "\n\nResolved mapping:\n";
  for (const auto& entry : mapping) summary << entry.first << " = " << entry.second << '\n';
  summary << "\nValidation: " << selected_count << " cue(s) ready to import (mode: "
          << mode_label(normalized_mode) << ").";

  if (!preview.parsed.table.rows.empty()) {
    summary << "\n\nFirst row:\n";
    bool first = true;
    for (const auto& cell : preview.parsed.table.rows.front().values) {
      if (!first) summary << '\n';
      first = false;
      summary << cell.first << " = " << cell.second;
    }
  }
  return summary.str();
}

bool run_cue_import_workflow(
  const CueImportWorkflowRequest& request,
  const CueImportHostUi& ui,
  const CueImportWorkflowCallbacks& callbacks,
  std::string* applied_mapping)
{
  if (applied_mapping) applied_mapping->clear();
  if (!ui.choose_source || !ui.message) return false;

  std::string source_path;
  if (!ui.choose_source(source_path)) return false;

  const auto source = load_cue_import_source(source_path);
  if (!source) {
    ui.message("ReaADR Import", source.error);
    return false;
  }

  const auto requested_mapping =
    parse_cue_import_mapping(request.serialized_mapping);
  if (!requested_mapping) {
    ui.message(request.preview_only ? "ReaADR Import Preview" : "ReaADR Import",
               requested_mapping.error);
    return false;
  }

  auto mapping = requested_mapping.mapping;
  if (!request.preview_only && request.serialized_mapping.empty() && ui.prompt) {
    std::string value;
    if (ui.prompt("ReaADR Import: Column Mapping",
                  "Optional mapping key=column;... (blank=auto-detect)", value)) {
      const auto prompted = parse_cue_import_mapping(value);
      if (!prompted) {
        ui.message("ReaADR Import", prompted.error);
        return false;
      }
      mapping = prompted.mapping;
    } else if (!request.persisted_mapping.empty()) {
      mapping = parse_cue_import_mapping(request.persisted_mapping, true).mapping;
    }
  }

  const auto characters =
    parse_cue_import_characters(request.serialized_characters);
  const std::string mode = normalize_cue_import_mode(request.mode);

  if (request.preview_only) {
    if (!callbacks.preview) return false;
    const auto preview = callbacks.preview(source.content, source.source_path, mapping);
    if (!preview) {
      ui.message("ReaADR Import Preview", preview.error);
      return false;
    }
    ui.message("ReaADR Import Preview",
               format_cue_import_preview_summary(preview, mapping, {mode, characters}));
    return true;
  }

  if (!callbacks.import) return false;
  const auto imported =
    callbacks.import(source.content, source.source_path, mapping, mode, characters);
  if (!imported) {
    ui.message("ReaADR Import", imported.error);
    return false;
  }

  if (applied_mapping) *applied_mapping = serialize_mapping(mapping);
  std::ostringstream summary;
  summary << "Imported " << imported.imported.cues.size() << " cue(s) from "
          << source.source_path << ".\n\nTracks created: "
          << imported.rendered.render.tracks_and_regions.tracks_created
          << "\nRegions created: "
          << imported.rendered.render.tracks_and_regions.regions_created;
  ui.message("ReaADR Import (Native)", summary.str());
  return true;
}

} // namespace reaadr::reaper
