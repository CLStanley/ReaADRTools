#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>

#include "cue_manager_window.hpp"
#include "cue_manager_ui_contract.hpp"
#include "cue_manager_lifecycle.hpp"
#include "win32_utf8.hpp"
#include "reaadr_core/domain_utils.hpp"
#include "reaadr_reaper/window_docking.hpp"

#include <reaper_plugin.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace reaadr::ui {
namespace {

constexpr const wchar_t* kWindowClass = L"ReaADRCueManagerWindow";
constexpr const char* kDockIdentifier = "reaadr.cue_manager";
constexpr const char* kWindowTitle = "ReaADR Tools - Cue Manager";
constexpr const wchar_t* kWindowTitleW = L"ReaADR Tools - Cue Manager";
constexpr int kMinWindowWidth = 1094;
constexpr int kMinWindowHeight = 750;
constexpr UINT kRefreshExistingWindow = WM_APP + 73;
constexpr UINT_PTR kRevisionTimer = 1;
constexpr UINT kRevisionPollMs = 250;
constexpr int kRows = 48300;
constexpr int kSearch = 48301;
constexpr int kCharacter = 48302;
constexpr int kStatus = 48303;
constexpr int kApplyFilter = 48304;
constexpr int kResetFilter = 48305;
constexpr int kRecord = 48306;
constexpr int kCueInfo = 48307;
constexpr int kCharacterFilter = 48308;
constexpr int kRefresh = 48309;
constexpr int kSync = 48310;
constexpr int kNewCue = 48311;
constexpr int kAddCue = 48312;
constexpr int kRemoveCue = 48313;
constexpr int kDetails = 48314;
constexpr int kEditCueId = 48315;
constexpr int kEditCharacter = 48316;
constexpr int kEditDialogue = 48317;
constexpr int kEditNotes = 48318;
constexpr int kEditType = 48319;
constexpr int kEditStart = 48320;
constexpr int kEditEnd = 48321;
constexpr int kEditStatus = 48322;
constexpr int kApplyEdit = 48323;
constexpr int kPrevious = 48324;
constexpr int kNext = 48325;
constexpr int kClose = 48326;
constexpr int kModuleCues = 48327;
constexpr int kModuleImport = 48328;
constexpr int kModuleSession = 48329;
constexpr int kModuleReports = 48330;
constexpr int kModuleOverlay = 48331;
constexpr int kModulePreferences = 48332;
constexpr int kModuleHelp = 48333;
constexpr int kLabelCueId = 48334;
constexpr int kLabelCharacter = 48335;
constexpr int kLabelType = 48336;
constexpr int kLabelStatus = 48337;
constexpr int kLabelDialogue = 48338;
constexpr int kLabelNotes = 48339;
constexpr int kLabelStart = 48340;
constexpr int kLabelEnd = 48341;
constexpr int kColumns = 48342;

CueManagerController* g_controller = nullptr;
double g_frame_rate = 24.0;
bool g_refreshing_rows = false;

CueManagerLifecycle::WindowHandle window_handle(HWND hwnd) { return reinterpret_cast<CueManagerLifecycle::WindowHandle>(hwnd); }
HWND control(HWND hwnd, int id) { return GetDlgItem(hwnd, id); }
std::string control_text(HWND hwnd, int id) { return win32::get_window_text_utf8(control(hwnd, id)); }

void create_child(HWND parent, const wchar_t* class_name, const wchar_t* text, DWORD style, int x, int y, int width, int height, int id)
{
  CreateWindowExW(0, class_name, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, parent,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
}

void move_control(HWND hwnd, int id, int x, int y, int width, int height)
{
  HWND child = control(hwnd, id);
  if (child) MoveWindow(child, x, y, width, height, TRUE);
}

void layout_window_controls(HWND hwnd)
{
  RECT client{};
  if (!GetClientRect(hwnd, &client)) return;
  const int width = client.right - client.left;
  const int height = client.bottom - client.top;
  constexpr int margin = 16;
  constexpr int list_top = 116;
  const int content_width = (std::max)(1, width - margin * 2);
  const int action_y = height - 43;
  const int dialogue_y = action_y - 30;
  const int identity_y = dialogue_y - 32;
  const int details_y = identity_y - 30;
  const int list_height = (std::max)(80, details_y - 8 - list_top);
  move_control(hwnd, kRows, margin, list_top, content_width, list_height);
  move_control(hwnd, kDetails, margin, details_y, content_width, 24);
  move_control(hwnd, kLabelCueId, 16, identity_y + 4, 52, 18); move_control(hwnd, kEditCueId, 70, identity_y, 118, 24);
  move_control(hwnd, kLabelCharacter, 198, identity_y + 4, 68, 18); move_control(hwnd, kEditCharacter, 270, identity_y, 220, 24);
  move_control(hwnd, kLabelType, 502, identity_y + 4, 38, 18); move_control(hwnd, kEditType, 544, identity_y, 140, 160);
  move_control(hwnd, kLabelStatus, 696, identity_y + 4, 44, 18); move_control(hwnd, kEditStatus, 744, identity_y, (std::max)(180, width - margin - 744), 180);
  const int split = margin + content_width * 46 / 100;
  const int dialogue_edit_x = 78;
  const int notes_label_x = split + 12;
  const int notes_edit_x = notes_label_x + 48;
  move_control(hwnd, kLabelDialogue, 16, dialogue_y + 4, 58, 18); move_control(hwnd, kEditDialogue, dialogue_edit_x, dialogue_y, (std::max)(120, notes_label_x - 12 - dialogue_edit_x), 24);
  move_control(hwnd, kLabelNotes, notes_label_x, dialogue_y + 4, 44, 18); move_control(hwnd, kEditNotes, notes_edit_x, dialogue_y, (std::max)(120, width - margin - notes_edit_x), 24);
  move_control(hwnd, kLabelStart, 16, action_y + 6, 42, 18); move_control(hwnd, kEditStart, 62, action_y + 2, 126, 24);
  move_control(hwnd, kLabelEnd, 198, action_y + 6, 34, 18); move_control(hwnd, kEditEnd, 236, action_y + 2, 126, 24);
  move_control(hwnd, kApplyEdit, 376, action_y, 94, 28);
  const int close_x = width - margin - 80;
  const int next_x = close_x - 104 - 74;
  const int previous_x = next_x - 6 - 84;
  move_control(hwnd, kPrevious, previous_x, action_y, 84, 28); move_control(hwnd, kNext, next_x, action_y, 74, 28); move_control(hwnd, kClose, close_x, action_y, 80, 28);
}

std::string display_timecode(const std::string& value)
{
  const auto parsed = core::parse_timecode(value, g_frame_rate);
  return parsed ? core::format_timecode(*parsed.seconds, g_frame_rate) : value;
}

void restore_window_layout(HWND hwnd)
{
  if (!g_controller) return;
  const auto layout = g_controller->load_window_layout();
  const int width = (std::max)(kMinWindowWidth, layout.width);
  const int height = (std::max)(kMinWindowHeight, layout.height);
  UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
  int x = 0, y = 0;
  if (layout.has_position) { x = layout.x; y = layout.y; }
  else { RECT current{}; if (GetWindowRect(hwnd, &current)) { x = static_cast<int>(current.left); y = static_cast<int>(current.top); } else flags |= SWP_NOMOVE; }
  SetWindowPos(hwnd, nullptr, x, y, width, height, flags);
  if (layout.dock >= 0) reaper::add_window_to_docker(hwnd, kWindowTitle, kDockIdentifier, layout.dock);
}

void save_window_layout(HWND hwnd)
{
  if (!g_controller || !hwnd) return;
  core::WindowLayout layout = g_controller->load_window_layout();
  const auto dock = reaper::inspect_window_dock_state(hwnd);
  layout.dock = dock.dock_index;
  RECT rect{};
  if (GetWindowRect(hwnd, &rect)) {
    layout.x = static_cast<int>(rect.left); layout.y = static_cast<int>(rect.top);
    layout.width = (std::max)(kMinWindowWidth, static_cast<int>(rect.right - rect.left));
    layout.height = (std::max)(kMinWindowHeight, static_cast<int>(rect.bottom - rect.top)); layout.has_position = true;
  }
  g_controller->save_window_layout(layout);
}

void close_manager_window(HWND hwnd)
{
  KillTimer(hwnd, kRevisionTimer); save_window_layout(hwnd);
  const auto dock = reaper::inspect_window_dock_state(hwnd); if (dock.docked()) reaper::remove_window_from_docker(hwnd);
  cue_manager_lifecycle().closed(window_handle(hwnd)); DestroyWindow(hwnd);
}

void set_text(HWND hwnd, int id, const std::string& value) { win32::set_control_text_utf8(hwnd, id, value); }

void populate_editor(HWND hwnd)
{
  const core::CueManagerRow* row = g_controller ? g_controller->selected_row() : nullptr;
  set_text(hwnd, kEditCueId, row ? row->cue_key : std::string{}); set_text(hwnd, kEditCharacter, row ? row->character : std::string{});
  set_text(hwnd, kEditDialogue, row ? row->dialogue : std::string{}); set_text(hwnd, kEditNotes, row ? row->notes : std::string{});
  set_text(hwnd, kEditType, row ? row->cue_type : std::string{}); set_text(hwnd, kEditStart, row ? row->start_time : std::string{});
  set_text(hwnd, kEditEnd, row ? row->end_time : std::string{}); set_text(hwnd, kEditStatus, row ? row->status : std::string{});
}

void update_details(HWND hwnd)
{
  const auto* row = g_controller ? g_controller->selected_row() : nullptr;
  const std::string text = row ? "Selected: " + row->cue_key + " | " + row->character + " | " + row->status + " | " + row->dialogue : "Selected: (none)";
  set_text(hwnd, kDetails, text); populate_editor(hwnd);
}

void refresh_rows(HWND hwnd)
{
  if (!g_controller) return; HWND table = control(hwnd, kRows); if (!table) return;
  g_refreshing_rows = true; ListView_DeleteAllItems(table); const auto& rows = g_controller->view().cues.rows; int selected = -1;
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto& row = rows[index];
    const std::array<std::string, 8> cells = {row.cue_key, row.character, display_timecode(row.start_time), display_timecode(row.end_time), row.status, row.cue_type, row.dialogue, row.notes};
    std::array<std::wstring, 8> wide_cells; for (std::size_t cell = 0; cell < cells.size(); ++cell) wide_cells[cell] = win32::utf8_to_wide(cells[cell]);
    LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = static_cast<int>(index); item.pszText = wide_cells[0].data(); ListView_InsertItemW(table, &item);
    for (int column_index = 1; column_index < 8; ++column_index) ListView_SetItemTextW(table, item.iItem, column_index, wide_cells[column_index].data());
    if (row.selected) selected = static_cast<int>(index);
  }
  if (selected >= 0) { ListView_SetItemState(table, selected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); ListView_EnsureVisible(table, selected, FALSE); }
  g_refreshing_rows = false; update_details(hwnd);
}

void show_error(HWND hwnd) { if (g_controller && !g_controller->view().error.empty()) win32::message_box_utf8(hwnd, g_controller->view().error, "ReaADR Cue Manager", MB_OK | MB_ICONERROR); }
void reload_and_refresh(HWND hwnd) { if (!g_controller) return; if (g_controller->reload()) refresh_rows(hwnd); else show_error(hwnd); }
void poll_external_changes(HWND hwnd) { if (!g_controller) return; bool changed = false; if (g_controller->reload_if_revision_changed(changed) && changed) refresh_rows(hwnd); }

void populate_new_cue(HWND hwnd)
{
  if (!g_controller) return; const auto cue = g_controller->default_add_options();
  set_text(hwnd, kEditCueId, cue.cue_key); set_text(hwnd, kEditCharacter, cue.character); set_text(hwnd, kEditDialogue, cue.dialogue); set_text(hwnd, kEditNotes, cue.notes);
  set_text(hwnd, kEditType, cue.cue_type); set_text(hwnd, kEditStart, cue.start_time); set_text(hwnd, kEditEnd, cue.end_time); set_text(hwnd, kEditStatus, cue.status);
  SetDlgItemTextW(hwnd, kDetails, L"New cue: edit the fields and choose Add Cue.");
}

void show_session_summary(HWND hwnd)
{
  if (!g_controller) return; const auto& view = g_controller->view();
  std::string summary = "Session: " + view.session_name + "\nRevision: " + view.revision + "\nTotal cues: " + std::to_string(view.total_cues) + "\nVisible cues: " + std::to_string(view.cues.rows.size());
  std::map<std::string, int> characters, statuses; for (const auto& row : view.cues.rows) { ++characters[row.character]; ++statuses[row.status]; }
  summary += "\n\nCharacters:"; for (const auto& entry : characters) summary += "\n  " + entry.first + ": " + std::to_string(entry.second);
  summary += "\n\nStatuses:"; for (const auto& entry : statuses) summary += "\n  " + entry.first + ": " + std::to_string(entry.second);
  win32::message_box_utf8(hwnd, summary, "ReaADR Session Summary", MB_OK | MB_ICONINFORMATION);
}

void show_session_tools(HWND hwnd)
{
  if (!g_controller) return;
  const int choice = MessageBoxW(hwnd, L"Yes: Validate the canonical ADR session.\nNo: Refresh/rebuild generated session artifacts.\nCancel: Return without changes.", L"ReaADR Session Tools", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (choice == IDYES) g_controller->trigger_action("validate_session"); else if (choice == IDNO) g_controller->trigger_action("refresh_session"); else return;
  reload_and_refresh(hwnd);
}

void show_reports_tools(HWND hwnd)
{
  if (!g_controller) return;
  constexpr UINT kSummary = 1, kCueSheet = 2, kTiming = 3, kMetadata = 4;
  HMENU menu = CreatePopupMenu();
  if (!menu) return;
  AppendMenuW(menu, MF_STRING, kSummary, L"Session Summary");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kCueSheet, L"Export Cue Sheet CSV");
  AppendMenuW(menu, MF_STRING, kTiming, L"Export Timing Report");
  AppendMenuW(menu, MF_STRING, kMetadata, L"Export Session Metadata");
  RECT anchor{};
  HWND reports = control(hwnd, kModuleReports);
  if (reports) GetWindowRect(reports, &anchor); else GetWindowRect(hwnd, &anchor);
  const UINT choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                                     anchor.left, anchor.bottom, 0, hwnd, nullptr);
  DestroyMenu(menu);
  if (choice == kSummary) show_session_summary(hwnd);
  else if (choice == kCueSheet) g_controller->trigger_action("export_cue_sheet");
  else if (choice == kTiming) g_controller->trigger_action("export_timing_report");
  else if (choice == kMetadata) g_controller->trigger_action("export_session_metadata");
}

