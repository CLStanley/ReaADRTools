# ReaADR Tools

Initial workflow:

1. Open REAPER.
2. Open the top-level `ReaADR Tools` menu.
3. Choose `Overlay Settings` to configure which video overlays should be
   generated, choose `Import Cue Sheet` to import cues, or choose a
   marker/region action for existing projects.
4. Select a CSV matching `docs/cue_sheet_template.csv` when importing.
5. Place the picture on `ADR Source Video`. The importer installs the ReaADR
   overlay as a Video Processor FX on that track.

The individual scripts can still be run directly if preferred.

The importer creates or reuses:

- `ADR Source Video`
- one `Cue - <Character>` cue-audio track per character
- additional `Cue - <Character> 2`, `Cue - <Character> 3`, etc. tracks when
  that character has overlapping cues
- one `<Character>` recording track per character
- matching `<Character> 2`, `<Character> 3`, etc. recording tracks when that
  character has overlapping cues
- one `[ReaADR]:id=<cue_id>` region per cue
- cue regions assigned to REAPER ruler lanes named by character where supported
- one optional cue audio item per cue on its character cue track when
  `assets/cue.wav` exists
- one Video Processor FX named `ReaADR Video Overlay` on `ADR Source Video`

The script tags created tracks with REAPER track ext state and uses stable
region names. Cue audio items are tagged with REAPER item ext state.
Re-running the same import updates existing ReaADR objects instead of
duplicating them.

Generated tracks and cue regions are color-coded by character. Cue status is
preserved in metadata and color-coded in the video overlay.

Optional cue sheet columns such as `direction`, `cue_type`, and `status` are
preserved during import/export. Direction and cue type are shown in the video
overlay when present and enabled in `Overlay Settings`.

After a cue sheet has been imported once, saving `Overlay Settings` refreshes
only the `ReaADR Video Overlay` FX from cached cue data. The CSV does not need
to be selected again unless cue content or timings change.

Actions:

- `Import Cue Sheet`: imports CSV cue sheets and populates the ADR project.
- `Export Cue Sheet`: exports cached ReaADR cues or project markers/regions to
  CSV for script writing, planning, or later re-import.
- `Generate Cues from Markers/Regions`: scans existing project markers and
  regions and generates cue aids from their start points.
- `Set Cue Status`: sets the status for the cue under the edit cursor and
  refreshes the video overlay.
- `Character Filter`: toggles active characters for focused recording passes.
  Inactive ReaADR cue/dialogue tracks are muted. Cue navigation, export, and
  video overlay content are left unchanged. An optional checkbox can hide
  inactive generated cue regions in REAPER's ruler lanes.
- `Clean Generated Cue Items`: removes generated cue audio items without
  deleting user recordings or regions.
- `Overlay Settings`: configures and refreshes the video overlay.

## Export Cue Sheet

`Export Cue Sheet` is designed for users who want to spot cues inside REAPER
first, then finish the script in a spreadsheet or text workflow.

The exported CSV always includes these columns:

```text
cue_id,character,start,end,line,direction,cue_type,status,notes
```

Blank columns are allowed. A user can create only timed markers/regions in
REAPER, export the CSV, fill in dialogue/direction/type/status/notes later,
and then re-import the CSV with `Import Cue Sheet`.

When exporting from existing ReaADR cue data, the saved ReaADR cue fields are
used directly. When exporting from ordinary project markers or regions, the
marker/region name is interpreted flexibly:

- `AOI` exports as character `AOI` with blank dialogue.
- `AOI: Fight them all off?` exports as character `AOI` and dialogue
  `Fight them all off?`.
- `AOI - Fight them all off?` exports the same way.
- Empty marker/region names export with blank character and dialogue.

This keeps quick spotting lightweight: name a region with just the character if
that is all you know, or add `Character: Dialogue` when the line is already
known.

Menu integration is handled by the native extension. The bundled ReaScripts can
still be run directly from the REAPER Action List if preferred.
