#include "xlsx_import.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace reaadr::reaper {
namespace {

std::string read_text_file(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) return {};
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

std::string native_separator()
{
#ifdef _WIN32
  return "\\";
#else
  return "/";
#endif
}

std::string join_native_path(const std::string& base, const std::string& relative)
{
  if (base.empty()) return relative;
  const char last = base[base.size() - 1];
  if (last == '/' || last == '\\') return base + relative;
  return base + native_separator() + relative;
}

std::string shell_quote_posix(const std::string& value)
{
  std::string out = "'";
  for (char ch : value) {
    if (ch == '\'') out += "'\"'\"'";
    else out += ch;
  }
  out += "'";
  return out;
}

#ifdef _WIN32
std::string powershell_quote(const std::string& value)
{
  std::string out = "'";
  for (char ch : value) {
    if (ch == '\'') out += '\'';
    out += ch;
  }
  out += "'";
  return out;
}
#endif

bool run_command(const std::string& command)
{
  return std::system(command.c_str()) == 0;
}

std::string make_temp_directory()
{
  const char* base_env = std::getenv("TMPDIR");
  if (!base_env || !*base_env) base_env = std::getenv("TEMP");
  if (!base_env || !*base_env) base_env = std::getenv("TMP");
#ifdef _WIN32
  const std::string base = (base_env && *base_env) ? base_env : ".";
#else
  const std::string base = (base_env && *base_env) ? base_env : "/tmp";
#endif
  const std::string dir = join_native_path(
    base,
    "reaadr_xlsx_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(std::rand()));
#ifdef _WIN32
  const std::string command =
    "powershell -NoProfile -ExecutionPolicy Bypass -Command \"New-Item -ItemType Directory -Force -LiteralPath " +
    powershell_quote(dir) + " | Out-Null\"";
#else
  const std::string command = "mkdir -p " + shell_quote_posix(dir);
#endif
  return run_command(command) ? dir : "";
}

void remove_temp_directory(const std::string& dir)
{
  if (dir.empty()) return;
#ifdef _WIN32
  const std::string command =
    "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Remove-Item -LiteralPath " +
    powershell_quote(dir) + " -Recurse -Force -ErrorAction SilentlyContinue\"";
#else
  const std::string command = "rm -rf " + shell_quote_posix(dir);
#endif
  run_command(command);
}

std::string xml_unescape(std::string value)
{
  auto replace_all = [&value](const std::string& from, const std::string& to) {
    std::string::size_type pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
      value.replace(pos, from.size(), to);
      pos += to.size();
    }
  };
  replace_all("&lt;", "<");
  replace_all("&gt;", ">");
  replace_all("&quot;", "\"");
  replace_all("&apos;", "'");
  replace_all("&amp;", "&");
  return value;
}

int column_letters_to_index(const std::string& letters)
{
  int index = 0;
  for (char ch : letters) {
    if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    if (ch < 'A' || ch > 'Z') continue;
    index = (index * 26) + (ch - 'A' + 1);
  }
  return index;
}

std::string regex_first_group(const std::string& text, const std::regex& pattern)
{
  std::smatch match;
  return std::regex_search(text, match, pattern) && match.size() > 1 ? match[1].str() : "";
}

std::vector<std::string> parse_xlsx_shared_strings(const std::string& dir)
{
  const std::string xml =
    read_text_file(join_native_path(join_native_path(dir, "xl"), "sharedStrings.xml"));
  std::vector<std::string> strings;
  if (xml.empty()) return strings;

  const std::regex si_re(R"(<si[\s\S]*?</si>)");
  const std::regex text_re(R"(<t[^>]*>([\s\S]*?)</t>)");
  for (std::sregex_iterator it(xml.begin(), xml.end(), si_re), end; it != end; ++it) {
    const std::string si = it->str();
    std::string value;
    for (std::sregex_iterator text_it(si.begin(), si.end(), text_re), text_end;
         text_it != text_end; ++text_it) {
      value += xml_unescape((*text_it)[1].str());
    }
    strings.push_back(value);
  }
  return strings;
}

std::string first_xlsx_sheet_path(const std::string& dir)
{
  const std::string workbook =
    read_text_file(join_native_path(join_native_path(dir, "xl"), "workbook.xml"));
  const std::string rels = read_text_file(
    join_native_path(join_native_path(join_native_path(dir, "xl"), "_rels"), "workbook.xml.rels"));
  const std::string rel_id =
    regex_first_group(workbook, std::regex(R"re(<sheet[^>]*r:id="([^"]+)")re"));
  if (!rel_id.empty() && !rels.empty()) {
    const std::regex rel_re(R"(<Relationship[^>]+>)");
    for (std::sregex_iterator it(rels.begin(), rels.end(), rel_re), end; it != end; ++it) {
      const std::string rel = it->str();
      const std::string id = regex_first_group(rel, std::regex(R"re(Id="([^"]+)")re"));
      std::string target = regex_first_group(rel, std::regex(R"re(Target="([^"]+)")re"));
      if (id == rel_id && !target.empty()) {
        if (target[0] == '/') target.erase(0, 1);
        if (target.rfind("xl/", 0) != 0) target = "xl/" + target;
        std::replace(target.begin(), target.end(), '/', native_separator()[0]);
        return join_native_path(dir, target);
      }
    }
  }
  return join_native_path(
    join_native_path(join_native_path(dir, "xl"), "worksheets"), "sheet1.xml");
}

std::string parse_xlsx_cell_value(
  const std::string& cell_xml,
  const std::vector<std::string>& shared_strings)
{
  const std::string cell_type =
    regex_first_group(cell_xml, std::regex(R"re(<c[^>]*t="([^"]+)")re"));
  if (cell_type == "inlineStr") {
    const std::regex text_re(R"(<t[^>]*>([\s\S]*?)</t>)");
    std::string value;
    for (std::sregex_iterator it(cell_xml.begin(), cell_xml.end(), text_re), end; it != end; ++it)
      value += xml_unescape((*it)[1].str());
    return value;
  }

  const std::string raw = xml_unescape(
    regex_first_group(cell_xml, std::regex(R"(<v[^>]*>([\s\S]*?)</v>)")));
  if (cell_type == "s") {
    const int index = std::atoi(raw.c_str());
    return index >= 0 && static_cast<std::size_t>(index) < shared_strings.size()
      ? shared_strings[static_cast<std::size_t>(index)]
      : "";
  }
  if (cell_type == "b") return raw == "1" ? "TRUE" : "FALSE";
  return raw;
}

std::string tsv_escape(std::string value)
{
  for (char& ch : value) {
    if (ch == '\r' || ch == '\n') ch = ' ';
  }
  const bool needs_quote =
    value.find('\t') != std::string::npos || value.find('"') != std::string::npos;
  if (!needs_quote) return value;

  std::string out = "\"";
  for (char ch : value) {
    if (ch == '"') out += '"';
    out += ch;
  }
  out += '"';
  return out;
}

bool extract_xlsx_to_directory(const std::string& path, const std::string& dir)
{
#ifdef _WIN32
  const std::string zip_path = join_native_path(dir, "workbook.zip");
  const std::string command =
    "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Copy-Item -LiteralPath " +
    powershell_quote(path) + " -Destination " + powershell_quote(zip_path) +
    " -Force; Expand-Archive -LiteralPath " + powershell_quote(zip_path) +
    " -DestinationPath " + powershell_quote(dir) + " -Force\"";
#else
  const std::string command =
    "(unzip -qq " + shell_quote_posix(path) + " -d " + shell_quote_posix(dir) +
    ") || (ditto -x -k " + shell_quote_posix(path) + " " + shell_quote_posix(dir) + ")";
#endif
  return run_command(command);
}

} // namespace

XlsxImportResult read_xlsx_first_sheet_as_tsv(const std::string& path)
{
  XlsxImportResult result;
  if (path.empty()) {
    result.error = "No XLSX path was provided.";
    return result;
  }

  const std::string dir = make_temp_directory();
  if (dir.empty()) {
    result.error = "Could not create a temporary directory for XLSX import.";
    return result;
  }

  if (!extract_xlsx_to_directory(path, dir)) {
    result.error =
      "Could not extract XLSX file. Make sure the system archive tools are available.";
  } else {
    const std::vector<std::string> shared_strings = parse_xlsx_shared_strings(dir);
    const std::string sheet_xml = read_text_file(first_xlsx_sheet_path(dir));
    if (sheet_xml.empty()) {
      result.error = "Could not read the first worksheet in the XLSX file.";
    } else {
      const std::regex row_re(R"(<row[^>]*>[\s\S]*?</row>)");
      const std::regex cell_re(R"(<c[^>]*>[\s\S]*?</c>)");
      const std::regex ref_re(R"(<c[^>]*r="([A-Z]+)\d+")");
      std::vector<std::map<int, std::string>> rows;
      int max_col = 0;

      for (std::sregex_iterator row_it(sheet_xml.begin(), sheet_xml.end(), row_re), row_end;
           row_it != row_end; ++row_it) {
        const std::string row_xml = row_it->str();
        std::map<int, std::string> row;
        bool has_value = false;
        for (std::sregex_iterator cell_it(row_xml.begin(), row_xml.end(), cell_re), cell_end;
             cell_it != cell_end; ++cell_it) {
          const std::string cell_xml = cell_it->str();
          const int col = column_letters_to_index(regex_first_group(cell_xml, ref_re));
          if (col <= 0) continue;
          const std::string value = parse_xlsx_cell_value(cell_xml, shared_strings);
          if (!value.empty()) has_value = true;
          row[col] = value;
          max_col = (std::max)(max_col, col);
        }
        if (has_value) rows.push_back(std::move(row));
      }

      if (rows.empty() || max_col <= 0) {
        result.error = "XLSX worksheet is empty.";
      } else {
        std::ostringstream tsv;
        for (const auto& row : rows) {
          for (int col = 1; col <= max_col; ++col) {
            if (col > 1) tsv << '\t';
            const auto found = row.find(col);
            if (found != row.end()) tsv << tsv_escape(found->second);
          }
          tsv << '\n';
        }
        result.tsv = tsv.str();
      }
    }
  }

  remove_temp_directory(dir);
  return result;
}

} // namespace reaadr::reaper