void show_overlay_tools(HWND hwnd)
{
  if (!g_controller) return;
  const int choice = MessageBoxW(hwnd, L"Yes: Refresh the video overlay.\nNo: Switch to the Actor overlay profile.\nCancel: Return without changes.", L"ReaADR Overlay", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (choice == IDYES) g_controller->trigger_action("refresh_overlay"); else if (choice == IDNO) g_controller->trigger_action("overlay_profile:actor"); else return;
  reload_and_refresh(hwnd);
}

void show_help(HWND hwnd)
{
  MessageBoxW(hwnd, L"Cues: browse, filter, edit, navigate, record, and inspect canonical cues.\n\nImport: choose a cue sheet and run the native transactional importer.\n\nSession: validate or refresh generated tracks, regions, cue audio, filters, and overlays.\n\nReports: inspect a session summary or export cue, timing, and metadata reports.\n\nOverlay: refresh native video overlay output and profiles.\n\nPreferences: edit native Manager and overlay preferences.", L"ReaADR Manager Help", MB_OK | MB_ICONINFORMATION);
}

void create_window_controls(HWND hwnd)
{
  create_child(hwnd, L"STATIC", L"ReaADR Cue Manager", 0, 16, 12, 210, 20, -1);
  create_child(hwnd, L"BUTTON", L"Cues", BS_PUSHBUTTON | WS_TABSTOP, 240, 8, 72, 26, kModuleCues); create_child(hwnd, L"BUTTON", L"Import", BS_PUSHBUTTON | WS_TABSTOP, 318, 8, 72, 26, kModuleImport);
  create_child(hwnd, L"BUTTON", L"Session", BS_PUSHBUTTON | WS_TABSTOP, 396, 8, 76, 26, kModuleSession); create_child(hwnd, L"BUTTON", L"Reports", BS_PUSHBUTTON | WS_TABSTOP, 478, 8, 76, 26, kModuleReports);
  create_child(hwnd, L"BUTTON", L"Overlay", BS_PUSHBUTTON | WS_TABSTOP, 560, 8, 76, 26, kModuleOverlay); create_child(hwnd, L"BUTTON", L"Preferences", BS_PUSHBUTTON | WS_TABSTOP, 642, 8, 96, 26, kModulePreferences); create_child(hwnd, L"BUTTON", L"Help", BS_PUSHBUTTON | WS_TABSTOP, 744, 8, 68, 26, kModuleHelp);
  create_child(hwnd, L"STATIC", L"Search", 0, 16, 50, 48, 18, -1); create_child(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 66, 46, 210, 24, kSearch);
  create_child(hwnd, L"STATIC", L"Character", 0, 288, 50, 66, 18, -1); create_child(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 356, 46, 160, 24, kCharacter);
  create_child(hwnd, L"STATIC", L"Status", 0, 528, 50, 48, 18, -1); create_child(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 578, 46, 150, 180, kStatus);
  create_child(hwnd, L"BUTTON", L"Apply", BS_PUSHBUTTON | WS_TABSTOP, 740, 46, 62, 24, kApplyFilter); create_child(hwnd, L"BUTTON", L"Reset", BS_PUSHBUTTON | WS_TABSTOP, 808, 46, 62, 24, kResetFilter);
  create_child(hwnd, L"BUTTON", L"Record Current Cue", BS_PUSHBUTTON | WS_TABSTOP, 16, 80, 130, 26, kRecord); create_child(hwnd, L"BUTTON", L"Cue Info", BS_PUSHBUTTON | WS_TABSTOP, 152, 80, 82, 26, kCueInfo);
  create_child(hwnd, L"BUTTON", L"Character Filter", BS_PUSHBUTTON | WS_TABSTOP, 240, 80, 110, 26, kCharacterFilter); create_child(hwnd, L"BUTTON", L"Refresh Session", BS_PUSHBUTTON | WS_TABSTOP, 356, 80, 112, 26, kRefresh);
  create_child(hwnd, L"BUTTON", L"Update From Regions", BS_PUSHBUTTON | WS_TABSTOP, 474, 80, 138, 26, kSync); create_child(hwnd, L"BUTTON", L"New Cue", BS_PUSHBUTTON | WS_TABSTOP, 624, 80, 74, 26, kNewCue);
  create_child(hwnd, L"BUTTON", L"Add Cue", BS_PUSHBUTTON | WS_TABSTOP, 704, 80, 74, 26, kAddCue); create_child(hwnd, L"BUTTON", L"Remove Cue", BS_PUSHBUTTON | WS_TABSTOP, 784, 80, 88, 26, kRemoveCue);
  create_child(hwnd, L"BUTTON", L"Columns", BS_PUSHBUTTON | WS_TABSTOP, 878, 80, 82, 26, kColumns);
  create_child(hwnd, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP, 16, 116, 1040, 452, kRows);
  HWND table = control(hwnd, kRows); ListView_SetExtendedListViewStyleEx(table, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
  const auto& columns = core::cue_manager_columns();
  for (std::size_t index = 0; index < columns.size(); ++index) { std::wstring label = win32::utf8_to_wide(columns[index].label); LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH; column.pszText = label.data(); column.cx = columns[index].width; ListView_InsertColumnW(table, static_cast<int>(index), &column); }
  create_child(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, 16, 576, 1040, 24, kDetails);
  create_child(hwnd, L"STATIC", L"Cue ID", 0, 16, 610, 52, 18, kLabelCueId); create_child(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 70, 606, 118, 24, kEditCueId);
  create_child(hwnd, L"STATIC", L"Character", 0, 198, 610, 68, 18, kLabelCharacter); create_child(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 270, 606, 220, 24, kEditCharacter);
  create_child(hwnd, L"STATIC", L"Type", 0, 502, 610, 38, 18, kLabelType); create_child(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 544, 606, 140, 160, kEditType);
  create_child(hwnd, L"STATIC", L"Status", 0, 696, 610, 44, 18, kLabelStatus); create_child(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 744, 606, 180, 180, kEditStatus);
  create_child(hwnd, L"STATIC", L"Dialogue", 0, 16, 642, 58, 18, kLabelDialogue); create_child(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 78, 638, 412, 24, kEditDialogue);
  create_child(hwnd, L"STATIC", L"Notes", 0, 502, 642, 44, 18, kLabelNotes); create_child(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 550, 638, 374, 24, kEditNotes);
  create_child(hwnd, L"STATIC", L"Start", 0, 16, 674, 42, 18, kLabelStart); create_child(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 62, 670, 126, 24, kEditStart);
  create_child(hwnd, L"STATIC", L"End", 0, 198, 674, 34, 18, kLabelEnd); create_child(hwnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 236, 670, 126, 24, kEditEnd);
  create_child(hwnd, L"BUTTON", L"Apply Edit", BS_PUSHBUTTON | WS_TABSTOP, 376, 668, 94, 28, kApplyEdit); create_child(hwnd, L"BUTTON", L"Previous", BS_PUSHBUTTON | WS_TABSTOP, 708, 668, 84, 28, kPrevious); create_child(hwnd, L"BUTTON", L"Next", BS_PUSHBUTTON | WS_TABSTOP, 798, 668, 74, 28, kNext); create_child(hwnd, L"BUTTON", L"Close", BS_DEFPUSHBUTTON | WS_TABSTOP, 976, 668, 80, 28, kClose);
  SendDlgItemMessageW(hwnd, kStatus, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Any"));
  for (const auto& status : core::cue_manager_status_choices()) { const std::wstring wide = win32::utf8_to_wide(status); SendDlgItemMessageW(hwnd, kStatus, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str())); SendDlgItemMessageW(hwnd, kEditStatus, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str())); }
  for (const auto& type : core::cue_manager_type_choices()) { const std::wstring wide = win32::utf8_to_wide(type); SendDlgItemMessageW(hwnd, kEditType, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str())); }
  SetDlgItemTextW(hwnd, kStatus, L"Any");
}

