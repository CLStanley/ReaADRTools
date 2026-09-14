#pragma once

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>

#include <string>
#include <vector>

namespace reaadr::ui::win32 {

inline std::wstring to_wide(const std::string& value)
{
  if (value.empty()) return {};
  const int length = MultiByteToWideChar(
    CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), wide.data(), length) <= 0)
    return {};
  return wide;
}

inline std::wstring utf8_to_wide(const std::string& value)
{
  return to_wide(value);
}

inline std::string to_utf8(const std::wstring& value)
{
  if (value.empty()) return {};
  const int length = WideCharToMultiByte(
    CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
    nullptr, 0, nullptr, nullptr);
  if (length <= 0) return {};
  std::string utf8(static_cast<std::size_t>(length), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), utf8.data(), length,
                          nullptr, nullptr) <= 0)
    return {};
  return utf8;
}

inline bool set_window_text_utf8(HWND hwnd, const std::string& value)
{
  const std::wstring wide = to_wide(value);
  return SetWindowTextW(hwnd, wide.c_str()) != FALSE;
}

inline bool set_dlg_item_text_utf8(HWND hwnd, int id, const std::string& value)
{
  const std::wstring wide = to_wide(value);
  return SetDlgItemTextW(hwnd, id, wide.c_str()) != FALSE;
}

inline bool set_control_text_utf8(HWND hwnd, int id, const std::string& value)
{
  return set_dlg_item_text_utf8(hwnd, id, value);
}

inline std::string window_text_utf8(HWND hwnd)
{
  if (!hwnd) return {};
  const int length = GetWindowTextLengthW(hwnd);
  if (length <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(length) + 1, L'\0');
  const int copied = GetWindowTextW(hwnd, wide.data(), length + 1);
  wide.resize(copied > 0 ? static_cast<std::size_t>(copied) : 0);
  return to_utf8(wide);
}

inline std::string get_window_text_utf8(HWND hwnd)
{
  return window_text_utf8(hwnd);
}

inline std::string dlg_item_text_utf8(HWND hwnd, int id)
{
  return window_text_utf8(GetDlgItem(hwnd, id));
}

inline int message_box_utf8(HWND owner, const std::string& message,
                            const std::string& title, UINT type)
{
  const std::wstring wide_message = to_wide(message);
  const std::wstring wide_title = to_wide(title);
  return MessageBoxW(owner, wide_message.c_str(), wide_title.c_str(), type);
}

inline LRESULT listbox_add_utf8(HWND listbox, const std::string& value)
{
  const std::wstring wide = to_wide(value);
  return SendMessageW(listbox, LB_ADDSTRING, 0,
                      reinterpret_cast<LPARAM>(wide.c_str()));
}

inline int list_view_insert_item_w(HWND list, LVITEMW* item)
{
  return static_cast<int>(SendMessageW(
    list, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(item)));
}

inline bool list_view_set_item_text_w(HWND list, int item_index, int subitem,
                                      wchar_t* text)
{
  LVITEMW item{};
  item.iSubItem = subitem;
  item.pszText = text;
  return SendMessageW(list, LVM_SETITEMTEXTW, static_cast<WPARAM>(item_index),
                      reinterpret_cast<LPARAM>(&item)) != FALSE;
}

inline int list_view_insert_column_w(HWND list, int column_index, LVCOLUMNW* column)
{
  return static_cast<int>(SendMessageW(
    list, LVM_INSERTCOLUMNW, static_cast<WPARAM>(column_index),
    reinterpret_cast<LPARAM>(column)));
}

} // namespace reaadr::ui::win32

#ifndef ListView_InsertItemW
#define ListView_InsertItemW(hwnd, item) \
  ::reaadr::ui::win32::list_view_insert_item_w((hwnd), (item))
#endif

#ifndef ListView_SetItemTextW
#define ListView_SetItemTextW(hwnd, item, subitem, text) \
  ::reaadr::ui::win32::list_view_set_item_text_w((hwnd), (item), (subitem), (text))
#endif

#ifndef ListView_InsertColumnW
#define ListView_InsertColumnW(hwnd, index, column) \
  ::reaadr::ui::win32::list_view_insert_column_w((hwnd), (index), (column))
#endif

#endif
