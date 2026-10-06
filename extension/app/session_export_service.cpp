#include "session_export_service.hpp"

#include <fstream>
#include <sstream>

namespace reaadr::reaper {
namespace {

std::string field(const core::Fields& values, const char* key)
{
  const auto found = values.find(key);
  return found == values.end() ? std::string() : found->second;
}

std::string first_field(
  const core::Fields& values, const char* primary, const char* fallback)
{
  const std::string value = field(values, primary);
  return value.empty() ? field(values, fallback) : value;
}

std::string csv(std::string value)
{
  std::string escaped = "\"";
  for (char ch : value) {
    if (ch == '"') escaped += "\"";
    escaped += ch;
  }
  escaped += '"';
  return escaped;
}

double number(const std::string& value)
{
  try {
    return value.empty() ? 0.0 : std::stod(value);
  } catch (...) {
    return 0.0;
  }
}

} // namespace

SessionExportResult format_session_export(
  const core::SessionModel& model,
  SessionExportKind kind)
{
  SessionExportResult result;
  std::ostringstream out;

  if (kind == SessionExportKind::cue_sheet) {
    out << "id,character,start_tc,end_tc,dialogue,notes,status,type\n";
    for (const auto& cue : model.cues) {
      out << csv(field(cue, "id")) << ','
          << csv(field(cue, "character")) << ','
          << csv(field(cue, "start_tc")) << ','
          << csv(field(cue, "end_tc")) << ','
          << csv(first_field(cue, "dialogue", "line")) << ','
          << csv(field(cue, "notes")) << ','
          << csv(field(cue, "status")) << ','
          << csv(first_field(cue, "type", "cue_type")) << '\n';
    }
  } else if (kind == SessionExportKind::timing_report) {
    out << "id,character,start_tc,end_tc,start_seconds,end_seconds,duration_seconds,status\n";
    for (const auto& cue : model.cues) {
      const std::string start = first_field(cue, "start_seconds", "start_time");
      const std::string end = first_field(cue, "end_seconds", "end_time");
      out << csv(field(cue, "id")) << ','
          << csv(field(cue, "character")) << ','
          << csv(field(cue, "start_tc")) << ','
          << csv(field(cue, "end_tc")) << ','
          << csv(start) << ','
          << csv(end) << ','
          << (number(end) - number(start)) << ','
          << csv(field(cue, "status")) << '\n';
    }
  } else {
    out << "key,value\n";
    for (const auto& entry : model.session)
      out << csv(entry.first) << ',' << csv(entry.second) << '\n';
    out << csv("cue_count") << ',' << model.cues.size() << '\n';
  }

  result.content = out.str();
  return result;
}

bool write_session_export(
  const std::string& path,
  const std::string& content,
  std::string* error)
{
  if (error) error->clear();
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    if (error) *error = "Could not create the selected CSV file.";
    return false;
  }
  file << content;
  if (!file) {
    if (error) *error = "The CSV export could not be completed.";
    return false;
  }
  return true;
}

} // namespace reaadr::reaper
