local Ownership = {}

function Ownership.cue_audio_item_matches(role, cue_key, target_keys)
  return role == "cue_audio" and tostring(cue_key or "") ~= "" and target_keys[tostring(cue_key)] == true
end

function Ownership.generated_track_role(role)
  return role == "source_video" or role == "cue_character" or role == "character"
end

return Ownership
