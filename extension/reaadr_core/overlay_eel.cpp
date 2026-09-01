#include "overlay_eel.hpp"

#include "domain_utils.hpp"
#include "lane_assignment.hpp"
#include "overlay_refresh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

namespace reaadr::core {
namespace {

std::string field(const Fields& fields, const char* key)
{
  const auto found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

std::string trim_ascii(const std::string& value)
{
  const auto is_space = [](unsigned char byte) {
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' ||
      byte == '\f' || byte == '\v';
  };
  std::size_t first = 0;
  while (first < value.size() && is_space(static_cast<unsigned char>(value[first]))) ++first;
  std::size_t last = value.size();
  while (last > first && is_space(static_cast<unsigned char>(value[last - 1]))) --last;
  return value.substr(first, last - first);
}

std::string lowercase_ascii(std::string value)
{
  for (char& byte : value) {
    if (byte >= 'A' && byte <= 'Z') byte = static_cast<char>(byte - 'A' + 'a');
  }
  return value;
}

std::string normalize_header(const std::string& value)
{
  std::string normalized;
  bool separator = false;
  for (unsigned char byte : lowercase_ascii(trim_ascii(value))) {
    const bool word = (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9');
    if (word) {
      if (separator && !normalized.empty()) normalized.push_back('_');
      normalized.push_back(static_cast<char>(byte));
      separator = false;
    } else {
      separator = true;
    }
  }
  return normalized;
}

bool parse_number(const std::string& value, double& output)
{
  const std::string cleaned = trim_ascii(value);
  char* end = nullptr;
  output = std::strtod(cleaned.c_str(), &end);
  return !cleaned.empty() && end && end != cleaned.c_str() && *end == '\0' &&
    std::isfinite(output);
}

std::string cue_key(const Fields& cue)
{
  const std::string id = sanitize_token(field(cue, "id"));
  return id.empty() ? field(cue, "source_line") : id;
}

std::string eel_quote(const std::string& value)
{
  std::string escaped;
  escaped.reserve(value.size() + 2);
  escaped.push_back('"');
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char byte = value[index];
    if (byte == '\\' || byte == '"') {
      escaped.push_back('\\');
      escaped.push_back(byte);
    } else if (byte == '\r') {
      escaped.push_back(' ');
      if (index + 1 < value.size() && value[index + 1] == '\n') ++index;
    } else if (byte == '\n') {
      escaped.push_back(' ');
    } else {
      escaped.push_back(byte);
    }
  }
  escaped.push_back('"');
  return escaped;
}

std::string fixed_number(double value, int precision)
{
  std::ostringstream output;
  output << std::fixed << std::setprecision(precision) << value;
  return output.str();
}

bool has_balanced_outer_parentheses(const std::string& value)
{
  if (value.size() < 2 || value.front() != '(' || value.back() != ')') return false;
  int depth = 0;
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] == '(') ++depth;
    else if (value[index] == ')') --depth;
    if (depth <= 0 && index + 1 < value.size()) return false;
  }
  return depth == 0;
}

std::vector<std::string> split_list(const std::string& value)
{
  std::vector<std::string> result;
  std::size_t begin = 0;
  while (begin <= value.size()) {
    const std::size_t comma = value.find(',', begin);
    const std::string item = trim_ascii(value.substr(begin, comma - begin));
    if (!item.empty()) result.push_back(item);
    if (comma == std::string::npos) break;
    begin = comma + 1;
  }
  return result;
}

std::string metadata_value(const Fields& cue, const std::string& key)
{
  const Fields metadata = deserialize_metadata(field(cue, "metadata"));
  const std::string wanted = normalize_header(key);
  for (const auto& [metadata_key, value] : metadata) {
    if (normalize_header(metadata_key) == wanted) return value;
  }
  return {};
}

std::vector<std::string> wrap_overlay_text(const std::string& value, std::size_t max_chars)
{
  std::vector<std::string> result;
  const std::string text = trim_ascii(value);
  max_chars = (std::max)(std::size_t{16}, max_chars);
  std::istringstream words(text);
  std::string word;
  std::string current;
  while (words >> word) {
    const std::string candidate = current.empty() ? word : current + " " + word;
    if (candidate.size() <= max_chars) {
      current = candidate;
      continue;
    }
    if (!current.empty()) result.push_back(current);
    while (word.size() > max_chars) {
      result.push_back(word.substr(0, max_chars));
      word.erase(0, max_chars);
    }
    current = word;
  }
  if (!current.empty()) result.push_back(current);
  return result;
}

