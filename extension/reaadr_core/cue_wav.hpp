#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace reaadr::core {

struct CueWavOptions {
  double frame_rate = 24.0;
  int sample_rate = 48000;
  double beep_frequency = 1000.0;
  double amplitude = 0.36;
  double interval_seconds = 0.5;
  int beep_count = 3;
};

struct CueWavResult {
  std::vector<std::uint8_t> bytes;
  double frame_rate = 0.0;
  double beep_seconds = 0.0;
  double duration_seconds = 0.0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

// Builds the native mono 16-bit ADR countdown asset. Each beep lasts one
// project frame and the default three-beep cadence is 500 ms. The
// result is kept in memory so validation completes before an existing project
// file is replaced.
CueWavResult build_cue_wav(const CueWavOptions& options = {});

// Writes through a sibling temporary file and atomically replaces the target.
// This keeps a disk-full or interrupted write from truncating a previously
// valid project cue asset.
bool write_cue_wav_file(const std::string& path,
                        const std::vector<std::uint8_t>& bytes,
                        std::string& error);

} // namespace reaadr::core
