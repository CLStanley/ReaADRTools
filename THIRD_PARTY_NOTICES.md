# Third-Party Notices

ReaADR Tools builds against the REAPER extension SDK and WDL. Their source is
not included in release packages. Exact revisions are recorded in
`extension/dependencies.lock`.

## REAPER extension SDK

- Project: Cockos REAPER SDK
- Source: https://github.com/justinfrankel/reaper-sdk
- Use: native REAPER extension interfaces and headers

Refer to the upstream repository for the license and notices applicable to the
SDK revision used by the build.

The SDK license included with the pinned revision permits use, alteration, and
redistribution for any purpose subject to origin, altered-source, and notice
requirements. ReaADR Tools does not claim authorship of the SDK.

## WDL

- Project: Cockos WDL
- Source: https://github.com/justinfrankel/WDL
- Use: native extension platform support

Refer to the upstream repository for the licenses and notices applicable to the
WDL revision used by the build.

## REAPER and ReaImGui

REAPER is a product of Cockos Incorporated. ReaADR Tools is an independent
project and is not affiliated with or endorsed by Cockos.

ReaADR Tools can use ReaImGui when it is installed in REAPER. ReaImGui is not
bundled in the ReaADR Tools release payload.

## Project assets

The ReaADR Tools logo files in `assets/` are project runtime assets. Their
ownership and redistribution status must be confirmed before a public beta.
