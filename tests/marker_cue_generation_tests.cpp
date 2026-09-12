#include "reaadr_core/marker_cue_generation.hpp"
#include "reaadr_reaper/marker_snapshot_adapter.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

struct ReaProject {};

namespace {

using reaadr::core::Fields;
using reaadr::core::MarkerCueGenerationOptions;
using reaadr::core::ProjectMarkerCueSource;
using reaadr::core::build_cues_from_project_markers;
using reaadr::reaper::MarkerSnapshotApi;
using reaadr::reaper::snapshot_project_markers;

struct FakeMarkerRecord {
  bool is_region = false;
  double start = 0.0;
  double end = 0.0;
  std::string name;
  int id = -1;
  int color = 0;
};

std::vector<FakeMarkerRecord> fake_markers;
bool fail_enumeration = false;

int fake_count_project_markers(ReaProject*, int* markers, int* regions)
{
  int marker_count = 0;
  int region_count = 0;
  for (const auto& record : fake_markers) {
    if (record.is_region) ++region_count;
    else ++marker_count;
  }
  if (markers) *markers = marker_count;
  if (regions) *regions = region_count;
  return marker_count + region_count;
}

int fake_enum_project_markers(ReaProject*, int index, bool* is_region,
                              double* position, double* region_end,
                              const char** name, int* marker_id, int* color)
{
  if (fail_enumeration || index < 0 || index >= static_cast<int>(fake_markers.size())) return 0;
  const auto& record = fake_markers[static_cast<std::size_t>(index)];
  if (is_region) *is_region = record.is_region;
  if (position) *position = record.start;
  if (region_end) *region_end = record.end;
  if (name) *name = record.name.c_str();
  if (marker_id) *marker_id = record.id;
  if (color) *color = record.color;
  return index + 1;
}

void require(bool condition, const std::string& message)
{
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

double field_number(const Fields& fields, const std::string& key)
{
  return std::stod(fields.at(key));
}

void test_marker_defaults()
{
  const std::vector<ProjectMarkerCueSource> sources = {
    {4, false, 1.0, 0.0, "Hello"},
  };
  const auto cues = build_cues_from_project_markers(sources);
  require(cues.size() == 1, "marker creates one cue");
  require(cues[0].at("id") == "4", "marker id preserved");
  require(cues[0].at("character") == "ADR", "default character is ADR");
  require(cues[0].at("line") == "Hello", "marker label becomes line");
  require(cues[0].at("cue_type") == "Marker", "marker cue type");
  require(cues[0].at("status") == "Not Recorded", "default status");
  require(std::abs(field_number(cues[0], "start_time") - 1.0) < 1e-9, "marker start time");
  require(std::abs(field_number(cues[0], "end_time") - 3.0) < 1e-9, "marker default duration");
}

void test_region_duration_and_invalid_duration_fallback()
{
  const std::vector<ProjectMarkerCueSource> sources = {
    {7, true, 2.0, 5.0, "Region line"},
    {8, true, 9.0, 8.0, "Broken region"},
  };
  const auto cues = build_cues_from_project_markers(sources);
  require(cues.size() == 2, "regions create cues");
  require(cues[0].at("cue_type") == "Region", "region cue type");
  require(std::abs(field_number(cues[0], "end_time") - 5.0) < 1e-9, "valid region end preserved");
  require(std::abs(field_number(cues[1], "end_time") - 11.0) < 1e-9, "invalid region uses default duration");
}

void test_empty_labels_and_filters()
{
  const std::vector<ProjectMarkerCueSource> sources = {
    {2, false, 0.0, 0.0, ""},
    {3, true, 1.0, 2.0, "   "},
  };
  MarkerCueGenerationOptions markers_only;
  markers_only.include_regions = false;
  auto cues = build_cues_from_project_markers(sources, markers_only);
  require(cues.size() == 1, "region filter applied");
  require(cues[0].at("line") == "Marker 2", "empty marker label synthesized");

  MarkerCueGenerationOptions regions_only;
  regions_only.include_markers = false;
  cues = build_cues_from_project_markers(sources, regions_only);
  require(cues.size() == 1, "marker filter applied");
  require(cues[0].at("line") == "Region 3", "empty region label synthesized");
}

void test_minimum_default_duration()
{
  const std::vector<ProjectMarkerCueSource> sources = {
    {1, false, 10.0, 0.0, "Short"},
  };
  MarkerCueGenerationOptions options;
  options.default_duration = 0.01;
  const auto cues = build_cues_from_project_markers(sources, options);
  require(std::abs(field_number(cues[0], "end_time") - 10.1) < 1e-9, "default duration clamps to 0.1 seconds");
}

void test_owned_label_standard_mode_matches_generate_cues()
{
  const std::string label = "[ReaADR]:id=CUE-12 ADR Cue 12 - Alice";
  const std::vector<ProjectMarkerCueSource> sources = {
    {99, true, 4.0, 6.0, label},
  };
  const auto cues = build_cues_from_project_markers(sources);
  require(cues[0].at("id") == "CUE-12", "owned label id extracted");
  require(cues[0].at("character") == "ADR", "standard generation keeps configured character");
  require(cues[0].at("line") == label, "standard generation keeps full owned label as line");
  require(cues[0].at("cue_type") == "Region", "standard generation keeps source cue type");
}

void test_owned_label_flexible_export_mode()
{
  const std::string label = "[ReaADR]:id=CUE-12 ADR Cue 12 - Alice";
  const std::vector<ProjectMarkerCueSource> sources = {
    {99, true, 4.0, 6.0, label},
  };
  MarkerCueGenerationOptions options;
  options.flexible_export = true;
  const auto cues = build_cues_from_project_markers(sources, options);
  require(cues[0].at("id") == "CUE-12", "flexible export owned id extracted");
  require(cues[0].at("character") == "Alice", "flexible export character extracted");
  require(cues[0].at("line").empty(), "flexible export clears owned label line");
  require(cues[0].at("cue_type") == "Dialogue", "flexible export uses dialogue cue type");
}

void test_marker_snapshot_adapter()
{
  ReaProject project;
  fake_markers = {
    {false, 1.25, 0.0, "Spot", 4, 10},
    {true, 5.0, 8.5, "Scene", 9, 20},
  };
  fail_enumeration = false;
  const MarkerSnapshotApi api = {fake_count_project_markers, fake_enum_project_markers};
  const auto result = snapshot_project_markers(&project, api);
  require(static_cast<bool>(result), "marker snapshot succeeds");
  require(result.sources.size() == 2, "marker snapshot preserves source count");
  require(!result.sources[0].is_region && result.sources[0].marker_id == 4,
          "marker snapshot preserves marker identity");
  require(result.sources[0].name == "Spot", "marker snapshot preserves marker name");
  require(result.sources[1].is_region && result.sources[1].marker_id == 9,
          "marker snapshot preserves region identity");
  require(std::abs(result.sources[1].end_time - 8.5) < 1e-9,
          "marker snapshot preserves region end");
}

void test_marker_snapshot_errors()
{
  ReaProject project;
  const MarkerSnapshotApi api = {fake_count_project_markers, fake_enum_project_markers};
  require(!snapshot_project_markers(nullptr, api), "marker snapshot rejects null project");
  require(!snapshot_project_markers(&project, {}), "marker snapshot rejects missing APIs");

  fake_markers = {{false, 1.0, 0.0, "Spot", 1, 0}};
  fail_enumeration = true;
  const auto failed = snapshot_project_markers(&project, api);
  require(!failed, "marker snapshot reports enumeration failure");
  require(failed.sources.empty(), "failed marker snapshot does not return partial data");
  fail_enumeration = false;
}

} // namespace

int main()
{
  test_marker_defaults();
  test_region_duration_and_invalid_duration_fallback();
  test_empty_labels_and_filters();
  test_minimum_default_duration();
  test_owned_label_standard_mode_matches_generate_cues();
  test_owned_label_flexible_export_mode();
  test_marker_snapshot_adapter();
  test_marker_snapshot_errors();
  std::cout << "marker_cue_generation_tests: PASS\n";
  return 0;
}
