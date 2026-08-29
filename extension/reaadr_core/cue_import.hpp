#pragma once

#include "session_model.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace reaadr::core {

struct RawCell {
  std::string value;
  std::string label;
};

struct DelimitedRow {
  int line_number = 0;
  Fields values;
  std::map<std::string, RawCell> raw_cells;
};

struct DelimitedTable {
  std::vector<std::string> headers;
  std::vector<std::string> raw_headers;
  std::vector<DelimitedRow> rows;
  char delimiter = ',';
  std::string delimiter_name = "CSV";
};

enum class TableParseError {
  none,
  empty,
};

struct TableParseResult {
  DelimitedTable table;
  TableParseError error = TableParseError::none;
  std::string message;

  explicit operator bool() const { return error == TableParseError::none; }
};

using ColumnMapping = std::map<std::string, std::string>;

enum class CueImportError {
  none,
  missing_required_mapping,
  invalid_row,
  no_cues,
};

struct CueImportResult {
  std::vector<Fields> cues;
  ColumnMapping mapping;
  CueImportError error = CueImportError::none;
  std::string message;

  explicit operator bool() const { return error == CueImportError::none; }
};

// Parses already-loaded text. File selection and I/O stay in the application
// layer so CSV, TSV, and XLSX-extracted text all share this deterministic path.
TableParseResult parse_delimited_content(const std::string& content, const std::string& source_path = {});

ColumnMapping default_column_mapping(const std::vector<std::string>& normalized_headers);
std::vector<std::string> missing_required_mapping(const ColumnMapping& mapping);

CueImportResult import_cues(const DelimitedTable& table,
                            double frame_rate,
                            const std::optional<ColumnMapping>& requested_mapping = std::nullopt);

} // namespace reaadr::core
