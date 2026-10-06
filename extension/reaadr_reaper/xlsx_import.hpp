#pragma once

#include <string>

namespace reaadr::reaper {

struct XlsxImportResult {
  std::string tsv;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Reads the first worksheet from an XLSX workbook and converts non-empty rows
// to tab-delimited text for the shared native cue-sheet parser.
XlsxImportResult read_xlsx_first_sheet_as_tsv(const std::string& path);

} // namespace reaadr::reaper
