# ReaADR Import Test Documents

These files exercise the supported script import formats:

- `cue_sheet_template.csv`: baseline CSV with quoted dialogue fields.
- `cue_sheet_template.tsv`: tab-delimited version of the same cue data.
- `cue_sheet_template_tab_table.txt`: plain text tab-delimited table.
- `cue_sheet_template.xlsx`: Excel workbook test file using the first worksheet.
- `ReaADR_Test_Anime_Cue_Sheet.csv`: larger CSV sample with studio-style fields.
- `ReaADR_Test_Anime_Cue_Sheet_Reordered.csv`: CSV sample with columns in a different order.

For `.xlsx` imports, ReaADR reads the first worksheet, treats the first non-empty row as headers, and imports cue rows below it.
