#include "cue_wav.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace reaadr::core {
namespace {

void append_le16(std::vector<std::uint8_t>& output, std::uint16_t value)
{
  output.push_back(static_cast<std::uint8_t>(value & 0xffU));
  output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void append_le32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
  output.push_back(static_cast<std::uint8_t>(value & 0xffU));
  output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
  output.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
  output.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
}

void append_ascii(std::vector<std::uint8_t>& output, const char* text, std::size_t length)
{
  output.insert(output.end(), text, text + length);
}

bool replace_file(const std::string& temporary_path, const std::string& target_path)
{
#ifdef _WIN32
  return MoveFileExA(temporary_path.c_str(), target_path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  return std::rename(temporary_path.c_str(), target_path.c_str()) == 0;
#endif
}

} // namespace

CueWavResult build_cue_wav(const CueWavOptions& options)
{
  CueWavResult result;
  const double frame_rate = std::isfinite(options.frame_rate) && options.frame_rate > 0.0
    ? options.frame_rate
    : 24.0;
  if (options.sample_rate <= 0 || options.beep_count <= 0 ||
      !std::isfinite(options.beep_frequency) || options.beep_frequency <= 0.0 ||
      !std::isfinite(options.amplitude) || options.amplitude < 0.0 || options.amplitude > 1.0 ||
      !std::isfinite(options.interval_seconds) || options.interval_seconds <= 0.0) {
    result.error = "Cue-WAV settings must use positive finite timing/frequency values and amplitude from 0 to 1.";
    return result;
  }
  if (static_cast<std::uint64_t>(options.sample_rate) * 2U >
      std::numeric_limits<std::uint32_t>::max()) {
    result.error = "The cue-WAV sample rate cannot be represented by the WAV header.";
    return result;
  }

  result.frame_rate = frame_rate;
  result.beep_seconds = 1.0 / frame_rate;
  result.duration_seconds = options.interval_seconds * static_cast<double>(options.beep_count);
  const double total_sample_value = std::floor(
    result.duration_seconds * static_cast<double>(options.sample_rate) + 0.5);
  if (!std::isfinite(total_sample_value) || total_sample_value < 1.0 ||
      total_sample_value > static_cast<double>(
        (std::numeric_limits<std::uint32_t>::max() - 36U) / 2U)) {
    result.error = "Cue-WAV settings produce an unsupported file size.";
    return result;
  }
  const std::uint32_t total_samples = static_cast<std::uint32_t>(total_sample_value);
  const double raw_beep_samples = (std::max)(
    1.0, std::floor(result.beep_seconds * static_cast<double>(options.sample_rate) + 0.5));
  const std::uint32_t beep_samples = static_cast<std::uint32_t>((std::min)(
    static_cast<double>(total_samples), raw_beep_samples));
  const std::uint32_t data_size = total_samples * 2U;

  result.bytes.reserve(static_cast<std::size_t>(44U) + static_cast<std::size_t>(data_size));
  append_ascii(result.bytes, "RIFF", 4);
  append_le32(result.bytes, 36U + data_size);
  append_ascii(result.bytes, "WAVEfmt ", 8);
  append_le32(result.bytes, 16U);
  append_le16(result.bytes, 1U);
  append_le16(result.bytes, 1U);
  append_le32(result.bytes, static_cast<std::uint32_t>(options.sample_rate));
  append_le32(result.bytes, static_cast<std::uint32_t>(options.sample_rate) * 2U);
  append_le16(result.bytes, 2U);
  append_le16(result.bytes, 16U);
  append_ascii(result.bytes, "data", 4);
  append_le32(result.bytes, data_size);

  constexpr double kPi = 3.141592653589793238462643383279502884;
  for (std::uint32_t sample = 0; sample < total_samples; ++sample) {
    double value = 0.0;
    for (int beep = 0; beep < options.beep_count; ++beep) {
      const std::uint64_t start = static_cast<std::uint64_t>(std::floor(
        static_cast<double>(beep) * options.interval_seconds * options.sample_rate + 0.5));
      if (sample >= start && static_cast<std::uint64_t>(sample) < start + beep_samples) {
        const double time = static_cast<double>(static_cast<std::uint64_t>(sample) - start) /
          static_cast<double>(options.sample_rate);
        value = std::sin(2.0 * kPi * options.beep_frequency * time) * options.amplitude;
        break;
      }
    }
    value = (std::max)(-1.0, (std::min)(1.0, value));
    const int signed_sample = static_cast<int>(std::floor(value * 32767.0));
    append_le16(result.bytes, static_cast<std::uint16_t>(signed_sample));
  }
  return result;
}

bool write_cue_wav_file(const std::string& path,
                        const std::vector<std::uint8_t>& bytes,
                        std::string& error)
{
  if (path.empty()) {
    error = "A destination path is required for the generated cue WAV.";
    return false;
  }
  if (bytes.empty()) {
    error = "The generated cue WAV is empty.";
    return false;
  }

  const std::string temporary_path = path + ".reaadr.tmp";
  {
    std::ofstream output(temporary_path, std::ios::binary | std::ios::trunc);
    if (!output) {
      error = "Could not open the temporary cue-WAV file for writing: " + temporary_path;
      return false;
    }
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.flush();
    if (!output) {
      output.close();
      std::remove(temporary_path.c_str());
      error = "Could not completely write the generated cue WAV: " + temporary_path;
      return false;
    }
  }

  if (!replace_file(temporary_path, path)) {
    std::remove(temporary_path.c_str());
    error = "Could not replace the project cue WAV: " + path;
    return false;
  }
  error.clear();
  return true;
}

} // namespace reaadr::core
