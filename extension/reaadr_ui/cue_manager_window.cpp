#include "cue_manager_window.hpp"
#include "reaadr_ui.hpp"
#include "cue_manager_ui_contract.hpp"
#include "reaadr_core/domain_utils.hpp"
#include "reaadr_reaper/window_docking.hpp"
#ifdef _WIN32
// Common Controls depends on the core Win32 declarations. Keep both includes
// local to the only translation unit that uses the native list-view API.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#endif
#include <reaper_plugin.h>
#ifndef _WIN32
#include <swell/swell-dlggen.h>
#endif
#include <algorithm>
#include <string>
#include <cstring>
#include <cctype>
#include <map>
#include <set>

namespace reaadr::ui {
namespace {
constexpr int kDialog = 48001;
constexpr int kColumnsDialog = 48107, kColumns = 48108, kColumnsReset = 48109;
constexpr int kColumnValue = 48110, kColumnDecrease = 48120, kColumnIncrease = 48130;
constexpr int kRows = 48002;
constexpr int kCueHeader = 48058;
constexpr int kDetails = 48003;
constexpr int kPrevious = 48004;
constexpr int kNext = 48005;
constexpr int kCharacterFilter = 48006;
constexpr int kApplyFilter = 48007;
constexpr int kEditDialogue = 48008, kEditNotes = 48009, kEditType = 48010;
constexpr int kEditStart = 48011, kEditEnd = 48012, kEditStatus = 48013, kApplyEdit = 48014;
constexpr int kEditCueId = 48015, kEditCharacter = 48016;
constexpr int kSearchFilter = 48017, kStatusFilter = 48018, kResetFilter = 48019;
constexpr int kJumpCueId = 48020, kJump = 48021;
constexpr int kNewCue = 48022, kAddCue = 48023, kRemoveCue = 48024;
constexpr int kTabImport = 48025, kTabCues = 48026, kTabSession = 48027;
constexpr int kTabReports = 48028, kTabOverlay = 48029, kTabPreferences = 48030, kTabHelp = 48031;
constexpr int kImportBrowse = 48032, kImportRun = 48033, kImportPreview = 48035;
constexpr int kImportMapping = 48034;
constexpr int kImportMode = 48036, kImportCharacters = 48037;
constexpr int kSessionValidate = 48039, kSessionRefresh = 48040, kSessionSync = 48041;
constexpr int kSessionClear = 48051, kSessionFilter = 48052, kSessionGenerate = 48140,
              kSessionDetectDialogue = 48141, kSessionAdoptLegacy = 48145;
constexpr int kOverlayRefresh = 48042, kPreferencesOpen = 48043;
constexpr int kOverlayActor = 48059, kOverlayEngineer = 48060,
              kOverlayStudio = 48061, kOverlayMinimal = 48062;
constexpr int kOverlayEnabled = 48063, kOverlayCueId = 48064, kOverlayCharacter = 48065,
              kOverlayDialogue = 48066, kOverlayStatus = 48067;
constexpr int kOverlayCueTimecode = 48068, kOverlayProjectTimer = 48069,
              kOverlayVisualCue = 48070, kOverlayDirection = 48071,
              kOverlayCueType = 48072, kOverlayStreamer = 48073,
              kOverlayFlash = 48074, kOverlayMetadata = 48075;
constexpr int kOverlayBgCueId = 48076, kOverlayBgCharacter = 48077,
              kOverlayBgCueTimecode = 48078, kOverlayBgProjectTimer = 48079,
              kOverlayBgDialogue = 48080, kOverlayBgDirection = 48081,
              kOverlayBgCueType = 48082, kOverlayBgStatus = 48083,
              kOverlayBgMetadata = 48084;
constexpr int kOverlayTextWhite = 48085, kOverlayTextYellow = 48086;
constexpr int kOverlayMetadataFields = 48087, kOverlayPreroll = 48088, kOverlaySaveSettings = 48089,
              kOverlayPrerollEachLoop = 48142;
constexpr int kPreferencesReload = 48054;
constexpr int kQuickAction1 = 48090, kQuickAction2 = 48091,
              kQuickAction3 = 48092, kQuickAction4 = 48093,
              kQuickActionSave = 48094;
constexpr int kPrefRememberLayout = 48095, kPrefHoverPreview = 48096,
              kPrefTooltips = 48097, kPrefNavigationWrap = 48098,
              kPrefAutoDock = 48099, kPrefSave = 48100;
constexpr int kHelpImport = 48044, kHelpCues = 48045, kHelpOverlay = 48046,
              kHelpReports = 48047, kHelpQuickActions = 48048;
constexpr int kHelpSearch = 48101, kHelpSearchRun = 48102;
constexpr int kReportsSummary = 48049, kReportsExport = 48050;
constexpr int kReportsTiming = 48103;
constexpr int kReportsMetadata = 48104;
constexpr int kCueRefresh = 48105, kCueSync = 48106, kCueRecord = 48143, kCueInfo = 48144;
CueManagerController* g_controller = nullptr;
// List notifications during rebuilding must not overwrite the canonical selection.
bool g_refreshing_rows = false;
double g_frame_rate = 24.0;
HWND g_window = nullptr;
constexpr const char* kDockIdentifier = "reaadr.cue_manager";
constexpr const char* kWindowTitle = "ReaADR Tools - Cue Manager";

std::string display_timecode(const std::string& value)
{
  const auto parsed = core::parse_timecode(value, g_frame_rate);
  return parsed ? core::format_timecode(*parsed.seconds, g_frame_rate) : value;
}

std::string cue_editor_text(HWND hwnd, int id)
{
  const HWND control = GetDlgItem(hwnd, id);
  return read_control_text(GetWindowTextLength(control), [control](char* text, int capacity) {
    GetWindowText(control, text, capacity);
  });
}

void populate_add_editor(HWND hwnd)
{
  if (!g_controller) return;
  const core::CueManagerAddOptions cue = g_controller->default_add_options();
  SetDlgItemText(hwnd, kEditCueId, cue.cue_key.c_str());
  SetDlgItemText(hwnd, kEditCharacter, cue.character.c_str());
  SetDlgItemText(hwnd, kEditDialogue, cue.dialogue.c_str());
  SetDlgItemText(hwnd, kEditNotes, cue.notes.c_str());
  SetDlgItemText(hwnd, kEditType, cue.cue_type.c_str());
  SetDlgItemText(hwnd, kEditStart, cue.start_time.c_str());
  SetDlgItemText(hwnd, kEditEnd, cue.end_time.c_str());
  SetDlgItemText(hwnd, kEditStatus, cue.status.c_str());
  SetDlgItemText(hwnd, kDetails, "New cue: edit the fields below, then choose Add Cue");
}

void update_editor(HWND hwnd)
{
  const core::CueManagerRow* row = g_controller ? g_controller->selected_row() : nullptr;
  SetDlgItemText(hwnd, kEditCueId, row ? row->cue_key.c_str() : "");
  SetDlgItemText(hwnd, kEditCharacter, row ? row->character.c_str() : "");
  SetDlgItemText(hwnd, kEditDialogue, row ? row->dialogue.c_str() : "");
  SetDlgItemText(hwnd, kEditNotes, row ? row->notes.c_str() : "");
  SetDlgItemText(hwnd, kEditType, row ? row->cue_type.c_str() : "");
  SetDlgItemText(hwnd, kEditStart, row ? row->start_time.c_str() : "");
  SetDlgItemText(hwnd, kEditEnd, row ? row->end_time.c_str() : "");
  SetDlgItemText(hwnd, kEditStatus, row ? row->status.c_str() : "");
}

void update_details(HWND hwnd, int index)
{
  if (!g_controller || index < 0 || static_cast<std::size_t>(index) >= g_controller->view().cues.rows.size()) return;
  const auto& row = g_controller->view().cues.rows[static_cast<std::size_t>(index)];
  const std::string details = "Selected: " + row.cue_key + " | " + row.character +
    " | " + row.status + " | " + row.dialogue;
  SetDlgItemText(hwnd, kDetails, details.c_str());
  update_editor(hwnd);
}

void refresh_rows(HWND hwnd)
{
  if (!g_controller) return;
  g_refreshing_rows = true;
  const HWND table = GetDlgItem(hwnd, kRows);
  ListView_DeleteAllItems(table);
  const auto& view = g_controller->view();
  int selected = -1;
  for (std::size_t i = 0; i < view.cues.rows.size(); ++i) {
    const auto& row = view.cues.rows[i];
    const std::string cells[] = {row.cue_key, row.character, display_timecode(row.start_time),
      display_timecode(row.end_time), row.status, row.cue_type, row.dialogue, row.notes};
    LVITEM item{};
    item.mask = LVIF_TEXT;
    item.iItem = static_cast<int>(i);
    item.pszText = const_cast<char*>(cells[0].c_str());
    ListView_InsertItem(table, &item);
    for (int column = 1; column < 8; ++column)
      ListView_SetItemText(table, item.iItem, column, const_cast<char*>(cells[column].c_str()));
    if (row.selected) selected = static_cast<int>(i);
  }
  if (selected >= 0) {
    ListView_SetItemState(table, selected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(table, selected, FALSE);
    update_details(hwnd, selected);
  } else {
    SetDlgItemText(hwnd, kDetails, "Selected: (none)");
    update_editor(hwnd);
  }
  g_refreshing_rows = false;
}

void apply_tab_visibility(HWND hwnd, const std::string& tab)
{
  const bool import = tab == "import";
  const bool cues = tab == "cues";
  const int import_controls[] = {
    kImportMapping, kImportMode, kImportCharacters, kImportBrowse, kImportRun, kImportPreview,
  };
  for (const int id : import_controls)
    ShowWindow(GetDlgItem(hwnd, id), import ? SW_SHOW : SW_HIDE);

  const int cue_controls[] = {
    kRows, kPrevious, kNext, kCharacterFilter, kApplyFilter,
    kEditDialogue, kEditNotes, kEditType, kEditStart, kEditEnd, kEditStatus,
    kApplyEdit, kEditCueId, kEditCharacter, kSearchFilter, kStatusFilter,
    kResetFilter, kJumpCueId, kJump, kNewCue, kAddCue, kRemoveCue, kCueRefresh, kCueSync,
    kCueRecord, kCueInfo, kColumns,
  };
  for (const int id : cue_controls)
    ShowWindow(GetDlgItem(hwnd, id), cues ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kCueHeader), cues ? SW_SHOW : SW_HIDE);

  const int session_controls[] = {
    kSessionValidate, kSessionRefresh, kSessionSync, kSessionGenerate, kSessionDetectDialogue,
    kSessionClear, kSessionFilter, kSessionAdoptLegacy,
  };
  for (const int id : session_controls)
    ShowWindow(GetDlgItem(hwnd, id), tab == "session" ? SW_SHOW : SW_HIDE);

  ShowWindow(GetDlgItem(hwnd, kOverlayRefresh), tab == "overlay" ? SW_SHOW : SW_HIDE);
  const int overlay_profiles[] = {kOverlayActor, kOverlayEngineer, kOverlayStudio, kOverlayMinimal};
  for (const int id : overlay_profiles)
    ShowWindow(GetDlgItem(hwnd, id), tab == "overlay" ? SW_SHOW : SW_HIDE);
  const int overlay_toggles[] = {kOverlayEnabled, kOverlayCueId, kOverlayCharacter,
    kOverlayDialogue, kOverlayStatus, kOverlayCueTimecode, kOverlayProjectTimer,
    kOverlayVisualCue, kOverlayDirection, kOverlayCueType, kOverlayStreamer,
    kOverlayFlash, kOverlayMetadata, kOverlayBgCueId, kOverlayBgCharacter,
    kOverlayBgCueTimecode, kOverlayBgProjectTimer, kOverlayBgDialogue,
    kOverlayBgDirection, kOverlayBgCueType, kOverlayBgStatus, kOverlayBgMetadata};
  for (const int id : overlay_toggles)
    ShowWindow(GetDlgItem(hwnd, id), tab == "overlay" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kOverlayTextWhite), tab == "overlay" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kOverlayTextYellow), tab == "overlay" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kOverlayMetadataFields), tab == "overlay" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kOverlayPreroll), tab == "overlay" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kOverlaySaveSettings), tab == "overlay" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kOverlayPrerollEachLoop), tab == "overlay" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kPreferencesOpen), tab == "preferences" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kPreferencesReload), tab == "preferences" ? SW_SHOW : SW_HIDE);
  const int quick_controls[] = {kQuickAction1, kQuickAction2, kQuickAction3, kQuickAction4, kQuickActionSave};
  for (const int id : quick_controls)
    ShowWindow(GetDlgItem(hwnd, id), tab == "preferences" ? SW_SHOW : SW_HIDE);
  const int pref_controls[] = {kPrefRememberLayout, kPrefHoverPreview, kPrefTooltips,
                               kPrefNavigationWrap, kPrefAutoDock, kPrefSave};
  for (const int id : pref_controls)
    ShowWindow(GetDlgItem(hwnd, id), tab == "preferences" ? SW_SHOW : SW_HIDE);
  const int help_controls[] = {kHelpImport, kHelpCues, kHelpOverlay, kHelpReports, kHelpQuickActions};
  for (const int id : help_controls)
    ShowWindow(GetDlgItem(hwnd, id), tab == "help" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kHelpSearch), tab == "help" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kHelpSearchRun), tab == "help" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kReportsSummary), tab == "reports" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kReportsExport), tab == "reports" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kReportsTiming), tab == "reports" ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, kReportsMetadata), tab == "reports" ? SW_SHOW : SW_HIDE);
}

