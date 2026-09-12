#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_AddMediaItemToTrack
#define REAPERAPI_WANT_AddProjectMarker2
#define REAPERAPI_WANT_AddTakeToMediaItem
#define REAPERAPI_WANT_ColorFromNative
#define REAPERAPI_WANT_ColorToNative
#define REAPERAPI_WANT_CountProjectMarkers
#define REAPERAPI_WANT_CountSelectedMediaItems
#define REAPERAPI_WANT_CountTrackMediaItems
#define REAPERAPI_WANT_CountTracks
#define REAPERAPI_WANT_CreateTakeAudioAccessor
#define REAPERAPI_WANT_DeleteProjectMarker
#define REAPERAPI_WANT_DeleteTrackMediaItem
#define REAPERAPI_WANT_DestroyAudioAccessor
#define REAPERAPI_WANT_EnumProjectMarkers3
#define REAPERAPI_WANT_GetActiveTake
#define REAPERAPI_WANT_GetAudioAccessorEndTime
#define REAPERAPI_WANT_GetAudioAccessorSamples
#define REAPERAPI_WANT_GetAudioAccessorStartTime
#define REAPERAPI_WANT_GetCursorPosition
#define REAPERAPI_WANT_GetMediaItemInfo_Value
#define REAPERAPI_WANT_GetMediaSourceLength
#define REAPERAPI_WANT_GetMediaTrackInfo_Value
#define REAPERAPI_WANT_GetPlayPosition
#define REAPERAPI_WANT_GetPlayState
#define REAPERAPI_WANT_GetProjectPathEx
#define REAPERAPI_WANT_GetRegionOrMarker
#define REAPERAPI_WANT_GetRegionOrMarkerInfo_Value
#define REAPERAPI_WANT_GetResourcePath
#define REAPERAPI_WANT_GetSelectedMediaItem
#define REAPERAPI_WANT_GetSet_LoopTimeRange2
#define REAPERAPI_WANT_GetSetMediaItemInfo_String
#define REAPERAPI_WANT_GetSetMediaItemTakeInfo
#define REAPERAPI_WANT_GetSetMediaItemTakeInfo_String
#define REAPERAPI_WANT_GetSetMediaTrackInfo_String
#define REAPERAPI_WANT_GetSetProjectInfo
#define REAPERAPI_WANT_GetSetProjectInfo_String
#define REAPERAPI_WANT_GetTrack
#define REAPERAPI_WANT_GetTrackMediaItem
#define REAPERAPI_WANT_InsertTrackAtIndex
#define REAPERAPI_WANT_Main_OnCommand
#define REAPERAPI_WANT_MoveMediaItemToTrack
#define REAPERAPI_WANT_PCM_Source_CreateFromFile
#define REAPERAPI_WANT_PCM_Source_Destroy
#define REAPERAPI_WANT_PreventUIRefresh
#define REAPERAPI_WANT_SetEditCurPos
#define REAPERAPI_WANT_SetMediaItemInfo_Value
#define REAPERAPI_WANT_SetMediaTrackInfo_Value
#define REAPERAPI_WANT_SetProjectMarker4
#define REAPERAPI_WANT_SetRegionOrMarkerInfo_Value
#define REAPERAPI_WANT_TimeMap_curFrameRate
#define REAPERAPI_WANT_TrackFX_AddByName
#define REAPERAPI_WANT_TrackFX_Delete
#define REAPERAPI_WANT_TrackFX_GetCount
#define REAPERAPI_WANT_TrackFX_GetEnabled
#define REAPERAPI_WANT_TrackFX_GetNamedConfigParm
#define REAPERAPI_WANT_TrackFX_SetEnabled
#define REAPERAPI_WANT_TrackFX_SetNamedConfigParm
#define REAPERAPI_WANT_TrackList_AdjustWindows
#define REAPERAPI_WANT_Undo_BeginBlock2
#define REAPERAPI_WANT_Undo_CanUndo2
#define REAPERAPI_WANT_Undo_DoUndo2
#define REAPERAPI_WANT_Undo_EndBlock2
#define REAPERAPI_WANT_UpdateArrange
#define REAPERAPI_WANT_ValidatePtr2

#include "native_host_services.hpp"

#include <array>
#include <cmath>
#include <ctime>

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>

