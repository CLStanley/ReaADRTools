# Native Extension Layout

The native extension is organized by dependency direction:

- `reaadr_core/` contains REAPER-independent session models, persistence,
  planning, and workflow rules.
- `reaadr_reaper/` contains REAPER adapters and host-facing render services.
- `app/` contains application services that coordinate core rules with host
  services and transactions.
- `reaadr_ui/` contains native presentation, controllers, and UI contracts. The
  `reaadr_ui/legacy/` area holds retained native shell experiments that are not part
  of the current build until their replacement is complete.
- `reaper_reaadr.cpp` is the extension entry point and host command wiring;
  platform build files remain beside it.

New code should follow the dependency flow `reaadr_core -> reaadr_reaper ->
app -> reaadr_ui/entry point`; the domain core must not include REAPER SDK headers.
