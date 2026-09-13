#include "app/quick_action_application_service.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

namespace {

void require(bool condition, const char* message)
{
  if (!condition) {
    std::cerr << "quick_action_application_service_tests: " << message << '\n';
    std::exit(1);
  }
}

class FakeProjectStateStore final : public reaadr::core::ProjectStateStore {
public:
  reaadr::core::StateReadResult read(const char* name_space, const char* key) const override
  {
    const std::string composite = std::string(name_space ? name_space : "") + "\n" +
      std::string(key ? key : "");
    const auto found = values.find(composite);
    if (found == values.end()) return {{}, reaadr::core::StateReadError::not_found};
    return {found->second, reaadr::core::StateReadError::none};
  }

  bool write(const char* name_space, const char* key, const std::string& value) override
  {
    const std::string composite = std::string(name_space ? name_space : "") + "\n" +
      std::string(key ? key : "");
    if (value.empty()) values.erase(composite);
    else values[composite] = value;
    return true;
  }

  std::map<std::string, std::string> values;
};

class FakeGlobalStateStore final : public reaadr::core::GlobalStateStore {
public:
  std::string read(const char* name_space, const char* key) const override
  {
    const std::string composite = std::string(name_space ? name_space : "") + "\n" +
      std::string(key ? key : "");
    const auto found = values.find(composite);
    return found == values.end() ? std::string() : found->second;
  }

  bool write(const char* name_space, const char* key, const std::string& value) override
  {
    const std::string composite = std::string(name_space ? name_space : "") + "\n" +
      std::string(key ? key : "");
    values[composite] = value;
    return true;
  }

  void set(const std::string& key, const std::string& value)
  {
    values[std::string(reaadr::core::SessionModelRepository::kNamespace) + "\n" + key] = value;
  }

  std::map<std::string, std::string> values;
};

} // namespace

int main()
{
  using namespace reaadr;

  FakeProjectStateStore project;
  FakeGlobalStateStore global;
  core::ManagerPreferencesRepository preferences(project, &global);

  std::string dispatched_action;
  reaper::QuickActionApplicationService service(
    preferences,
    [&dispatched_action](const std::string& action, std::string*) {
      dispatched_action = action;
      return true;
    });

  global.set("quick_action_1", "record_cue");
  auto result = service.run(1);
  require(static_cast<bool>(result), "configured quick action should run");
  require(result.dispatched, "successful quick action should report dispatch");
  require(!result.used_default, "known persisted key should not use fallback");
  require(result.quick_action.key == "record_cue", "resolved key should be returned");
  require(dispatched_action == "record_cue", "semantic record action should be dispatched");

  global.set("quick_action_1", "invalid_saved_value");
  dispatched_action.clear();
  result = service.run(1);
  require(static_cast<bool>(result), "invalid persisted key should use Lua-compatible fallback");
  require(result.used_default, "fallback should be reported to the caller");
  require(result.quick_action.key == "import", "slot one should fall back to import");
  require(dispatched_action == "import", "fallback semantic action should be dispatched");

  result = service.run(5);
  require(!result, "out-of-range quick action slot should fail");
  require(!result.dispatched, "invalid slot should not dispatch anything");

  reaper::QuickActionApplicationService failing(
    preferences,
    [](const std::string&, std::string* error) {
      if (error) *error = "dispatch failed on purpose";
      return false;
    });
  global.set("quick_action_2", "cue_manager");
  result = failing.run(2);
  require(!result, "dispatcher failure should fail the application request");
  require(result.error == "dispatch failed on purpose",
          "dispatcher error should propagate without being rewritten");

  reaper::QuickActionApplicationService missing_dispatcher(preferences, {});
  result = missing_dispatcher.run(2);
  require(!result, "missing dispatcher should fail cleanly");
  require(result.error.find("dispatcher") != std::string::npos,
          "missing dispatcher error should identify the unavailable dispatcher");

  std::cout << "quick_action_application_service_tests: ok\n";
  return 0;
}