void update_tab_details(HWND hwnd)
{
  if (!g_controller) return;
  const auto& view = g_controller->view();
  std::string details = "Active tab: " + view.active_tab;
  if (view.active_tab == "session" || view.active_tab == "reports") {
    details += " | Session: " + view.session_name + " | Cues: " +
      std::to_string(view.total_cues) + " | Revision: " + view.revision;
  } else if (view.active_tab == "overlay") {
    details += " | Overlay profile: " + core::detect_overlay_profile(view.preferences.overlay);
  } else if (view.active_tab == "preferences") {
    details += " | Hover preview: " + std::string(view.preferences.hover_preview ? "on" : "off") +
      " | Tooltips: " + (view.preferences.tooltips ? "on" : "off") +
      " | Navigation wrap: " + (view.preferences.navigation_wrap ? "on" : "off") +
      " | Quick actions: ";
    for (std::size_t index = 0; index < view.preferences.quick_actions.size(); ++index) {
      if (index) details += ", ";
      details += std::to_string(index + 1) + "=" + view.preferences.quick_actions[index];
    }
  } else if (view.active_tab == "cues") {
    std::set<std::string> characters;
    for (const auto& row : view.cues.rows) if (!row.character.empty()) characters.insert(row.character);
    details += " | Session: " + view.session_name + " | Cues: " +
      std::to_string(view.total_cues) + " | Characters: " + std::to_string(characters.size());
  } else if (view.active_tab == "help") {
    details += " | Select a topic below for native guidance";
  }
  SetDlgItemText(hwnd, kDetails, details.c_str());
}

