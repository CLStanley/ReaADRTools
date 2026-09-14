#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>

#include "cue_manager_window.hpp"
#include "cue_manager_ui_contract.hpp"
#include "reaadr_core/domain_utils.hpp"
#include "reaadr_reaper/window_docking.hpp"

#include <reaper_plugin.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace reaadr::ui {
namespace {

constexpr const char* kWindowClass = "ReaADRCueManagerWindow";
constexpr const char* kDockIdentifier = "reaadr.cue_manager";
constexpr const char* kWindowTitle = "ReaADR Tools - Cue Manager";
constexpr int kMinWindowWidth = 1094;
constexpr int kMinWindowHeight = 750;
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

CueManagerController* g_controller = nullptr;
double g_frame_rate = 24.0;
bool g_refreshing_rows = false;

HWND control(HWND hwnd, int id)
{
  return GetDlgItem(hwnd, id);
}

std::string control_text(HWND hwnd, int id)
{
  HWND child = control(hwnd, id);
  const int length = child ? GetWindowTextLengthA(child) : 0;
  std::string value(static_cast<std::size_t>(length), '\0');
  if (length > 0) GetWindowTextA(child, value.data(), length + 1);
  return value;
}

void create_child(HWND parent, const char* class_name, const char* text,
                  DWORD style, int x, int y, int width, int height, int id)
{
  CreateWindowExA(0, class_name, text, WS_CHILD | WS_VISIBLE | style,
                  x, y, width, height, parent,
                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                  GetModuleHandle(nullptr), nullptr);
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
  int x = 0;
  int y = 0;
  if (layout.has_position) {
    x = layout.x;
    y = layout.y;
  } else {
    RECT current{};
    if (GetWindowRect(hwnd, &current)) {
      x = static_cast<int>(current.left);
      y = static_cast<int>(current.top);
    } else {
      flags |= SWP_NOMOVE;
    }
  }
  SetWindowPos(hwnd, nullptr, x, y, width, height, flags);

  if (layout.dock >= 0)
    reaper::add_window_to_docker(hwnd, kWindowTitle, kDockIdentifier, layout.dock);
}

void save_window_layout(HWND hwnd)
{
  if (!g_controller || !hwnd) return;
  core::WindowLayout layout = g_controller->load_window_layout();
  const auto dock = reaper::inspect_window_dock_state(hwnd);
  layout.dock = dock.dock_index;

  RECT rect{};
  if (GetWindowRect(hwnd, &rect)) {
    layout.x = static_cast<int>(rect.left);
    layout.y = static_cast<int>(rect.top);
    layout.width = (std::max)(kMinWindowWidth, static_cast<int>(rect.right - rect.left));
    layout.height = (std::max)(kMinWindowHeight, static_cast<int>(rect.bottom - rect.top));
    layout.has_position = true;
  }
  g_controller->save_window_layout(layout);
}

void close_manager_window(HWND hwnd)
{
  save_window_layout(hwnd);
  const auto dock = reaper::inspect_window_dock_state(hwnd);
  if (dock.docked()) reaper::remove_window_from_docker(hwnd);
  DestroyWindow(hwnd);
}

void populate_editor(HWND hwnd)
{
  const core::CueManagerRow* row = g_controller ? g_controller->selected_row() : nullptr;
  SetDlgItemTextA(hwnd, kEditCueId, row ? row->cue_key.c_str() : "");
  SetDlgItemTextA(hwnd, kEditCharacter, row ? row->character.c_str() : "");
  SetDlgItemTextA(hwnd, kEditDialogue, row ? row->dialogue.c_str() : "");
  SetDlgItemTextA(hwnd, kEditNotes, row ? row->notes.c_str() : "");
  SetDlgItemTextA(hwnd, kEditType, row ? row->cue_type.c_str() : "");
  SetDlgItemTextA(hwnd, kEditStart, row ? row->start_time.c_str() : "");
  SetDlgItemTextA(hwnd, kEditEnd, row ? row->end_time.c_str() : "");
  SetDlgItemTextA(hwnd, kEditStatus, row ? row->status.c_str() : "");
}

void update_details(HWND hwnd)
{
  const auto* row = g_controller ? g_controller->selected_row() : nullptr;
  const std::string text = row
    ? "Selected: " + row->cue_key + " | " + row->character + " | " + row->status + " | " + row->dialogue
    : "Selected: (none)";
  SetDlgItemTextA(hwnd, kDetails, text.c_str());
  populate_editor(hwnd);
}

void refresh_rows(HWND hwnd)
{
  if (!g_controller) return;
  HWND table = control(hwnd, kRows);
  if (!table) return;
  g_refreshing_rows = true;
  ListView_DeleteAllItems(table);
  const auto& rows = g_controller->view().cues.rows;
  int selected = -1;
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto& row = rows[index];
    const std::string cells[] = {
      row.cue_key, row.character, display_timecode(row.start_time),
      display_timecode(row.end_time), row.status, row.cue_type,
      row.dialogue, row.notes,
    };
    LVITEMA item{};
    item.mask = LVIF_TEXT;
    item.iItem = static_cast<int>(index);
    item.pszText = const_cast<char*>(cells[0].c_str());
    ListView_InsertItemA(table, &item);
    for (int column_index = 1; column_index < 8; ++column_index)
      ListView_SetItemTextA(table, item.iItem, column_index,
                            const_cast<char*>(cells[column_index].c_str()));
    if (row.selected) selected = static_cast<int>(index);
  }
  if (selected >= 0) {
    ListView_SetItemState(table, selected, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(table, selected, FALSE);
  }
  g_refreshing_rows = false;
  update_details(hwnd);
}

void show_error(HWND hwnd)
{
  if (g_controller && !g_controller->view().error.empty())
    MessageBoxA(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", MB_OK | MB_ICONERROR);
}

void reload_and_refresh(HWND hwnd)
{
  if (!g_controller) return;
  if (g_controller->reload()) refresh_rows(hwnd);
  else show_error(hwnd);
}

void populate_new_cue(HWND hwnd)
{
  if (!g_controller) return;
  const auto cue = g_controller->default_add_options();
  SetDlgItemTextA(hwnd, kEditCueId, cue.cue_key.c_str());
  SetDlgItemTextA(hwnd, kEditCharacter, cue.character.c_str());
  SetDlgItemTextA(hwnd, kEditDialogue, cue.dialogue.c_str());
  SetDlgItemTextA(hwnd, kEditNotes, cue.notes.c_str());
  SetDlgItemTextA(hwnd, kEditType, cue.cue_type.c_str());
  SetDlgItemTextA(hwnd, kEditStart, cue.start_time.c_str());
  SetDlgItemTextA(hwnd, kEditEnd, cue.end_time.c_str());
  SetDlgItemTextA(hwnd, kEditStatus, cue.status.c_str());
  SetDlgItemTextA(hwnd, kDetails, "New cue: edit the fields and choose Add Cue.");
}

void show_session_summary(HWND hwnd)
{
  if (!g_controller) return;
  const auto& view = g_controller->view();
  std::string summary = "Session: " + view.session_name +
    "\nRevision: " + view.revision +
    "\nTotal cues: " + std::to_string(view.total_cues) +
    "\nVisible cues: " + std::to_string(view.cues.rows.size());
  std::map<std::string, int> characters;
  std::map<std::string, int> statuses;
  for (const auto& row : view.cues.rows) {
    ++characters[row.character];
    ++statuses[row.status];
  }
  summary += "\n\nCharacters:";
  for (const auto& entry : characters)
    summary += "\n  " + entry.first + ": " + std::to_string(entry.second);
  summary += "\n\nStatuses:";
  for (const auto& entry : statuses)
    summary += "\n  " + entry.first + ": " + std::to_string(entry.second);
  MessageBoxA(hwnd, summary.c_str(), "ReaADR Session Summary", MB_OK | MB_ICONINFORMATION);
}

void show_session_tools(HWND hwnd)
{
  if (!g_controller) return;
  const int choice = MessageBoxA(hwnd,
    "Yes: Validate the canonical ADR session.\n"
    "No: Refresh/rebuild generated session artifacts.\n"
    "Cancel: Return without changes.",
    "ReaADR Session Tools", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (choice == IDYES) g_controller->trigger_action("validate_session");
  else if (choice == IDNO) g_controller->trigger_action("refresh_session");
  else return;
  reload_and_refresh(hwnd);
}

void show_reports_tools(HWND hwnd)
{
  if (!g_controller) return;
  const int choice = MessageBoxA(hwnd,
    "Yes: Export Cue Sheet CSV.\n"
    "No: Export Timing Report.\n"
    "Cancel: Open the session summary instead.",
    "ReaADR Reports", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (choice == IDYES) g_controller->trigger_action("export_cue_sheet");
  else if (choice == IDNO) g_controller->trigger_action("export_timing_report");
  else show_session_summary(hwnd);
}

void show_overlay_tools(HWND hwnd)
{
  if (!g_controller) return;
  const int choice = MessageBoxA(hwnd,
    "Yes: Refresh the video overlay.\n"
    "No: Switch to the Actor overlay profile.\n"
    "Cancel: Return without changes.",
    "ReaADR Overlay", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (choice == IDYES) g_controller->trigger_action("refresh_overlay");
  else if (choice == IDNO) g_controller->trigger_action("overlay_profile:actor");
  else return;
  reload_and_refresh(hwnd);
}

void show_help(HWND hwnd)
{
  MessageBoxA(hwnd,
    "Cues: browse, filter, edit, navigate, record, and inspect canonical cues.\n\n"
    "Import: choose a cue sheet and run the native transactional importer.\n\n"
    "Session: validate or refresh generated tracks, regions, cue audio, filters, and overlays.\n\n"
    "Reports: export cue/timing data or inspect a native session summary.\n\n"
    "Overlay: refresh native video overlay output and profiles.\n\n"
    "Preferences: edit native Manager and overlay preferences.",
    "ReaADR Manager Help", MB_OK | MB_ICONINFORMATION);
}

void create_window_controls(HWND hwnd)
{
  create_child(hwnd, "STATIC", "ReaADR Cue Manager", 0, 16, 12, 210, 20, -1);
  create_child(hwnd, "BUTTON", "Cues", BS_PUSHBUTTON | WS_TABSTOP, 240, 8, 72, 26, kModuleCues);
  create_child(hwnd, "BUTTON", "Import", BS_PUSHBUTTON | WS_TABSTOP, 318, 8, 72, 26, kModuleImport);
  create_child(hwnd, "BUTTON", "Session", BS_PUSHBUTTON | WS_TABSTOP, 396, 8, 76, 26, kModuleSession);
  create_child(hwnd, "BUTTON", "Reports", BS_PUSHBUTTON | WS_TABSTOP, 478, 8, 76, 26, kModuleReports);
  create_child(hwnd, "BUTTON", "Overlay", BS_PUSHBUTTON | WS_TABSTOP, 560, 8, 76, 26, kModuleOverlay);
  create_child(hwnd, "BUTTON", "Preferences", BS_PUSHBUTTON | WS_TABSTOP, 642, 8, 96, 26, kModulePreferences);
  create_child(hwnd, "BUTTON", "Help", BS_PUSHBUTTON | WS_TABSTOP, 744, 8, 68, 26, kModuleHelp);

  create_child(hwnd, "STATIC", "Search", 0, 16, 50, 48, 18, -1);
  create_child(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 66, 46, 210, 24, kSearch);
  create_child(hwnd, "STATIC", "Character", 0, 288, 50, 66, 18, -1);
  create_child(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 356, 46, 160, 24, kCharacter);
  create_child(hwnd, "STATIC", "Status", 0, 528, 50, 48, 18, -1);
  create_child(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 578, 46, 150, 180, kStatus);
  create_child(hwnd, "BUTTON", "Apply", BS_PUSHBUTTON | WS_TABSTOP, 740, 46, 62, 24, kApplyFilter);
  create_child(hwnd, "BUTTON", "Reset", BS_PUSHBUTTON | WS_TABSTOP, 808, 46, 62, 24, kResetFilter);

  create_child(hwnd, "BUTTON", "Record Current Cue", BS_PUSHBUTTON | WS_TABSTOP, 16, 80, 130, 26, kRecord);
  create_child(hwnd, "BUTTON", "Cue Info", BS_PUSHBUTTON | WS_TABSTOP, 152, 80, 82, 26, kCueInfo);
  create_child(hwnd, "BUTTON", "Character Filter", BS_PUSHBUTTON | WS_TABSTOP, 240, 80, 110, 26, kCharacterFilter);
  create_child(hwnd, "BUTTON", "Refresh Session", BS_PUSHBUTTON | WS_TABSTOP, 356, 80, 112, 26, kRefresh);
  create_child(hwnd, "BUTTON", "Update From Regions", BS_PUSHBUTTON | WS_TABSTOP, 474, 80, 138, 26, kSync);
  create_child(hwnd, "BUTTON", "New Cue", BS_PUSHBUTTON | WS_TABSTOP, 624, 80, 74, 26, kNewCue);
  create_child(hwnd, "BUTTON", "Add Cue", BS_PUSHBUTTON | WS_TABSTOP, 704, 80, 74, 26, kAddCue);
  create_child(hwnd, "BUTTON", "Remove Cue", BS_PUSHBUTTON | WS_TABSTOP, 784, 80, 88, 26, kRemoveCue);

  create_child(hwnd, WC_LISTVIEWA, "", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS |
               WS_BORDER | WS_TABSTOP, 16, 116, 1040, 452, kRows);
  HWND table = control(hwnd, kRows);
  ListView_SetExtendedListViewStyleEx(table, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
  const auto& columns = core::cue_manager_columns();
  for (std::size_t index = 0; index < columns.size(); ++index) {
    LVCOLUMNA column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = const_cast<char*>(columns[index].label.c_str());
    column.cx = columns[index].width;
    ListView_InsertColumnA(table, static_cast<int>(index), &column);
  }

  create_child(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, 16, 576, 1040, 24, kDetails);

  create_child(hwnd, "STATIC", "Cue ID", 0, 16, 610, 52, 18, -1);
  create_child(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 70, 606, 118, 24, kEditCueId);
  create_child(hwnd, "STATIC", "Character", 0, 198, 610, 68, 18, -1);
  create_child(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 270, 606, 220, 24, kEditCharacter);
  create_child(hwnd, "STATIC", "Type", 0, 502, 610, 38, 18, -1);
  create_child(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 544, 606, 140, 160, kEditType);
  create_child(hwnd, "STATIC", "Status", 0, 696, 610, 44, 18, -1);
  create_child(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 744, 606, 180, 180, kEditStatus);

  create_child(hwnd, "STATIC", "Dialogue", 0, 16, 642, 58, 18, -1);
  create_child(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 78, 638, 412, 24, kEditDialogue);
  create_child(hwnd, "STATIC", "Notes", 0, 502, 642, 44, 18, -1);
  create_child(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 550, 638, 374, 24, kEditNotes);

  create_child(hwnd, "STATIC", "Start", 0, 16, 674, 42, 18, -1);
  create_child(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 62, 670, 126, 24, kEditStart);
  create_child(hwnd, "STATIC", "End", 0, 198, 674, 34, 18, -1);
  create_child(hwnd, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 236, 670, 126, 24, kEditEnd);
  create_child(hwnd, "BUTTON", "Apply Edit", BS_PUSHBUTTON | WS_TABSTOP, 376, 668, 94, 28, kApplyEdit);
  create_child(hwnd, "BUTTON", "Previous", BS_PUSHBUTTON | WS_TABSTOP, 708, 668, 84, 28, kPrevious);
  create_child(hwnd, "BUTTON", "Next", BS_PUSHBUTTON | WS_TABSTOP, 798, 668, 74, 28, kNext);
  create_child(hwnd, "BUTTON", "Close", BS_DEFPUSHBUTTON | WS_TABSTOP, 976, 668, 80, 28, kClose);

  SendDlgItemMessageA(hwnd, kStatus, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Any"));
  for (const auto& status : core::cue_manager_status_choices()) {
    SendDlgItemMessageA(hwnd, kStatus, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(status.c_str()));
    SendDlgItemMessageA(hwnd, kEditStatus, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(status.c_str()));
  }
  for (const auto& type : core::cue_manager_type_choices())
    SendDlgItemMessageA(hwnd, kEditType, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(type.c_str()));
  SetDlgItemTextA(hwnd, kStatus, "Any");
}

void apply_table_filters(HWND hwnd)
{
  if (!g_controller) return;
  std::string status = control_text(hwnd, kStatus);
  if (status == "Any") status.clear();
  if (g_controller->set_filters(control_text(hwnd, kSearch),
                                control_text(hwnd, kCharacter), status))
    refresh_rows(hwnd);
  else
    show_error(hwnd);
}

void add_cue(HWND hwnd)
{
  if (!g_controller) return;
  core::CueManagerAddOptions cue;
  cue.cue_key = control_text(hwnd, kEditCueId);
  cue.character = control_text(hwnd, kEditCharacter);
  cue.dialogue = control_text(hwnd, kEditDialogue);
  cue.notes = control_text(hwnd, kEditNotes);
  cue.cue_type = control_text(hwnd, kEditType);
  cue.start_time = control_text(hwnd, kEditStart);
  cue.end_time = control_text(hwnd, kEditEnd);
  cue.status = control_text(hwnd, kEditStatus);
  std::string error;
  if (g_controller->add_cue(cue, error)) refresh_rows(hwnd);
  else if (!error.empty()) MessageBoxA(hwnd, error.c_str(), "ReaADR Cue Manager", MB_OK | MB_ICONERROR);
}

void apply_edit(HWND hwnd)
{
  if (!g_controller) return;
  core::CueManagerEditOptions edit;
  edit.new_cue_key = control_text(hwnd, kEditCueId);
  edit.new_character = control_text(hwnd, kEditCharacter);
  edit.dialogue = control_text(hwnd, kEditDialogue);
  edit.dialogue_set = true;
  edit.notes = control_text(hwnd, kEditNotes);
  edit.notes_set = true;
  edit.cue_type = control_text(hwnd, kEditType);
  edit.start_time = control_text(hwnd, kEditStart);
  edit.end_time = control_text(hwnd, kEditEnd);
  edit.status = control_text(hwnd, kEditStatus);
  std::string error;
  if (g_controller->edit_selected(edit, error)) refresh_rows(hwnd);
  else if (!error.empty()) MessageBoxA(hwnd, error.c_str(), "ReaADR Cue Manager", MB_OK | MB_ICONERROR);
}

LRESULT CALLBACK cue_manager_wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  switch (message) {
    case WM_CREATE:
      create_window_controls(hwnd);
      refresh_rows(hwnd);
      return 0;
    case WM_NOTIFY: {
      if (!g_controller || g_refreshing_rows) break;
      const auto* header = reinterpret_cast<const NMHDR*>(lparam);
      if (!header || header->idFrom != kRows) break;
      const auto* change = reinterpret_cast<const NMLISTVIEW*>(lparam);
      if (header->code == LVN_COLUMNCLICK) {
        const auto& columns = core::cue_manager_columns();
        if (change->iSubItem >= 0 && static_cast<std::size_t>(change->iSubItem) < columns.size() &&
            g_controller->sort_by(columns[change->iSubItem].key))
          refresh_rows(hwnd);
        return 0;
      }
      if (header->code == LVN_ITEMCHANGED && (change->uNewState & LVIS_SELECTED)) {
        if (g_controller->select_index(change->iItem)) update_details(hwnd);
        else { refresh_rows(hwnd); show_error(hwnd); }
        return 0;
      }
      if (header->code == NM_DBLCLK) {
        const auto* row = g_controller->selected_row();
        if (row) {
          std::string error;
          if (!g_controller->navigate_to_id(row->cue_key, error) && !error.empty())
            MessageBoxA(hwnd, error.c_str(), "ReaADR Cue Manager", MB_OK | MB_ICONERROR);
          refresh_rows(hwnd);
        }
        return 0;
      }
      break;
    }
    case WM_COMMAND: {
      const int command = LOWORD(wparam);
      if (command == kModuleCues) { reload_and_refresh(hwnd); return 0; }
      if (command == kModuleImport) {
        if (g_controller) {
          g_controller->trigger_import({}, false, "all", {});
          reload_and_refresh(hwnd);
        }
        return 0;
      }
      if (command == kModuleSession) { show_session_tools(hwnd); return 0; }
      if (command == kModuleReports) { show_reports_tools(hwnd); return 0; }
      if (command == kModuleOverlay) { show_overlay_tools(hwnd); return 0; }
      if (command == kModulePreferences) {
        if (g_controller) {
          g_controller->trigger_action("preferences");
          reload_and_refresh(hwnd);
        }
        return 0;
      }
      if (command == kModuleHelp) { show_help(hwnd); return 0; }
      if (command == kApplyFilter) { apply_table_filters(hwnd); return 0; }
      if (command == kResetFilter) {
        SetDlgItemTextA(hwnd, kSearch, "");
        SetDlgItemTextA(hwnd, kCharacter, "");
        SetDlgItemTextA(hwnd, kStatus, "Any");
        if (g_controller && g_controller->set_filters({}, {}, {})) refresh_rows(hwnd);
        return 0;
      }
      if (command == kRecord || command == kCueInfo || command == kCharacterFilter ||
          command == kRefresh || command == kSync) {
        if (!g_controller) return 0;
        const char* action = command == kRecord ? "record_cue" :
          command == kCueInfo ? "cue_info" :
          command == kCharacterFilter ? "character_filter" :
          command == kRefresh ? "refresh_session" : "sync_regions";
        g_controller->trigger_action(action);
        reload_and_refresh(hwnd);
        return 0;
      }
      if (command == kNewCue) { populate_new_cue(hwnd); return 0; }
      if (command == kAddCue) { add_cue(hwnd); return 0; }
      if (command == kRemoveCue) {
        const auto* row = g_controller ? g_controller->selected_row() : nullptr;
        if (!row) {
          MessageBoxA(hwnd, "Select a cue before removing it.", "ReaADR Cue Manager", MB_OK | MB_ICONINFORMATION);
          return 0;
        }
        const std::string prompt = "Remove cue " + row->cue_key + " (" + row->character + ")?";
        if (MessageBoxA(hwnd, prompt.c_str(), "ReaADR Cue Manager", MB_YESNO | MB_ICONWARNING) == IDYES) {
          std::string error;
          if (g_controller->remove_selected(error)) refresh_rows(hwnd);
          else if (!error.empty()) MessageBoxA(hwnd, error.c_str(), "ReaADR Cue Manager", MB_OK | MB_ICONERROR);
        }
        return 0;
      }
      if (command == kApplyEdit) { apply_edit(hwnd); return 0; }
      if (command == kPrevious || command == kNext) {
        if (g_controller) {
          const bool moved = command == kNext ? g_controller->navigate_next() : g_controller->navigate_previous();
          if (moved) refresh_rows(hwnd); else show_error(hwnd);
        }
        return 0;
      }
      if (command == kClose || command == IDCANCEL) {
        close_manager_window(hwnd);
        return 0;
      }
      break;
    }
    case WM_CLOSE:
      close_manager_window(hwnd);
      return 0;
  }
  return DefWindowProcA(hwnd, message, wparam, lparam);
}

} // namespace

bool show_cue_manager(CueManagerController& controller, double frame_rate)
{
  INITCOMMONCONTROLSEX common_controls{sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx(&common_controls);

  HINSTANCE instance = GetModuleHandle(nullptr);
  WNDCLASSA window_class{};
  window_class.lpfnWndProc = cue_manager_wnd_proc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  window_class.lpszClassName = kWindowClass;
  if (!RegisterClassA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return false;

  g_controller = &controller;
  g_frame_rate = frame_rate;
  HWND owner = GetForegroundWindow();
  HWND window = CreateWindowExA(WS_EX_DLGMODALFRAME, kWindowClass,
    kWindowTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME,
    CW_USEDEFAULT, CW_USEDEFAULT, kMinWindowWidth, kMinWindowHeight,
    owner, nullptr, instance, nullptr);
  if (!window) {
    g_controller = nullptr;
    return false;
  }

  restore_window_layout(window);
  const bool docked = reaper::inspect_window_dock_state(window).docked();
  if (owner && !docked) EnableWindow(owner, FALSE);
  ShowWindow(window, SW_SHOW);
  if (docked) reaper::activate_docked_window(window);
  UpdateWindow(window);
  MSG message{};
  while (IsWindow(window) && GetMessage(&message, nullptr, 0, 0) > 0) {
    if (!IsDialogMessage(window, &message)) {
      TranslateMessage(&message);
      DispatchMessage(&message);
    }
  }
  if (owner && !docked) {
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
  }
  g_controller = nullptr;
  return true;
}

} // namespace reaadr::ui

#endif