void show_column_widths(HWND hwnd)
{
  HWND table = control(hwnd, kRows);
  if (!table) return;
  const auto& columns = core::cue_manager_columns();
  HMENU menu = CreatePopupMenu();
  if (!menu) return;
  constexpr UINT kReset = 49000;
  AppendMenuW(menu, MF_STRING, kReset, L"Reset all columns");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  for (std::size_t index = 0; index < columns.size(); ++index) {
    const std::wstring label = win32::utf8_to_wide(columns[index].label);
    HMENU submenu = CreatePopupMenu();
    AppendMenuW(submenu, MF_STRING, 49100 + static_cast<UINT>(index), L"Narrower");
    AppendMenuW(submenu, MF_STRING, 49200 + static_cast<UINT>(index), L"Wider");
    const std::wstring title = label + L" (" + std::to_wstring(ListView_GetColumnWidth(table, static_cast<int>(index))) + L")";
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(submenu), title.c_str());
  }
  RECT anchor{};
  HWND button = control(hwnd, kColumns);
  if (button) GetWindowRect(button, &anchor); else GetWindowRect(hwnd, &anchor);
  const UINT choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                                     anchor.left, anchor.bottom, 0, hwnd, nullptr);
  DestroyMenu(menu);
  if (choice == kReset) {
    for (std::size_t index = 0; index < columns.size(); ++index)
      ListView_SetColumnWidth(table, static_cast<int>(index), columns[index].width);
  } else if (choice >= 49100 && choice < 49100 + columns.size()) {
    const int index = static_cast<int>(choice - 49100);
    ListView_SetColumnWidth(table, index,
      core::adjust_cue_manager_column_width(ListView_GetColumnWidth(table, index), false));
  } else if (choice >= 49200 && choice < 49200 + columns.size()) {
    const int index = static_cast<int>(choice - 49200);
    ListView_SetColumnWidth(table, index,
      core::adjust_cue_manager_column_width(ListView_GetColumnWidth(table, index), true));
  }
}

