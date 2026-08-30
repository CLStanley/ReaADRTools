#include "reaadr_core/session_model.hpp"
#include "reaadr_core/domain_utils.hpp"
#include "reaadr_core/cue_import.hpp"
#include "reaadr_core/model_repository.hpp"
#include "reaadr_core/render_plan.hpp"
#include "reaadr_core/session_builder.hpp"
#include "reaadr_core/session_commit.hpp"
#include "reaadr_core/session_mutation.hpp"
#include "reaadr_reaper/project_state.hpp"
#include "reaadr_reaper/project_transaction.hpp"
#include "reaadr_reaper/track_region_adapter.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <map>
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
    values[full_key] = value;
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

struct FakeTrack {
  std::map<std::string, std::string> strings;
  int color = 0;
};

struct FakeRegion {
  int id = -1;
  std::string name;
  double start_time = 0.0;
  double end_time = 0.0;
  int color = 0;
};

struct RenderAdapterProbe {
  std::vector<FakeTrack> tracks;
  std::vector<FakeRegion> regions;
  bool fail_region_update = false;
  int next_region_id = 100;
  int window_adjustments = 0;
  int arrange_updates = 0;
};

RenderAdapterProbe render_adapter_probe;

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
  return track && std::string(parameter ? parameter : "") == "I_CUSTOMCOLOR" ? fake_track(track)->color : 0.0;
}

bool fake_set_track_value(MediaTrack* track, const char* parameter, double value)
{
  if (!track || std::string(parameter ? parameter : "") != "I_CUSTOMCOLOR") return false;
  fake_track(track)->color = static_cast<int>(value);
  return true;
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

void check(bool condition, const std::string& message)
{
  if (!condition) {
    ++failures;
    std::cerr << "not ok - " << message << '\n';
  }
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

void test_track_region_adapter()
{
  constexpr int custom_color_flag = 0x1000000;
  render_adapter_probe = {};
  render_adapter_probe.tracks.push_back({{
    {"P_NAME", "Old Name"},
    {"P_EXT:ReaADR.role", "character"},
    {"P_EXT:ReaADR.key", "Actor.lane1"},
  }, fake_color_to_native(1, 2, 3) | custom_color_flag});
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

} // namespace

int main()
{
  test_encoding();
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
  test_track_region_adapter();
  if (failures != 0) {
    std::cerr << failures << " native core test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "ok - native core tests\n";
  return EXIT_SUCCESS;
}
