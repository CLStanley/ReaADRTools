# ReaADR Tools Beta Readiness

This checklist separates automated repository readiness from behavior that must
be verified inside REAPER. A checked build does not imply that a workflow was
tested in the host application.

## Automated candidate checks

- [ ] Lua syntax passes for every file in `scripts/` and `tests/`.
- [ ] Deterministic Lua tests pass.
- [ ] ShellCheck passes for packaging, dependency, and test shell scripts.
- [ ] Pinned REAPER SDK and WDL revisions verify successfully.
- [ ] Supported native binaries compile without warnings treated as errors by the release policy.
- [ ] Parallel and serial builds produce equivalent runtime layouts.
- [ ] Every release payload passes `validate-release-package.sh`.
- [ ] No wrong-platform native binary is present in a platform package.
- [ ] No test, SDK, Git, log, or compiler artifact is present in a release package.
- [ ] Release archives have published SHA-256 checksums.

## Repository and legal checks

- [ ] An explicit open-source license has been selected and added.
- [ ] `THIRD_PARTY_NOTICES.md` has been reviewed against upstream licenses.
- [ ] Logo ownership and redistribution rights are confirmed.
- [ ] Current-tree and history secret scans are reviewed.
- [ ] Test fixtures contain only fictional or authorized data.
- [ ] README, User Guide, architecture, build, and packaging documentation agree.

## Clean-environment installation checks

- [ ] Windows x64 install and uninstall are tested with a clean REAPER resource directory.
- [ ] Linux x64 install and uninstall are tested with a clean REAPER resource directory.
- [ ] macOS is not advertised until its native build and runtime behavior are tested.
- [ ] Updating an existing installation does not remove project data or preferences.
- [ ] Uninstall removes only ReaADR-owned program files.
- [ ] REAPER restart requirements are clear.

## Manual REAPER workflow checks

- [ ] Import CSV, TSV, TXT, and XLSX cue sheets.
- [ ] Adopt legacy project regions explicitly.
- [ ] Add, edit, remove, renumber, and navigate cues.
- [ ] Update cues from manually moved regions.
- [ ] Refresh from the Session Model and verify expected sync direction.
- [ ] Record normally, loop, stop externally, press Escape, and close the window.
- [ ] Confirm unrelated track record-arm states are restored.
- [ ] Confirm unrelated regions, tracks, media, recordings, and FX survive cleanup.
- [ ] Verify overlay profiles and character filtering.
- [ ] Inject a sync failure and verify Undo-backed rollback.
- [ ] Save, close, and reopen projects with populated and intentionally empty Session Models.

## Beta communication

- [ ] Release notes list supported platforms and known issues.
- [ ] Session Model compatibility is stated.
- [ ] Testers have installation, update, uninstall, and feedback instructions.
- [ ] Bug-report instructions warn users not to attach confidential production material unintentionally.

## Known pre-beta blockers

The repository does not yet declare an open-source license. This cleanup change
does not choose one on the project owner's behalf. A license must be selected
before public beta distribution.

macOS native build and runtime support are not currently verified and must not
be advertised as supported.