void apply_table_filters(HWND hwnd)
{
  if (!g_controller) return; std::string status = control_text(hwnd, kStatus); if (status == "Any") status.clear();
  if (g_controller->set_filters(control_text(hwnd, kSearch), control_text(hwnd, kCharacter), status)) refresh_rows(hwnd); else show_error(hwnd);
}

void add_cue(HWND hwnd)
{
  if (!g_controller) return; core::CueManagerAddOptions cue;
  cue.cue_key = control_text(hwnd, kEditCueId); cue.character = control_text(hwnd, kEditCharacter); cue.dialogue = control_text(hwnd, kEditDialogue); cue.notes = control_text(hwnd, kEditNotes); cue.cue_type = control_text(hwnd, kEditType); cue.start_time = control_text(hwnd, kEditStart); cue.end_time = control_text(hwnd, kEditEnd); cue.status = control_text(hwnd, kEditStatus);
  std::string error; if (g_controller->add_cue(cue, error)) refresh_rows(hwnd); else if (!error.empty()) win32::message_box_utf8(hwnd, error, "ReaADR Cue Manager", MB_OK | MB_ICONERROR);
}

void apply_edit(HWND hwnd)
{
  if (!g_controller) return; core::CueManagerEditOptions edit;
  edit.new_cue_key = control_text(hwnd, kEditCueId); edit.new_character = control_text(hwnd, kEditCharacter); edit.dialogue = control_text(hwnd, kEditDialogue); edit.dialogue_set = true; edit.notes = control_text(hwnd, kEditNotes); edit.notes_set = true; edit.cue_type = control_text(hwnd, kEditType); edit.start_time = control_text(hwnd, kEditStart); edit.end_time = control_text(hwnd, kEditEnd); edit.status = control_text(hwnd, kEditStatus);
  std::string error; if (g_controller->edit_selected(edit, error)) refresh_rows(hwnd); else if (!error.empty()) win32::message_box_utf8(hwnd, error, "ReaADR Cue Manager", MB_OK | MB_ICONERROR);
}

