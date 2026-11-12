# LuxVelum host (2026-11-12)

LuxVelum (fog, light and water; a fork of jealous-sound's CoAVolFog, GPL) ships as `LuxVelum64.dll` with fueryin's
"Conquest of Azeroth – Ultra-HD Mod Pack". Its exe was a CoA64 0.1.1 build with a host for that dll and no CoA Custom
races. CoA64 now hosts the same dll, so the pack's LuxVelum runs with CoA Custom.

## API

Reconstructed from the pack's `CoA64.pdb` / `CoA64.map` (types `LuxHostApi`, `LuxCallbacks`, `LuxDayNight` ... and
the functions of `LuxVelumClient.cpp.obj`); layouts in `Engine/Source/Gx/LuxVelum.h`, version 1, both tables 160 bytes.
`CoA::LuxVelum::Load` (MainFiberProc, after `CoA::Attach`) loads `<exe dir>\LuxVelum64.dll`, fills `LuxHostApi` and
calls `LuxVelum_Attach`; the callbacks it returns become `g_luxVelum`. Absent dll or `-noluxvelum`: nothing happens.

Host reads (`CoA/Source/Runtime/LuxVelumClient.cpp`): the D3D9 device (D3D9 api only), view and projection matrices,
viewport depth range, camera and target, camera liquid, map (loading screen map id), far clip, day/night (fog, colours,
sun and moon, honouring `overrideDayProgress`), stock fog read/write, screen effects and the glow passes' alpha, M2
point lights, the viewer's interior WMO groups, liquid settings, units standing in water, `CWorld::s_waterRipples` and
`DayNight::RenderGlares`.

Engine calls, at the places the 0.1.1 exe called them:

| Hook | Where |
|---|---|
| filterD3D9Proc | `CGxDeviceD3d::ILoadD3dLib`, `CGxDeviceD3d9Ex::ILoadD3dLib` (wraps Direct3DCreate9) |
| frameBegin / frameEnd | `CGWorldFrame::RenderWorld` around `OnWorldRender` |
| opaqueDone | `OnWorldRender` after `M2PASS_0` |
| liquidBegin / liquidEnd | `OnWorldRender` around `RenderLiquidPass` (camera above water) |
| glareBegin / glareEnd | `OnWorldRender`; non-zero glareBegin skips `DayNight::RenderGlares` |
| worldDone | `OnWorldRender` before `FFX::Finish` |
| worldTextBegin / End | `PLAYERNAMEDESC::Render` around the name batch |
| waterPassBegin / End | `CWorldScene::RenderLiquidPass` around the flush (argument: transparent instances queued) |
| waterDrawBegin / End | `CMaterialWater(NoSpec)::Render`, on every return path |
| m2BatchFog (observeM2Fog) | `CM2SceneRender::SetupLighting` before `SetFogParams` |
| keepStaleLights | `CM2Scene::SelectLights` keeps lights that were not drawn |
| farClipClamp | `World::ValidateFarClip` after CoA's clamp |

## Switching off

No `LuxVelum64.dll`, `-noluxvelum`, or `Enable=0` in `LuxVelum.ini` (the dll's own switch). AFK Realm's HD pack page
sets the latter.