namespace reaadr::reaper {
namespace {

MediaTrack* overlay_get_track(ReaProject* project, int index)
{
  return GetTrack ? GetTrack(project, index) : nullptr;
}

bool validate_track(ReaProject* project, MediaTrack* track)
{
  return ValidatePtr2 && ValidatePtr2(project, track, "MediaTrack*");
}

bool get_set_track_string(MediaTrack* track, const char* parameter,
                          char* value, bool set_value)
{
  return GetSetMediaTrackInfo_String &&
    GetSetMediaTrackInfo_String(track, parameter, value, set_value);
}

bool set_track_value(MediaTrack* track, const char* parameter, double value)
{
  if (!SetMediaTrackInfo_Value) return false;
  SetMediaTrackInfo_Value(track, parameter, value);
  return true;
}

bool get_loop_time_range(double* start, double* end)
{
  if (!GetSet_LoopTimeRange2 || !start || !end) return false;
  GetSet_LoopTimeRange2(nullptr, false, true, start, end, false);
  return std::isfinite(*start) && std::isfinite(*end);
}

bool set_loop_time_range(double start, double end)
{
  if (!GetSet_LoopTimeRange2 || !std::isfinite(start) || !std::isfinite(end))
    return false;
  GetSet_LoopTimeRange2(nullptr, true, true, &start, &end, false);
  return true;
}

bool set_edit_cursor_position(double position, bool move_view, bool seek_play)
{
  if (!SetEditCurPos || !std::isfinite(position)) return false;
  SetEditCurPos(position, move_view, seek_play);
  return true;
}

bool run_main_command(int command)
{
  if (!Main_OnCommand || command <= 0) return false;
  Main_OnCommand(command, 0);
  return true;
}

} // namespace

TrackRegionApi native_track_region_api()
{
  return {
    CountTracks, GetTrack, InsertTrackAtIndex, GetSetMediaTrackInfo_String,
    GetMediaTrackInfo_Value, SetMediaTrackInfo_Value, CountProjectMarkers,
    EnumProjectMarkers3, SetProjectMarker4, AddProjectMarker2,
    DeleteProjectMarker, ColorToNative, ColorFromNative,
    TrackList_AdjustWindows, UpdateArrange,
  };
}

RulerLaneApi native_ruler_lane_api()
{
  return {
    GetSetProjectInfo, GetSetProjectInfo_String, CountProjectMarkers,
    EnumProjectMarkers3, GetRegionOrMarker, GetRegionOrMarkerInfo_Value,
    SetRegionOrMarkerInfo_Value, ColorToNative, ColorFromNative,
  };
}

CueAudioApi native_cue_audio_api()
{
  return {
    CountTracks, GetTrack, GetSetMediaTrackInfo_String, CountTrackMediaItems,
    GetTrackMediaItem, GetSetMediaItemInfo_String, GetMediaItemInfo_Value,
    SetMediaItemInfo_Value, AddMediaItemToTrack, DeleteTrackMediaItem,
    MoveMediaItemToTrack, GetActiveTake, AddTakeToMediaItem,
    GetSetMediaItemTakeInfo_String, GetSetMediaItemTakeInfo,
    PCM_Source_CreateFromFile, GetMediaSourceLength, PCM_Source_Destroy,
  };
}

TransactionApi native_transaction_api()
{
  return {
    Undo_BeginBlock2, Undo_EndBlock2, Undo_CanUndo2, Undo_DoUndo2,
    PreventUIRefresh,
  };
}

MarkerSnapshotApi native_marker_snapshot_api()
{
  return {CountProjectMarkers, EnumProjectMarkers3};
}

DialogueDetectionApi native_dialogue_detection_api()
{
  return {
    CountSelectedMediaItems,
    GetSelectedMediaItem,
    GetActiveTake,
    CreateTakeAudioAccessor,
    DestroyAudioAccessor,
    GetAudioAccessorStartTime,
    GetAudioAccessorEndTime,
    GetAudioAccessorSamples,
    GetMediaItemInfo_Value,
  };
}

OverlayRefreshApi native_overlay_refresh_api()
{
  return {
    CountTracks,
    overlay_get_track,
    validate_track,
    get_set_track_string,
    TrackFX_GetCount,
    TrackFX_GetNamedConfigParm,
    TrackFX_GetEnabled,
    TrackFX_AddByName,
    TrackFX_Delete,
    TrackFX_SetNamedConfigParm,
    TrackFX_SetEnabled,
    TrackList_AdjustWindows,
    UpdateArrange,
  };
}

RecordArmApi native_record_arm_api()
{
  return {
    CountTracks,
    GetTrack,
    validate_track,
    GetMediaTrackInfo_Value,
    set_track_value,
  };
}

RecordingSetupApi native_recording_setup_api()
{
  return {
    CountTracks,
    GetTrack,
    validate_track,
    get_set_track_string,
  };
}

RecordingTransportApi native_recording_transport_api()
{
  return {
    get_loop_time_range,
    set_loop_time_range,
    set_edit_cursor_position,
    run_main_command,
  };
}

int native_play_state()
{
  return GetPlayState ? GetPlayState() : 0;
}

double native_play_position()
{
  const double position = GetPlayPosition ? GetPlayPosition() : 0.0;
  return std::isfinite(position) ? position : 0.0;
}

double native_cursor_position()
{
  const double position = GetCursorPosition ? GetCursorPosition() : 0.0;
  return std::isfinite(position) ? position : 0.0;
}

double native_project_frame_rate(ReaProject* project)
{
  bool drop_frame = false;
  const double frame_rate = TimeMap_curFrameRate
    ? TimeMap_curFrameRate(project, &drop_frame) : 24.0;
  return std::isfinite(frame_rate) && frame_rate > 0.0 ? frame_rate : 24.0;
}

std::string native_project_cue_audio_path(ReaProject* project)
{
  std::array<char, 4096> path = {};
  if (GetProjectPathEx)
    GetProjectPathEx(project, path.data(), static_cast<int>(path.size()));
  std::string directory = path.data();
  if (directory.empty() && GetResourcePath) {
    const char* resource_path = GetResourcePath();
    if (resource_path) directory = resource_path;
  }
  if (directory.empty()) directory = ".";
  const char last = directory.back();
  if (last != '/' && last != '\\') directory.push_back('/');
  return directory + "reaadr_cue.wav";
}

std::string native_utc_timestamp()
{
  const std::time_t now = std::time(nullptr);
  std::tm utc = {};
#ifdef _WIN32
  if (gmtime_s(&utc, &now) != 0) return {};
#else
  if (!gmtime_r(&now, &utc)) return {};
#endif
  std::array<char, 32> buffer = {};
  return std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ", &utc)
    ? std::string(buffer.data()) : std::string();
}

} // namespace reaadr::reaper