LRESULT CALLBACK cue_manager_wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  switch (message) {
    case WM_CREATE: create_window_controls(hwnd); layout_window_controls(hwnd); refresh_rows(hwnd); return 0;
    case WM_SIZE: if (wparam != SIZE_MINIMIZED) layout_window_controls(hwnd); return 0;
    case WM_GETMINMAXINFO: { auto* info = reinterpret_cast<MINMAXINFO*>(lparam); if (info) { info->ptMinTrackSize.x = kMinWindowWidth; info->ptMinTrackSize.y = kMinWindowHeight; } return 0; }
    case WM_TIMER: if (wparam == kRevisionTimer) poll_external_changes(hwnd); return 0;
    case kRefreshExistingWindow: reload_and_refresh(hwnd); return 0;
    case WM_NOTIFY: {
      if (!g_controller || g_refreshing_rows) break; const auto* header = reinterpret_cast<const NMHDR*>(lparam); if (!header || header->idFrom != kRows) break; const auto* change = reinterpret_cast<const NMLISTVIEW*>(lparam);
      if (header->code == LVN_COLUMNCLICK) { const auto& columns = core::cue_manager_columns(); if (change->iSubItem >= 0 && static_cast<std::size_t>(change->iSubItem) < columns.size() && g_controller->sort_by(columns[change->iSubItem].key)) refresh_rows(hwnd); return 0; }
      if (header->code == LVN_ITEMCHANGED && (change->uNewState & LVIS_SELECTED)) { if (g_controller->select_index(change->iItem)) update_details(hwnd); else { refresh_rows(hwnd); show_error(hwnd); } return 0; }
      if (header->code == NM_DBLCLK) { const auto* row = g_controller->selected_row(); if (row) { std::string error; if (!g_controller->navigate_to_id(row->cue_key, error) && !error.empty()) win32::message_box_utf8(hwnd, error, "ReaADR Cue Manager", MB_OK | MB_ICONERROR); refresh_rows(hwnd); } return 0; }
      break;
    }
    case WM_COMMAND: {
      const int command = LOWORD(wparam);
      if (command == kModuleCues) { reload_and_refresh(hwnd); return 0; }
      if (command == kModuleImport) { if (g_controller) { g_controller->trigger_import({}, false, "all", {}); reload_and_refresh(hwnd); } return 0; }
      if (command == kModuleSession) { show_session_tools(hwnd); return 0; } if (command == kModuleReports) { show_reports_tools(hwnd); return 0; } if (command == kModuleOverlay) { show_overlay_tools(hwnd); return 0; }
      if (command == kModulePreferences) { if (g_controller) { g_controller->trigger_action("preferences"); reload_and_refresh(hwnd); } return 0; }
      if (command == kModuleHelp) { show_help(hwnd); return 0; } if (command == kColumns) { show_column_widths(hwnd); return 0; } if (command == kApplyFilter) { apply_table_filters(hwnd); return 0; }
      if (command == kResetFilter) { SetDlgItemTextW(hwnd, kSearch, L""); SetDlgItemTextW(hwnd, kCharacter, L""); SetDlgItemTextW(hwnd, kStatus, L"Any"); if (g_controller && g_controller->set_filters({}, {}, {})) refresh_rows(hwnd); return 0; }
      if (command == kRecord || command == kCueInfo || command == kCharacterFilter || command == kRefresh || command == kSync) { if (!g_controller) return 0; const char* action = command == kRecord ? "record_cue" : command == kCueInfo ? "cue_info" : command == kCharacterFilter ? "character_filter" : command == kRefresh ? "refresh_session" : "sync_regions"; g_controller->trigger_action(action); reload_and_refresh(hwnd); return 0; }
      if (command == kNewCue) { populate_new_cue(hwnd); return 0; } if (command == kAddCue) { add_cue(hwnd); return 0; }
      if (command == kRemoveCue) { const auto* row = g_controller ? g_controller->selected_row() : nullptr; if (!row) { MessageBoxW(hwnd, L"Select a cue before removing it.", L"ReaADR Cue Manager", MB_OK | MB_ICONINFORMATION); return 0; } const std::string prompt = "Remove cue " + row->cue_key + " (" + row->character + ")?"; if (win32::message_box_utf8(hwnd, prompt, "ReaADR Cue Manager", MB_YESNO | MB_ICONWARNING) == IDYES) { std::string error; if (g_controller->remove_selected(error)) refresh_rows(hwnd); else if (!error.empty()) win32::message_box_utf8(hwnd, error, "ReaADR Cue Manager", MB_OK | MB_ICONERROR); } return 0; }
      if (command == kApplyEdit) { apply_edit(hwnd); return 0; }
      if (command == kPrevious || command == kNext) { if (g_controller) { const bool moved = command == kNext ? g_controller->navigate_next() : g_controller->navigate_previous(); if (moved) refresh_rows(hwnd); else show_error(hwnd); } return 0; }
      if (command == kClose || command == IDCANCEL) { close_manager_window(hwnd); return 0; }
      break;
    }
    case WM_CLOSE: close_manager_window(hwnd); return 0;
    case WM_NCDESTROY: KillTimer(hwnd, kRevisionTimer); cue_manager_lifecycle().closed(window_handle(hwnd)); g_controller = nullptr; break;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

} // namespace

