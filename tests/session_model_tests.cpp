#include "reaadr_core/session_model.hpp"
#include "reaadr_core/domain_utils.hpp"
#include "reaadr_core/cue_import.hpp"
#include "reaadr_core/cue_navigation.hpp"
#include "reaadr_core/cue_status.hpp"
#include "reaadr_core/cue_wav.hpp"
#include "reaadr_core/event_log.hpp"
#include "reaadr_core/model_repository.hpp"
#include "reaadr_core/overlay_refresh.hpp"
#include "reaadr_core/region_timing_sync.hpp"
#include "reaadr_core/record_arm.hpp"
#include "reaadr_core/recording_setup.hpp"
#include "reaadr_core/recording_transport.hpp"
#include "reaadr_core/recording_preferences.hpp"
#include "reaadr_core/render_plan.hpp"
#include "reaadr_core/session_builder.hpp"
#include "reaadr_core/session_commit.hpp"
#include "reaadr_core/session_mutation.hpp"
#include "reaadr_reaper/project_state.hpp"
#include "reaadr_reaper/project_transaction.hpp"
#include "reaadr_reaper/overlay_refresh_adapter.hpp"
#include "reaadr_reaper/cue_navigation_service.hpp"
#include "reaadr_reaper/record_arm_adapter.hpp"
#include "reaadr_reaper/recording_setup_adapter.hpp"
#include "reaadr_reaper/recording_application_service.hpp"
#include "reaadr_reaper/recording_transport_executor.hpp"
#include "reaadr_reaper/render_artifact_adapter.hpp"
#include "reaadr_reaper/session_render_service.hpp"
#include "reaadr_reaper/track_region_adapter.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

class FakeProjectStateStore final : public reaadr::core::ProjectStateStore {
public:
  reaadr::core::StateReadResult read(const char* name_space, const char* key) const override
  {
    const auto found = values.find(std::string(name_space) + ":" + key);
    if (found == values.end()) return {{}, reaadr::core::StateReadError::not_found};
    return {found->second, reaadr::core::StateReadError::none};
  }

  bool write(const char* name_space, const char* key, const std::string& value) override
  {
    const std::string full_key = std::string(name_space) + ":" + key;
    if (!writes_succeed) return false;
    if (!failed_write_key.empty() && full_key == failed_write_key && failed_writes_remaining != 0) {
      if (failed_writes_remaining > 0) --failed_writes_remaining;
      return false;
    }
    if (value.empty()) values.erase(full_key);
    else values[full_key] = value;
    return true;
  }

  std::map<std::string, std::string> values;
  bool writes_succeed = true;
  std::string failed_write_key;
  int failed_writes_remaining = 0;
};

std::string project_state_value;
bool project_state_exists = true;
std::string project_state_written;

int fake_get_project_state(ReaProject*, const char*, const char*, char* output, int output_size)
{
  if (!project_state_exists || output_size <= 0) return 0;
  const std::size_t count = (std::min)(project_state_value.size(), static_cast<std::size_t>(output_size - 1));
  std::memcpy(output, project_state_value.data(), count);
  output[count] = '\0';
  return 1;
}

int fake_set_project_state(ReaProject*, const char*, const char*, const char* value)
{
  project_state_written = value ? value : "";
  return static_cast<int>(project_state_written.size());
}

int navigation_play_state = 0;
double navigation_play_position = 0.0;
double navigation_cursor_position = 0.0;
int navigation_cursor_moves = 0;
bool navigation_move_view = false;
bool navigation_seek_play = false;

int fake_get_play_state() { return navigation_play_state; }
double fake_get_play_position() { return navigation_play_position; }
double fake_get_cursor_position() { return navigation_cursor_position; }
void fake_set_edit_cursor_position(double position, bool move_view, bool seek_play)
{
  navigation_cursor_position = position;
  navigation_move_view = move_view;
  navigation_seek_play = seek_play;
  ++navigation_cursor_moves;
}

reaadr::reaper::CueNavigationApi fake_navigation_api()
{
  return {
    fake_get_play_state,
    fake_get_play_position,
    fake_get_cursor_position,
    fake_set_edit_cursor_position,
  };
}

struct TransactionProbe {
  int begins = 0;
  int ends = 0;
  int undos = 0;
  int refresh_balance = 0;
  std::string end_description;
  std::string available_undo;
};

TransactionProbe transaction_probe;

void fake_begin(ReaProject*) { ++transaction_probe.begins; }
void fake_end(ReaProject*, const char* description, int)
{
  ++transaction_probe.ends;
  transaction_probe.end_description = description ? description : "";
}
const char* fake_can_undo(ReaProject*)
{
  return transaction_probe.available_undo.empty() ? nullptr : transaction_probe.available_undo.c_str();
}
int fake_undo(ReaProject*) { ++transaction_probe.undos; return 1; }
void fake_prevent_refresh(int amount) { transaction_probe.refresh_balance += amount; }

reaadr::reaper::TransactionApi fake_transaction_api()
{
  return {fake_begin, fake_end, fake_can_undo, fake_undo, fake_prevent_refresh};
}

struct FakeSource {
  std::string path;
  double length = 0.0;
};

struct FakeTake {
  std::map<std::string, std::string> strings;
  FakeSource* source = nullptr;
};

struct FakeItem {
  std::map<std::string, std::string> strings;
  std::map<std::string, double> values;
  bool has_take = false;
  FakeTake take;
};

struct FakeFx {
  std::string renamed_name;
  std::string video_code;
  bool enabled = false;
};

struct FakeTrack {
  std::map<std::string, std::string> strings;
  int color = 0;
  std::vector<std::unique_ptr<FakeItem>> items;
  bool muted = false;
  double record_armed = 0.0;
  std::vector<FakeFx> effects;
};

struct FakeRegion {
  int id = -1;
  std::string name;
  double start_time = 0.0;
  double end_time = 0.0;
  int color = 0;
  int ruler_lane = 0;
  bool hidden = false;
};

struct FakeRulerLane {
  std::string name;
  int color = 0;
  bool hidden = false;
};

struct RenderAdapterProbe {
  std::vector<FakeTrack> tracks;
  std::vector<FakeRegion> regions;
  std::vector<FakeRulerLane> ruler_lanes;
  std::map<std::string, double> source_lengths;
  std::set<FakeSource*> live_sources;
  bool fail_region_update = false;
  bool fail_track_mute = false;
  FakeTrack* fail_record_arm_track = nullptr;
  std::set<FakeTrack*> invalid_record_arm_tracks;
  bool fail_region_hidden = false;
  bool fail_item_value = false;
  bool fail_overlay_add = false;
  bool fail_overlay_delete = false;
  std::string fail_overlay_set_parameter;
  int fail_overlay_set_remaining = 0;
  int last_overlay_instantiate = 0;
  int next_region_id = 100;
  int window_adjustments = 0;
  int arrange_updates = 0;
};

RenderAdapterProbe render_adapter_probe;

void destroy_fake_source(FakeSource* source)
{
  if (!source) return;
  const auto found = render_adapter_probe.live_sources.find(source);
  if (found == render_adapter_probe.live_sources.end()) return;
  render_adapter_probe.live_sources.erase(found);
  delete source;
}

FakeTrack* fake_track(MediaTrack* track)
{
  return reinterpret_cast<FakeTrack*>(track);
}

int fake_count_tracks(ReaProject*)
{
  return static_cast<int>(render_adapter_probe.tracks.size());
}

MediaTrack* fake_get_track(ReaProject*, int index)
{
  if (index < 0 || static_cast<std::size_t>(index) >= render_adapter_probe.tracks.size()) return nullptr;
  return reinterpret_cast<MediaTrack*>(&render_adapter_probe.tracks[static_cast<std::size_t>(index)]);
}

void fake_insert_track(int index, bool)
{
  if (index < 0 || static_cast<std::size_t>(index) > render_adapter_probe.tracks.size()) return;
  render_adapter_probe.tracks.insert(render_adapter_probe.tracks.begin() + index, FakeTrack{});
}

bool fake_get_set_track_string(MediaTrack* track, const char* parameter, char* value, bool set)
{
  if (!track || !parameter || !value) return false;
  FakeTrack* current = fake_track(track);
  if (set) {
    current->strings[parameter] = value;
  } else {
    const auto found = current->strings.find(parameter);
    std::strcpy(value, found == current->strings.end() ? "" : found->second.c_str());
  }
  return true;
}

double fake_get_track_value(MediaTrack* track, const char* parameter)
{
  if (!track) return 0.0;
  const std::string name = parameter ? parameter : "";
  if (name == "I_CUSTOMCOLOR") return fake_track(track)->color;
  if (name == "B_MUTE") return fake_track(track)->muted ? 1.0 : 0.0;
  if (name == "I_RECARM") return fake_track(track)->record_armed;
  return 0.0;
}

bool fake_set_track_value(MediaTrack* track, const char* parameter, double value)
{
  if (!track) return false;
  const std::string name = parameter ? parameter : "";
  if (name == "I_CUSTOMCOLOR") {
    fake_track(track)->color = static_cast<int>(value);
    return true;
  }
  if (name == "B_MUTE" && !render_adapter_probe.fail_track_mute) {
    fake_track(track)->muted = value != 0.0;
    return true;
  }
  if (name == "I_RECARM" && fake_track(track) != render_adapter_probe.fail_record_arm_track) {
    fake_track(track)->record_armed = value;
    return true;
  }
  return false;
}

bool fake_validate_record_arm_track(ReaProject*, MediaTrack* track)
{
  return track && render_adapter_probe.invalid_record_arm_tracks.count(fake_track(track)) == 0;
}

reaadr::reaper::RecordArmApi fake_record_arm_api()
{
  return {
    fake_count_tracks,
    fake_get_track,
    fake_validate_record_arm_track,
    fake_get_track_value,
    fake_set_track_value,
  };
}

int stale_recording_track_index = -1;
int stale_recording_track_reads = 0;

MediaTrack* fake_get_recording_track(ReaProject* project, int index)
{
  MediaTrack* track = fake_get_track(project, index);
  if (track && index == stale_recording_track_index && ++stale_recording_track_reads == 2) {
    fake_track(track)->strings["P_EXT:ReaADR.role"] = "user_audio";
  }
  return track;
}

reaadr::reaper::RecordingSetupApi fake_recording_setup_api()
{
  return {
    fake_count_tracks,
    fake_get_recording_track,
    fake_validate_record_arm_track,
    fake_get_set_track_string,
  };
}

int fake_track_fx_get_count(MediaTrack* track)
{
  return track ? static_cast<int>(fake_track(track)->effects.size()) : -1;
}

bool fake_track_fx_get_named_config(MediaTrack* track, int fx_index,
                                    const char* parameter, char* value, int value_size)
{
  if (!track || fx_index < 0 || !parameter || !value || value_size <= 0 ||
      static_cast<std::size_t>(fx_index) >= fake_track(track)->effects.size()) {
    return false;
  }
  const FakeFx& effect = fake_track(track)->effects[static_cast<std::size_t>(fx_index)];
  const std::string name = parameter;
  const std::string* source = nullptr;
  if (name == "renamed_name") source = &effect.renamed_name;
  else if (name == "VIDEO_CODE") source = &effect.video_code;
  else return false;
  std::snprintf(value, static_cast<std::size_t>(value_size), "%s", source->c_str());
  return true;
}

bool fake_track_fx_get_enabled(MediaTrack* track, int fx_index)
{
  return track && fx_index >= 0 &&
    static_cast<std::size_t>(fx_index) < fake_track(track)->effects.size() &&
    fake_track(track)->effects[static_cast<std::size_t>(fx_index)].enabled;
}

int fake_track_fx_add_by_name(MediaTrack* track, const char*, bool, int instantiate)
{
  if (!track || render_adapter_probe.fail_overlay_add) return -1;
  render_adapter_probe.last_overlay_instantiate = instantiate;
  fake_track(track)->effects.push_back({});
  return static_cast<int>(fake_track(track)->effects.size() - 1);
}

bool fake_track_fx_delete(MediaTrack* track, int fx_index)
{
  if (!track || render_adapter_probe.fail_overlay_delete || fx_index < 0 ||
      static_cast<std::size_t>(fx_index) >= fake_track(track)->effects.size()) {
    return false;
  }
  fake_track(track)->effects.erase(fake_track(track)->effects.begin() + fx_index);
  return true;
}

bool fake_track_fx_set_named_config(MediaTrack* track, int fx_index,
                                    const char* parameter, const char* value)
{
  if (!track || fx_index < 0 || !parameter ||
      static_cast<std::size_t>(fx_index) >= fake_track(track)->effects.size()) {
    return false;
  }
  const std::string name = parameter;
  if (render_adapter_probe.fail_overlay_set_parameter == name &&
      render_adapter_probe.fail_overlay_set_remaining != 0) {
    if (render_adapter_probe.fail_overlay_set_remaining > 0) {
      --render_adapter_probe.fail_overlay_set_remaining;
    }
    return false;
  }
  FakeFx& effect = fake_track(track)->effects[static_cast<std::size_t>(fx_index)];
  if (name == "renamed_name") effect.renamed_name = value ? value : "";
  else if (name == "VIDEO_CODE") effect.video_code = value ? value : "";
  else if (name != "DONE") return false;
  return true;
}

void fake_track_fx_set_enabled(MediaTrack* track, int fx_index, bool enabled)
{
  if (!track || fx_index < 0 ||
      static_cast<std::size_t>(fx_index) >= fake_track(track)->effects.size()) {
    return;
  }
  fake_track(track)->effects[static_cast<std::size_t>(fx_index)].enabled = enabled;
}

void fake_adjust_track_windows(bool);
void fake_update_arrange();

reaadr::reaper::OverlayRefreshApi fake_overlay_refresh_api()
{
  return {
    fake_count_tracks,
    fake_get_track,
    fake_validate_record_arm_track,
    fake_get_set_track_string,
    fake_track_fx_get_count,
    fake_track_fx_get_named_config,
    fake_track_fx_get_enabled,
    fake_track_fx_add_by_name,
    fake_track_fx_delete,
    fake_track_fx_set_named_config,
    fake_track_fx_set_enabled,
    fake_adjust_track_windows,
    fake_update_arrange,
  };
}

struct RecordingTransportProbe {
  double loop_start = 0.0;
  double loop_end = 0.0;
  double cursor_position = 0.0;
  bool cursor_move_view = false;
  bool cursor_seek_play = false;
  bool fail_get_loop = false;
  bool fail_set_loop = false;
  bool fail_cursor = false;
  int fail_command = 0;
  std::vector<int> commands;
};

RecordingTransportProbe recording_transport_probe;

bool fake_get_loop_time_range(double* start, double* end)
{
  if (recording_transport_probe.fail_get_loop || !start || !end) return false;
  *start = recording_transport_probe.loop_start;
  *end = recording_transport_probe.loop_end;
  return true;
}

bool fake_set_loop_time_range(double start, double end)
{
  if (recording_transport_probe.fail_set_loop) return false;
  recording_transport_probe.loop_start = start;
  recording_transport_probe.loop_end = end;
  return true;
}

bool fake_set_recording_cursor(double position, bool move_view, bool seek_play)
{
  if (recording_transport_probe.fail_cursor) return false;
  recording_transport_probe.cursor_position = position;
  recording_transport_probe.cursor_move_view = move_view;
  recording_transport_probe.cursor_seek_play = seek_play;
  return true;
}

bool fake_run_recording_command(int command)
{
  recording_transport_probe.commands.push_back(command);
  return recording_transport_probe.fail_command != command;
}

reaadr::reaper::RecordingTransportApi fake_recording_transport_api()
{
  return {
    fake_get_loop_time_range,
    fake_set_loop_time_range,
    fake_set_recording_cursor,
    fake_run_recording_command,
  };
}

bool recording_overlay_refresh_succeeds = true;
int recording_overlay_refreshes = 0;

bool fake_refresh_recording_overlay()
{
  ++recording_overlay_refreshes;
  return recording_overlay_refresh_succeeds;
}

int fake_count_project_markers(ReaProject*, int* markers, int* regions)
{
  if (markers) *markers = 0;
  if (regions) *regions = static_cast<int>(render_adapter_probe.regions.size());
  return static_cast<int>(render_adapter_probe.regions.size());
}

int fake_enum_project_markers(ReaProject*, int index, bool* is_region, double* start_time,
                              double* end_time, const char** name, int* id, int* color)
{
  if (index < 0 || static_cast<std::size_t>(index) >= render_adapter_probe.regions.size()) return 0;
  const FakeRegion& region = render_adapter_probe.regions[static_cast<std::size_t>(index)];
  if (is_region) *is_region = true;
  if (start_time) *start_time = region.start_time;
  if (end_time) *end_time = region.end_time;
  if (name) *name = region.name.c_str();
  if (id) *id = region.id;
  if (color) *color = region.color;
  return 1;
}

bool fake_set_project_marker(ReaProject*, int id, bool is_region, double start_time,
                             double end_time, const char* name, int color, int)
{
  if (!is_region || render_adapter_probe.fail_region_update) return false;
  for (FakeRegion& region : render_adapter_probe.regions) {
    if (region.id != id) continue;
    region.start_time = start_time;
    region.end_time = end_time;
    region.name = name ? name : "";
    region.color = color;
    return true;
  }
  return false;
}

