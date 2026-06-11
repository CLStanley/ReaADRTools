# ReaADR Tools

Initial workflow:

1. Open REAPER.
2. Run `scripts/ReaADR_Menu.lua` as a ReaScript.
3. Choose `Overlay Settings` to configure which video overlays should be
   generated, or choose `Import Cue Sheet` to import cues.
4. Select a CSV matching `docs/cue_sheet_template.csv` when importing.
5. Place the picture on `ADR Source Video`. The importer installs the ReaADR
   overlay as a Video Processor FX on that track.

The individual scripts can still be run directly if preferred.

The importer creates or reuses:

- `ADR Cues`
- `ADR Source Video`
- one `<Character>` track per character
- one `[ReaADR]:id=<cue_id>` region per cue
- one optional cue audio item per cue on `ADR Cues` when `assets/cue.wav`
  exists
- one Video Processor FX named `ReaADR Video Overlay` on `ADR Source Video`

The script tags created tracks with REAPER track ext state and uses stable
region names. Cue audio items are tagged with REAPER item ext state.
Re-running the same import updates existing ReaADR objects instead of
duplicating them.

Generated tracks are color-coded by role, and character tracks and cue regions
share a deterministic color per character.

Optional cue sheet columns such as `direction` and `cue_type` are shown in the
video overlay when present and enabled in `Overlay Settings`.

After a cue sheet has been imported once, saving `Overlay Settings` refreshes
only the `ReaADR Video Overlay` FX from cached cue data. The CSV does not need
to be selected again unless cue content or timings change.

Menu integration:

1. In REAPER, open `Actions > Show action list`.
2. Add `scripts/ReaADR_Menu.lua` as a ReaScript if it is not already listed.
3. Open `Options > Customize menus/toolbars`.
4. Choose the target menu, such as `Main file` or `Main extensions`.
5. Add the action `Custom: ReaADR_Menu.lua` and name it `ReaADR`.

That menu entry opens the ReaADR launcher with `Import Cue Sheet` and
`Overlay Settings`.
