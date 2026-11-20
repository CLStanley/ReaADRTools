# Legacy Lua implementation (archive only)

This directory preserves the original 36 ReaADR Tools Lua scripts recovered
from the `Production` branch during the C++ migration.

These files are **not** part of the active extension, REAPER action mappings,
build source lists, or release packaging. Do not register them automatically
or add them back to the C++ extension's startup or menu code.

The archive retains the original `scripts/` relative layout to make it
possible to inspect historical behavior or manually recover a feature.
The scripts are preserved as historical source, not maintained or tested
against the current C++ model.

To compare an archived script with its historical source, use the corresponding
`scripts/<filename>.lua` path on the `Production` branch.

Only remove or reorganize this archive with explicit approval.