int fake_add_project_marker(ReaProject*, bool is_region, double start_time, double end_time,
                            const char* name, int, int color)
{
  if (!is_region) return -1;
  const int id = render_adapter_probe.next_region_id++;
  render_adapter_probe.regions.push_back({id, name ? name : "", start_time, end_time, color});
  return id;
}

bool fake_delete_project_marker(ReaProject*, int id, bool is_region)
{
  if (!is_region) return false;
  const auto found = std::find_if(render_adapter_probe.regions.begin(), render_adapter_probe.regions.end(),
    [id](const FakeRegion& region) { return region.id == id; });
  if (found == render_adapter_probe.regions.end()) return false;
  render_adapter_probe.regions.erase(found);
  return true;
}

int fake_color_to_native(int red, int green, int blue)
{
  return red | (green << 8) | (blue << 16);
}

void fake_color_from_native(int color, int* red, int* green, int* blue)
{
  if (red) *red = color & 0xff;
  if (green) *green = (color >> 8) & 0xff;
  if (blue) *blue = (color >> 16) & 0xff;
}

void fake_adjust_track_windows(bool) { ++render_adapter_probe.window_adjustments; }
void fake_update_arrange() { ++render_adapter_probe.arrange_updates; }

reaadr::reaper::TrackRegionApi fake_render_api()
{
  return {
    fake_count_tracks,
    fake_get_track,
    fake_insert_track,
    fake_get_set_track_string,
    fake_get_track_value,
    fake_set_track_value,
    fake_count_project_markers,
    fake_enum_project_markers,
    fake_set_project_marker,
    fake_add_project_marker,
    fake_delete_project_marker,
    fake_color_to_native,
    fake_color_from_native,
    fake_adjust_track_windows,
    fake_update_arrange,
  };
}

double fake_get_set_project_info(ReaProject*, const char* parameter, double value, bool set)
{
  const std::string name = parameter ? parameter : "";
  if (name == "RULER_LANE_COUNT") {
    if (set && value >= 0.0) render_adapter_probe.ruler_lanes.resize(static_cast<std::size_t>(value));
    return static_cast<double>(render_adapter_probe.ruler_lanes.size());
  }
  const auto colon = name.find(':');
  if (colon == std::string::npos) return 0.0;
  const int index = std::atoi(name.substr(colon + 1).c_str());
  if (index < 0 || static_cast<std::size_t>(index) >= render_adapter_probe.ruler_lanes.size()) return 0.0;
  FakeRulerLane& lane = render_adapter_probe.ruler_lanes[static_cast<std::size_t>(index)];
  if (name.rfind("RULER_LANE_COLOR:", 0) == 0) {
    if (set) lane.color = static_cast<int>(value);
    return lane.color;
  }
  if (name.rfind("RULER_LANE_HIDDEN:", 0) == 0) {
    if (set) lane.hidden = value != 0.0;
    return lane.hidden ? 1.0 : 0.0;
  }
  return 0.0;
}

bool fake_get_set_project_info_string(ReaProject*, const char* parameter, char* value, bool set)
{
  if (!parameter || !value) return false;
  const std::string name = parameter;
  if (name.rfind("RULER_LANE_NAME:", 0) != 0) return false;
  const int index = std::atoi(name.substr(std::strlen("RULER_LANE_NAME:")).c_str());
  if (index < 0 || static_cast<std::size_t>(index) >= render_adapter_probe.ruler_lanes.size()) return false;
  FakeRulerLane& lane = render_adapter_probe.ruler_lanes[static_cast<std::size_t>(index)];
  if (set) lane.name = value;
  else std::strcpy(value, lane.name.c_str());
  return true;
}

ProjectMarker* fake_get_region_or_marker(ReaProject*, int index, const char*)
{
  if (index < 0 || static_cast<std::size_t>(index) >= render_adapter_probe.regions.size()) return nullptr;
  return reinterpret_cast<ProjectMarker*>(&render_adapter_probe.regions[static_cast<std::size_t>(index)]);
}

double fake_get_region_value(ReaProject*, ProjectMarker* marker, const char* parameter)
{
  if (!marker) return 0.0;
  const std::string name = parameter ? parameter : "";
  if (name == "I_LANENUMBER") return reinterpret_cast<FakeRegion*>(marker)->ruler_lane;
  if (name == "B_HIDDEN") return reinterpret_cast<FakeRegion*>(marker)->hidden ? 1.0 : 0.0;
  return 0.0;
}

double fake_set_region_value(ReaProject*, ProjectMarker* marker, const char* parameter, double value)
{
  if (!marker) return 0.0;
  const std::string name = parameter ? parameter : "";
  if (name == "I_LANENUMBER") {
    reinterpret_cast<FakeRegion*>(marker)->ruler_lane = static_cast<int>(value);
    return value;
  }
  if (name == "B_HIDDEN" && !render_adapter_probe.fail_region_hidden) {
    reinterpret_cast<FakeRegion*>(marker)->hidden = value != 0.0;
    return value;
  }
  return 0.0;
}

reaadr::reaper::RulerLaneApi fake_ruler_lane_api()
{
  return {
    fake_get_set_project_info,
    fake_get_set_project_info_string,
    fake_count_project_markers,
    fake_enum_project_markers,
    fake_get_region_or_marker,
    fake_get_region_value,
    fake_set_region_value,
    fake_color_to_native,
    fake_color_from_native,
  };
}

FakeItem* fake_item(MediaItem* item) { return reinterpret_cast<FakeItem*>(item); }
FakeTake* fake_take(MediaItem_Take* take) { return reinterpret_cast<FakeTake*>(take); }

int fake_count_track_items(MediaTrack* track)
{
  return track ? static_cast<int>(fake_track(track)->items.size()) : -1;
}

MediaItem* fake_get_track_item(MediaTrack* track, int index)
{
  if (!track || index < 0 || static_cast<std::size_t>(index) >= fake_track(track)->items.size()) return nullptr;
  return reinterpret_cast<MediaItem*>(fake_track(track)->items[static_cast<std::size_t>(index)].get());
}

bool fake_get_set_item_string(MediaItem* item, const char* parameter, char* value, bool set)
{
  if (!item || !parameter || !value) return false;
  FakeItem* current = fake_item(item);
  if (set) current->strings[parameter] = value;
  else {
    const auto found = current->strings.find(parameter);
    std::strcpy(value, found == current->strings.end() ? "" : found->second.c_str());
  }
  return true;
}

double fake_get_item_value(MediaItem* item, const char* parameter)
{
  if (!item || !parameter) return 0.0;
  const auto found = fake_item(item)->values.find(parameter);
  return found == fake_item(item)->values.end() ? 0.0 : found->second;
}

bool fake_set_item_value(MediaItem* item, const char* parameter, double value)
{
  if (!item || !parameter || render_adapter_probe.fail_item_value) return false;
  fake_item(item)->values[parameter] = value;
  return true;
}

MediaItem* fake_add_item_to_track(MediaTrack* track)
{
  if (!track) return nullptr;
  auto item = std::make_unique<FakeItem>();
  FakeItem* pointer = item.get();
  fake_track(track)->items.push_back(std::move(item));
  return reinterpret_cast<MediaItem*>(pointer);
}

bool fake_delete_track_item(MediaTrack* track, MediaItem* item)
{
  if (!track || !item) return false;
  auto& items = fake_track(track)->items;
  const auto found = std::find_if(items.begin(), items.end(),
    [&](const std::unique_ptr<FakeItem>& candidate) { return candidate.get() == fake_item(item); });
  if (found == items.end()) return false;
  destroy_fake_source((*found)->take.source);
  items.erase(found);
  return true;
}

bool fake_move_item_to_track(MediaItem* item, MediaTrack* destination)
{
  if (!item || !destination) return false;
  for (FakeTrack& track : render_adapter_probe.tracks) {
    const auto found = std::find_if(track.items.begin(), track.items.end(),
      [&](const std::unique_ptr<FakeItem>& candidate) { return candidate.get() == fake_item(item); });
    if (found == track.items.end()) continue;
    std::unique_ptr<FakeItem> moved = std::move(*found);
    track.items.erase(found);
    fake_track(destination)->items.push_back(std::move(moved));
    return true;
  }
  return false;
}

MediaItem_Take* fake_get_active_item_take(MediaItem* item)
{
  return item && fake_item(item)->has_take
    ? reinterpret_cast<MediaItem_Take*>(&fake_item(item)->take)
    : nullptr;
}

MediaItem_Take* fake_add_take_to_item(MediaItem* item)
{
  if (!item) return nullptr;
  fake_item(item)->has_take = true;
  return reinterpret_cast<MediaItem_Take*>(&fake_item(item)->take);
}

bool fake_get_set_take_string(MediaItem_Take* take, const char* parameter, char* value, bool set)
{
  if (!take || !parameter || !value) return false;
  FakeTake* current = fake_take(take);
  if (set) current->strings[parameter] = value;
  else {
    const auto found = current->strings.find(parameter);
    std::strcpy(value, found == current->strings.end() ? "" : found->second.c_str());
  }
  return true;
}

void* fake_get_set_take_info(MediaItem_Take* take, const char* parameter, void* value)
{
  if (!take || std::string(parameter ? parameter : "") != "P_SOURCE") return nullptr;
  FakeTake* current = fake_take(take);
  FakeSource* old = current->source;
  if (value) current->source = static_cast<FakeSource*>(value);
  return old;
}

PCM_source* fake_create_source(const char* path)
{
  const std::string source_path = path ? path : "";
  const auto found = render_adapter_probe.source_lengths.find(source_path);
  if (found == render_adapter_probe.source_lengths.end()) return nullptr;
  auto* source = new FakeSource{source_path, found->second};
  render_adapter_probe.live_sources.insert(source);
  return reinterpret_cast<PCM_source*>(source);
}

double fake_get_source_length(PCM_source* source, bool* length_is_quarters)
{
  if (length_is_quarters) *length_is_quarters = false;
  return source ? reinterpret_cast<FakeSource*>(source)->length : 0.0;
}

void fake_destroy_source(PCM_source* source)
{
  destroy_fake_source(reinterpret_cast<FakeSource*>(source));
}

reaadr::reaper::CueAudioApi fake_cue_audio_api()
{
  return {
    fake_count_tracks,
    fake_get_track,
    fake_get_set_track_string,
    fake_count_track_items,
    fake_get_track_item,
    fake_get_set_item_string,
    fake_get_item_value,
    fake_set_item_value,
    fake_add_item_to_track,
    fake_delete_track_item,
    fake_move_item_to_track,
    fake_get_active_item_take,
    fake_add_take_to_item,
    fake_get_set_take_string,
    fake_get_set_take_info,
    fake_create_source,
    fake_get_source_length,
    fake_destroy_source,
  };
}

void check(bool condition, const std::string& message)
{
  if (!condition) {
    ++failures;
    std::cerr << "not ok - " << message << '\n';
  }
}

