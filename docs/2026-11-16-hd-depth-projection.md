# HD depth reads: the device projection (2026-11-16)

## Symptom

Azeroth Remastered's water (hdWater 1) looked flat and opaque everywhere, shallow shores included; the same in every
CoA64 build that carried the HD port. hdWaterDebug 1 (water depth) and 6 (world depth) were white on every water.

## What was measured

- hdWaterInfo: Forever PBR water on, forever_data found, shaders in use.
- The linear world depth target held smooth, plausible values (read back down the screen's middle column).
- Swapping the depth and world images between stages 6 and 7 showed both stages deliver what is bound.
- Test views 9..12 (kept in HdWaterShaders.h): the world came out 10+ yards behind the surface even at a beach,
  and the ratio world / surface was about 2 near the shore, growing with distance: a factor, not an offset.

## Cause

HD::CaptureCamera copied CGxDevice::m_projection, the engine's projection with depth -1..1. The Vulkan device writes
depth with m_projNative (XformSetProjection rescales it to 0..1). Reading 0..1 depth with the -1..1 coefficients gives
about 2 f z / (z + f), roughly twice the true distance up close. HdWater, HdFog, HdAO, HdBloom, HdContactShadow and
HdGI all read the depth through that camera.

## Fix

CaptureCamera takes m_projNative. Confirmed in game: transparent shores, a depth gradient in hdWaterDebug 1.