void update_overlay_controls(HWND hwnd)
{
  if (!g_controller) return;
  const auto& overlay = g_controller->view().preferences.overlay;
  const std::pair<int, bool> values[] = {
    {kOverlayEnabled, overlay.enabled}, {kOverlayCueId, overlay.show_cue_id},
    {kOverlayCharacter, overlay.show_character}, {kOverlayDialogue, overlay.show_dialogue},
    {kOverlayStatus, overlay.show_status}, {kOverlayCueTimecode, overlay.show_cue_timecode},
    {kOverlayProjectTimer, overlay.show_project_timer}, {kOverlayVisualCue, overlay.show_visual_cue},
    {kOverlayDirection, overlay.show_direction}, {kOverlayCueType, overlay.show_cue_type},
    {kOverlayStreamer, overlay.show_streamer}, {kOverlayFlash, overlay.show_flash},
    {kOverlayMetadata, overlay.show_metadata}, {kOverlayBgCueId, overlay.bg_cue_id},
    {kOverlayBgCharacter, overlay.bg_character}, {kOverlayBgCueTimecode, overlay.bg_cue_timecode},
    {kOverlayBgProjectTimer, overlay.bg_project_timer}, {kOverlayBgDialogue, overlay.bg_dialogue},
    {kOverlayBgDirection, overlay.bg_direction}, {kOverlayBgCueType, overlay.bg_cue_type},
    {kOverlayBgStatus, overlay.bg_status}, {kOverlayBgMetadata, overlay.bg_metadata},
    {kOverlayPrerollEachLoop, overlay.include_preroll_each_loop},
  };
  for (const auto& value : values) CheckDlgButton(hwnd, value.first, value.second ? BST_CHECKED : BST_UNCHECKED);
  SetDlgItemText(hwnd, kOverlayMetadataFields, overlay.metadata_fields.c_str());
  SetDlgItemText(hwnd, kOverlayPreroll, std::to_string(overlay.preroll_seconds).c_str());
}

void update_quick_action_controls(HWND hwnd)
{
  if (!g_controller) return;
  const auto& actions = g_controller->view().preferences.quick_actions;
  const int ids[] = {kQuickAction1, kQuickAction2, kQuickAction3, kQuickAction4};
  for (std::size_t index = 0; index < actions.size(); ++index)
    SetDlgItemText(hwnd, ids[index], actions[index].c_str());
}

void update_preference_controls(HWND hwnd)
{
  if (!g_controller) return;
  const auto& preferences = g_controller->view().preferences;
  CheckDlgButton(hwnd, kPrefRememberLayout, preferences.remember_layout ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwnd, kPrefHoverPreview, preferences.hover_preview ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwnd, kPrefTooltips, preferences.tooltips ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwnd, kPrefNavigationWrap, preferences.navigation_wrap ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwnd, kPrefAutoDock, preferences.cue_manager_auto_dock ? BST_CHECKED : BST_UNCHECKED);
}

const char* overlay_key_for_control(int id)
{
  switch (id) {
    case kOverlayEnabled: return "enabled"; case kOverlayCueId: return "show_cue_id";
    case kOverlayCharacter: return "show_character"; case kOverlayDialogue: return "show_dialogue";
    case kOverlayStatus: return "show_status"; case kOverlayCueTimecode: return "show_cue_timecode";
    case kOverlayProjectTimer: return "show_project_timer"; case kOverlayVisualCue: return "show_visual_cue";
    case kOverlayDirection: return "show_direction"; case kOverlayCueType: return "show_cue_type";
    case kOverlayStreamer: return "show_streamer"; case kOverlayFlash: return "show_flash";
    case kOverlayMetadata: return "show_metadata"; case kOverlayBgCueId: return "bg_cue_id";
    case kOverlayBgCharacter: return "bg_character"; case kOverlayBgCueTimecode: return "bg_cue_timecode";
    case kOverlayBgProjectTimer: return "bg_project_timer"; case kOverlayBgDialogue: return "bg_dialogue";
    case kOverlayBgDirection: return "bg_direction"; case kOverlayBgCueType: return "bg_cue_type";
    case kOverlayBgStatus: return "bg_status"; case kOverlayBgMetadata: return "bg_metadata";
    default: return nullptr;
  }
}

#ifndef _WIN32
// The child edits its parent table directly; no session or preference writes are
// needed because Lua likewise keeps these widths only for the current window.
INT_PTR columns_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  if (message == WM_INITDIALOG)
    SetWindowLong(hwnd, GWL_USERDATA, lparam);
  const HWND table = reinterpret_cast<HWND>(GetWindowLong(hwnd, GWL_USERDATA));
  if (!table) return 0;
  if (message == WM_COMMAND && (LOWORD(wparam) == IDCANCEL || LOWORD(wparam) == IDOK)) {
    EndDialog(hwnd, 0);
    return 1;
  }
  const int command = LOWORD(wparam);
  const bool decrease = command >= kColumnDecrease && command < kColumnDecrease + 8;
  const bool increase = command >= kColumnIncrease && command < kColumnIncrease + 8;
  if (message == WM_COMMAND && (decrease || increase)) {
    const int index = command - (increase ? kColumnIncrease : kColumnDecrease);
    ListView_SetColumnWidth(table, index,
      core::adjust_cue_manager_column_width(ListView_GetColumnWidth(table, index), increase));
  }
  if (message == WM_COMMAND && command == kColumnsReset) {
    const auto& columns = core::cue_manager_columns();
    for (std::size_t i = 0; i < columns.size(); ++i)
      ListView_SetColumnWidth(table, static_cast<int>(i), columns[i].width);
  }
  if (message == WM_INITDIALOG || (message == WM_COMMAND &&
      (decrease || increase || command == kColumnsReset))) {
    for (int i = 0; i < 8; ++i) {
      const auto width = std::to_string(ListView_GetColumnWidth(table, i));
      SetDlgItemText(hwnd, kColumnValue + i, width.c_str());
    }
    return 1;
  }
  return 0;
}
#endif