std::array<int, 3> status_rgb(const std::string& status)
{
  const std::string normalized = lowercase_ascii(normalize_status(status));
  if (normalized == "in progress") return {255, 165, 0};
  if (normalized == "recorded") return {0, 174, 239};
  if (normalized == "needs review") return {170, 100, 220};
  if (normalized == "approved") return {0, 210, 90};
  if (normalized == "needs retake") return {255, 0, 0};
  return {120, 120, 120};
}

struct CueView {
  const Fields* cue = nullptr;
  std::size_t input_index = 0;
  std::string key;
  double start_time = 0.0;
  double end_time = 0.0;
};

} // namespace

OverlayEelResult build_overlay_eel(const SessionModel& model, const OverlayEelOptions& options)
{
  OverlayEelResult result;
  result.selected_cue_key = !options.selected_region_cue_key.empty()
    ? options.selected_region_cue_key
    : (!options.selected_item_cue_key.empty()
        ? options.selected_item_cue_key : options.active_overlay_cue_key);

  const double frame_rate = std::isfinite(options.frame_rate) && options.frame_rate > 0.0
    ? options.frame_rate : 24.0;
  const double rounded_fps = (std::max)(1.0, std::floor(frame_rate + 0.5));
  const int display_fps = static_cast<int>((std::min)(
    rounded_fps, static_cast<double>((std::numeric_limits<int>::max)())));

  std::vector<int> lanes(model.cues.size(), 1);
  if (options.character_filter.hide_inactive_regions && options.character_filter.enabled()) {
    const LaneAssignmentResult assigned =
      assign_character_lanes(model.cues, (std::max)(0.0, options.settings.preroll_seconds));
    if (!assigned) {
      result.error = assigned.error;
      return result;
    }
    lanes = assigned.lanes;
  }

  std::vector<CueView> cues;
  cues.reserve(model.cues.size());
  for (std::size_t index = 0; index < model.cues.size(); ++index) {
    const Fields& cue = model.cues[index];
    CueView view;
    view.cue = &cue;
    view.input_index = index;
    view.key = cue_key(cue);
    if (view.key.empty()) {
      result.error = "An overlay cue has no stable cue key.";
      return result;
    }
    if (!parse_number(field(cue, "start_time"), view.start_time) ||
        !parse_number(field(cue, "end_time"), view.end_time) ||
        view.end_time < view.start_time) {
      result.error = "Cue " + view.key + " has invalid overlay timing.";
      return result;
    }
    std::string character = trim_ascii(field(cue, "character"));
    if (character.empty()) character = "Unassigned";
    if (options.character_filter.hide_inactive_regions && options.character_filter.enabled() &&
        !character_lane_is_active(options.character_filter, character, lanes[index])) {
      continue;
    }
    cues.push_back(std::move(view));
  }

  std::stable_sort(cues.begin(), cues.end(), [&result](const CueView& left, const CueView& right) {
    const bool left_selected = !result.selected_cue_key.empty() && left.key == result.selected_cue_key;
    const bool right_selected = !result.selected_cue_key.empty() && right.key == result.selected_cue_key;
    if (left_selected != right_selected) return left_selected;
    if (left.start_time != right.start_time) return left.start_time < right.start_time;
    const std::string left_id = field(*left.cue, "id");
    const std::string right_id = field(*right.cue, "id");
    if (left_id != right_id) return left_id < right_id;
    return left.input_index < right.input_index;
  });
  result.displayed_cue_count = cues.size();

  std::vector<std::string> lines = {
    kOverlayCodeMarker,
    "gfx_blit(0);",
    "w = project_w > 0 ? project_w : 1920;",
    "h = project_h > 0 ? project_h : 1080;",
    "now = project_time;",
    "font_cue = max(42, h * 0.070);",
    "font_meta = max(30, h * 0.045);",
    "font_timer = max(38, h * 0.058);",
    "font_direction = max(34, h * 0.050);",
    "font_dialogue = max(42, h * 0.060);",
    "font_status = max(24, h * 0.034);",
    "margin = max(24, w * 0.035);",
    "pad = max(12, h * 0.018);",
    "overlay_drawn = 0;",
    "active_region_count = 0;",
    "display_fps = " + std::to_string(display_fps) + ";",
  };

  const std::string text_mode = lowercase_ascii(trim_ascii(options.settings.text_color));
  const std::string base_text_rgba = text_mode == "yellow"
    ? "1, 0.93, 0.48, 1" : "1, 1, 1, 1";
  const auto append_text_draw = [&lines](
    const std::string& text_var, const std::string& font_var, const std::string& color_expr,
    const std::string& x_expr, const std::string& y_expr,
    const std::string& measure_w_var, const std::string& measure_h_var,
    bool background, const std::string& pad_x_expr, const std::string& pad_y_expr) {
    lines.push_back("gfx_setfont(" + font_var + ", \"Arial\");");
    lines.push_back("gfx_str_measure(" + text_var + ", " + measure_w_var + ", " + measure_h_var + ");");
    if (background) {
      lines.push_back("gfx_set(0, 0, 0, 0.62); gfx_fillrect(max(0, (" + x_expr + ") - (" +
        pad_x_expr + ")), (" + y_expr + ") - (" + pad_y_expr + "), min(w, " + measure_w_var +
        " + ((" + pad_x_expr + ") * 2)), " + measure_h_var + " + ((" + pad_y_expr + ") * 2));");
    }
    lines.push_back("gfx_set(" + color_expr + "); gfx_str_draw(" + text_var + ", " +
      x_expr + ", " + y_expr + ");");
  };

  for (const CueView& view : cues) {
    lines.push_back("now >= " + fixed_number(view.start_time, 6) + " && now <= " +
      fixed_number(view.end_time, 6) + " ? (active_region_count += 1);");
  }

  if (options.settings.show_project_timer) {
    lines.push_back("project_total_frames = floor(max(0, now) * display_fps + 0.5); project_frames = project_total_frames - floor(project_total_frames / display_fps) * display_fps; project_total_seconds = floor(project_total_frames / display_fps); project_seconds = project_total_seconds - floor(project_total_seconds / 60) * 60; project_total_minutes = floor(project_total_seconds / 60); project_minutes = project_total_minutes - floor(project_total_minutes / 60) * 60; project_hours = floor(project_total_minutes / 60); sprintf(#project_tc, \"%02d:%02d:%02d:%02d\", project_hours, project_minutes, project_seconds, project_frames);");
    lines.push_back("#timeline_label = \"Timeline SMPTE\";");
    append_text_draw("#timeline_label", "font_status", base_text_rgba,
      "(w - tlw) * 0.5", "margin * 0.40", "tlw", "tlh",
      options.settings.bg_project_timer, "pad * 0.9", "pad * 0.45");
    append_text_draw("#project_tc", "font_timer", base_text_rgba,
      "(w - ptw) * 0.5", "margin * 0.40 + font_status", "ptw", "pth",
      options.settings.bg_project_timer, "pad * 1.1", "pad * 0.55");
  }

  for (const CueView& view : cues) {
    const Fields& cue = *view.cue;
    const double preroll = (std::max)(0.0, options.settings.preroll_seconds);
    const double item_start = (std::max)(0.0, view.start_time - preroll);
    std::string note_text = trim_ascii(!trim_ascii(field(cue, "notes")).empty()
      ? field(cue, "notes") : field(cue, "direction"));
    if (!note_text.empty() && !has_balanced_outer_parentheses(note_text)) {
      note_text = "(" + note_text + ")";
    }
    const std::string cue_status = normalize_status(field(cue, "status"));
    const std::array<int, 3> rgb = status_rgb(cue_status);

    std::string condition = "overlay_drawn == 0 && now >= " + fixed_number(item_start, 6) +
      " && now <= " + fixed_number(view.end_time, 6);
    if (result.selected_cue_key.empty() || view.key != result.selected_cue_key) {
      condition += " && (now >= " + fixed_number(view.start_time, 6) + " || active_region_count == 0)";
    }
    lines.push_back(condition + " ? (");
    lines.push_back("overlay_drawn = 1;");
    lines.push_back("cue_start = " + fixed_number(view.start_time, 6) + "; cue_end = " +
      fixed_number(view.end_time, 6) + ";");
    lines.push_back("rel = now - cue_start; until_cue = cue_start - now;");
    lines.push_back("pre = max(0.001, " + fixed_number((std::max)(0.001, view.start_time - item_start), 6) + ");");

    if (options.settings.show_flash) {
      lines.push_back("flash = abs(rel) < 0.10 ? 1 : 0;");
      lines.push_back("flash ? (gfx_set(1, 1, 1, 0.32); gfx_fillrect(0, 0, w, h));");
    }
    if (options.settings.show_streamer) {
      lines.push_back("until_cue > 0 && until_cue <= pre ? (progress = 1 - (until_cue / pre); x = w * progress; gfx_set(0.0, 0.65, 1, 0.96); gfx_fillrect(0, h * 0.48 - 9, x, 18); gfx_set(1, 1, 1, 0.96); gfx_fillrect(x - 4, h * 0.34, 8, h * 0.28));");
    }
    if (options.settings.show_visual_cue) {
      lines.push_back("abs(rel) < 0.05 ? (gfx_set(1, 1, 1, 0.98); gfx_fillrect(w * 0.5 - 8, h * 0.30, 16, h * 0.40); gfx_fillrect(w * 0.35, h * 0.50 - 8, w * 0.30, 16));");
      lines.push_back("until_cue > 0 && until_cue <= pre ? (pulse = 1 - until_cue / pre; cue_x = w * 0.5; cue_y = h * 0.50; cue_size = max(34, h * 0.065) + pulse * max(28, h * 0.05); gfx_set(1, 0.86, 0.15, 0.90); gfx_fillrect(cue_x - cue_size * 0.5, cue_y - 5, cue_size, 10); gfx_fillrect(cue_x - 5, cue_y - cue_size * 0.5, 10, cue_size));");
    }
    if (options.settings.show_cue_id) {
      lines.push_back("#cue_number = " + eel_quote("Cue #" + field(cue, "id")) + ";");
      append_text_draw("#cue_number", "font_cue", base_text_rgba, "margin", "margin * 0.55",
        "cuew", "cueh", options.settings.bg_cue_id, "pad * 1.2", "pad * 0.6");
    }
    if (options.settings.show_character) {
      lines.push_back("#character = " + eel_quote(field(cue, "character")) + ";");
      append_text_draw("#character", "font_meta", base_text_rgba, "margin",
        "margin * 0.55 + font_cue + pad * 0.4", "charw", "charh",
        options.settings.bg_character, "pad * 1.0", "pad * 0.45");
    }
    if (options.settings.show_cue_timecode) {
      lines.push_back("#cue_tc = " + eel_quote(format_timecode(view.start_time, display_fps)) + ";");
      lines.push_back("#cue_tc_label = \"Cue SMPTE\";");
      append_text_draw("#cue_tc_label", "font_status", base_text_rgba, "w - margin - ctlw",
        "margin * 0.50", "ctlw", "ctlh", options.settings.bg_cue_timecode,
        "pad * 0.9", "pad * 0.45");
      append_text_draw("#cue_tc", "font_timer", base_text_rgba, "w - margin - ctw",
        "margin * 0.50 + font_status", "ctw", "cth", options.settings.bg_cue_timecode,
        "pad * 1.1", "pad * 0.55");
    } else {
      lines.push_back("cth = 0;");
    }

    const std::string media_time = metadata_value(cue, "Media Time");
    if (!media_time.empty()) {
      lines.push_back("#media_time_label = \"Media Time\";");
      lines.push_back("#media_time = " + eel_quote(media_time) + ";");
      append_text_draw("#media_time_label", "font_status", "0.72, 0.78, 0.84, 1",
        "w - margin - mtlabelw", "margin * 0.50 + font_status + font_timer + pad * 0.70",
        "mtlabelw", "mtlabelh", options.settings.bg_metadata, "pad * 0.9", "pad * 0.45");
      append_text_draw("#media_time", "font_meta", base_text_rgba, "w - margin - mtw",
        "margin * 0.50 + font_status + font_timer + font_status + pad", "mtw", "mth",
        options.settings.bg_metadata, "pad * 1.0", "pad * 0.45");
    }

    if (options.settings.show_cue_type && !field(cue, "cue_type").empty()) {
      lines.push_back("#cue_type = " + eel_quote(field(cue, "cue_type")) + ";");
      lines.push_back(options.settings.bg_cue_type
        ? "gfx_setfont(font_status, \"Arial\"); gfx_str_measure(#cue_type, typew, typeh); type_y = margin * 0.50 + font_status + font_timer + pad * 1.65; gfx_set(0, 0, 0, 0.62); gfx_fillrect(max(0, w - margin - typew - pad * 1.8), type_y - pad * 0.30, min(w, typew + pad * 2.2), typeh + pad * 0.75); gfx_set(1, 1, 1, 1); gfx_str_draw(#cue_type, w - margin - typew - pad * 0.9, type_y);"
        : "gfx_setfont(font_status, \"Arial\"); gfx_str_measure(#cue_type, typew, typeh); type_y = margin * 0.50 + font_status + font_timer + pad * 1.65; gfx_set(0.0, 0.50, 0.95, 0.82); gfx_fillrect(w - margin - typew - pad * 1.6, type_y - pad * 0.25, typew + pad * 1.8, typeh + pad * 0.65); gfx_set(1, 1, 1, 1); gfx_str_draw(#cue_type, w - margin - typew - pad * 0.8, type_y);");
    }

    if (options.settings.show_metadata) {
      std::size_t metadata_index = 0;
      for (const std::string& key : split_list(options.settings.metadata_fields)) {
        const std::string value = metadata_value(cue, key);
        if (value.empty()) continue;
        ++metadata_index;
        if (metadata_index > 5) break;
        const std::string index = std::to_string(metadata_index);
        lines.push_back("#metadata_" + index + " = " + eel_quote(key + ": " + value) + ";");
        append_text_draw("#metadata_" + index, "font_status", base_text_rgba,
          "w - margin - mdw", "margin * 0.75 + cth + pad * " +
            fixed_number(2.0 + metadata_index * 1.25, 1),
          "mdw", "mdh", options.settings.bg_metadata, "pad * 0.9", "pad * 0.40");
      }
    }

    if (options.settings.show_status) {
      lines.push_back("#status = " + eel_quote("Status: " + cue_status) + ";");
      const std::string color = fixed_number(rgb[0] / 255.0, 3) + ", " +
        fixed_number(rgb[1] / 255.0, 3) + ", " + fixed_number(rgb[2] / 255.0, 3) + ", 1";
      append_text_draw("#status", "font_status", color, "margin",
        "margin * 0.55 + font_cue + font_meta + pad * 1.4", "statusw", "statush",
        options.settings.bg_status, "pad * 0.9", "pad * 0.40");
    }
    if (options.settings.show_direction && !note_text.empty()) {
      lines.push_back("#direction = " + eel_quote(note_text) + ";");
      append_text_draw("#direction", "font_direction", base_text_rgba, "(w - dirw) * 0.5",
        "h * 0.66", "dirw", "dirh", options.settings.bg_direction,
        "pad * 1.1", "pad * 0.50");
    }

    if (options.settings.show_dialogue && !field(cue, "line").empty()) {
      const std::vector<std::string> dialogue = wrap_overlay_text(field(cue, "line"), 52);
      lines.push_back("dialogue_base_y = h * 0.80 - ((" +
        std::to_string(dialogue.size()) + " - 1) * (font_dialogue * 0.46));");
      lines.push_back("dialogue_box_w = 0; dialogue_box_h = 0;");
      for (std::size_t index = 0; index < dialogue.size(); ++index) {
        const std::string number = std::to_string(index + 1);
        lines.push_back("#dialogue_" + number + " = " + eel_quote(dialogue[index]) + ";");
        lines.push_back("gfx_setfont(font_dialogue, \"Arial\"); gfx_str_measure(#dialogue_" + number +
          ", dlgw_" + number + ", dlgh_" + number + "); dialogue_box_w = max(dialogue_box_w, dlgw_" +
          number + "); dialogue_box_h = max(dialogue_box_h, ((" + number +
          " - 1) * (font_dialogue * 0.92)) + dlgh_" + number + ");");
      }
      if (options.settings.bg_dialogue) {
        lines.push_back("dialogue_box_x = max(0, max(margin, (w - dialogue_box_w) * 0.5) - pad * 1.5);");
        lines.push_back("dialogue_box_y = dialogue_base_y - pad;");
        lines.push_back("dialogue_box_draw_w = min(w - dialogue_box_x, dialogue_box_w + pad * 3);");
        lines.push_back("dialogue_box_draw_h = dialogue_box_h + pad * 2;");
        lines.push_back("gfx_set(0, 0, 0, 0.62); gfx_fillrect(dialogue_box_x, dialogue_box_y, dialogue_box_draw_w, dialogue_box_draw_h);");
      }
      for (std::size_t index = 0; index < dialogue.size(); ++index) {
        const std::string number = std::to_string(index + 1);
        append_text_draw("#dialogue_" + number, "font_dialogue", base_text_rgba,
          "max(margin, (w - dw) * 0.5)",
          "dialogue_base_y + ((" + number + " - 1) * (font_dialogue * 0.92))",
          "dw", "dh", false, "pad * 1.5", "pad");
      }
    }
    lines.push_back(");");
  }

  std::ostringstream code;
  for (std::size_t index = 0; index < lines.size(); ++index) {
    if (index != 0) code << '\n';
    code << lines[index];
  }
  result.video_code = code.str();
  return result;
}

} // namespace reaadr::core
