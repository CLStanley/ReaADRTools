#include "reaadr_core/marker_cue_generation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using reaadr::core::Fields;
using reaadr::core::MarkerCueGenerationOptions;
using reaadr::core::ProjectMarkerCueSource;
using reaadr::core::build_cues_from_project_markers;

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

} // namespace

int main()
{
  test_marker_defaults();
  test_region_duration_and_invalid_duration_fallback();
  test_empty_labels_and_filters();
  test_minimum_default_duration();
  test_owned_label_standard_mode_matches_generate_cues();
  test_owned_label_flexible_export_mode();
  std::cout << "marker_cue_generation_tests: PASS\n";
  return 0;
}
