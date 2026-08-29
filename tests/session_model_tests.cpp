#include "reaadr_core/session_model.hpp"
#include "reaadr_core/domain_utils.hpp"
#include "reaadr_core/cue_import.hpp"
#include "reaadr_core/model_repository.hpp"
#include "reaadr_core/session_builder.hpp"
#include "reaadr_reaper/project_state.hpp"
#include "reaadr_reaper/project_transaction.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

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
    values[std::string(name_space) + ":" + key] = value;
    return writes_succeed;
  }

  std::map<std::string, std::string> values;
  bool writes_succeed = true;
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
      {"_reaadr_lane", "1"},
    },
    {
      {"id", "002"}, {"character", "Miyuki"}, {"start_time", "2"}, {"end_time", "3"},
      {"line", "Second"}, {"status", "recording"}, {"script_id", "script-1"},
      {"script_name", "Episode 1"}, {"import_timestamp", "2026-08-29T12:00:00Z"},
      {"metadata", ""}, {"_reaadr_lane", "2"},
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
        "session builder names base and overlap lane tracks consistently");
  check(static_cast<bool>(reaadr::core::parse_session_model(
          reaadr::core::serialize_session_model(built.model))),
        "a built session survives canonical model serialization");

  reaadr::core::SessionBuildOptions missing_id;
  check(!reaadr::core::build_session_model({}, missing_id),
        "session builder refuses to create an anonymous source-of-truth model");
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
  if (failures != 0) {
    std::cerr << failures << " native core test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "ok - native core tests\n";
  return EXIT_SUCCESS;
}