#ifndef _WIN32
INT_PTR cue_manager_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  if (message == WM_INITDIALOG) {
    g_window = hwnd;
    if (g_controller) {
      const auto layout = g_controller->load_window_layout();
      RECT current{};
      if (GetWindowRect(hwnd, &current)) {
        SetWindowPos(hwnd, nullptr, layout.has_position ? layout.x : current.left,
                     layout.has_position ? layout.y : current.top,
                     (std::max)(800, layout.width), (std::max)(600, layout.height),
                     SWP_NOZORDER | SWP_NOACTIVATE);
      }
      if (layout.dock >= 0)
        reaper::add_window_to_docker(hwnd, kWindowTitle, kDockIdentifier, layout.dock);
    }
    const HWND table = GetDlgItem(hwnd, kRows);
    ListView_SetExtendedListViewStyleEx(table, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    const auto& columns = core::cue_manager_columns();
    for (std::size_t i = 0; i < columns.size(); ++i) {
      LVCOLUMN column{};
      column.mask = LVCF_TEXT | LVCF_WIDTH;
      column.pszText = const_cast<char*>(columns[i].label.c_str());
      column.cx = columns[i].width;
      ListView_InsertColumn(table, static_cast<int>(i), &column);
    }
    SetDlgItemText(hwnd, kImportMode, "all");
    const char* import_modes[] = {"all", "selected", "update"};
    for (const char* mode : import_modes)
      SendDlgItemMessage(hwnd, kImportMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(mode));
    for (const auto& choice : core::cue_manager_type_choices())
      SendDlgItemMessage(hwnd, kEditType, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.c_str()));
    for (const auto& choice : core::cue_manager_status_choices())
      SendDlgItemMessage(hwnd, kEditStatus, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.c_str()));
    SendDlgItemMessage(hwnd, kStatusFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Any"));
    for (const auto& choice : core::cue_manager_status_choices())
      SendDlgItemMessage(hwnd, kStatusFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.c_str()));
    SetDlgItemText(hwnd, kStatusFilter, "Any");
    const int quick_ids[] = {kQuickAction1, kQuickAction2, kQuickAction3, kQuickAction4};
    for (const auto& choice : core::manager_quick_action_choices())
      for (const int id : quick_ids)
        SendDlgItemMessage(hwnd, id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.c_str()));
    if (g_controller) {
      const std::string mapping = g_controller->last_import_mapping();
      if (!mapping.empty()) SetDlgItemText(hwnd, kImportMapping, mapping.c_str());
      refresh_rows(hwnd);
    }
    if (g_controller) {
      apply_tab_visibility(hwnd, g_controller->view().active_tab);
      update_tab_details(hwnd);
      update_overlay_controls(hwnd);
      update_quick_action_controls(hwnd);
      update_preference_controls(hwnd);
    }
    return 1;
  }
  if (message == WM_COMMAND && (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL)) {
    if (g_controller) {
      RECT rect{};
      if (GetWindowRect(hwnd, &rect)) {
        core::WindowLayout layout;
        layout.x = rect.left; layout.y = rect.top;
        layout.width = rect.right - rect.left; layout.height = rect.bottom - rect.top;
        layout.dock = reaper::inspect_window_dock_state(hwnd).dock_index;
        layout.has_position = true;
        g_controller->save_window_layout(layout);
      }
    }
    if (reaper::inspect_window_dock_state(hwnd).docked()) reaper::remove_window_from_docker(hwnd);
    DestroyWindow(hwnd);
    return 1;
  }
  if (message == WM_CLOSE) {
    SendMessage(hwnd, WM_COMMAND, IDCANCEL, 0);
    return 1;
  }
  if (message == WM_DESTROY) {
    if (g_window == hwnd) g_window = nullptr;
    g_controller = nullptr;
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kColumns) {
    DialogBoxParam(nullptr, MAKEINTRESOURCE(kColumnsDialog), hwnd, columns_proc,
                   reinterpret_cast<LPARAM>(GetDlgItem(hwnd, kRows)));
    return 1;
  }
  if (message == WM_NOTIFY && g_controller && !g_refreshing_rows) {
    const auto* header = reinterpret_cast<const NMHDR*>(lparam);
    if (header && header->idFrom == kRows) {
      const auto* change = reinterpret_cast<const NMLISTVIEW*>(lparam);
      if (header->code == LVN_COLUMNCLICK) {
        const auto& columns = core::cue_manager_columns();
        if (change->iSubItem >= 0 && static_cast<std::size_t>(change->iSubItem) < columns.size() &&
            g_controller->sort_by(columns[change->iSubItem].key)) refresh_rows(hwnd);
      } else if (header->code == LVN_ITEMCHANGED &&
                 (change->uNewState & LVIS_SELECTED)) {
        // SWELL selection notifications omit uChanged; uNewState identifies selection.
        if (g_controller->select_index(change->iItem)) {
          update_details(hwnd, change->iItem);
        } else {
          // Undo the visual highlight as well as the persisted selection; the
          // refresh guard suppresses recursive notifications while rebuilding.
          const auto error = g_controller->view().error;
          refresh_rows(hwnd);
          if (!error.empty()) MessageBox(hwnd, error.c_str(), "ReaADR Cue Manager", 0);
        }
      } else if (header->code == NM_DBLCLK) {
        const auto* row = g_controller->selected_row();
        if (row) {
          const std::string key = row->cue_key;
          std::string error;
          if (!g_controller->navigate_to_id(key, error) && !error.empty())
            MessageBox(hwnd, error.c_str(), "ReaADR Cue Manager", 0);
          refresh_rows(hwnd);
        }
      }
    }
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kApplyFilter) {
    char query[256] = {}, character[256] = {}, status[128] = {};
    GetDlgItemText(hwnd, kSearchFilter, query, sizeof(query));
    GetDlgItemText(hwnd, kCharacterFilter, character, sizeof(character));
    GetDlgItemText(hwnd, kStatusFilter, status, sizeof(status));
    if (std::strcmp(status, "Any") == 0) status[0] = '\0';
    if (g_controller && g_controller->set_filters(query, character, status)) refresh_rows(hwnd);
    return 1;
  }
  if (message == WM_COMMAND) {
    const int command = LOWORD(wparam);
    const char* tab = command == kTabImport ? "import" : command == kTabCues ? "cues" :
      command == kTabSession ? "session" : command == kTabReports ? "reports" :
      command == kTabOverlay ? "overlay" : command == kTabPreferences ? "preferences" :
      command == kTabHelp ? "help" : nullptr;
    if (tab && g_controller && g_controller->set_tab(tab)) {
      const std::string title = "ReaADR Manager - " + g_controller->view().active_tab;
      SetWindowText(hwnd, title.c_str());
      apply_tab_visibility(hwnd, g_controller->view().active_tab);
      update_tab_details(hwnd);
      update_overlay_controls(hwnd);
      update_quick_action_controls(hwnd);
      update_preference_controls(hwnd);
      return 1;
    }
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kImportBrowse) {
    if (g_controller) {
      char mapping[2048] = {};
      char mode[64] = {}, characters[512] = {};
      GetDlgItemText(hwnd, kImportMapping, mapping, sizeof(mapping));
      GetDlgItemText(hwnd, kImportMode, mode, sizeof(mode));
      GetDlgItemText(hwnd, kImportCharacters, characters, sizeof(characters));
      g_controller->trigger_import(mapping, true, mode, characters);
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kImportRun) {
    if (g_controller) {
      char mapping[2048] = {};
      char mode[64] = {}, characters[512] = {};
      GetDlgItemText(hwnd, kImportMapping, mapping, sizeof(mapping));
      GetDlgItemText(hwnd, kImportMode, mode, sizeof(mode));
      GetDlgItemText(hwnd, kImportCharacters, characters, sizeof(characters));
      g_controller->trigger_import(mapping, false, mode, characters);
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kImportPreview) {
    if (g_controller) {
      char mapping[2048] = {};
      char mode[64] = {}, characters[512] = {};
      GetDlgItemText(hwnd, kImportMapping, mapping, sizeof(mapping));
      GetDlgItemText(hwnd, kImportMode, mode, sizeof(mode));
      GetDlgItemText(hwnd, kImportCharacters, characters, sizeof(characters));
      g_controller->trigger_import(mapping, true, mode, characters);
    }
    return 1;
  }
  if (message == WM_COMMAND &&
      (LOWORD(wparam) == kSessionValidate || LOWORD(wparam) == kSessionRefresh ||
       LOWORD(wparam) == kSessionSync || LOWORD(wparam) == kSessionGenerate ||
       LOWORD(wparam) == kSessionDetectDialogue || LOWORD(wparam) == kSessionAdoptLegacy ||
       LOWORD(wparam) == kCueRefresh ||
       LOWORD(wparam) == kCueSync)) {
    if (g_controller) {
      const char* action = LOWORD(wparam) == kSessionValidate ? "validate_session" :
        (LOWORD(wparam) == kSessionRefresh || LOWORD(wparam) == kCueRefresh) ? "refresh_session" :
        LOWORD(wparam) == kSessionGenerate ? "generate_cues" :
        LOWORD(wparam) == kSessionDetectDialogue ? "detect_dialogue" :
        LOWORD(wparam) == kSessionAdoptLegacy ? "adopt_legacy_project" : "sync_regions";
      g_controller->trigger_action(action);
      if (g_controller->reload()) {
        refresh_rows(hwnd);
        update_tab_details(hwnd);
      } else {
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
      }
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kSessionClear) {
    if (g_controller) {
      std::string character;
      char buffer[512] = {};
      GetDlgItemText(hwnd, kCharacterFilter, buffer, sizeof(buffer));
      character = buffer;
      std::string prompt = "Clear generated cues";
      if (!character.empty()) prompt += " for " + character;
      prompt += "?\n\nThis rebuilds the native generated cue artifacts for the selected scope.";
      if (MessageBox(hwnd, prompt.c_str(), "ReaADR Cue Manager", MB_YESNO | MB_ICONWARNING) == IDYES) {
        g_controller->trigger_action("clear_character_cues");
        if (!g_controller->view().error.empty())
          MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
        else if (g_controller->reload()) { refresh_rows(hwnd); update_tab_details(hwnd); }
      }
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kSessionFilter) {
    if (g_controller) {
      g_controller->trigger_action("character_filter");
      if (!g_controller->view().error.empty())
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
      else if (g_controller->reload()) { refresh_rows(hwnd); update_tab_details(hwnd); }
    }
    return 1;
  }
  if (message == WM_COMMAND &&
      (LOWORD(wparam) == kOverlayRefresh || LOWORD(wparam) == kPreferencesOpen)) {
    if (g_controller) {
      g_controller->trigger_action(LOWORD(wparam) == kOverlayRefresh ? "refresh_overlay" : "preferences");
      if (!g_controller->view().error.empty())
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
      else if (g_controller->reload()) {
        update_tab_details(hwnd);
        update_overlay_controls(hwnd);
        update_quick_action_controls(hwnd);
        update_preference_controls(hwnd);
      }
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) >= kOverlayActor && LOWORD(wparam) <= kOverlayMinimal) {
    if (g_controller) {
      const char* profile = LOWORD(wparam) == kOverlayActor ? "actor" :
        LOWORD(wparam) == kOverlayEngineer ? "engineer" :
        LOWORD(wparam) == kOverlayStudio ? "studio" : "minimal";
      g_controller->trigger_action(std::string("overlay_profile:") + profile);
      if (!g_controller->view().error.empty())
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
      else if (g_controller->reload()) { update_tab_details(hwnd); update_overlay_controls(hwnd); }
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) >= kOverlayEnabled && LOWORD(wparam) <= kOverlayBgMetadata) {
    if (g_controller) {
      if (const char* key = overlay_key_for_control(LOWORD(wparam))) {
        g_controller->trigger_action(std::string("overlay_toggle:") + key);
        if (!g_controller->view().error.empty())
          MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
        else if (g_controller->reload()) { update_tab_details(hwnd); update_overlay_controls(hwnd); }
      }
    }
    return 1;
  }
  if (message == WM_COMMAND &&
      (LOWORD(wparam) == kOverlayTextWhite || LOWORD(wparam) == kOverlayTextYellow)) {
    if (g_controller) {
      g_controller->trigger_action(std::string("overlay_text_color:") +
        (LOWORD(wparam) == kOverlayTextYellow ? "yellow" : "white"));
      if (!g_controller->view().error.empty())
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
      else if (g_controller->reload()) { update_tab_details(hwnd); update_overlay_controls(hwnd); }
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kOverlayPrerollEachLoop) {
    if (g_controller) {
      g_controller->trigger_action("overlay_toggle:include_preroll_each_loop");
      if (!g_controller->view().error.empty())
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
      else if (g_controller->reload()) { update_tab_details(hwnd); update_overlay_controls(hwnd); }
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kOverlaySaveSettings) {
    if (g_controller) {
      char metadata[512] = {}, preroll[64] = {};
      GetDlgItemText(hwnd, kOverlayMetadataFields, metadata, sizeof(metadata));
      GetDlgItemText(hwnd, kOverlayPreroll, preroll, sizeof(preroll));
      g_controller->trigger_action(std::string("overlay_settings:") + metadata + "|" + preroll);
      if (!g_controller->view().error.empty())
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
      else if (g_controller->reload()) { update_tab_details(hwnd); update_overlay_controls(hwnd); }
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kPreferencesReload) {
    if (g_controller && g_controller->reload()) update_tab_details(hwnd);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kQuickActionSave) {
    if (g_controller) {
      char values[4][64] = {};
      const int ids[] = {kQuickAction1, kQuickAction2, kQuickAction3, kQuickAction4};
      std::string action = "quick_actions:";
      for (int index = 0; index < 4; ++index) {
        GetDlgItemText(hwnd, ids[index], values[index], sizeof(values[index]));
        if (index) action += ',';
        action += values[index];
      }
      g_controller->trigger_action(action);
      if (!g_controller->view().error.empty())
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
      else if (g_controller->reload()) { update_tab_details(hwnd); update_quick_action_controls(hwnd); }
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kPrefSave) {
    if (g_controller) {
      const int ids[] = {kPrefRememberLayout, kPrefHoverPreview, kPrefTooltips,
                         kPrefNavigationWrap, kPrefAutoDock};
      const char* keys[] = {"remember_layout", "hover_preview", "tooltips",
                            "navigation_wrap", "cue_manager_auto_dock"};
      std::string action = "preference_toggles:";
      for (int index = 0; index < 5; ++index) {
        if (index) action += ',';
        action += keys[index];
        action += '=';
        action += IsDlgButtonChecked(hwnd, ids[index]) == BST_CHECKED ? "1" : "0";
      }
      g_controller->trigger_action(action);
      if (!g_controller->view().error.empty())
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
      else if (g_controller->reload()) { update_tab_details(hwnd); update_preference_controls(hwnd); }
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) >= kHelpImport && LOWORD(wparam) <= kHelpQuickActions) {
    const char* title = "ReaADR Manager Help";
    const char* text = LOWORD(wparam) == kHelpImport ?
      "Import loads CSV, TSV, TAB, TXT, or XLSX cue sheets. Preview headers first, then use optional key=column mappings. Modes are all, selected, or update." :
      LOWORD(wparam) == kHelpCues ?
      "Cue Management filters and edits the canonical ADR session. Select a row to edit dialogue, timing, status, or type; changes rebuild generated artifacts transactionally." :
      LOWORD(wparam) == kHelpOverlay ?
      "Video Overlay rebuilds overlay effects from the canonical session model. Refresh after changing cues or overlay preferences." :
      LOWORD(wparam) == kHelpReports ?
      "Reports can export cue and recording information for review or spreadsheet workflows. Export remains available from the established ReaADR action surface." :
      "Quick Actions are exposed through the ReaADR action and menu surface. Use Session Tools for validation, refresh, and region synchronization.";
    MessageBox(hwnd, text, title, 0);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kHelpSearchRun) {
    char query[256] = {};
    GetDlgItemText(hwnd, kHelpSearch, query, sizeof(query));
    std::string value(query);
    for (char& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const char* result = value.find("import") != std::string::npos ?
      "Import Help: use CSV, TSV, TAB, TXT, or XLSX files; preview headers and configure mappings." :
      value.find("cue") != std::string::npos ?
      "Cue Help: browse, filter, edit, navigate, and refresh canonical session cues." :
      value.find("overlay") != std::string::npos ?
      "Overlay Help: choose a profile, toggle elements, edit metadata/preroll, and refresh." :
      value.find("report") != std::string::npos || value.find("export") != std::string::npos ?
      "Reports Help: review the session summary or export the canonical cue model to CSV." :
      value.find("quick") != std::string::npos || value.find("preference") != std::string::npos ?
      "Preferences Help: configure quick actions and persisted Manager UI toggles." :
      "Search topics: import, cues, overlay, reports, quick actions, preferences.";
    MessageBox(hwnd, result, "ReaADR Manager Help", 0);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kReportsSummary) {
    if (g_controller) {
      const auto& view = g_controller->view();
      std::string summary = "Session: " + view.session_name +
        "\nRevision: " + view.revision +
        "\nTotal cues: " + std::to_string(view.total_cues) +
        "\nVisible cues: " + std::to_string(view.cues.rows.size());
      std::map<std::string, int> characters, statuses;
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
      MessageBox(hwnd, summary.c_str(), "ReaADR Session Summary", 0);
    }
    return 1;
  }
  if (message == WM_COMMAND &&
      (LOWORD(wparam) == kReportsExport || LOWORD(wparam) == kReportsTiming ||
       LOWORD(wparam) == kReportsMetadata)) {
    if (g_controller) {
      const char* action = LOWORD(wparam) == kReportsExport ? "export_cue_sheet" :
        LOWORD(wparam) == kReportsTiming ? "export_timing_report" : "export_session_metadata";
      g_controller->trigger_action(action);
      if (!g_controller->view().error.empty())
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
    }
    return 1;
  }
  if (message == WM_COMMAND && (LOWORD(wparam) == kCueRecord || LOWORD(wparam) == kCueInfo)) {
    if (g_controller) {
      g_controller->trigger_action(LOWORD(wparam) == kCueRecord ? "record_cue" : "cue_info");
      if (!g_controller->view().error.empty())
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
      else if (g_controller->reload()) refresh_rows(hwnd);
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kResetFilter) {
    SetDlgItemText(hwnd, kSearchFilter, "");
    SetDlgItemText(hwnd, kCharacterFilter, "");
    SetDlgItemText(hwnd, kStatusFilter, "Any");
    if (g_controller && g_controller->set_filters({}, {}, {})) refresh_rows(hwnd);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kJump) {
    char cue_id[128] = {};
    GetDlgItemText(hwnd, kJumpCueId, cue_id, sizeof(cue_id));
    std::string error;
    if (g_controller && g_controller->navigate_to_id(cue_id, error)) {
    SetDlgItemText(hwnd, kSearchFilter, "");
    SetDlgItemText(hwnd, kCharacterFilter, "");
    SetDlgItemText(hwnd, kStatusFilter, "Any");
      refresh_rows(hwnd);
    }
    else if (!error.empty()) MessageBox(nullptr, error.c_str(), "ReaADR Cue Manager", 0);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kNewCue) {
    populate_add_editor(hwnd);
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kAddCue) {
    if (g_controller) {
      const auto cue_id = cue_editor_text(hwnd, kEditCueId);
      const auto character = cue_editor_text(hwnd, kEditCharacter);
      const auto dialogue = cue_editor_text(hwnd, kEditDialogue);
      const auto notes = cue_editor_text(hwnd, kEditNotes);
      const auto type = cue_editor_text(hwnd, kEditType);
      const auto start = cue_editor_text(hwnd, kEditStart);
      const auto end = cue_editor_text(hwnd, kEditEnd);
      const auto status = cue_editor_text(hwnd, kEditStatus);
      core::CueManagerAddOptions cue;
      cue.cue_key = cue_id; cue.character = character; cue.dialogue = dialogue;
      cue.notes = notes; cue.cue_type = type; cue.status = status;
      cue.start_time = start; cue.end_time = end;
      std::string error;
      if (g_controller->add_cue(cue, error)) refresh_rows(hwnd);
      else if (!error.empty()) MessageBox(nullptr, error.c_str(), "ReaADR Cue Manager", 0);
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kRemoveCue) {
    const core::CueManagerRow* row = g_controller ? g_controller->selected_row() : nullptr;
    if (!row) {
      MessageBox(nullptr, "Select a cue before removing it.", "ReaADR Cue Manager", 0);
      return 1;
    }
    const std::string prompt = "Remove cue " + row->cue_key + " (" + row->character +
      ")? Remaining cues will be renumbered and cue regions/audio will be rebuilt.";
    if (MessageBox(hwnd, prompt.c_str(), "ReaADR Cue Manager", MB_YESNO | MB_ICONWARNING) == IDYES) {
      std::string error;
      if (g_controller->remove_selected(error)) refresh_rows(hwnd);
      else if (!error.empty()) MessageBox(nullptr, error.c_str(), "ReaADR Cue Manager", 0);
    }
    return 1;
  }
  if (message == WM_COMMAND && LOWORD(wparam) == kApplyEdit) {
    if (g_controller) {
      const auto cue_id = cue_editor_text(hwnd, kEditCueId);
      const auto character = cue_editor_text(hwnd, kEditCharacter);
      const auto dialogue = cue_editor_text(hwnd, kEditDialogue);
      const auto notes = cue_editor_text(hwnd, kEditNotes);
      const auto type = cue_editor_text(hwnd, kEditType);
      const auto start = cue_editor_text(hwnd, kEditStart);
      const auto end = cue_editor_text(hwnd, kEditEnd);
      const auto status = cue_editor_text(hwnd, kEditStatus);
      core::CueManagerEditOptions edit;
      edit.new_cue_key = cue_id; edit.new_character = character;
      edit.dialogue = dialogue; edit.dialogue_set = true;
      edit.notes = notes; edit.notes_set = true;
      edit.cue_type = type; edit.status = status;
      edit.start_time = start; edit.end_time = end;
      std::string error;
      if (g_controller->edit_selected(edit, error)) refresh_rows(hwnd);
      else if (!error.empty()) MessageBox(nullptr, error.c_str(), "ReaADR Cue Manager", 0);
    }
    return 1;
  }
  if (message == WM_COMMAND && (LOWORD(wparam) == kPrevious || LOWORD(wparam) == kNext)) {
    if (g_controller) {
      const bool moved = LOWORD(wparam) == kNext ? g_controller->navigate_next() : g_controller->navigate_previous();
      if (moved) refresh_rows(hwnd);
      else if (!g_controller->view().error.empty())
        MessageBox(hwnd, g_controller->view().error.c_str(), "ReaADR Cue Manager", 0);
    }
    return 1;
  }
  return 0;
}

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2(kDialog, SWELL_DLG_WS_FLIPPED,
  "ReaADR Tools - Cue Manager", 1180, 820)
BEGIN
  LTEXT "ReaADR Cue Manager", -1, 16, 12, 300, 16
  PUSHBUTTON "Import", kTabImport, 320, 10, 72, 22
  PUSHBUTTON "Cues", kTabCues, 396, 10, 72, 22
  PUSHBUTTON "Session", kTabSession, 472, 10, 72, 22
  PUSHBUTTON "Reports", kTabReports, 548, 10, 72, 22
  PUSHBUTTON "Overlay", kTabOverlay, 624, 10, 72, 22
  PUSHBUTTON "Preferences", kTabPreferences, 700, 10, 92, 22
  PUSHBUTTON "Help", kTabHelp, 796, 10, 72, 22
  LTEXT "Cue sheet import uses the native transactional parser and renderer.", -1, 16, 70, 620, 16
  LTEXT "Mapping (optional)", -1, 16, 92, 110, 16
  EDITTEXT kImportMapping, 126, 90, 500, 20, ES_AUTOHSCROLL
  LTEXT "Mode (all/selected/update)", -1, 16, 118, 150, 16
  COMBOBOX kImportMode, 126, 116, 120, 80, CBS_DROPDOWNLIST | WS_VSCROLL
  LTEXT "Characters (; separated)", -1, 260, 118, 150, 16
  EDITTEXT kImportCharacters, 414, 116, 300, 20, ES_AUTOHSCROLL
  PUSHBUTTON "Preview Cue Sheet", kImportBrowse, 650, 66, 130, 24
  PUSHBUTTON "Import", kImportRun, 786, 66, 80, 24
  PUSHBUTTON "Preview Headers", kImportPreview, 872, 66, 120, 24
  PUSHBUTTON "Check Session", kSessionValidate, 16, 150, 120, 24
  PUSHBUTTON "Refresh Session", kSessionRefresh, 142, 150, 120, 24
  PUSHBUTTON "Update From Regions", kSessionSync, 268, 150, 150, 24
  PUSHBUTTON "Generate From Markers/Regions", kSessionGenerate, 428, 150, 190, 24
  PUSHBUTTON "Adopt Existing Regions", kSessionAdoptLegacy, 16, 214, 180, 24
  PUSHBUTTON "Detect Dialogue", kSessionDetectDialogue, 628, 150, 130, 24
  PUSHBUTTON "Clear Character Cues", kSessionClear, 768, 150, 150, 24
  PUSHBUTTON "Character Filter", kSessionFilter, 928, 150, 120, 24
  PUSHBUTTON "Refresh Video Overlay", kOverlayRefresh, 16, 150, 160, 24
  PUSHBUTTON "Actor", kOverlayActor, 184, 150, 80, 24
  PUSHBUTTON "Engineer", kOverlayEngineer, 272, 150, 90, 24
  PUSHBUTTON "Studio", kOverlayStudio, 370, 150, 80, 24
  PUSHBUTTON "Minimal", kOverlayMinimal, 458, 150, 80, 24
  CHECKBOX "Enable video overlay", kOverlayEnabled, 16, 190, 180, 20
  CHECKBOX "Cue ID", kOverlayCueId, 16, 216, 120, 20
  CHECKBOX "Character", kOverlayCharacter, 144, 216, 120, 20
  CHECKBOX "Dialogue", kOverlayDialogue, 272, 216, 120, 20
  CHECKBOX "Status", kOverlayStatus, 400, 216, 100, 20
  CHECKBOX "Cue Timecode", kOverlayCueTimecode, 16, 242, 130, 20
  CHECKBOX "Project Timer", kOverlayProjectTimer, 154, 242, 130, 20
  CHECKBOX "Visual Cue", kOverlayVisualCue, 292, 242, 110, 20
  CHECKBOX "Direction", kOverlayDirection, 410, 242, 100, 20
  CHECKBOX "Cue Type", kOverlayCueType, 518, 242, 100, 20
  CHECKBOX "Streamer", kOverlayStreamer, 626, 242, 100, 20
  CHECKBOX "Flash", kOverlayFlash, 734, 242, 80, 20
  CHECKBOX "Metadata", kOverlayMetadata, 822, 242, 100, 20
  LTEXT "Text Backgrounds", -1, 16, 272, 160, 16
  CHECKBOX "Cue ID", kOverlayBgCueId, 16, 294, 100, 20
  CHECKBOX "Character", kOverlayBgCharacter, 124, 294, 110, 20
  CHECKBOX "Cue Timecode", kOverlayBgCueTimecode, 242, 294, 130, 20
  CHECKBOX "Project Timer", kOverlayBgProjectTimer, 380, 294, 130, 20
  CHECKBOX "Dialogue", kOverlayBgDialogue, 518, 294, 100, 20
  CHECKBOX "Direction", kOverlayBgDirection, 626, 294, 100, 20
  CHECKBOX "Cue Type", kOverlayBgCueType, 734, 294, 100, 20
  CHECKBOX "Status", kOverlayBgStatus, 842, 294, 90, 20
  CHECKBOX "Metadata", kOverlayBgMetadata, 940, 294, 100, 20
  LTEXT "Text Color", -1, 16, 326, 90, 16
  PUSHBUTTON "White", kOverlayTextWhite, 112, 322, 80, 24
  PUSHBUTTON "Yellow", kOverlayTextYellow, 200, 322, 80, 24
  LTEXT "Metadata Fields (comma separated)", -1, 16, 356, 210, 16
  EDITTEXT kOverlayMetadataFields, 232, 352, 500, 20, ES_AUTOHSCROLL
  LTEXT "Preroll (seconds)", -1, 16, 386, 110, 16
  EDITTEXT kOverlayPreroll, 132, 382, 90, 20, ES_AUTOHSCROLL
  CHECKBOX "Include pre-roll each loop", kOverlayPrerollEachLoop, 232, 380, 180, 20
  PUSHBUTTON "Save Overlay Settings", kOverlaySaveSettings, 420, 378, 170, 24
  PUSHBUTTON "Open Preferences", kPreferencesOpen, 16, 150, 140, 24
  PUSHBUTTON "Reload Preferences", kPreferencesReload, 164, 150, 150, 24
  LTEXT "Quick Action 1", -1, 16, 190, 100, 16
  COMBOBOX kQuickAction1, 122, 186, 220, 100, CBS_DROPDOWNLIST | WS_VSCROLL
  LTEXT "Quick Action 2", -1, 16, 220, 100, 16
  COMBOBOX kQuickAction2, 122, 216, 220, 100, CBS_DROPDOWNLIST | WS_VSCROLL
  LTEXT "Quick Action 3", -1, 16, 250, 100, 16
  COMBOBOX kQuickAction3, 122, 246, 220, 100, CBS_DROPDOWNLIST | WS_VSCROLL
  LTEXT "Quick Action 4", -1, 16, 280, 100, 16
  COMBOBOX kQuickAction4, 122, 276, 220, 100, CBS_DROPDOWNLIST | WS_VSCROLL
  PUSHBUTTON "Save Quick Actions", kQuickActionSave, 360, 186, 160, 24
  CHECKBOX "Remember window layout", kPrefRememberLayout, 360, 226, 220, 20
  CHECKBOX "Show cue preview on hover", kPrefHoverPreview, 360, 252, 220, 20
  CHECKBOX "Show tooltips", kPrefTooltips, 360, 278, 160, 20
  CHECKBOX "Wrap cue navigation", kPrefNavigationWrap, 360, 304, 180, 20
  CHECKBOX "Open Cue Manager docked", kPrefAutoDock, 360, 330, 220, 20
  PUSHBUTTON "Save UI Preferences", kPrefSave, 360, 362, 160, 24
  PUSHBUTTON "Import Help", kHelpImport, 16, 150, 120, 24
  PUSHBUTTON "Cue Help", kHelpCues, 142, 150, 100, 24
  PUSHBUTTON "Overlay Help", kHelpOverlay, 248, 150, 110, 24
  PUSHBUTTON "Reports Help", kHelpReports, 364, 150, 110, 24
  PUSHBUTTON "Quick Actions", kHelpQuickActions, 480, 150, 120, 24
  LTEXT "Search Help", -1, 16, 190, 90, 16
  EDITTEXT kHelpSearch, 112, 186, 360, 20, ES_AUTOHSCROLL
  PUSHBUTTON "Search", kHelpSearchRun, 480, 184, 90, 24
  PUSHBUTTON "Session Summary", kReportsSummary, 16, 150, 140, 24
  PUSHBUTTON "Export Cue Sheet CSV", kReportsExport, 164, 150, 160, 24
  PUSHBUTTON "Export Timing Report", kReportsTiming, 332, 150, 160, 24
  PUSHBUTTON "Export Session Metadata", kReportsMetadata, 500, 150, 180, 24
  LTEXT "Search", -1, 16, 42, 48, 14
  EDITTEXT kSearchFilter, 66, 40, 220, 20, ES_AUTOHSCROLL
  LTEXT "Character", -1, 294, 42, 68, 14
  EDITTEXT kCharacterFilter, 364, 40, 170, 20, ES_AUTOHSCROLL
  LTEXT "Status", -1, 542, 42, 48, 14
  COMBOBOX kStatusFilter, 592, 40, 145, 120, CBS_DROPDOWNLIST | WS_VSCROLL
  PUSHBUTTON "Apply", kApplyFilter, 745, 40, 58, 20
  PUSHBUTTON "Reset", kResetFilter, 807, 40, 58, 20
  LTEXT "Jump", -1, 878, 42, 38, 14
  EDITTEXT kJumpCueId, 918, 40, 130, 20, ES_AUTOHSCROLL
  PUSHBUTTON "Go", kJump, 1054, 40, 50, 20
  PUSHBUTTON "New Cue", kNewCue, 16, 70, 82, 20
  PUSHBUTTON "Record Current Cue", kCueRecord, 884, 70, 140, 24
  PUSHBUTTON "Cue Info", kCueInfo, 1030, 70, 90, 24
  PUSHBUTTON "Add Cue", kAddCue, 104, 70, 82, 20
  PUSHBUTTON "Remove Cue", kRemoveCue, 192, 70, 96, 20
  PUSHBUTTON "Update Cues From Regions", kCueSync, 300, 70, 180, 20
  PUSHBUTTON "Refresh Session", kCueRefresh, 486, 70, 120, 20
  PUSHBUTTON "Columns", kColumns, 618, 70, 84, 20
  EDITTEXT kDetails, 16, 696, 1010, 20, ES_AUTOHSCROLL | ES_READONLY
  LTEXT "Cue ID", -1, 16, 730, 50, 14
  EDITTEXT kEditCueId, 70, 728, 120, 20, ES_AUTOHSCROLL
  LTEXT "Character", -1, 200, 730, 68, 14
  EDITTEXT kEditCharacter, 270, 728, 260, 20, ES_AUTOHSCROLL
  LTEXT "Dialogue", -1, 16, 756, 70, 14
  EDITTEXT kEditDialogue, 90, 754, 330, 20, ES_AUTOHSCROLL
  LTEXT "Notes", -1, 430, 756, 50, 14
  EDITTEXT kEditNotes, 480, 754, 330, 20, ES_AUTOHSCROLL
  LTEXT "Type", -1, 820, 756, 40, 14
  COMBOBOX kEditType, 860, 754, 100, 80, CBS_DROPDOWNLIST | WS_VSCROLL
  LTEXT "Start", -1, 16, 782, 50, 14
  EDITTEXT kEditStart, 70, 780, 120, 20, ES_AUTOHSCROLL
  LTEXT "End", -1, 200, 782, 40, 14
  EDITTEXT kEditEnd, 245, 780, 120, 20, ES_AUTOHSCROLL
  LTEXT "Status", -1, 380, 782, 50, 14
  COMBOBOX kEditStatus, 435, 780, 180, 80, CBS_DROPDOWNLIST | WS_VSCROLL
  PUSHBUTTON "Apply Edit", kApplyEdit, 630, 778, 100, 24
  LTEXT "Cues", kCueHeader, 16, 98, 1124, 14
  CONTROL "", kRows, "SysListView32", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP | WS_BORDER, 16, 114, 1124, 578
  PUSHBUTTON "Previous", kPrevious, 740, 778, 90, 24
  PUSHBUTTON "Next", kNext, 836, 778, 90, 24
  DEFPUSHBUTTON "Close", IDCANCEL, 1050, 778, 90, 24
END
SWELL_DEFINE_DIALOG_RESOURCE_END2(kDialog)

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2(kColumnsDialog, SWELL_DLG_WS_FLIPPED,
  "ReaADR Column Widths", 340, 310)
BEGIN
  LTEXT "Column widths", -1, 16, 12, 210, 18
  LTEXT "Cue", -1, 16, 43, 120, 18
  PUSHBUTTON "-", kColumnDecrease + 0, 144, 40, 36, 22
  LTEXT "", kColumnValue + 0, 192, 43, 50, 18
  PUSHBUTTON "+", kColumnIncrease + 0, 260, 40, 36, 22
  LTEXT "Character", -1, 16, 70, 120, 18
  PUSHBUTTON "-", kColumnDecrease + 1, 144, 67, 36, 22
  LTEXT "", kColumnValue + 1, 192, 70, 50, 18
  PUSHBUTTON "+", kColumnIncrease + 1, 260, 67, 36, 22
  LTEXT "Start SMPTE", -1, 16, 97, 120, 18
  PUSHBUTTON "-", kColumnDecrease + 2, 144, 94, 36, 22
  LTEXT "", kColumnValue + 2, 192, 97, 50, 18
  PUSHBUTTON "+", kColumnIncrease + 2, 260, 94, 36, 22
  LTEXT "End SMPTE", -1, 16, 124, 120, 18
  PUSHBUTTON "-", kColumnDecrease + 3, 144, 121, 36, 22
  LTEXT "", kColumnValue + 3, 192, 124, 50, 18
  PUSHBUTTON "+", kColumnIncrease + 3, 260, 121, 36, 22
  LTEXT "Status", -1, 16, 151, 120, 18
  PUSHBUTTON "-", kColumnDecrease + 4, 144, 148, 36, 22
  LTEXT "", kColumnValue + 4, 192, 151, 50, 18
  PUSHBUTTON "+", kColumnIncrease + 4, 260, 148, 36, 22
  LTEXT "Type", -1, 16, 178, 120, 18
  PUSHBUTTON "-", kColumnDecrease + 5, 144, 175, 36, 22
  LTEXT "", kColumnValue + 5, 192, 178, 50, 18
  PUSHBUTTON "+", kColumnIncrease + 5, 260, 175, 36, 22
  LTEXT "Line", -1, 16, 205, 120, 18
  PUSHBUTTON "-", kColumnDecrease + 6, 144, 202, 36, 22
  LTEXT "", kColumnValue + 6, 192, 205, 50, 18
  PUSHBUTTON "+", kColumnIncrease + 6, 260, 202, 36, 22
  LTEXT "Notes", -1, 16, 232, 120, 18
  PUSHBUTTON "-", kColumnDecrease + 7, 144, 229, 36, 22
  LTEXT "", kColumnValue + 7, 192, 232, 50, 18
  PUSHBUTTON "+", kColumnIncrease + 7, 260, 229, 36, 22
  PUSHBUTTON "Reset Columns", kColumnsReset, 16, 270, 124, 24
  DEFPUSHBUTTON "Close", IDCANCEL, 236, 270, 76, 24
END
SWELL_DEFINE_DIALOG_RESOURCE_END2(kColumnsDialog)
#endif
}

bool show_cue_manager(CueManagerController& controller, double frame_rate)
{
#ifndef _WIN32
  g_frame_rate = frame_rate;
  if (g_window && IsWindow(g_window)) {
    ShowWindow(g_window, SW_SHOW);
    reaper::activate_docked_window(g_window);
    SetForegroundWindow(g_window);
    return true;
  }
  g_controller = &controller;
  g_window = CreateDialogParam(nullptr, MAKEINTRESOURCE(kDialog), nullptr, cue_manager_proc, 0);
  if (!g_window) {
    g_controller = nullptr;
    return false;
  }
  ShowWindow(g_window, SW_SHOW);
  if (reaper::inspect_window_dock_state(g_window).docked())
    reaper::activate_docked_window(g_window);
  SetForegroundWindow(g_window);
  return true;
#else
  (void)controller;
  (void)frame_rate;
  return false;
#endif
}
} // namespace reaadr::ui