bool show_cue_manager(CueManagerController& controller, double frame_rate)
{
  auto& lifecycle = cue_manager_lifecycle();
  if (lifecycle.is_open()) { HWND existing = reinterpret_cast<HWND>(lifecycle.window()); if (existing && IsWindow(existing)) { PostMessageW(existing, kRefreshExistingWindow, 0, 0); ShowWindow(existing, SW_SHOW); reaper::activate_docked_window(existing); SetForegroundWindow(existing); return true; } lifecycle.closed(lifecycle.window()); }
  INITCOMMONCONTROLSEX common_controls{sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&common_controls);
  HINSTANCE instance = GetModuleHandleW(nullptr); WNDCLASSW window_class{}; window_class.lpfnWndProc = cue_manager_wnd_proc; window_class.hInstance = instance; window_class.hCursor = LoadCursor(nullptr, IDC_ARROW); window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1); window_class.lpszClassName = kWindowClass;
  if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
  g_controller = &controller; g_frame_rate = frame_rate; HWND owner = GetForegroundWindow();
  HWND window = CreateWindowExW(0, kWindowClass, kWindowTitleW, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME, CW_USEDEFAULT, CW_USEDEFAULT, kMinWindowWidth, kMinWindowHeight, owner, nullptr, instance, nullptr);
  if (!window) { g_controller = nullptr; return false; }
  if (!lifecycle.begin_open(window_handle(window))) { DestroyWindow(window); g_controller = nullptr; return false; }
  restore_window_layout(window); ShowWindow(window, SW_SHOW); if (reaper::inspect_window_dock_state(window).docked()) reaper::activate_docked_window(window); UpdateWindow(window); SetTimer(window, kRevisionTimer, kRevisionPollMs, nullptr);
  return true;
}

} // namespace reaadr::ui

#endif
