#include "app/cue_import_application_service.hpp"
#include "app/cue_import_request.hpp"
#include "app/cue_import_workflow.hpp"
#include "app/script_identity.hpp"
#include "reaadr_core/event_log.hpp"
#include "reaadr_core/model_repository.hpp"
#include "reaadr_core/session_builder.hpp"
#include "reaadr_reaper/character_filter_adapter.hpp"
#include "reaadr_reaper/session_render_service.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message)
{
  if (!condition) {
    ++failures;
    std::cerr << "not ok - " << message << '\n';
  }
}

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
    if (value.empty()) values.erase(full_key);
    else values[full_key] = value;
    return true;
  }

  std::map<std::string, std::string> values;
};

struct ImportFixture {
  FakeProjectStateStore store;
  reaadr::core::SessionModelRepository repository{store};
  reaadr::core::EventLogRepository events{store};
  reaadr::core::CharacterFilterRepository character_filter{store};
  reaadr::reaper::SessionRenderService renderer{
    repository, events, character_filter, nullptr, {}, {}, {}, {}};

  reaadr::reaper::CueImportApplicationService service(double frame_rate = 24.0)
  {
    return reaadr::reaper::CueImportApplicationService(renderer, frame_rate, &repository);
  }
};

const std::string csv =
  "Cue Number,Actor,In Time,Out Time,Dialogue\n"
  "A1,Actor,00:00:01:00,00:00:02:00,Hello\n"
  "B1,Beta,3,4,Goodbye\n";

void seed_session(ImportFixture& fixture, const std::string& source_path)
{
  const auto parsed = reaadr::core::parse_delimited_content(csv, source_path);
  const auto imported = reaadr::core::import_cues(parsed.table, 24.0);
  auto cues = imported.cues;
  const auto script = reaadr::reaper::derive_native_script_identity(source_path, cues);
  reaadr::reaper::annotate_imported_cues(cues, script);

  reaadr::core::SessionBuildOptions options;
  options.session_id = "import-application-test";
  const auto built = reaadr::core::build_session_model(cues, options);
  check(static_cast<bool>(built), "import application fixture builds canonical session");
  check(fixture.repository.save(built.model), "import application fixture saves canonical session");
}

void test_import_request_parsing()
{
  check(reaadr::reaper::normalize_cue_import_mode(" Import Entire Sheet ") == "all",
        "import request normalizes full-import aliases");
  check(reaadr::reaper::normalize_cue_import_mode("characters") == "selected",
        "import request normalizes selected-character aliases");
  check(reaadr::reaper::normalize_cue_import_mode("merge") == "update",
        "import request normalizes historical update aliases");

  const auto characters =
    reaadr::reaper::parse_cue_import_characters(" Actor ; ;Beta;  Gamma ");
  check(characters.size() == 3 && characters[0] == "Actor" &&
          characters[1] == "Beta" && characters[2] == "Gamma",
        "import request trims and discards blank character selections");

  const auto mapping = reaadr::reaper::parse_cue_import_mapping(
    " cue_id = Cue Number ; character = Actor ; start = In Time ; end = Out Time ");
  check(mapping && mapping.mapping && mapping.mapping->at("cue_id") == "Cue Number" &&
          mapping.mapping->at("character") == "Actor",
        "import request parses trimmed explicit column mappings");

  const auto malformed =
    reaadr::reaper::parse_cue_import_mapping("cue_id=Cue Number;broken");
  check(!malformed &&
          malformed.error == "Mappings must use key=column pairs separated by semicolons.",
        "import request rejects malformed explicit mappings consistently");

  const auto tolerant =
    reaadr::reaper::parse_cue_import_mapping("cue_id=Cue Number;broken", true);
  check(tolerant && tolerant.mapping && tolerant.mapping->size() == 1,
        "import request can recover valid entries from persisted legacy mappings");
}

void test_import_preview_summary()
{
  ImportFixture fixture;
  auto service = fixture.service();
  const auto preview = service.preview_content(import_csv(), "episode.csv", std::nullopt);
  check(preview, "preview summary fixture imports");
  const std::string summary = reaadr::reaper::format_cue_import_preview_summary(
    preview, std::nullopt, {"selected", {"Actor"}});
  check(summary.find("Detected comma") != std::string::npos,
        "preview summary reports detected delimiter");
  check(summary.find("2 data row(s)") != std::string::npos,
        "preview summary reports row count");
  check(summary.find("1 cue(s) ready to import (mode: selected characters)") != std::string::npos,
        "preview summary applies selected-character filtering");
  check(summary.find("Resolved mapping:") != std::string::npos,
        "preview summary exposes resolved mapping");
}

void test_preview_and_repository_guards()
{
  ImportFixture fixture;
  auto service = fixture.service();
  const auto preview = service.preview_content(csv, "episode.csv", std::nullopt);
  check(preview && preview.imported.cues.size() == 2,
        "native import preview parses and maps cue rows without mutating the session");
  check(fixture.repository.load().error == reaadr::core::SessionLoadError::missing,
        "native import preview leaves a missing canonical session untouched");

  reaadr::reaper::CueImportApplicationService no_repository(fixture.renderer, 24.0, nullptr);
  reaadr::reaper::SessionRenderOptions options;
  const auto full = no_repository.import_content(csv, "episode.csv", std::nullopt, options, "all");
  check(!full && full.error == "Native full import requires a canonical session repository.",
        "full import refuses to bypass the canonical session repository");
  const auto selected = no_repository.import_content(
    csv, "episode.csv", std::nullopt, options, "selected", {"Actor"});
  check(!selected && selected.error == "Native selected import requires a canonical session repository.",
        "selected import refuses to bypass the canonical session repository");
  const auto update = no_repository.import_content(
    csv, "episode.csv", std::nullopt, options, "update", {"Actor"});
  check(!update && update.error == "Native update import requires a canonical session repository.",
        "update import refuses to bypass the canonical session repository");
}

void test_existing_script_mode_guards()
{
  ImportFixture fixture;
  seed_session(fixture, "episode.csv");
  auto service = fixture.service();
  reaadr::reaper::SessionRenderOptions options;

  const auto duplicate_script = service.import_content(
    csv, "episode.csv", std::nullopt, options, "all");
  check(!duplicate_script && duplicate_script.error.find("already present") != std::string::npos,
        "full import rejects a script already represented in the canonical session");

  const auto duplicate_character = service.import_content(
    csv, "episode.csv", std::nullopt, options, "selected", {"Actor"});
  check(!duplicate_character &&
          duplicate_character.error ==
            "Character Actor is already imported from this script. Use Update Existing Import instead.",
        "selected import cannot silently replace an already imported script character");

  const auto missing_character = service.import_content(
    csv, "episode.csv", std::nullopt, options, "update", {"Missing"});
  check(!missing_character &&
          missing_character.error == "No cues remain after applying the native update selection.",
        "update rejects a selection that is absent from the revised source before rendering");

  const auto unsupported = service.import_content(
    csv, "episode.csv", std::nullopt, options, "surprise-mode");
  check(!unsupported && unsupported.error == "Unsupported native import mode: surprise-mode",
        "native import rejects unknown modes before any render mutation");
}

} // namespace

int main()
{
  test_import_request_parsing();
  test_import_preview_summary();
  test_preview_and_repository_guards();
  test_existing_script_mode_guards();
  if (failures != 0) {
    std::cerr << failures << " cue import application test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "ok - cue import application tests\n";
  return EXIT_SUCCESS;
}
