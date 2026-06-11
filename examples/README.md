# ReaADR Tools

Initial workflow:

1. Open REAPER.
2. Optional: run `scripts/ReaADR_Overlay_Settings.lua` to choose which video
   overlays should be generated.
3. Run `scripts/ReaADR_Import_Cue_Sheet.lua` as a ReaScript.
4. Select a CSV matching `docs/cue_sheet_template.csv`.

The importer creates or reuses:

- `ADR`
- `ADR Cues`
- `ADR Streamers`
- `ADR Character - <Character>` tracks
- one `[ReaADR]:id=<cue_id>` region per cue
- one `[ReaADR]:id=<cue_id>` cue start marker per cue
- one optional cue audio item per cue on `ADR Cues` when `assets/cue.wav`
  exists
- one dedicated Video Processor overlay item per cue on `ADR Video Overlays`

The script tags created tracks with REAPER track ext state and uses stable
marker/region names. Cue audio and overlay items are tagged with REAPER item
ext state. Re-running the same import updates existing ReaADR objects instead
of duplicating them.
