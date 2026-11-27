# Underwater glow: blank screen (2026-11-23)

**Symptom.** Under water the whole world renders as one flat fog colour; the UI is fine. Happens without Helios;
`ffxGlow 0` hides it. Ascension.exe on the same install does not show it.

**Where.** `CGWorldFrame::UpdateScreenEffectParams` sets `EffectGlow::m_mode = 1` under water, which runs
`m_altPasses` (Box4, Gauss4, `PassGlowWave`) instead of `m_passes` (Box4, Gauss4, `PassGlow`). A debug build with
switches showed: running the land passes under water works, and skipping only `PassGlowWave` works.

**What was ruled out.**
- `PassGlowWave` ctor/Execute, `SetupGeo_Quad`, `SetupIB`, the PCT3 attribute table, the wave constants and the
  C44Matrix helpers match the original 3.3.5a binary (IDA, 12340).
- The D3D state of the wave draw (dumped after `Draw`): texcoord index 0/1/2, no texture transforms, wave
  128x128 A8R8G8B8 wrap, scene 2560x1440 and quarter 640x360 clamp, decl POSITION/COLOR/TEXCOORD0-2 at stride 40,
  ps c0/c1 = 3 and 0.75 texels, fog and blending off, no vertex shader. All as intended.
- Forcing stages 0-2 to their own texcoords in `FFX::SetupGx` did not help.
- The `patch-A.MPQ` ps_3_0 FFXGlow*/FFXGlowWave are GXSH version 0x10004, which this loader rejects, so both
  passes use Blizzard's ps_2_0 shaders. The wave shader is the glow shader plus a wave-texture offset of the scene
  lookup (at most 3 texels). Ascension.exe reads the 0x10004 files and uses DXVK 2.5.3; CoA64 uses DXVK 3.1.1.

**Fix.** `EffectGlow::Execute` always runs `m_passes`; the underwater tint still applies (SetParameters writes
both pass sets). The wavy distortion is gone; the owner preferred this look.

**Open.** Why the ps_2_0 wave pass outputs a flat colour on this build is unknown. If the wave is wanted back:
diff the DXVK 3.1.1 and 2.5.3 handling of a fixed-function VS with a ps_2_0 dependent read, or read back the
wave texture and the s_full[0] copy after `FFX::Finish`.