std::uint16_t read_le16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
  return static_cast<std::uint16_t>(bytes[offset]) |
    static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t read_le32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
  return static_cast<std::uint32_t>(bytes[offset]) |
    (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
    (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
    (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

void test_cue_wav()
{
  const auto cue_wav = reaadr::core::build_cue_wav();
  check(cue_wav && cue_wav.bytes.size() == 288044 &&
          std::string(cue_wav.bytes.begin(), cue_wav.bytes.begin() + 4) == "RIFF" &&
          std::string(cue_wav.bytes.begin() + 8, cue_wav.bytes.begin() + 12) == "WAVE",
        "native cue-WAV generation emits a complete three-second RIFF asset");
  check(read_le16(cue_wav.bytes, 20) == 1 && read_le16(cue_wav.bytes, 22) == 1 &&
          read_le32(cue_wav.bytes, 24) == 48000 && read_le16(cue_wav.bytes, 34) == 16 &&
          read_le32(cue_wav.bytes, 40) == 288000,
        "native cue-WAV header declares mono 48 kHz 16-bit PCM");

  // A 1 kHz sine at 48 kHz reaches its first positive quarter-cycle after 12
  // samples. Silence between beeps proves their frame-length gate is applied.
  const auto sample = [&](std::size_t index) {
    return static_cast<std::int16_t>(read_le16(cue_wav.bytes, 44 + index * 2));
  };
  check(sample(0) == 0 && sample(12) > 11000 && sample(3000) == 0 &&
          sample(48000) == 0 && sample(48012) > 11000,
        "native cue-WAV samples contain frame-length beeps at one-second intervals");

  reaadr::core::CueWavOptions fallback;
  fallback.frame_rate = 0.0;
  const auto fallback_wav = reaadr::core::build_cue_wav(fallback);
  check(fallback_wav && std::abs(fallback_wav.frame_rate - 24.0) < 0.000001,
        "native cue-WAV generation retains the Lua 24 fps fallback");
  fallback.amplitude = 1.1;
  check(!reaadr::core::build_cue_wav(fallback),
        "native cue-WAV generation rejects unsafe amplitude settings");

  const std::string path = "/tmp/reaadr-native-cue-wav-test.wav";
  std::string write_error;
  check(reaadr::core::write_cue_wav_file(path, cue_wav.bytes, write_error),
        "native cue-WAV writing atomically publishes the generated asset");
  std::ifstream input(path, std::ios::binary);
  const std::vector<std::uint8_t> written{
    std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  check(written == cue_wav.bytes,
        "published native cue-WAV bytes exactly match the validated in-memory asset");
  std::remove(path.c_str());
  std::remove((path + ".reaadr.tmp").c_str());
}

void test_event_log_repository()
{
  FakeProjectStateStore store;
  reaadr::core::EventLogRepository events(store);
  check(events.load() && events.load().lines.empty(),
        "a missing native event log loads as an empty compatibility history");

  store.values["ReaADRTools:event_counter"] = "7";
  store.values["ReaADRTools:event_log_v1"] = "old-event-1\nold-event-2";
  reaadr::core::EventPublishOptions options;
  options.utc_timestamp = "2026-08-30T15:00:00Z";
  options.session_id = "session-evt";
  options.source = "native test";
  options.batch_id = "batch|1";
  options.log_limit = 2;
  const auto published = events.publish("CueUpdated", {
    {"cue_count", "2"}, {"detail", "a|b"},
  }, options);
  const auto loaded = events.load();
  const std::string expected =
    "evt_00000008|2026-08-30T15%3A00%3A00Z|session-evt|CueUpdated|native test|"
    "batch%7C1|cue_count%3D2%3Bdetail%3Da%7Cb";
  check(published && published.counter == 8 && published.event.event_id == "evt_00000008" &&
          store.values.at("ReaADRTools:event_counter") == "8",
        "native event publication advances the Lua-compatible project counter");
  check(loaded && loaded.lines.size() == 2 && loaded.lines[0] == "old-event-2" &&
          loaded.lines[1] == expected,
        "native event publication matches Lua encoding and bounded-log retention");

  FakeProjectStateStore failing_store;
  failing_store.failed_write_key = "ReaADRTools:event_log_v1";
  failing_store.failed_writes_remaining = 1;
  reaadr::core::EventLogRepository failing_events(failing_store);
  const auto failed = failing_events.publish("SyncFull", {}, options);
  check(!failed && failing_store.values.at("ReaADRTools:event_counter") == "1" &&
          failing_store.values.count("ReaADRTools:event_log_v1") == 0,
        "a failed event-log append reserves its ID without publishing a partial line");
  const auto after_gap = failing_events.publish("SyncFull", {}, options);
  check(after_gap && after_gap.event.event_id == "evt_00000002",
        "native event IDs are never reused after a persistence failure");
}

void test_encoding()
{
  const std::string original = "tab\tline\nnext=雪%";
  check(reaadr::core::decode_field(reaadr::core::encode_field(original)) == original,
        "field encoding round trips control characters and UTF-8");
  check(reaadr::core::decode_field("literal%QZvalue") == "literal%QZvalue",
        "invalid percent sequences remain literal");

  const reaadr::core::Fields metadata = {{"empty", ""}, {"spaces", " \t "}, {"studio field", "café&tea"}};
  const auto decoded = reaadr::core::deserialize_metadata(reaadr::core::serialize_metadata(metadata));
  check(decoded.size() == 1 && decoded.at("studio field") == "café&tea",
        "metadata matches the Lua non-empty-value format");
}

void test_model_round_trip()
{
  reaadr::core::SessionModel model;
  model.session = {{"session_id", "session_1"}, {"session_name", "Session\nOne"}};
  model.project_metadata = {{"path", "a\tb=c\n雪"}};
  model.timecode = {{"frame_rate", "23.976"}};
  model.state = {{"active_script_id", "script-1"}, {"last_operation", "import"}};
  model.dirty_flags = {{"cues_modified", "true"}};
  model.scripts.push_back({{"script_id", "script-1"}, {"cue_count", "1"}});
  model.characters.push_back({{"character_id", "character_1"}, {"cue_count", "1"}});
  model.cues.push_back({
    {"id", "01"},
    {"character", "Miyuki 雪"},
    {"start_time", "1"},
    {"end_time", "2"},
    {"line", "tab\tline\nnext="},
    {"status", "Not Recorded"},
    {"metadata", reaadr::core::serialize_metadata({{"unicode", "café"}})},
    {"session_cue_id", "script-1:character_1:01"},
  });
  model.tracks.push_back({{"track_id", "track_1"}, {"assigned_cues", "script-1:character_1:01"}});
  model.regions.push_back({{"region_id", "region_1"}, {"start_time", "1"}, {"end_time", "2"}});
  model.imports.push_back({{"script_id", "script-1"}, {"file_hash", "file_1"}});
  model.unknown_records.push_back("future\tvalue=preserved");

  const std::string blob = reaadr::core::serialize_session_model(model);
  const auto parsed = reaadr::core::parse_session_model(blob);
  check(static_cast<bool>(parsed), "serialized model parses");
  check(parsed.model.session_id() == "session_1", "session ID survives round trip");
  check(parsed.model.session.at("session_name") == "Session\nOne", "session name survives round trip");
  check(parsed.model.project_metadata.at("path") == "a\tb=c\n雪", "project metadata survives round trip");
  check(parsed.model.cues.size() == 1 && parsed.model.cues[0].at("line") == "tab\tline\nnext=",
        "cue text survives round trip");
  check(reaadr::core::deserialize_metadata(parsed.model.cues[0].at("metadata")).at("unicode") == "café",
        "cue metadata survives nested encoding");
  check(parsed.model.unknown_records == model.unknown_records, "future record types are preserved");
  check(reaadr::core::serialize_session_model(parsed.model) == blob, "canonical model serialization is stable");
}

void test_model_errors()
{
  check(reaadr::core::parse_session_model("").error == reaadr::core::ParseError::empty_model,
        "empty model differs from invalid model");
  check(reaadr::core::parse_session_model("session\tsession_id=").error == reaadr::core::ParseError::missing_session_id,
        "empty session ID is invalid");
  check(static_cast<bool>(reaadr::core::parse_session_model("session\tsession_id=empty-session")),
        "a valid session may contain zero cues");
  const auto cue_without_status = reaadr::core::parse_session_model(
    "session\tsession_id=session-1\ncue\tid=01");
  check(cue_without_status.model.cues[0].at("status") == "Not Recorded",
        "model loading applies the canonical default cue status");
}

void test_lua_compatible_golden_blob()
{
  const std::string blob =
    "session\tsession_id=session_abc\tsession_name=A%3DB%0AC\n"
    "project_metadata\tempty=\tpath=a%09b%3Dc\n"
    "timecode\tframe_rate=24\n"
    "state\tactive_script_id=\tlast_operation=save_session\n"
    "dirty\tkey=cues_modified\tvalue=false\n"
    "cue\tcharacter=Actor\tend_time=2\tid=01\tline=Hello%09world\tstart_time=1\tstatus=Not Recorded\n"
    "future_record\tanswer=42";
  const auto parsed = reaadr::core::parse_session_model(blob);
  check(static_cast<bool>(parsed), "Lua-compatible golden model parses");
  check(parsed.model.session.at("session_name") == "A=B\nC", "Lua percent encoding is decoded");
  check(parsed.model.cues[0].at("line") == "Hello\tworld", "Lua cue fields are decoded");
  check(reaadr::core::serialize_session_model(parsed.model) == blob, "golden Lua model serializes identically");
}

void test_model_repository()
{
  FakeProjectStateStore store;
  reaadr::core::SessionModelRepository repository(store);
  check(repository.load().error == reaadr::core::SessionLoadError::missing,
        "repository distinguishes a missing session model");

  store.values["ReaADRTools:adr_session_model_v1"] = "session\tsession_id=";
  check(repository.load().error == reaadr::core::SessionLoadError::invalid_model,
        "repository reports an invalid session model");

  reaadr::core::SessionModel model;
  model.session = {{"session_id", "session_repository"}, {"session_name", "Repository Test"}};
  check(repository.save(model), "repository saves a valid model");
  check(store.values.at("ReaADRTools:adr_session_id") == "session_repository",
        "repository keeps the compatibility session ID synchronized");
  const auto loaded = repository.load();
  check(static_cast<bool>(loaded) && loaded.model.session_id() == "session_repository",
        "repository loads the saved canonical model");

  check(repository.revision().revision == 0, "missing session revision defaults to zero");
  const auto first_revision = repository.bump_revision();
  check(first_revision && first_revision.revision == 1 &&
          store.values.at("ReaADRTools:session_revision") == "1",
        "repository persists a monotonic session revision");

  const std::string saved_blob = store.values.at("ReaADRTools:adr_session_model_v1");
  const auto snapshot = repository.create_snapshot("Native mutation", "2026-08-30T12:00:00Z");
  check(snapshot && snapshot.snapshot.model_blob == saved_blob && snapshot.snapshot.revision == "1",
        "repository snapshots the model and its revision together");
  check(store.values.at("ReaADRTools:session_snapshot_last_label") == "Native mutation" &&
          store.values.at("ReaADRTools:session_snapshot_last_timestamp") == "2026-08-30T12:00:00Z",
        "repository persists snapshot audit fields");

  store.values["ReaADRTools:adr_session_model_v1"] = "session\tsession_id=mutated";
  store.values["ReaADRTools:adr_session_id"] = "mutated";
  store.values["ReaADRTools:session_revision"] = "8";
  const auto restored = repository.restore_snapshot(snapshot.snapshot);
  check(restored && restored.revision == 9,
        "snapshot restore publishes one revision newer than current state");
  check(store.values.at("ReaADRTools:adr_session_model_v1") == saved_blob &&
          store.values.at("ReaADRTools:adr_session_id") == "session_repository" &&
          store.values.at("ReaADRTools:session_revision") == "9",
        "snapshot restore synchronizes model intent, session identity, and revision");

  reaadr::core::SessionSnapshot newer_snapshot = snapshot.snapshot;
  newer_snapshot.revision = "20";
  const auto newer_restore = repository.restore_snapshot(newer_snapshot);
  check(newer_restore && newer_restore.revision == 21,
        "snapshot restore remains monotonic when the snapshot revision is newer");

  store.failed_write_key = "ReaADRTools:session_revision";
  store.failed_writes_remaining = 1;
  check(!repository.bump_revision(), "revision persistence failures are reported");
  store.failed_write_key.clear();
}

void test_reaper_project_state_adapter()
{
  project_state_exists = true;
  project_state_value.assign(70U * 1024U, 'x');
  project_state_written.clear();
  reaadr::reaper::ProjectStateStore store(nullptr, {fake_get_project_state, fake_set_project_state});
  const auto loaded = store.read("ReaADRTools", "large_value");
  check(static_cast<bool>(loaded) && loaded.value == project_state_value,
        "REAPER extstate adapter grows its buffer for large models");
  check(store.write("ReaADRTools", "key", "written"), "REAPER extstate adapter writes values");
  check(project_state_written == "written", "REAPER extstate adapter forwards the complete value");
  check(store.write("ReaADRTools", "key", ""),
        "REAPER extstate adapter accepts a zero-sized deletion result");

  project_state_exists = false;
  check(store.read("ReaADRTools", "missing").error == reaadr::core::StateReadError::not_found,
        "REAPER extstate adapter reports missing values");
}

void test_transaction_scopes()
{
  transaction_probe = {};
  {
    reaadr::reaper::ProjectTransaction outer(nullptr, fake_transaction_api(), "ReaADR: Native operation");
    check(outer.owns_undo_block(), "outer transaction owns the REAPER undo block");
    {
      reaadr::reaper::ProjectTransaction inner(nullptr, fake_transaction_api(), "ignored nested label");
      check(!inner.owns_undo_block(), "nested transaction joins the outer undo block");
      inner.mark_failed();
    }
    transaction_probe.available_undo = "ReaADR: Native operation (failed)";
  }
  check(transaction_probe.begins == 1 && transaction_probe.ends == 1,
        "nested transactions begin and end one undo block");
  check(transaction_probe.end_description == "ReaADR: Native operation (failed)",
        "a nested failure labels the outer undo block");
  check(transaction_probe.undos == 1, "a failed transaction rolls back its own undo point");

  transaction_probe = {};
  try {
    reaadr::reaper::ProjectTransaction transaction(nullptr, fake_transaction_api(), "ReaADR: Exception path");
    transaction_probe.available_undo = "ReaADR: Exception path (failed)";
    throw std::runtime_error("expected test exception");
  } catch (const std::runtime_error&) {
  }
  check(transaction_probe.undos == 1, "an exception marks and rolls back the transaction");

  transaction_probe = {};
  try {
    reaadr::reaper::UiRefreshScope refresh(fake_prevent_refresh);
    throw std::runtime_error("expected refresh test exception");
  } catch (const std::runtime_error&) {
  }
  check(transaction_probe.refresh_balance == 0, "UI refresh suppression balances on exceptions");
}

void test_domain_utilities()
{
  check(reaadr::core::normalize_status("  needs_review ") == "Needs Review",
        "status normalization accepts Lua-compatible separators");
  check(reaadr::core::normalize_status("recording") == "In Progress",
        "status normalization maps workflow aliases");
  check(reaadr::core::normalize_status(" Studio Hold ") == "Studio Hold",
        "status normalization preserves unknown studio states");

  check(reaadr::core::stable_id("character", {"script-1", "Miyuki 雪"}) == "character_f80b22b4",
        "stable character IDs match the Lua fallback algorithm");
  check(reaadr::core::stable_id("track", {"character_123", "cues", "1"}) == "track_36e4fc00",
        "stable track IDs match the Lua fallback algorithm");
  check(reaadr::core::stable_id("region", {"script-1:character_1:01"}) == "region_2aa7a354",
        "stable region IDs match the Lua fallback algorithm");

  const auto frame_time = reaadr::core::parse_timecode("01:02:03:12", 24.0);
  check(frame_time && std::abs(*frame_time.seconds - 3723.5) < 0.000001,
        "four-field timecode parses frames using the requested rate");
  const auto minute_time = reaadr::core::parse_timecode("02:03.5");
  check(minute_time && std::abs(*minute_time.seconds - 123.5) < 0.000001,
        "minute timecode parses fractional seconds");
  check(reaadr::core::parse_timecode("bad time").error == "Unsupported time format: bad time",
        "unsupported timecode retains the actionable Lua error");
  check(reaadr::core::format_timecode(3723.5, 24.0) == "01:02:03:12",
        "timecode formatting matches the Lua frame rounding behavior");
  check(reaadr::core::format_timecode(-1.0, 24.0) == "00:00:00:00",
        "timecode formatting clamps negative positions");
}

void test_cue_import()
{
  const std::string csv =
    "\xEF\xBB\xBF" "Cue Number,Actor,In Time,Out Time,Dialogue,Studio Note\r\n"
    "\r\n"
    "001,Miyuki,00:00:01:00,00:00:02:12,\"Hello, \"\"world\"\"\",Keep this\r\n"
    "002,Miyuki,3.5,4.25,Second line,\r\n";
  const auto table = reaadr::core::parse_delimited_content(csv, "cues.csv");
  check(static_cast<bool>(table), "CSV inspection succeeds");
  check(table.table.delimiter == ',' && table.table.delimiter_name == "CSV",
        "CSV delimiter is detected");
  check(table.table.rows.size() == 2 && table.table.rows[0].line_number == 3,
        "blank physical lines are skipped without losing source line numbers");

  const auto imported = reaadr::core::import_cues(table.table, 24.0);
  check(static_cast<bool>(imported) && imported.cues.size() == 2,
        "mapped CSV rows import as cues");
  check(imported.mapping.at("cue_id") == "cue_number" && imported.mapping.at("character") == "actor",
        "default mapping follows the established header aliases");
  check(imported.cues[0].at("line") == "Hello, \"world\"",
        "quoted delimiters and escaped quotes match the Lua parser");
  const auto metadata = reaadr::core::deserialize_metadata(imported.cues[0].at("metadata"));
  check(metadata.at("Studio Note") == "Keep this", "unmapped columns are retained as labelled metadata");
  check(imported.cues[0].at("source_line") == "3", "imported cues retain their physical source line");
  check(imported.cues[0].at("start_time") == "1" && imported.cues[0].at("end_time") == "2.5",
        "imported timecode is converted to seconds");

  const auto tsv = reaadr::core::parse_delimited_content(
    "ID\tRole\tStart\tEnd\n1\tActor\t0\t1\n",
    "forced.tab");
  check(tsv.table.delimiter == '\t' && tsv.table.delimiter_name == "TSV",
        "TAB and TSV extensions force tab parsing");

  const auto duplicate_table = reaadr::core::parse_delimited_content(
    "cue_id,character,start,end\nA 1,Actor,0,1\nA_1,Actor,1,2\n",
    "duplicate.csv");
  const auto duplicate = reaadr::core::import_cues(duplicate_table.table, 24.0);
  check(duplicate.error == reaadr::core::CueImportError::invalid_row &&
          duplicate.message == "Line 3 cue A_1: duplicate cue_id",
        "duplicate detection uses the sanitized cue identity and source line");

  const auto missing_table = reaadr::core::parse_delimited_content("Name,Text\nActor,Line\n", "missing.csv");
  check(reaadr::core::import_cues(missing_table.table, 24.0).error ==
          reaadr::core::CueImportError::missing_required_mapping,
        "missing required column mappings are reported before row processing");
}

void test_session_builder()
{
  std::vector<reaadr::core::Fields> cues = {
    {
      {"id", "001"}, {"character", "Miyuki"}, {"start_time", "1"}, {"end_time", "2.5"},
      {"line", "First"}, {"status", "pending"}, {"script_id", "script-1"},
      {"script_name", "Episode 1"}, {"import_timestamp", "2026-08-29T12:00:00Z"},
      {"metadata", reaadr::core::serialize_metadata({{"Studio Note", "Keep"}})},
    },
    {
      {"id", "002"}, {"character", "Miyuki"}, {"start_time", "2"}, {"end_time", "3"},
      {"line", "Second"}, {"status", "recording"}, {"script_id", "script-1"},
      {"script_name", "Episode 1"}, {"import_timestamp", "2026-08-29T12:00:00Z"},
      {"metadata", ""},
    },
  };
  reaadr::core::SessionBuildOptions options;
  options.session_id = "session-native";
  options.session_name = "Native Session";
  options.project_metadata = {{"project_name", "Anime"}};
  options.frame_rate = "24";
  options.refresh_version = "7";
  options.last_operation = "native_import";
  options.cues_modified = true;

  const auto built = reaadr::core::build_session_model(cues, options);
  check(static_cast<bool>(built), "native session builder succeeds with an explicit session ID");
  check(built.model.scripts.size() == 1 && built.model.characters.size() == 1,
        "session builder derives script and character collections");
  check(built.model.cues.size() == 2 && built.model.tracks.size() == 4 && built.model.regions.size() == 2,
        "session builder derives lane tracks and regions for every cue");
  check(built.model.imports.size() == 1 && built.model.scripts[0].at("cue_count") == "2",
        "session builder creates import identity and cue counts");
  check(built.model.cues[0].at("status") == "Not Recorded" &&
          built.model.cues[1].at("status") == "In Progress",
        "session builder canonicalizes cue statuses");
  check(built.model.cues[0].count("_reaadr_lane") == 0,
        "transient lane fields do not leak into the canonical cue model");
  check(built.model.tracks[0].at("track_name") == "Miyuki Cues" &&
          built.model.tracks[2].at("track_name") == "Miyuki Cues #2",
        "session builder derives and names overlap lanes without transient caller fields");
  check(static_cast<bool>(reaadr::core::parse_session_model(
          reaadr::core::serialize_session_model(built.model))),
        "a built session survives canonical model serialization");

  reaadr::core::SessionBuildOptions missing_id;
  check(!reaadr::core::build_session_model({}, missing_id),
        "session builder refuses to create an anonymous source-of-truth model");
}

void test_session_cue_replacement()
{
  reaadr::core::SessionBuildOptions initial_options;
  initial_options.session_id = "preserved-session";
  initial_options.session_name = "Original Name";
  initial_options.project_metadata = {{"client", "Original Client"}};
  initial_options.frame_rate = "23.976";
  initial_options.refresh_version = "12";
  const auto initial = reaadr::core::build_session_model({{
    {"id", "OLD-1"}, {"character", "Old Actor"}, {"start_time", "1"}, {"end_time", "2"},
    {"line", "Old line"}, {"script_id", "old-script"}, {"script_name", "Old Script"},
    {"metadata", ""},
  }}, initial_options);
  check(static_cast<bool>(initial), "replacement fixture builds its initial model");

  reaadr::core::SessionModel existing = initial.model;
  existing.state["active_script_id"] = "old-script";
  existing.state["future_state"] = "preserve";
  existing.dirty_flags["future_dirty"] = "preserve";
  existing.unknown_records.push_back("future_record\tvalue=preserve");
  existing.scripts[0]["metadata"] = "studio=preserve";
  existing.characters[0]["metadata"] = "voice=preserve";
  existing.imports[0]["file_hash"] = "original-import-hash";
  existing.scripts.push_back({
    {"script_id", "removed-script"}, {"script_name", "Historical"},
    {"cue_count", "4"}, {"metadata", "history=preserve"},
  });
  existing.characters.push_back({
    {"character_id", "removed-character"}, {"character_name", "Historical Actor"},
    {"cue_count", "4"}, {"metadata", "history=preserve"},
  });

  const std::vector<reaadr::core::Fields> replacement_cues = {
    {
      {"id", "NEW-1"}, {"character", "Old Actor"}, {"start_time", "10"}, {"end_time", "11"},
      {"line", "Updated line"}, {"script_id", "old-script"}, {"script_name", "Updated Script Name"},
      {"metadata", ""},
    },
    {
      {"id", "NEW-2"}, {"character", "New Actor"}, {"start_time", "12"}, {"end_time", "13"},
      {"line", "New line"}, {"script_id", "new-script"}, {"script_name", "New Script"},
      {"metadata", ""},
    },
  };
  reaadr::core::CueReplacementOptions options;
  options.build.session_id = "must-not-replace-existing-id";
  options.build.frame_rate = "60";
  options.last_operation = "native_replace";
  const auto replacement = reaadr::core::replace_session_cues(&existing, replacement_cues, options);
  check(static_cast<bool>(replacement), "native cue replacement succeeds");
  const auto& model = replacement.model;
  check(model.session_id() == "preserved-session" && model.session.at("session_name") == "Original Name",
        "cue replacement preserves session identity and name");
  check(model.project_metadata.at("client") == "Original Client" && model.timecode.at("frame_rate") == "23.976",
        "cue replacement preserves project metadata and timecode");
  check(model.state.at("active_script_id") == "old-script" && model.state.at("future_state") == "preserve" &&
          model.state.at("last_operation") == "native_replace",
        "cue replacement preserves runtime state while recording its operation");
  check(model.dirty_flags.at("future_dirty") == "preserve" &&
          model.dirty_flags.at("cues_modified") == "true",
        "cue replacement preserves future dirty flags and marks cues modified");
  check(model.unknown_records == existing.unknown_records,
        "cue replacement preserves unknown future model records");
  check(model.cues.size() == 2 && model.regions.size() == 2 && model.tracks.size() == 4,
        "cue replacement completely rebuilds cue-derived collections");
  check(model.scripts[0].at("metadata") == "studio=preserve" &&
          model.scripts[0].at("script_name") == "Updated Script Name" &&
          model.scripts[0].at("cue_count") == "1",
        "cue replacement merges derived script fields without erasing metadata");
  check(model.scripts[1].at("cue_count") == "0" && model.scripts[1].at("metadata") == "history=preserve",
        "historical scripts remain available with a zero active cue count");
  check(model.characters[1].at("cue_count") == "0" && model.characters[1].at("metadata") == "history=preserve",
        "historical characters remain available with preserved metadata");
  check(model.imports[0].at("file_hash") == "original-import-hash" && model.imports.size() == 2,
        "existing import identity wins while new script history is appended");
  check(replacement_cues[0].count("character_id") == 0,
        "cue replacement does not mutate caller-owned cue rows");

  reaadr::core::CueReplacementOptions new_session_options;
  new_session_options.build.session_id = "new-session";
  const auto new_session = reaadr::core::replace_session_cues(nullptr, replacement_cues, new_session_options);
  check(new_session && new_session.model.session_id() == "new-session",
        "cue replacement can construct the initial model when no session exists");

  const auto emptied = reaadr::core::replace_session_cues(&existing, {}, options);
  check(emptied && emptied.model.cues.empty() && emptied.model.tracks.empty() && emptied.model.regions.empty(),
        "cue replacement preserves a valid model-backed session with zero cues");
  check(emptied.model.scripts[0].at("cue_count") == "0" &&
          emptied.model.characters[0].at("cue_count") == "0",
        "empty replacement retains historical records with zero active cue counts");
}

void test_session_commit_service()
{
  FakeProjectStateStore store;
  reaadr::core::SessionModelRepository repository(store);
  reaadr::core::SessionBuildOptions initial_options;
  initial_options.session_id = "commit-session";
  const auto initial = reaadr::core::build_session_model({{
    {"id", "1"}, {"character", "Actor"}, {"start_time", "0"}, {"end_time", "1"},
    {"line", "Before"}, {"script_id", "script"}, {"metadata", ""},
  }}, initial_options);
  check(repository.save(initial.model), "commit fixture saves its initial model");
  store.values["ReaADRTools:session_revision"] = "5";

  reaadr::core::SessionCommitOptions options;
  options.replacement.build.session_id = "ignored-for-existing-session";
  options.replacement.last_operation = "native_commit";
  options.snapshot_label = "Native commit test";
  options.utc_timestamp = "2026-08-30T13:00:00Z";
  const std::vector<reaadr::core::Fields> updated_cues = {{
    {"id", "2"}, {"character", "Actor"}, {"start_time", "2"}, {"end_time", "3"},
    {"line", "After"}, {"script_id", "script"}, {"metadata", ""},
  }};

  const auto committed = reaadr::core::commit_session_cues(repository, updated_cues, options);
  check(committed && committed.revision == 6 && committed.model.cues[0].at("line") == "After",
        "model-only commit snapshots, replaces, saves, and bumps revision");
  check(store.values.at("ReaADRTools:session_snapshot_last_label") == "Native commit test",
        "model-only commit persists its safety snapshot before mutation");

  const std::string committed_blob = store.values.at("ReaADRTools:adr_session_model_v1");
  const std::string committed_id = store.values.at("ReaADRTools:adr_session_id");
  store.failed_write_key = "ReaADRTools:session_revision";
  store.failed_writes_remaining = 1;
  const auto failed = reaadr::core::commit_session_cues(repository, {{
    {"id", "3"}, {"character", "Actor"}, {"start_time", "4"}, {"end_time", "5"},
    {"line", "Must roll back"}, {"script_id", "script"}, {"metadata", ""},
  }}, options);
  check(!failed && failed.rolled_back,
        "a revision failure triggers model snapshot rollback");
  check(store.values.at("ReaADRTools:adr_session_model_v1") == committed_blob &&
          store.values.at("ReaADRTools:adr_session_id") == committed_id,
        "failed commit rollback restores the prior model and compatibility ID");
  check(store.values.at("ReaADRTools:session_revision") == "7",
        "failed commit rollback publishes a fresh revision for observers");
}

void test_render_planner()
{
  reaadr::core::SessionModel previous;
  previous.session = {{"session_id", "render-session"}};
  previous.cues = {
    {{"id", "A1"}, {"character", "Actor"}, {"start_time", "10"}, {"end_time", "12"},
     {"region_id", "region-a1"}},
    {{"id", "A2"}, {"character", "Actor"}, {"start_time", "12"}, {"end_time", "13"},
     {"region_id", "region-a2"}},
    {{"id", "OLD"}, {"character", "Actor"}, {"start_time", "20"}, {"end_time", "21"},
     {"region_id", "region-old"}},
  };
  reaadr::core::SessionModel current = previous;
  current.cues.pop_back();

  const reaadr::core::RgbColor actor_color = {175, 122, 197, true};
  reaadr::core::ProjectRenderState project;
  project.tracks = {
    {0, "cue_character", "Actor.lane1", "Cue - Actor", actor_color},
    // A legacy exact-name track without ownership metadata may be adopted and
    // tagged; an already-owned foreign track would not qualify for adoption.
    {1, "", "", "Actor", {}},
  };
  project.regions = {
    {10, "[ReaADR]:id=A1 ADR Cue A1 - Actor", 10.0, 12.0, actor_color},
    {11, "[ReaADR]:id=A2 ADR Cue A2 - Actor", 11.5, 13.0, actor_color},
    {12, "[ReaADR]:id=OLD ADR Cue OLD - Actor", 20.0, 21.0, actor_color},
    {13, "[ReaADR] personal ADR Cue", 30.0, 31.0, actor_color},
  };

  const auto planned = reaadr::core::build_render_plan(current, &previous, project);
  check(static_cast<bool>(planned), "native rendering plan succeeds for a valid canonical model");
  check(planned.plan.track_mutations.size() == 3,
        "render planning reuses exact ownership, adopts an unowned exact name, and creates overlap tracks");
  check(planned.plan.track_mutations[0].kind == reaadr::core::RenderMutationKind::update &&
          planned.plan.track_mutations[0].project_index == 1,
        "legacy exact-name track adoption is an explicit metadata update");

  int updated_regions = 0;
  int removed_regions = 0;
  bool removed_user_region = false;
  for (const auto& mutation : planned.plan.region_mutations) {
    if (mutation.kind == reaadr::core::RenderMutationKind::update) ++updated_regions;
    if (mutation.kind == reaadr::core::RenderMutationKind::remove) {
      ++removed_regions;
      if (mutation.existing_id == 13) removed_user_region = true;
    }
  }
  check(updated_regions == 1, "render planning updates only the region whose timing drifted");
  check(removed_regions == 1 && !removed_user_region,
        "stale cleanup removes only an exact region proven by the previous model");

  const auto without_ownership_proof = reaadr::core::build_render_plan(current, nullptr, project);
  check(without_ownership_proof && without_ownership_proof.plan.region_mutations.size() == 1,
        "render planning never infers stale-region ownership from a broad name substring");

  reaadr::core::SessionModel duplicate_regions = current;
  duplicate_regions.cues.push_back({
    {"id", "A1"}, {"character", "Actor"}, {"start_time", "40"}, {"end_time", "41"},
  });
  check(!reaadr::core::build_render_plan(duplicate_regions, nullptr, {}),
        "render planning rejects duplicate generated region identities before mutation");

  reaadr::core::SessionModel invalid_timing = current;
  invalid_timing.cues[0]["start_time"] = "not-a-number";
  check(!reaadr::core::build_render_plan(invalid_timing, nullptr, {}),
        "render planning rejects invalid timing before any host call");

  reaadr::core::SessionModel unrelated_previous = previous;
  unrelated_previous.session["session_id"] = "another-session";
  check(!reaadr::core::build_render_plan(current, &unrelated_previous, project),
        "stale cleanup refuses ownership evidence from another session");

  reaadr::core::SessionModel colliding_keys;
  colliding_keys.session = {{"session_id", "unicode-key-session"}};
  colliding_keys.cues = {
    {{"id", "1"}, {"character", "雪"}, {"start_time", "1"}, {"end_time", "2"}},
    {{"id", "2"}, {"character", "雨"}, {"start_time", "5"}, {"end_time", "6"}},
  };
  check(!reaadr::core::build_render_plan(colliding_keys, nullptr, {}),
        "render planning reports legacy key collisions instead of merging distinct character tracks");
}

void test_character_filter()
{
  check(reaadr::core::sanitize_token("  Lead Actor! ") == "Lead_Actor" &&
          reaadr::core::character_filter_key("  Lead Actor! ") == "lead_actor" &&
          reaadr::core::character_filter_target_key("Lead Actor", 2) == "lead_actor.lane2",
        "native character-filter keys share Lua-compatible ownership sanitization");
  check(reaadr::core::encode_character_filter_tokens({"lead_actor.lane2", "beta"}) ==
          "beta,lead_actor.lane2",
        "native character-filter encoding sorts the same persisted tokens as Lua");

  FakeProjectStateStore store;
  reaadr::core::CharacterFilterRepository repository(store);
  const auto selected = reaadr::core::parse_character_filter_state("lead_actor.lane2", true);
  check(repository.save(selected), "native character-filter state persists through the project repository");
  const auto loaded = repository.load();
  check(loaded && loaded.state.enabled() && loaded.state.hide_inactive_regions &&
          reaadr::core::character_lane_is_active(loaded.state, "Lead Actor", 2) &&
          !reaadr::core::character_lane_is_active(loaded.state, "Lead Actor", 1),
        "native character-filter repository retains lane-specific selection semantics");

  reaadr::core::SessionBuildOptions build_options;
  build_options.session_id = "filter-session";
  build_options.preroll_seconds = 3.0;
  const auto built = reaadr::core::build_session_model({
    {{"id", "A1"}, {"character", "Lead Actor"}, {"start_time", "10"}, {"end_time", "12"}},
    {{"id", "A2"}, {"character", "Lead Actor"}, {"start_time", "11"}, {"end_time", "13"}},
    {{"id", "B1"}, {"character", "Beta"}, {"start_time", "20"}, {"end_time", "21"}},
  }, build_options);
  check(static_cast<bool>(built), "character-filter fixture builds a canonical session model");

  reaadr::core::CharacterFilterProjectState project;
  project.tracks = {
    {0, "cue_character", "Lead_Actor.lane1", false},
    {1, "character", "Lead_Actor.lane2", true},
    {2, "user_audio", "Lead_Actor.lane1", false},
    {3, "cue_character", "Beta.lane1", false},
  };
  project.regions = {
    {10, "[ReaADR]:id=A1 ADR Cue A1 - Lead Actor", false},
    {11, "[ReaADR]:id=A2 ADR Cue A2 - Lead Actor", true},
    {12, "[ReaADR]:id=B1 ADR Cue B1 - Beta", false},
    {13, "User Region", false},
  };
  const auto planned = reaadr::core::build_character_filter_plan(
    built.model, loaded.state, project, 3.0);
  check(planned && planned.plan.track_mutations.size() == 3 &&
          planned.plan.region_mutations.size() == 3,
        "native filter planning updates drifted owned tracks and exact model regions only");

  render_adapter_probe = {};
  for (const auto& track : project.tracks) {
    FakeTrack fake;
    fake.strings = {{"P_EXT:ReaADR.role", track.role}, {"P_EXT:ReaADR.key", track.key}};
    fake.muted = track.muted;
    render_adapter_probe.tracks.push_back(std::move(fake));
  }
  for (const auto& region : project.regions) {
    render_adapter_probe.regions.push_back({
      region.id, region.name, 0.0, 1.0, 0, 0, region.hidden,
    });
  }
  transaction_probe = {};
  const auto applied = reaadr::reaper::apply_character_filter_plan_transactionally(
    nullptr, fake_render_api(), fake_ruler_lane_api(), fake_transaction_api(),
    planned.plan, "ReaADR: Native character filter test");
  check(applied && applied.tracks_muted == 2 && applied.tracks_unmuted == 1 &&
          applied.regions_hidden == 2 && applied.regions_shown == 1,
        "native character-filter adapter applies mute and region visibility changes transactionally");
  check(!render_adapter_probe.tracks[2].muted && !render_adapter_probe.regions[3].hidden,
        "native character filtering leaves non-owned tracks and regions unchanged");

  const auto inspected = reaadr::reaper::inspect_character_filter_project(
    nullptr, fake_render_api(), fake_ruler_lane_api());
  const auto idempotent = inspected
    ? reaadr::core::build_character_filter_plan(built.model, loaded.state, inspected.state, 3.0)
    : reaadr::core::CharacterFilterPlanResult{};
  check(inspected && idempotent && idempotent.plan.empty(),
        "an inspected native character filter produces an empty idempotent follow-up plan");

  reaadr::core::CharacterFilterPlan stale;
  stale.track_mutations.push_back({0, "cue_character", "wrong-key", false});
  transaction_probe = {};
  transaction_probe.available_undo = "ReaADR: Stale native filter (failed)";
  const auto failed = reaadr::reaper::apply_character_filter_plan_transactionally(
    nullptr, fake_render_api(), fake_ruler_lane_api(), fake_transaction_api(), stale,
    "ReaADR: Stale native filter");
  check(!failed && transaction_probe.undos == 1 && transaction_probe.refresh_balance == 0,
        "stale character-filter ownership fails inside a balanced rollback transaction");
}

void test_region_timing_sync()
{
  reaadr::core::SessionModel model;
  model.session = {{"session_id", "region-sync-session"}};
  model.cues = {
    {{"id", "A1"}, {"character", "Actor"}, {"start_time", "10"}, {"end_time", "12"},
     {"line", "Keep every non-timing field"}},
    {{"id", "B2"}, {"character", "Beta"}, {"start_time", "20"}, {"end_time", "22"}},
  };
  const std::string first_name = reaadr::core::render_region_name(model.cues[0]);
  const std::string second_name = reaadr::core::render_region_name(model.cues[1]);

  const auto synchronized = reaadr::core::sync_cue_timings_from_regions(model, {
    {10, first_name, 15.25, 17.5, {}},
    {11, "User Region", 100.0, 101.0, {}},
  });
  check(synchronized && synchronized.changed_cues == 1 && synchronized.missing_regions == 1 &&
          synchronized.cues[0].at("start_time") == "15.25" &&
          synchronized.cues[0].at("end_time") == "17.5" &&
          synchronized.cues[0].at("line") == "Keep every non-timing field" &&
          synchronized.cues[1].at("start_time") == "20",
        "region timing sync updates exact owned matches while preserving cue data and missing cues");

  const auto within_tolerance = reaadr::core::sync_cue_timings_from_regions(model, {
    {10, first_name, 10.0004, 11.9996, {}},
    {11, second_name, 20.0, 22.0, {}},
  });
  check(within_tolerance && within_tolerance.changed_cues == 0 &&
          within_tolerance.missing_regions == 0 &&
          within_tolerance.cues[0].at("start_time") == "10",
        "region timing sync preserves canonical values when drift is within tolerance");

  const auto reversed = reaadr::core::sync_cue_timings_from_regions(model, {
    {10, first_name, 15.0, 14.0, {}},
  });
  check(reversed && reversed.changed_cues == 1 &&
          reversed.cues[0].at("start_time") == "15" &&
          reversed.cues[0].at("end_time") == "15",
        "region timing sync clamps a reversed project region to a non-negative duration");

  const auto ambiguous = reaadr::core::sync_cue_timings_from_regions(model, {
    {10, first_name, 15.0, 16.0, {}},
    {12, first_name, 17.0, 18.0, {}},
  });
  check(!ambiguous && ambiguous.error.find("Multiple project regions") != std::string::npos,
        "region timing sync rejects ambiguous generated-region ownership");

  reaadr::core::RegionTimingSyncOptions invalid_options;
  invalid_options.timing_epsilon = std::nan("");
  const auto invalid = reaadr::core::sync_cue_timings_from_regions(
    model, {{10, first_name, 15.0, 16.0, {}}}, invalid_options);
  check(!invalid, "region timing sync rejects an invalid timing tolerance");
}

void test_cue_navigation()
{
  reaadr::core::SessionModel model;
  model.session = {{"session_id", "navigation-session"}};
  model.cues = {
    {{"id", "B"}, {"character", "Actor"}, {"start_time", "20"}, {"end_time", "22"}},
    {{"id", "A2"}, {"character", "Actor"}, {"start_time", "10"}, {"end_time", "12"}},
    {{"id", "C"}, {"character", "Actor"}, {"start_time", "30"}, {"end_time", "32"}},
    {{"id", "A1"}, {"character", "Actor"}, {"start_time", "10"}, {"end_time", "11"}},
  };
  const auto catalog = reaadr::core::build_cue_navigation_catalog(model);
  check(catalog && catalog.cues.size() == 4 && catalog.cues[0].cue_id == "A1" &&
          catalog.cues[1].cue_id == "A2" && catalog.cues[2].cue_id == "B" &&
          catalog.cues[3].cue_id == "C",
        "native cue navigation sorts by timeline position and cue ID like Lua");
  check(reaadr::core::find_next_cue(catalog.cues, 10.0)->cue_id == "B" &&
          reaadr::core::find_previous_cue(catalog.cues, 20.0)->cue_id == "A2" &&
          reaadr::core::find_next_cue(catalog.cues, 30.0)->cue_id == "A1" &&
          reaadr::core::find_previous_cue(catalog.cues, 10.0)->cue_id == "C",
        "native next and previous navigation retain epsilon and wraparound behavior");
  check(reaadr::core::find_cue_by_id(catalog.cues, " A2 ")->cue_id == "A2" &&
          reaadr::core::find_cue_by_id(catalog.cues, "a")->cue_id == "A1" &&
          reaadr::core::find_cue_at_position(catalog.cues, 10.5)->cue_id == "A1",
        "native cue lookup preserves exact, partial case-insensitive, and inclusive position matching");

  reaadr::core::SessionModel duplicate_model = model;
  duplicate_model.cues.push_back({
    {"id", "B!"}, {"character", "Actor"}, {"start_time", "40"}, {"end_time", "42"},
  });
  check(!reaadr::core::build_cue_navigation_catalog(duplicate_model),
        "native cue navigation rejects ambiguous persisted selection keys");
  reaadr::core::SessionModel invalid_model = model;
  invalid_model.cues[0]["start_time"] = "not-a-time";
  check(!reaadr::core::build_cue_navigation_catalog(invalid_model),
        "native cue navigation rejects invalid canonical timing");

  FakeProjectStateStore selection_store;
  selection_store.values["ReaADRTools:manager_selected_cue_key"] = "A1";
  selection_store.values["ReaADRTools:active_overlay_cue_key"] = "A1";
  reaadr::core::CueSelectionRepository selection_repository(selection_store);
  selection_store.failed_write_key = "ReaADRTools:active_overlay_cue_key";
  selection_store.failed_writes_remaining = 1;
  const auto selection_failed = selection_repository.save_selected_cue("B");
  check(!selection_failed && selection_failed.rolled_back &&
          selection_store.values.at("ReaADRTools:manager_selected_cue_key") == "A1" &&
          selection_store.values.at("ReaADRTools:active_overlay_cue_key") == "A1",
        "paired native cue selection rolls back if the overlay key cannot be persisted");

  FakeProjectStateStore service_store;
  reaadr::core::SessionModelRepository model_repository(service_store);
  check(model_repository.save(model), "cue-navigation fixture saves a canonical session model");
  reaadr::core::CueSelectionRepository service_selection(service_store);
  reaadr::reaper::CueNavigationService service(
    model_repository, service_selection, fake_navigation_api());

  navigation_play_state = 0;
  navigation_cursor_position = 10.0;
  navigation_cursor_moves = 0;
  const auto next = service.navigate_next();
  check(next && next.cue.cue_id == "B" && navigation_cursor_position == 20.0 &&
          navigation_cursor_moves == 1 && navigation_move_view && !navigation_seek_play &&
          service_store.values.at("ReaADRTools:manager_selected_cue_key") == "B" &&
          service_store.values.at("ReaADRTools:active_overlay_cue_key") == "B",
        "native next-cue navigation synchronizes selection before moving the edit cursor");

  navigation_play_state = 1;
  navigation_play_position = 20.0;
  navigation_cursor_position = 99.0;
  const auto playing_next = service.navigate_next();
  check(playing_next && playing_next.cue.cue_id == "C" && navigation_cursor_position == 30.0,
        "native navigation uses play position while REAPER is playing");
  const auto jumped = service.navigate_to_id("a2");
  check(jumped && jumped.cue.cue_id == "A2" && navigation_cursor_position == 10.0 &&
          model_repository.revision().revision == 0,
        "native ID navigation moves to canonical timing without creating a model revision");

  service_store.failed_write_key = "ReaADRTools:active_overlay_cue_key";
  service_store.failed_writes_remaining = 1;
  const double cursor_before_failure = navigation_cursor_position;
  const auto failed_jump = service.navigate_to_id("B");
  check(!failed_jump && failed_jump.selection.rolled_back &&
          navigation_cursor_position == cursor_before_failure &&
          service_store.values.at("ReaADRTools:manager_selected_cue_key") == "A2" &&
          service_store.values.at("ReaADRTools:active_overlay_cue_key") == "A2",
        "native navigation leaves the cursor and prior selection intact after persistence failure");
}

void test_record_arm_manager()
{
  const auto domain_plan = reaadr::core::build_record_arm_isolation_plan(
    {{0, 1.0}, {1, 0.0}, {2, 1.0}}, 1);
  check(domain_plan && domain_plan.mutations.size() == 3 &&
          domain_plan.mutations[0].armed == 0.0 &&
          domain_plan.mutations[1].armed == 1.0 &&
          domain_plan.mutations[2].armed == 0.0,
        "native record-arm planning isolates exactly one captured target");
  check(!reaadr::core::build_record_arm_isolation_plan({{0, 1.0}, {0, 0.0}}, 0) &&
          !reaadr::core::build_record_arm_isolation_plan({{0, 1.0}}, 2),
        "native record-arm planning rejects duplicate entries and an unknown target");

  render_adapter_probe = {};
  render_adapter_probe.tracks.resize(3);
  render_adapter_probe.tracks[0].record_armed = 1.0;
  render_adapter_probe.tracks[1].record_armed = 0.0;
  render_adapter_probe.tracks[2].record_armed = 1.0;
  MediaTrack* first = fake_get_track(nullptr, 0);
  MediaTrack* second = fake_get_track(nullptr, 1);
  MediaTrack* third = fake_get_track(nullptr, 2);
  reaadr::reaper::RecordArmManager manager(
    nullptr, fake_record_arm_api(), fake_transaction_api());

  transaction_probe = {};
  const auto isolated = manager.capture_and_isolate(second);
  check(isolated && manager.has_snapshot() &&
          render_adapter_probe.tracks[0].record_armed == 0.0 &&
          render_adapter_probe.tracks[1].record_armed == 1.0 &&
          render_adapter_probe.tracks[2].record_armed == 0.0 &&
          transaction_probe.begins == 1 && transaction_probe.ends == 1 &&
          transaction_probe.refresh_balance == 0,
        "native record-arm capture isolates a target in one balanced transaction");

  transaction_probe = {};
  const auto reisolated = manager.capture_and_isolate(third);
  check(reisolated && manager.has_snapshot() &&
          render_adapter_probe.tracks[0].record_armed == 0.0 &&
          render_adapter_probe.tracks[1].record_armed == 0.0 &&
          render_adapter_probe.tracks[2].record_armed == 1.0,
        "native record-arm isolation can change loop targets without replacing the original snapshot");

  transaction_probe = {};
  const auto restored = manager.restore();
  const auto restored_again = manager.restore();
  check(restored && restored_again && !manager.has_snapshot() &&
          render_adapter_probe.tracks[0].record_armed == 1.0 &&
          render_adapter_probe.tracks[1].record_armed == 0.0 &&
          render_adapter_probe.tracks[2].record_armed == 1.0 &&
          transaction_probe.begins == 1 && transaction_probe.ends == 1,
        "native record-arm restoration is complete and idempotent");

  render_adapter_probe = {};
  render_adapter_probe.tracks.resize(3);
  render_adapter_probe.tracks[0].record_armed = 1.0;
  render_adapter_probe.tracks[2].record_armed = 1.0;
  first = fake_get_track(nullptr, 0);
  second = fake_get_track(nullptr, 1);
  third = fake_get_track(nullptr, 2);
  reaadr::reaper::RecordArmManager failing_manager(
    nullptr, fake_record_arm_api(), fake_transaction_api());
  render_adapter_probe.fail_record_arm_track = fake_track(third);
  transaction_probe = {};
  const auto isolation_failed = failing_manager.capture_and_isolate(second);
  check(!isolation_failed && isolation_failed.restored_after_failure &&
          !failing_manager.has_snapshot() &&
          render_adapter_probe.tracks[0].record_armed == 1.0 &&
          render_adapter_probe.tracks[1].record_armed == 0.0 &&
          render_adapter_probe.tracks[2].record_armed == 1.0 &&
          transaction_probe.end_description == "ReaADR: isolate recording track (failed)" &&
          transaction_probe.refresh_balance == 0,
        "failed native isolation compensates earlier mutations and releases a new snapshot");

  render_adapter_probe.fail_record_arm_track = nullptr;
  check(static_cast<bool>(failing_manager.capture_and_isolate(second)),
        "record-arm restore-failure fixture isolates its target");
  render_adapter_probe.fail_record_arm_track = fake_track(first);
  transaction_probe = {};
  const auto restore_failed = failing_manager.restore();
  check(!restore_failed && failing_manager.has_snapshot() &&
          render_adapter_probe.tracks[1].record_armed == 0.0 &&
          render_adapter_probe.tracks[2].record_armed == 1.0,
        "native record-arm restore retains only tracks that need a retry");
  render_adapter_probe.fail_record_arm_track = nullptr;
  check(failing_manager.restore() && !failing_manager.has_snapshot() &&
          render_adapter_probe.tracks[0].record_armed == 1.0,
        "native record-arm restoration completes after a transient host failure");

  render_adapter_probe = {};
  render_adapter_probe.tracks.resize(2);
  render_adapter_probe.tracks[0].record_armed = 1.0;
  first = fake_get_track(nullptr, 0);
  second = fake_get_track(nullptr, 1);
  reaadr::reaper::RecordArmManager deleted_track_manager(
    nullptr, fake_record_arm_api(), fake_transaction_api());
  check(static_cast<bool>(deleted_track_manager.capture_and_isolate(second)),
        "deleted-track restore fixture captures record-arm state");
  render_adapter_probe.invalid_record_arm_tracks.insert(fake_track(first));
  const auto deleted_restored = deleted_track_manager.restore();
  check(deleted_restored && deleted_restored.tracks_skipped == 1 &&
          !deleted_track_manager.has_snapshot(),
        "native record-arm restoration safely skips tracks deleted during deferred recording");
}

void test_recording_setup()
{
  reaadr::core::SessionModel model;
  model.session = {{"session_id", "recording-setup-session"}};
  model.cues = {
    {{"id", "A1"}, {"character", "Actor"}, {"start_time", "10"}, {"end_time", "12"}},
    {{"id", "A2"}, {"character", "Actor"}, {"start_time", "11"}, {"end_time", "13"}},
    {{"id", "B1"}, {"character", "Beta"}, {"start_time", "20"}, {"end_time", "22"}},
  };
  const std::vector<reaadr::core::ExistingTrack> tracks = {
    {0, "user_audio", "Actor.lane2", "User", {}},
    {1, "character", "Actor.lane1", "Actor", {}},
    {2, "character", "Actor.lane2", "Actor 2", {}},
    {3, "character", "Beta.lane1", "Beta", {}},
  };
  reaadr::core::RecordingSetupOptions options;
  options.cue_key = "A2";
  options.preroll_seconds = 3.0;
  const auto planned = reaadr::core::build_recording_setup_plan(model, tracks, options);
  check(planned && planned.plan.cue_model_index == 1 && planned.plan.lane == 2 &&
          planned.plan.track_project_index == 2 && !planned.plan.used_lane_fallback &&
          planned.plan.cue_start == 11.0 && planned.plan.cue_end == 13.0 &&
          planned.plan.record_start == 8.0,
        "native recording setup resolves canonical timing, overlap lane, preroll, and exact owned track");

  const std::vector<reaadr::core::ExistingTrack> fallback_tracks = {
    {1, "character", "Actor.lane1", "Actor", {}},
    {2, "user_audio", "Actor.lane2", "User", {}},
  };
  const auto fallback =
    reaadr::core::build_recording_setup_plan(model, fallback_tracks, options);
  check(fallback && fallback.plan.track_project_index == 1 && fallback.plan.used_lane_fallback,
        "native recording setup retains the Lua lane-one compatibility fallback without adopting user tracks");

  std::vector<reaadr::core::ExistingTrack> ambiguous_tracks = tracks;
  ambiguous_tracks.push_back({4, "character", "actor.lane2", "Duplicate", {}});
  check(!reaadr::core::build_recording_setup_plan(model, ambiguous_tracks, options),
        "native recording setup rejects duplicate exact ownership matches");
  reaadr::core::RecordingSetupOptions invalid_options = options;
  invalid_options.preroll_seconds = std::nan("");
  check(!reaadr::core::build_recording_setup_plan(model, tracks, invalid_options),
        "native recording setup rejects invalid preroll");

  FakeProjectStateStore store;
  reaadr::core::SessionModelRepository repository(store);
  check(repository.save(model), "recording-setup fixture saves its canonical session");
  render_adapter_probe = {};
  render_adapter_probe.tracks.resize(3);
  render_adapter_probe.tracks[0].strings = {
    {"P_EXT:ReaADR.role", "character"}, {"P_EXT:ReaADR.key", "Actor.lane1"},
    {"P_NAME", "Actor"},
  };
  render_adapter_probe.tracks[1].strings = {
    {"P_EXT:ReaADR.role", "character"}, {"P_EXT:ReaADR.key", "Actor.lane2"},
    {"P_NAME", "Actor 2"},
  };
  render_adapter_probe.tracks[2].strings = {
    {"P_EXT:ReaADR.role", "character"}, {"P_EXT:ReaADR.key", "Beta.lane1"},
    {"P_NAME", "Beta"},
  };
  stale_recording_track_index = -1;
  stale_recording_track_reads = 0;
  reaadr::reaper::RecordingSetupService service(
    repository, nullptr, fake_recording_setup_api());
  const auto prepared = service.prepare(options);
  check(prepared && prepared.target_track == fake_get_track(nullptr, 1) &&
          prepared.plan.expected_track_key == "Actor.lane2",
        "native recording setup adapter returns a revalidated non-owning target handle");

  stale_recording_track_index = 1;
  stale_recording_track_reads = 0;
  const auto stale = service.prepare(options);
  check(!stale && !stale.target_track && stale.error.find("changed") != std::string::npos,
        "native recording setup fails closed when target ownership changes after inspection");
  stale_recording_track_index = -1;
  stale_recording_track_reads = 0;
}

void test_recording_transport()
{
  const reaadr::core::RecordingTransportContext context{7.0, 10.0, 12.0};
  reaadr::core::RecordingTransportState state;
  auto transition = reaadr::core::advance_recording_transport(
    state, context, {reaadr::core::RecordingTransportEvent::start, 0, 0.0});
  check(transition && transition.state.mode == reaadr::core::RecordingTransportMode::preroll &&
          !transition.state.operation_finalized && transition.actions.move_cursor &&
          transition.actions.cursor_position == 7.0 && transition.actions.isolate_recording_track &&
          transition.actions.play && !transition.actions.record &&
          transition.actions.refresh_active_cue,
        "native recording transport starts a first take in preroll with explicit host intents");

  transition = reaadr::core::advance_recording_transport(
    transition.state, context, {reaadr::core::RecordingTransportEvent::tick, 1, 10.0});
  check(transition && transition.state.mode == reaadr::core::RecordingTransportMode::recording &&
          transition.state.take_count == 1 && transition.actions.record,
        "native recording transport punches in at the canonical cue start");

  transition = reaadr::core::advance_recording_transport(
    transition.state, context, {reaadr::core::RecordingTransportEvent::tick, 5, 12.0});
  check(transition && transition.state.mode == reaadr::core::RecordingTransportMode::idle &&
          transition.state.operation_finalized && transition.actions.stop &&
          transition.actions.restore_record_arm && transition.actions.finalize_recorded_takes,
        "native recording transport stops and finalizes a completed non-loop take");

  reaadr::core::RecordingTransportState externally_stopped;
  externally_stopped.mode = reaadr::core::RecordingTransportMode::preroll;
  externally_stopped.operation_finalized = false;
  const auto preroll_stopped = reaadr::core::advance_recording_transport(
    externally_stopped, context, {reaadr::core::RecordingTransportEvent::tick, 0, 8.0});
  check(preroll_stopped && preroll_stopped.state.mode == reaadr::core::RecordingTransportMode::idle &&
          preroll_stopped.actions.restore_record_arm &&
          !preroll_stopped.actions.stop && !preroll_stopped.actions.finalize_recorded_takes,
        "external stop during preroll cleans up without counting or finalizing a take");

  reaadr::core::RecordingTransportState loop_state;
  loop_state.loop_enabled = true;
  auto loop = reaadr::core::advance_recording_transport(
    loop_state, context, {reaadr::core::RecordingTransportEvent::start, 0, 0.0});
  check(loop && loop.actions.configure_loop_range && loop.state.loop_range_active,
        "loop recording configures its range before the first preroll");
  loop = reaadr::core::advance_recording_transport(
    loop.state, context, {reaadr::core::RecordingTransportEvent::tick, 1, 10.0});
  loop = reaadr::core::advance_recording_transport(
    loop.state, context, {reaadr::core::RecordingTransportEvent::tick, 5, 12.0});
  check(loop && loop.state.mode == reaadr::core::RecordingTransportMode::loop_wait &&
          loop.state.take_count == 1 && loop.actions.stop &&
          !loop.actions.restore_record_arm && !loop.actions.finalize_recorded_takes,
        "completed loop take waits for transport settlement without releasing recording state");
  loop = reaadr::core::advance_recording_transport(
    loop.state, context, {reaadr::core::RecordingTransportEvent::tick, 0, 12.0});
  check(loop && loop.state.mode == reaadr::core::RecordingTransportMode::preroll &&
          loop.actions.move_cursor && loop.actions.cursor_position == 7.0 &&
          loop.actions.isolate_recording_track && loop.actions.play,
        "settled loop transport starts the next take through the shared preroll path");

  reaadr::core::RecordingTransportState toggled;
  auto toggle = reaadr::core::advance_recording_transport(
    toggled, context, {reaadr::core::RecordingTransportEvent::toggle_loop, 0, 0.0});
  check(toggle && toggle.state.loop_enabled && toggle.state.loop_range_active &&
          toggle.actions.configure_loop_range,
        "enabling loop recording configures the bounded preroll-to-cue-end range");
  toggle = reaadr::core::advance_recording_transport(
    toggle.state, context,
    {reaadr::core::RecordingTransportEvent::toggle_preroll_each_loop, 0, 0.0});
  check(toggle && !toggle.state.include_preroll_each_loop && !toggle.state.loop_range_active &&
          toggle.actions.restore_loop_range && toggle.actions.persist_preroll_preference,
        "disabling per-loop preroll restores the user's loop range and persists the choice");
  toggle = reaadr::core::advance_recording_transport(
    toggle.state, context,
    {reaadr::core::RecordingTransportEvent::toggle_preroll_each_loop, 0, 0.0});
  check(toggle && toggle.state.include_preroll_each_loop && toggle.state.loop_range_active &&
          toggle.actions.configure_loop_range && toggle.actions.persist_preroll_preference,
        "re-enabling per-loop preroll configures the loop range and persists the choice");

  reaadr::core::RecordingTransportState immediate_loop;
  immediate_loop.mode = reaadr::core::RecordingTransportMode::loop_wait;
  immediate_loop.loop_enabled = true;
  immediate_loop.include_preroll_each_loop = false;
  immediate_loop.operation_finalized = false;
  immediate_loop.take_count = 1;
  const auto immediate = reaadr::core::advance_recording_transport(
    immediate_loop, context, {reaadr::core::RecordingTransportEvent::tick, 0, 12.0});
  check(immediate && immediate.state.mode == reaadr::core::RecordingTransportMode::recording &&
          immediate.state.take_count == 2 && immediate.actions.cursor_position == 10.0 &&
          immediate.actions.record && !immediate.actions.play,
        "loop recording can start subsequent takes immediately when per-loop preroll is disabled");

  reaadr::core::RecordingTransportState abort_state;
  abort_state.mode = reaadr::core::RecordingTransportMode::recording;
  abort_state.operation_finalized = false;
  abort_state.loop_range_active = true;
  abort_state.take_count = 2;
  const auto aborted = reaadr::core::advance_recording_transport(
    abort_state, context, {reaadr::core::RecordingTransportEvent::abort, 5, 11.0});
  check(aborted && aborted.state.mode == reaadr::core::RecordingTransportMode::idle &&
          aborted.actions.stop && aborted.actions.restore_loop_range &&
          aborted.actions.restore_record_arm && aborted.actions.finalize_recorded_takes,
        "recording abort converges transport, loop, arm, and take cleanup into one transition");

  const auto invalid = reaadr::core::advance_recording_transport(
    {}, {11.0, 10.0, 12.0}, {reaadr::core::RecordingTransportEvent::start, 0, 0.0});
  check(!invalid, "native recording transport rejects an invalid preroll window");
}

void test_recording_transport_executor()
{
  render_adapter_probe = {};
  render_adapter_probe.tracks.resize(2);
  render_adapter_probe.tracks[0].record_armed = 1.0;
  render_adapter_probe.tracks[1].record_armed = 0.0;
  transaction_probe = {};
  recording_transport_probe = {};
  recording_transport_probe.loop_start = 20.0;
  recording_transport_probe.loop_end = 25.0;

  const reaadr::core::RecordingTransportContext context{7.0, 10.0, 12.0};
  reaadr::core::RecordingTransportState state;
  state.loop_enabled = true;
  auto transition = reaadr::core::advance_recording_transport(
    state, context, {reaadr::core::RecordingTransportEvent::start, 0, 0.0});
  reaadr::reaper::RecordArmManager arm_manager(
    nullptr, fake_record_arm_api(), fake_transaction_api());
  reaadr::reaper::RecordingTransportExecutor executor(
    arm_manager, fake_recording_transport_api());
  MediaTrack* target = fake_get_track(nullptr, 1);
  auto applied = executor.apply(transition, context, target);
  check(applied && applied.state_accepted && executor.has_active_loop_range() &&
          recording_transport_probe.loop_start == 7.0 &&
          recording_transport_probe.loop_end == 12.0 &&
          recording_transport_probe.cursor_position == 7.0 &&
          recording_transport_probe.cursor_move_view &&
          !recording_transport_probe.cursor_seek_play &&
          recording_transport_probe.commands == std::vector<int>{1007} &&
          render_adapter_probe.tracks[0].record_armed == 0.0 &&
          render_adapter_probe.tracks[1].record_armed == 1.0 &&
          applied.pending.refresh_active_cue,
        "recording executor configures the loop, isolates the track, and starts preroll in order");

  transition = reaadr::core::advance_recording_transport(
    transition.state, context, {reaadr::core::RecordingTransportEvent::tick, 1, 10.0});
  applied = executor.apply(transition, context, target);
  check(applied && applied.state_accepted &&
          recording_transport_probe.commands == std::vector<int>({1007, 1013}),
        "recording executor punches in through the REAPER record command");

  transition = reaadr::core::advance_recording_transport(
    transition.state, context, {reaadr::core::RecordingTransportEvent::tick, 5, 12.0});
  applied = executor.apply(transition, context, target);
  check(applied && applied.state_accepted &&
          recording_transport_probe.commands == std::vector<int>({1007, 1013, 1016}) &&
          executor.has_active_loop_range() && arm_manager.has_snapshot(),
        "recording executor stops a loop pass without prematurely restoring operation state");

  transition = reaadr::core::advance_recording_transport(
    transition.state, context,
    {reaadr::core::RecordingTransportEvent::stop_requested, 0, 12.0});
  recording_transport_probe.fail_set_loop = true;
  applied = executor.apply(transition, context, target);
  check(!applied && !applied.state_accepted && executor.has_active_loop_range() &&
          arm_manager.has_snapshot() &&
          render_adapter_probe.tracks[1].record_armed == 1.0,
        "recording executor retains cleanup state when the user's loop range cannot be restored");
  recording_transport_probe.fail_set_loop = false;
  applied = executor.apply(transition, context, target);
  check(applied && applied.state_accepted && !executor.has_active_loop_range() &&
          recording_transport_probe.loop_start == 20.0 &&
          recording_transport_probe.loop_end == 25.0 &&
          render_adapter_probe.tracks[0].record_armed == 1.0 &&
          render_adapter_probe.tracks[1].record_armed == 0.0 &&
          !arm_manager.has_snapshot() && applied.pending.finalize_recorded_takes,
        "recording executor restores the user's loop and arm state before deferring take finalization");

  const auto preference = reaadr::core::advance_recording_transport(
    {}, context,
    {reaadr::core::RecordingTransportEvent::toggle_preroll_each_loop, 0, 0.0});
  applied = executor.apply(preference, context, target);
  check(applied && applied.state_accepted && applied.pending.persist_preroll_preference,
        "recording executor leaves preference persistence to the application coordinator");

  render_adapter_probe.tracks[0].record_armed = 1.0;
  render_adapter_probe.tracks[1].record_armed = 0.0;
  recording_transport_probe = {};
  recording_transport_probe.loop_start = 30.0;
  recording_transport_probe.loop_end = 35.0;
  recording_transport_probe.fail_cursor = true;
  reaadr::reaper::RecordArmManager failure_arm_manager(
    nullptr, fake_record_arm_api(), fake_transaction_api());
  reaadr::reaper::RecordingTransportExecutor failure_executor(
    failure_arm_manager, fake_recording_transport_api());
  const auto failed_start = failure_executor.apply(
    reaadr::core::advance_recording_transport(
      state, context, {reaadr::core::RecordingTransportEvent::start, 0, 0.0}),
    context, target);
  check(!failed_start && !failed_start.state_accepted &&
          !failure_executor.has_active_loop_range() &&
          recording_transport_probe.loop_start == 30.0 &&
          recording_transport_probe.loop_end == 35.0 &&
          !failure_arm_manager.has_snapshot(),
        "recording executor restores a configured loop range when cursor setup fails");

  recording_transport_probe.fail_cursor = false;
  recording_transport_probe.fail_command = 1007;
  const auto failed_play = failure_executor.apply(
    reaadr::core::advance_recording_transport(
      state, context, {reaadr::core::RecordingTransportEvent::start, 0, 0.0}),
    context, target);
  check(!failed_play && !failed_play.state_accepted &&
          !failure_executor.has_active_loop_range() &&
          recording_transport_probe.loop_start == 30.0 &&
          recording_transport_probe.loop_end == 35.0 &&
          render_adapter_probe.tracks[0].record_armed == 1.0 &&
          render_adapter_probe.tracks[1].record_armed == 0.0 &&
          !failure_arm_manager.has_snapshot(),
        "recording executor compensates loop and arm state when preroll cannot start");
}

void test_recording_application_service()
{
  reaadr::core::SessionBuildOptions build_options;
  build_options.session_id = "recording-application-session";
  const auto built = reaadr::core::build_session_model({{
    {"id", "A1"}, {"character", "Actor"}, {"start_time", "10"},
    {"end_time", "12"}, {"line", "Line"}, {"status", "Not Recorded"},
    {"script_id", "script-1"}, {"metadata", ""},
  }}, build_options);
  check(static_cast<bool>(built),
        "recording application fixture builds a canonical session");

  const auto updated = reaadr::core::update_cue_status(built.model, {
    "A1", " recorded ", "record_cue",
  });
  check(updated && updated.changed && updated.normalized_status == "Recorded" &&
          updated.cue.at("status") == "Recorded" &&
          updated.model.regions == built.model.regions &&
          updated.model.tracks == built.model.tracks &&
          updated.model.state.at("last_operation") == "record_cue",
        "cue status mutation updates only canonical workflow state and preserves derived records");

  reaadr::core::SessionModel duplicate = built.model;
  duplicate.cues.push_back(duplicate.cues.front());
  check(!reaadr::core::update_cue_status(duplicate, {"A1", "Recorded", "record_cue"}),
        "cue status mutation fails closed when a cue key is ambiguous");

  FakeProjectStateStore commit_store;
  reaadr::core::SessionModelRepository commit_repository(commit_store);
  check(commit_repository.save(built.model), "cue status commit fixture saves its session");
  commit_store.values["ReaADRTools:session_revision"] = "4";
  reaadr::core::CueStatusCommitOptions commit_options;
  commit_options.update = {"A1", "Recorded", "record_cue"};
  commit_options.snapshot_label = "Finalize Recording Takes";
  commit_options.utc_timestamp = "2026-08-31T15:00:00Z";
  auto committed = reaadr::core::commit_cue_status(commit_repository, commit_options);
  check(committed && committed.update.changed && committed.revision == 5 &&
          commit_repository.load().model.cues[0].at("status") == "Recorded",
        "cue status commit snapshots, persists, and revises canonical recording state");
  committed = reaadr::core::commit_cue_status(commit_repository, commit_options);
  check(committed && !committed.update.changed && committed.revision == 5,
        "repeating an applied cue status commit is revision-free");

  commit_options.update.status = "Approved";
  commit_store.failed_write_key = "ReaADRTools:session_revision";
  commit_store.failed_writes_remaining = 1;
  const auto failed_commit =
    reaadr::core::commit_cue_status(commit_repository, commit_options);
  check(!failed_commit && failed_commit.rolled_back &&
          commit_repository.load().model.cues[0].at("status") == "Recorded" &&
          commit_repository.revision().revision == 6,
        "cue status revision failure restores the prior canonical model with a fresh revision");

  FakeProjectStateStore preference_store;
  reaadr::core::RecordingPreferenceRepository preference_repository(preference_store);
  check(preference_repository.load().include_preroll_each_loop,
        "missing native recording preference uses the Lua-compatible true default");
  auto preference = preference_repository.save_include_preroll_each_loop(false);
  check(preference && preference.changed &&
          preference_store.values.at(
            "ReaADRTools:overlay.include_preroll_each_loop") == "0" &&
          !preference_repository.load().include_preroll_each_loop,
        "native recording preference writes the transitional Lua project key");
  preference = preference_repository.save_include_preroll_each_loop(false);
  check(preference && !preference.changed,
        "unchanged recording preference persistence is a no-op");

  FakeProjectStateStore app_store;
  reaadr::core::SessionModelRepository app_repository(app_store);
  check(app_repository.save(built.model), "recording coordinator fixture saves its session");
  app_store.values["ReaADRTools:session_revision"] = "2";
  reaadr::core::CueSelectionRepository selection_repository(app_store);
  reaadr::core::RecordingPreferenceRepository app_preferences(app_store);
  reaadr::core::EventLogRepository events(app_store);
  recording_overlay_refresh_succeeds = true;
  recording_overlay_refreshes = 0;
  transaction_probe = {};
  reaadr::reaper::RecordingApplicationService service(
    app_repository, selection_repository, app_preferences, events, nullptr,
    fake_transaction_api(), {fake_refresh_recording_overlay});
  reaadr::reaper::RecordingApplicationOptions options;
  options.cue_key = "A1";
  options.include_preroll_each_loop = false;
  options.status_commit.snapshot_label = "Finalize Native Recording";
  options.status_commit.utc_timestamp = "2026-08-31T15:05:00Z";
  options.event.utc_timestamp = "2026-08-31T15:05:00Z";

  reaadr::reaper::PendingRecordingApplicationActions pending;
  pending.refresh_active_cue = true;
  auto applied = service.apply(pending, options);
  check(applied && !applied.remaining.refresh_active_cue &&
          app_store.values.at("ReaADRTools:manager_selected_cue_key") == "A1" &&
          app_store.values.at("ReaADRTools:active_overlay_cue_key") == "A1" &&
          applied.overlay_refreshes == 1,
        "recording coordinator synchronizes canonical selection before refreshing the overlay");

  pending = {};
  pending.persist_preroll_preference = true;
  applied = service.apply(pending, options);
  check(applied && !applied.remaining.persist_preroll_preference &&
          applied.preference.changed &&
          app_store.values.at("ReaADRTools:overlay.include_preroll_each_loop") == "0",
        "recording coordinator consumes preference work through the compatibility repository");

  pending = {};
  pending.finalize_recorded_takes = true;
  applied = service.apply(pending, options);
  check(applied && !applied.remaining.finalize_recorded_takes &&
          applied.status.update.changed && app_repository.revision().revision == 3 &&
          app_repository.load().model.cues[0].at("status") == "Recorded" &&
          applied.event && events.load().lines.size() == 1,
        "recording coordinator commits Recorded status, refreshes overlay, and publishes CueUpdated");
  applied = service.apply(pending, options);
  check(applied && !applied.status.update.changed &&
          app_repository.revision().revision == 3 && events.load().lines.size() == 1,
        "recording finalization retry refreshes the overlay without duplicate revision or event");

  app_store.values["ReaADRTools:manager_selected_cue_key"] = "previous-manager";
  app_store.values["ReaADRTools:active_overlay_cue_key"] = "previous-overlay";
  recording_overlay_refresh_succeeds = false;
  pending = {};
  pending.refresh_active_cue = true;
  applied = service.apply(pending, options);
  check(!applied && applied.remaining.refresh_active_cue &&
          applied.selection_rolled_back &&
          app_store.values.at("ReaADRTools:manager_selected_cue_key") == "previous-manager" &&
          app_store.values.at("ReaADRTools:active_overlay_cue_key") == "previous-overlay",
        "failed recording overlay refresh restores the exact prior paired selection");

  check(app_repository.save(built.model),
        "recording coordinator rollback fixture restores Not Recorded status");
  const std::size_t events_before_failure = events.load().lines.size();
  pending = {};
  pending.finalize_recorded_takes = true;
  applied = service.apply(pending, options);
  check(!applied && applied.remaining.finalize_recorded_takes &&
          applied.model_rolled_back &&
          app_repository.load().model.cues[0].at("status") == "Not Recorded" &&
          events.load().lines.size() == events_before_failure,
        "failed post-recording overlay refresh restores canonical status and retains retry work");

  recording_overlay_refresh_succeeds = true;
  applied = service.apply(applied.remaining, options);
  check(applied && !applied.remaining.finalize_recorded_takes &&
          app_repository.load().model.cues[0].at("status") == "Recorded" &&
          events.load().lines.size() == events_before_failure + 1,
        "retained recording finalization succeeds once overlay refresh recovers");

  check(app_repository.save(built.model),
        "recording event-warning fixture restores Not Recorded status");
  const std::size_t events_before_warning = events.load().lines.size();
  app_store.failed_write_key = "ReaADRTools:event_log_v1";
  app_store.failed_writes_remaining = 1;
  applied = service.apply(pending, options);
  check(applied && !applied.remaining.finalize_recorded_takes &&
          !applied.event_warning.empty() &&
          app_repository.load().model.cues[0].at("status") == "Recorded" &&
          events.load().lines.size() == events_before_warning,
        "recording event publication warning does not retry an applied model/view commit");
}

void test_overlay_refresh_adapter()
{
  const std::string old_code =
    std::string(reaadr::core::kOverlayCodeMarker) + "\nold_overlay();";
  const std::string new_code =
    std::string(reaadr::core::kOverlayCodeMarker) + "\nnew_overlay();";
  const reaadr::core::OverlayRefreshOptions enabled{true, new_code};

  check(!reaadr::core::build_overlay_refresh_plan({}, enabled),
        "overlay planner requires the exact owned source-video track");
  std::vector<reaadr::core::ExistingOverlayTrack> domain_tracks = {
    {0, "source_video", "source_video", {
      {0, "User Video Processor", "user_code();", true},
    }},
  };
  auto planned = reaadr::core::build_overlay_refresh_plan(domain_tracks, enabled);
  check(planned && planned.plan.mutation == reaadr::core::OverlayMutationKind::create,
        "overlay planner ignores unowned user effects when creating its generated effect");

  domain_tracks[0].effects.push_back({1, "", old_code, false});
  planned = reaadr::core::build_overlay_refresh_plan(domain_tracks, enabled);
  check(planned && planned.plan.mutation == reaadr::core::OverlayMutationKind::update &&
          planned.plan.existing.fx_index == 1,
        "overlay planner recognizes the Lua-compatible generated-code ownership marker");
  domain_tracks[0].effects[1] = {
    1, reaadr::core::kOverlayFxName, new_code, true,
  };
  planned = reaadr::core::build_overlay_refresh_plan(domain_tracks, enabled);
  check(planned && planned.plan.mutation == reaadr::core::OverlayMutationKind::none,
        "overlay planner treats an exact enabled overlay as a no-op");

  auto duplicates = domain_tracks;
  duplicates.push_back({1, "source_video", "source_video", {}});
  check(!reaadr::core::build_overlay_refresh_plan(duplicates, enabled),
        "overlay planner fails closed for duplicate owned source-video tracks");
  domain_tracks[0].effects.push_back({2, reaadr::core::kOverlayFxName, old_code, true});
  check(!reaadr::core::build_overlay_refresh_plan(domain_tracks, enabled),
        "overlay planner refuses ambiguous generated overlay ownership");

  render_adapter_probe = {};
  render_adapter_probe.tracks.resize(1);
  FakeTrack& source_track = render_adapter_probe.tracks[0];
  source_track.strings = {
    {"P_EXT:ReaADR.role", "source_video"},
    {"P_EXT:ReaADR.key", "source_video"},
  };
  source_track.effects.push_back({"User Video Processor", "user_code();", true});
  transaction_probe = {};
  auto applied = reaadr::reaper::refresh_generated_overlay_transactionally(
    nullptr, fake_overlay_refresh_api(), fake_transaction_api(), enabled,
    "ReaADR: refresh video overlay");
  check(applied && applied.effects_created == 1 && source_track.effects.size() == 2 &&
          source_track.effects[0].renamed_name == "User Video Processor" &&
          source_track.effects[1].renamed_name == reaadr::core::kOverlayFxName &&
          source_track.effects[1].video_code == new_code &&
          source_track.effects[1].enabled &&
          render_adapter_probe.last_overlay_instantiate == -1 &&
          transaction_probe.refresh_balance == 0,
        "overlay adapter creates a distinct owned Video processor without adopting user FX");

  const int adjustments = render_adapter_probe.window_adjustments;
  applied = reaadr::reaper::refresh_generated_overlay_transactionally(
    nullptr, fake_overlay_refresh_api(), fake_transaction_api(), enabled,
    "ReaADR: refresh video overlay");
  check(applied && applied.effects_created == 0 && applied.effects_updated == 0 &&
          render_adapter_probe.window_adjustments == adjustments,
        "unchanged overlay refresh avoids FX and window mutations");

  const reaadr::core::OverlayRefreshOptions update_options{true, old_code};
  applied = reaadr::reaper::refresh_generated_overlay_transactionally(
    nullptr, fake_overlay_refresh_api(), fake_transaction_api(), update_options,
    "ReaADR: refresh video overlay");
  check(applied && applied.effects_updated == 1 && source_track.effects.size() == 2 &&
          source_track.effects[1].video_code == old_code,
        "overlay adapter updates its existing effect in place and preserves chain order");

  const auto inspected = reaadr::reaper::inspect_overlay_project(
    nullptr, fake_overlay_refresh_api());
  planned = reaadr::core::build_overlay_refresh_plan(inspected.tracks, enabled);
  source_track.strings["P_EXT:ReaADR.role"] = "user_video";
  applied = reaadr::reaper::apply_overlay_refresh_plan_transactionally(
    nullptr, fake_overlay_refresh_api(), fake_transaction_api(), planned.plan,
    "ReaADR: refresh video overlay");
  check(!applied && source_track.effects[1].video_code == old_code,
        "overlay adapter rejects stale source-track ownership before FX mutation");
  source_track.strings["P_EXT:ReaADR.role"] = "source_video";

  source_track.effects.resize(1);
  render_adapter_probe.fail_overlay_set_parameter = "VIDEO_CODE";
  render_adapter_probe.fail_overlay_set_remaining = 1;
  applied = reaadr::reaper::refresh_generated_overlay_transactionally(
    nullptr, fake_overlay_refresh_api(), fake_transaction_api(), enabled,
    "ReaADR: refresh video overlay");
  check(!applied && applied.restored_after_failure && source_track.effects.size() == 1 &&
          source_track.effects[0].renamed_name == "User Video Processor",
        "failed overlay creation removes only the incomplete generated effect");

  render_adapter_probe.fail_overlay_set_parameter.clear();
  applied = reaadr::reaper::refresh_generated_overlay_transactionally(
    nullptr, fake_overlay_refresh_api(), fake_transaction_api(), update_options,
    "ReaADR: refresh video overlay");
  check(applied && source_track.effects.size() == 2 &&
          source_track.effects[1].video_code == old_code,
        "overlay failure fixture recreates a valid owned effect");
  render_adapter_probe.fail_overlay_set_parameter = "VIDEO_CODE";
  render_adapter_probe.fail_overlay_set_remaining = 1;
  applied = reaadr::reaper::refresh_generated_overlay_transactionally(
    nullptr, fake_overlay_refresh_api(), fake_transaction_api(), enabled,
    "ReaADR: refresh video overlay");
  check(!applied && applied.restored_after_failure && source_track.effects.size() == 2 &&
          source_track.effects[1].video_code == old_code,
        "failed overlay update restores the prior generated configuration");

  render_adapter_probe.fail_overlay_set_parameter.clear();
  source_track.effects.push_back({reaadr::core::kOverlayFxName, new_code, true});
  const std::size_t ambiguous_count = source_track.effects.size();
  applied = reaadr::reaper::refresh_generated_overlay_transactionally(
    nullptr, fake_overlay_refresh_api(), fake_transaction_api(), enabled,
    "ReaADR: refresh video overlay");
  check(!applied && source_track.effects.size() == ambiguous_count,
        "overlay adapter leaves ambiguous generated effects untouched");

  source_track.effects.pop_back();
  const reaadr::core::OverlayRefreshOptions disabled{false, {}};
  applied = reaadr::reaper::refresh_generated_overlay_transactionally(
    nullptr, fake_overlay_refresh_api(), fake_transaction_api(), disabled,
    "ReaADR: disable video overlay");
  check(applied && applied.effects_removed == 1 && source_track.effects.size() == 1 &&
          source_track.effects[0].renamed_name == "User Video Processor",
        "disabling overlays removes only the exactly owned generated effect");
}

void test_track_region_adapter()
{
  constexpr int custom_color_flag = 0x1000000;
  render_adapter_probe = {};
  render_adapter_probe.tracks.push_back({{
    {"P_NAME", "Old Name"},
    {"P_EXT:ReaADR.role", "character"},
    {"P_EXT:ReaADR.key", "Actor.lane1"},
  }, fake_color_to_native(1, 2, 3) | custom_color_flag, {}, false, 0.0, {}});
  render_adapter_probe.regions.push_back({7, "Existing", 1.0, 2.0,
    fake_color_to_native(1, 2, 3) | custom_color_flag});

  reaadr::reaper::TrackRegionAdapter adapter(nullptr, fake_render_api());
  const auto inspection = adapter.inspect();
  check(inspection && inspection.state.tracks.size() == 1 && inspection.state.regions.size() == 1,
        "REAPER adapter inspects owned tracks and regions through injected host calls");
  check(inspection.state.tracks[0].color == reaadr::core::RgbColor{1, 2, 3, true},
        "REAPER adapter converts platform-native colors back to domain RGB");

  reaadr::core::RenderPlan plan;
  plan.track_mutations.push_back({
    reaadr::core::RenderMutationKind::update, 0,
    {"character", "Actor.lane1", "Actor", {175, 122, 197, true}},
  });
  plan.track_mutations.push_back({
    reaadr::core::RenderMutationKind::create, 0,
    {"cue_character", "Actor.lane1", "Cue - Actor", {175, 122, 197, true}},
  });
  plan.region_mutations.push_back({
    reaadr::core::RenderMutationKind::update, 7,
    {"region-existing", "Existing", 1.5, 2.5, {175, 122, 197, true}},
  });
  plan.region_mutations.push_back({
    reaadr::core::RenderMutationKind::create, -1,
    {"region-new", "New", 3.0, 4.0, {72, 201, 176, true}},
  });

  transaction_probe = {};
  const auto applied = reaadr::reaper::apply_render_plan_transactionally(
    nullptr, fake_render_api(), fake_transaction_api(), plan, "ReaADR: Render native tracks and regions");
  check(applied && applied.tracks_created == 1 && applied.tracks_updated == 1 &&
          applied.regions_created == 1 && applied.regions_updated == 1,
        "transactional adapter applies the complete planned track/region mutation set");
  check(transaction_probe.begins == 1 && transaction_probe.ends == 1 && transaction_probe.refresh_balance == 0,
        "successful rendering is one undo point with balanced UI refresh suppression");
  check(render_adapter_probe.tracks[0].strings.at("P_NAME") == "Actor" &&
          render_adapter_probe.tracks[1].strings.at("P_EXT:ReaADR.role") == "cue_character",
        "track application writes names and explicit ownership metadata");
  check(render_adapter_probe.window_adjustments == 1 && render_adapter_probe.arrange_updates == 1,
        "successful rendering refreshes REAPER's track and arrange views once");

  render_adapter_probe.fail_region_update = true;
  transaction_probe = {};
  transaction_probe.available_undo = "ReaADR: Failed render (failed)";
  reaadr::core::RenderPlan failing_plan;
  failing_plan.region_mutations.push_back({
    reaadr::core::RenderMutationKind::update, 7,
    {"region-existing", "Existing", 8.0, 9.0, {175, 122, 197, true}},
  });
  const auto failed = reaadr::reaper::apply_render_plan_transactionally(
    nullptr, fake_render_api(), fake_transaction_api(), failing_plan, "ReaADR: Failed render");
  check(!failed && transaction_probe.undos == 1,
        "a host mutation failure marks and rolls back only the matching render transaction");
  check(transaction_probe.refresh_balance == 0,
        "failed rendering still balances UI refresh suppression");

  reaadr::core::RenderPlan forbidden_track_delete;
  forbidden_track_delete.track_mutations.push_back({
    reaadr::core::RenderMutationKind::remove, 0,
    {"character", "Actor.lane1", "Actor", {175, 122, 197, true}},
  });
  check(!adapter.apply(forbidden_track_delete),
        "REAPER adapter refuses caller-supplied track deletion plans to protect recordings");
}

void test_extended_render_planner()
{
  reaadr::core::SessionModel previous;
  previous.session = {{"session_id", "artifact-session"}};
  previous.cues = {
    {{"id", "A1"}, {"character", "Actor"}, {"start_time", "10"}, {"end_time", "12"}},
    {{"id", "A2"}, {"character", "Actor"}, {"start_time", "12"}, {"end_time", "13"}},
    {{"id", "OLD"}, {"character", "Actor"}, {"start_time", "20"}, {"end_time", "21"}},
  };
  reaadr::core::SessionModel current = previous;
  current.cues.pop_back();

  const reaadr::core::RgbColor actor_color = {175, 122, 197, true};
  reaadr::core::ProjectRenderState project;
  project.ruler_lanes = {
    {0, "Actor", actor_color, false},
    {1, "Wrong label", {}, true},
  };
  project.region_lanes = {
    {"[ReaADR]:id=A1 ADR Cue A1 - Actor", 0},
    {"[ReaADR]:id=A2 ADR Cue A2 - Actor", 0},
  };
  project.cue_audio_items = {
    {0, 0, "cue_character", "Actor.lane1", "cue_audio", "A1", "/cue.wav",
     "[ReaADR]:id=A1 Cue Audio A1 - Actor", 7.0, 3.0, false, true},
    {0, 1, "cue_character", "Actor.lane1", "cue_audio", "A2", "/cue.wav",
     "[ReaADR]:id=A2 Cue Audio A2 - Actor", 9.0, 3.0, false, true},
    {0, 2, "cue_character", "Actor.lane1", "cue_audio", "A1", "/cue.wav",
     "duplicate", 7.0, 3.0, false, true},
    {0, 3, "cue_character", "Actor.lane1", "cue_audio", "OLD", "/cue.wav",
     "old", 17.0, 3.0, false, true},
    {0, 4, "cue_character", "Actor.lane1", "user_audio", "OLD", "/user.wav",
     "user", 17.0, 3.0, false, true},
  };

  reaadr::core::RenderPlanOptions options;
  options.cue_audio_path = "/cue.wav";
  options.cue_audio_duration = 3.0;
  const auto planned = reaadr::core::build_render_plan(current, &previous, project, options);
  check(planned && planned.plan.minimum_ruler_lane_count == 0 &&
          planned.plan.ruler_lane_mutations.size() == 1,
        "extended render planning updates only ruler lanes that drifted");
  check(planned.plan.region_lane_mutations.size() == 1 &&
          planned.plan.region_lane_mutations[0].lane_index == 1,
        "extended render planning assigns overlapping cue regions to their derived ruler lane");

  int audio_updates = 0;
  int audio_removals = 0;
  bool replaces_unchanged_source = false;
  bool touches_user_item = false;
  for (const auto& mutation : planned.plan.cue_audio_mutations) {
    if (mutation.kind == reaadr::core::RenderMutationKind::update) {
      ++audio_updates;
      replaces_unchanged_source = mutation.desired.replace_source;
    } else if (mutation.kind == reaadr::core::RenderMutationKind::remove) {
      ++audio_removals;
      if (mutation.existing_item_index == 4) touches_user_item = true;
    }
  }
  check(audio_updates == 1 && !replaces_unchanged_source,
        "cue-audio planning moves a lane-changed item without needlessly replacing its source");
  check(audio_removals == 2 && !touches_user_item,
        "cue-audio cleanup removes owned duplicates and prior-model stale items but ignores user roles");

  reaadr::core::RenderPlanOptions invalid_audio;
  invalid_audio.cue_audio_path = "/cue.wav";
  check(!reaadr::core::build_render_plan(current, &previous, project, invalid_audio),
        "cue-audio planning requires a measured positive source duration");
}

void test_complete_render_adapter()
{
  render_adapter_probe = {};
  render_adapter_probe.source_lengths["/cue.wav"] = 3.0;

  reaadr::core::RenderPlan plan;
  plan.track_mutations.push_back({
    reaadr::core::RenderMutationKind::create, 0,
    {"cue_character", "Actor.lane1", "Cue - Actor", {175, 122, 197, true}},
  });
  plan.region_mutations.push_back({
    reaadr::core::RenderMutationKind::create, -1,
    {"region-a1", "[ReaADR]:id=A1 ADR Cue A1 - Actor", 10.0, 12.0, {175, 122, 197, true}},
  });
  plan.minimum_ruler_lane_count = 1;
  plan.ruler_lane_mutations.push_back({0, "Actor", {175, 122, 197, true}, false});
  plan.region_lane_mutations.push_back({"[ReaADR]:id=A1 ADR Cue A1 - Actor", 0});
  plan.cue_audio_mutations.push_back({
    reaadr::core::RenderMutationKind::create, 0, 0,
    {"A1", "Actor.lane1", "/cue.wav", "[ReaADR]:id=A1 Cue Audio A1 - Actor", 7.0, 3.0, true},
  });

  reaadr::reaper::CueAudioAdapter cue_adapter(nullptr, fake_cue_audio_api());
  const auto source = cue_adapter.inspect_source("/cue.wav");
  check(source && std::abs(source.duration - 3.0) < 0.000001,
        "cue-audio adapter measures the REAPER source before domain planning");

  transaction_probe = {};
  const auto applied = reaadr::reaper::apply_complete_render_plan_transactionally(
    nullptr, fake_render_api(), fake_ruler_lane_api(), fake_cue_audio_api(), fake_transaction_api(),
    plan, "ReaADR: Render complete native artifacts");
  check(applied && applied.tracks_and_regions.tracks_created == 1 &&
          applied.tracks_and_regions.regions_created == 1 &&
          applied.ruler_lanes.lanes_updated == 1 && applied.ruler_lanes.regions_assigned == 1 &&
          applied.cue_audio.items_created == 1,
        "complete rendering creates dependencies before ruler assignments and cue-audio items");
  check(transaction_probe.begins == 1 && transaction_probe.ends == 1 &&
          transaction_probe.refresh_balance == 0,
        "complete native artifact rendering uses one balanced outer transaction");
  check(render_adapter_probe.window_adjustments == 1 && render_adapter_probe.arrange_updates == 1,
        "complete native artifact rendering performs one final project view refresh");
  check(render_adapter_probe.tracks.size() == 1 && render_adapter_probe.tracks[0].items.size() == 1 &&
          render_adapter_probe.regions.size() == 1 && render_adapter_probe.ruler_lanes.size() == 1,
        "complete rendering materializes every planned project artifact");

  const auto inspection = reaadr::reaper::inspect_complete_render_state(
    nullptr, fake_render_api(), fake_ruler_lane_api(), fake_cue_audio_api());
  check(inspection && inspection.state.ruler_lanes[0].name == "Actor" &&
          inspection.state.region_lanes[0].lane_index == 0,
        "complete inspection combines track, region, and ruler-lane state for replanning");
  check(inspection && inspection.state.cue_audio_items.size() == 1 &&
          inspection.state.cue_audio_items[0].cue_key == "A1" &&
          inspection.state.cue_audio_items[0].has_take,
        "complete inspection includes explicit cue-audio ownership and take state");

  reaadr::core::SessionModel rendered_model;
  rendered_model.session = {{"session_id", "complete-render-session"}};
  rendered_model.cues = {{
    {"id", "A1"}, {"character", "Actor"}, {"start_time", "10"}, {"end_time", "12"},
    {"region_id", "region-a1"},
  }};
  reaadr::core::RenderPlanOptions rendered_options;
  rendered_options.create_dialogue_tracks = false;
  rendered_options.cue_audio_path = "/cue.wav";
  rendered_options.cue_audio_duration = 3.0;
  const auto idempotent = reaadr::core::build_render_plan(
    rendered_model, &rendered_model, inspection.state, rendered_options);
  check(idempotent && idempotent.plan.empty(),
        "a complete inspected render produces an empty idempotent follow-up plan");

  reaadr::core::RenderPlan stale_source_plan;
  stale_source_plan.cue_audio_mutations.push_back({
    reaadr::core::RenderMutationKind::create, 0, 0,
    {"A2", "Actor.lane1", "/cue.wav", "A2", 9.0, 3.0, true},
  });
  render_adapter_probe.source_lengths["/cue.wav"] = 4.0;
  transaction_probe = {};
  transaction_probe.available_undo = "ReaADR: Stale cue source (failed)";
  const auto failed = reaadr::reaper::apply_complete_render_plan_transactionally(
    nullptr, fake_render_api(), fake_ruler_lane_api(), fake_cue_audio_api(), fake_transaction_api(),
    stale_source_plan, "ReaADR: Stale cue source");
  check(!failed && transaction_probe.undos == 1 && transaction_probe.refresh_balance == 0,
        "cue-source changes fail and roll back the complete native render transaction");

  for (FakeTrack& track : render_adapter_probe.tracks) {
    for (const auto& item : track.items) destroy_fake_source(item->take.source);
  }
  check(render_adapter_probe.live_sources.empty(),
        "cue-audio adapter test releases every transferred fake media source");
}

void test_session_render_service()
{
  const std::string cue_path = "/tmp/reaadr-session-render-service-cue.wav";
  FakeProjectStateStore store;
  reaadr::core::SessionModelRepository repository(store);
  reaadr::core::EventLogRepository events(store);
  reaadr::core::CharacterFilterRepository character_filter(store);
  check(character_filter.save(reaadr::core::parse_character_filter_state("__none__", true)),
        "session render fixture saves an active native character filter");
  render_adapter_probe = {};
  render_adapter_probe.source_lengths[cue_path] = 3.0;

  reaadr::reaper::SessionRenderOptions options;
  options.commit.replacement.build.session_id = "native-render-session";
  options.commit.replacement.build.frame_rate = "30";
  options.commit.replacement.build.preroll_seconds = 2.5;
  options.commit.replacement.last_operation = "native_commit_and_render";
  options.commit.snapshot_label = "Native session render test";
  options.commit.utc_timestamp = "2026-08-30T14:00:00Z";
  options.render.create_dialogue_tracks = false;
  // This intentionally differs from the commit setting. The service must use
  // the canonical build preroll for both lane assignment and visible render.
  options.render.preroll_seconds = 99.0;
  options.cue_audio_path = cue_path;

  const std::vector<reaadr::core::Fields> cues = {{
    {"id", "A1"}, {"character", "Actor"}, {"start_time", "10"}, {"end_time", "12"},
    {"line", "First line"}, {"script_id", "script"}, {"metadata", ""},
  }};
  transaction_probe = {};
  reaadr::reaper::SessionRenderService service(
    repository, events, character_filter, nullptr, fake_render_api(), fake_ruler_lane_api(),
    fake_cue_audio_api(),
    fake_transaction_api());
  const auto rendered = service.commit_and_render(cues, options);
  const auto loaded = repository.load();
  check(rendered && loaded && loaded.model.cues.size() == 1 &&
          loaded.model.cues[0].at("line") == "First line" && rendered.commit.revision == 1,
        "session render service commits the canonical model before reporting success");
  check(std::abs(rendered.cue_wav.frame_rate - 30.0) < 0.000001 &&
          transaction_probe.begins == 1 && transaction_probe.ends == 1 && transaction_probe.undos == 0,
        "session render service derives cue audio from model timecode and uses one outer undo block");
  check(render_adapter_probe.tracks.size() == 1 && render_adapter_probe.regions.size() == 1 &&
          render_adapter_probe.tracks[0].items.size() == 1 &&
          render_adapter_probe.tracks[0].muted && render_adapter_probe.regions[0].hidden,
        "session render service synchronizes artifacts and reapplies the persisted character filter");
  check(rendered.events.size() == 2 && rendered.event_warning.empty() &&
          events.load().lines.size() == 2 &&
          events.load().lines.back().find("|SyncFull|") != std::string::npos,
        "successful native session rendering publishes SessionSaved and SyncFull events");

  // Event history is diagnostic. A failed append must remain visible to the
  // caller without misreporting an already committed session as failed.
  store.failed_write_key = "ReaADRTools:event_log_v1";
  store.failed_writes_remaining = 1;
  transaction_probe = {};
  const auto rendered_with_warning = service.commit_and_render(cues, options);
  check(rendered_with_warning && !rendered_with_warning.event_warning.empty() &&
          rendered_with_warning.events.size() == 2 && !rendered_with_warning.events[0] &&
          rendered_with_warning.events[1],
        "event persistence warnings do not turn a successful native render into a retryable failure");

  check(character_filter.save(reaadr::core::parse_character_filter_state("actor", false)),
        "session render fixture changes its persisted character filter");
  render_adapter_probe.fail_region_hidden = true;
  options.commit.replacement.build.frame_rate = "60";
  transaction_probe = {};
  transaction_probe.available_undo = options.undo_description + " (failed)";
  const auto failed = service.commit_and_render({{
    {"id", "A1"}, {"character", "Actor"}, {"start_time", "20"}, {"end_time", "22"},
    {"line", "Must roll back"}, {"script_id", "script"}, {"metadata", ""},
  }}, options);
  const auto restored = repository.load();
  check(!failed && failed.model_rolled_back &&
          std::abs(failed.cue_wav.frame_rate - 30.0) < 0.000001 && transaction_probe.undos == 1 &&
          transaction_probe.begins == 1 && transaction_probe.ends == 1,
        "a character-filter failure preserves timecode and rolls back project and model");
  check(restored && restored.model.cues[0].at("line") == "First line" &&
          restored.model.cues[0].at("start_time") == "10" &&
          repository.revision().revision == 4,
        "render rollback republishes the prior canonical model under a fresh revision");
  check(events.load().lines.size() == 3 &&
          store.values.at("ReaADRTools:event_counter") == "4",
        "failed rendering does not publish success events into project history");

  for (FakeTrack& track : render_adapter_probe.tracks) {
    for (const auto& item : track.items) destroy_fake_source(item->take.source);
  }
  check(render_adapter_probe.live_sources.empty(),
        "session render service test releases transferred fake media sources");

  // A failure during the first-ever render must delete the temporary model,
  // not leave an empty value that later loads as a corrupt session.
  FakeProjectStateStore new_store;
  reaadr::core::SessionModelRepository new_repository(new_store);
  reaadr::core::EventLogRepository new_events(new_store);
  reaadr::core::CharacterFilterRepository new_character_filter(new_store);
  render_adapter_probe = {};
  render_adapter_probe.source_lengths[cue_path] = 3.0;
  transaction_probe = {};
  transaction_probe.available_undo = options.undo_description + " (failed)";
  options.render.timing_epsilon = std::nan("");
  options.commit.replacement.build.frame_rate = "30";
  reaadr::reaper::SessionRenderService new_service(
    new_repository, new_events, new_character_filter, nullptr, fake_render_api(),
    fake_ruler_lane_api(), fake_cue_audio_api(),
    fake_transaction_api());
  const auto first_render_failed = new_service.commit_and_render(cues, options);
  check(!first_render_failed && first_render_failed.model_rolled_back &&
          new_repository.load().error == reaadr::core::SessionLoadError::missing,
        "first-session render rollback restores a genuinely missing canonical model");

  std::remove(cue_path.c_str());
  std::remove((cue_path + ".reaadr.tmp").c_str());
}

void test_region_timing_render_service()
{
  const std::string cue_path = "/tmp/reaadr-region-timing-render-service-cue.wav";
  FakeProjectStateStore store;
  reaadr::core::SessionModelRepository repository(store);
  reaadr::core::EventLogRepository events(store);
  reaadr::core::CharacterFilterRepository character_filter(store);
  render_adapter_probe = {};
  render_adapter_probe.source_lengths[cue_path] = 3.0;

  reaadr::reaper::SessionRenderOptions render_options;
  render_options.commit.replacement.build.session_id = "region-render-session";
  render_options.commit.replacement.build.preroll_seconds = 3.0;
  render_options.commit.replacement.last_operation = "initial_render";
  render_options.commit.utc_timestamp = "2026-08-30T15:00:00Z";
  render_options.event.utc_timestamp = render_options.commit.utc_timestamp;
  render_options.event.source = "region_timing_test";
  render_options.render.create_dialogue_tracks = false;
  render_options.cue_audio_path = cue_path;

  reaadr::reaper::SessionRenderService service(
    repository, events, character_filter, nullptr, fake_render_api(), fake_ruler_lane_api(),
    fake_cue_audio_api(), fake_transaction_api());
  const std::vector<reaadr::core::Fields> cues = {{
    {"id", "A1"}, {"character", "Actor"}, {"start_time", "10"}, {"end_time", "12"},
    {"line", "Moved line"}, {"script_id", "script"}, {"metadata", ""},
  }};
  transaction_probe = {};
  const auto initial = service.commit_and_render(cues, render_options);
  check(initial && render_adapter_probe.regions.size() == 1,
        "region timing render fixture creates a synchronized native session");

  render_adapter_probe.regions[0].start_time = 14.0;
  render_adapter_probe.regions[0].end_time = 16.0;
  reaadr::reaper::RegionTimingRenderOptions sync_options;
  sync_options.session = render_options;
  transaction_probe = {};
  const auto synchronized = service.sync_region_timings_and_render(sync_options);
  const auto loaded = repository.load();
  check(synchronized && synchronized.timing.changed_cues == 1 && loaded &&
          loaded.model.cues[0].at("start_time") == "14" &&
          loaded.model.cues[0].at("end_time") == "16" &&
          loaded.model.regions[0].at("start_time") == "14" &&
          loaded.model.dirty_flags.at("regions_modified") == "true" &&
          loaded.model.state.at("last_operation") == "update_cues_from_regions",
        "region timing render service promotes visible timing into every canonical derived record");
  check(transaction_probe.begins == 1 && transaction_probe.ends == 1 &&
          transaction_probe.undos == 0 &&
          std::abs(render_adapter_probe.regions[0].start_time - 14.0) < 0.000001 &&
          std::abs(render_adapter_probe.tracks[0].items[0]->values.at("D_POSITION") - 11.0) < 0.000001,
        "region timing render service rebuilds dependent cue audio in one project transaction");
  const auto event_log = events.load();
  check(event_log && event_log.lines.size() == 4 &&
          event_log.lines[2].find("|CueTimingUpdated|") != std::string::npos &&
          event_log.lines[3].find("|SyncFull|") != std::string::npos,
        "region timing render service publishes the Lua-compatible timing and sync events");

  const std::uint64_t revision = repository.revision().revision;
  transaction_probe = {};
  const auto unchanged = service.sync_region_timings_and_render(sync_options);
  check(unchanged && unchanged.timing.changed_cues == 0 &&
          repository.revision().revision == revision && transaction_probe.begins == 0,
        "unchanged region timing is a true no-op without a model revision or Undo point");

  render_adapter_probe.regions[0].start_time = 18.0;
  render_adapter_probe.regions[0].end_time = 20.0;
  render_adapter_probe.fail_item_value = true;
  transaction_probe = {};
  transaction_probe.available_undo = "ReaADR: update cues from regions (failed)";
  const auto failed = service.sync_region_timings_and_render(sync_options);
  const auto restored = repository.load();
  check(!failed && failed.render.model_rolled_back && transaction_probe.undos == 1 &&
          restored && restored.model.cues[0].at("start_time") == "14" &&
          events.load().lines.size() == 4,
        "failed region timing rendering rolls back the model and publishes no success events");
  render_adapter_probe.fail_item_value = false;

  for (FakeTrack& track : render_adapter_probe.tracks) {
    for (const auto& item : track.items) destroy_fake_source(item->take.source);
  }
  check(render_adapter_probe.live_sources.empty(),
        "region timing render service test releases transferred fake media sources");
  std::remove(cue_path.c_str());
  std::remove((cue_path + ".reaadr.tmp").c_str());
}

} // namespace

int main()
{
  test_encoding();
  test_cue_wav();
  test_event_log_repository();
  test_model_round_trip();
  test_model_errors();
  test_lua_compatible_golden_blob();
  test_model_repository();
  test_reaper_project_state_adapter();
  test_transaction_scopes();
  test_domain_utilities();
  test_cue_import();
  test_session_builder();
  test_session_cue_replacement();
  test_session_commit_service();
  test_render_planner();
  test_character_filter();
  test_region_timing_sync();
  test_cue_navigation();
  test_record_arm_manager();
  test_recording_setup();
  test_recording_transport();
  test_recording_transport_executor();
  test_recording_application_service();
  test_overlay_refresh_adapter();
  test_track_region_adapter();
  test_extended_render_planner();
  test_complete_render_adapter();
  test_session_render_service();
  test_region_timing_render_service();
  if (failures != 0) {
    std::cerr << failures << " native core test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "ok - native core tests\n";
  return EXIT_SUCCESS;
}
