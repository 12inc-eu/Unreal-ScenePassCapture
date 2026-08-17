# Scene Pass Capture

Mirrors the renderer's live buffers into your own render targets, driven by a data asset. No scene
re-render: it taps the render graph mid-frame and blits, so the cost is one copy per requested pass
rather than one full scene render per pass.

## What it costs

A blit is bandwidth bound. At 1080p RGBA16f that is about 33 MB of traffic per pass, so roughly
0.03-0.09 ms depending on the GPU. Six passes at 1080p lands around 0.2-0.5 ms total. Compare that
with the SceneCaptureComponent approach, which re-runs visibility and the base pass per capture and
costs several milliseconds of GPU plus a serialized chunk of render thread per pass.

The render targets themselves are the real memory cost, and that is the same whichever approach you
use. Budget 16.6 MB per RGBA16f target at 1080p, 66 MB at 4K. Use the narrowest format that works:
R32f for depth, RGB10A2 for normals, RGBA8 for anything already tonemapped.

## Measuring the real cost

The Cost and Validation pane estimates from bandwidth. To measure what is actually spent:

```bash
stat GPU
```

Look for **Scene Pass Capture**. That stat wraps every capture pass, the post-opaque blits, the
Lumen reads and the post-process taps, so it is the whole plugin's GPU cost in one line, per frame,
measured on the hardware rather than derived.

It also shows up in Unreal Insights, alongside a `ScenePassCapture_BuildSnapshot` CPU scope covering
the game-thread work of resolving render targets each frame.

To sanity check that the number is really yours, toggle `r.ScenePassCapture.Enabled 0` and watch it
go to zero.

Note for engine upgrades: this uses `RDG_EVENT_SCOPE_STAT`, not `RDG_GPU_STAT_SCOPE`. The latter was
deprecated in 5.8 when the legacy GPU profiler was removed and now silently does nothing.

## Usage

1. Right click in the content browser, **Rendering, Scene Pass Capture Profile**.
2. Add an entry per pass. Pick the `Source` and hit **Create Target**, which makes a correctly
   configured render target and assigns it.
3. Turn capture on, either automatically or by hand.

### Automatically (no Blueprint, no console command)

**Project Settings, Plugins, Scene Pass Capture**:

- **Default Profile**: the profile to use
- **Auto Start In Game**: starts capturing when a game or PIE world begins play
- **Auto Start In Editor**: starts capturing when a map finishes loading in the editor, without
  entering play

This is the "just make it work" path. It is stored in `DefaultGame.ini` so it travels with the
project, and it needs no changes to your GameMode or GameState. The subsystem handles it from
`OnWorldBeginPlay` for game and PIE worlds, and from `OnWorldComponentsUpdated` for editor worlds,
which never begin play. It fires once per world.

### By hand

From the profile editor toolbar, hit **Capture**. Or from Blueprint or C++:

```
Get Scene Pass Capture Subsystem -> Start Capture (Profile)
```

`Capture Single Frame` grabs exactly one frame and goes back to idle, which is what you want for
one-off grabs.

### The kill switch

`r.ScenePassCapture.Enabled 0` disables all capture globally without touching the profile, and `1`
re-enables it. It is only an override, not the way you start capture: leaving it at its default of
`1` does nothing on its own.

## Resolution

Targets do not have to match the rendered resolution. The blit rescales, using point filtering for
depth sources and bilinear for everything else.

**Resolution Scale** on each entry picks a fraction of the rendered view size: 1/1, 1/2, 1/4, 1/6 or
1/8. It drives both `Resize Targets To Viewport` and the Create Target button. Halving the scale
quarters the pixel count, so memory and blit bandwidth drop with it. Most passes that feed an effect
rather than the screen look fine at 1/2 or 1/4.

**Resize Targets To Viewport** on the profile keeps every target matched to the rendered view,
scaled by each entry's Resolution Scale.

Note that it sizes from the **view rect, not the viewport**. A camera with Constrain Aspect Ratio
renders into a letterboxed sub-rect, and sizing to the full viewport would stretch that image across
the wrong aspect. The plugin uses `UnscaledViewRect`, which is the constrained region, so a 16:9
constrained camera in an ultrawide viewport produces 16:9 targets with no distortion.

## The profile editor

Double clicking a profile opens a dedicated editor rather than the generic data asset window.

### Create Target

Each pass entry has a **Create Target** button under the Target picker. It makes a render target
already configured for the selected Source and assigns it:

- format chosen from the Source *and* its modifiers, so Scene Depth with Depth Normalize Range 0
  gets R32f but with a range set gets R8, and World Normal gets RGBA8 or RGBA16f depending on
  Decode Normals
- sized to the active viewport, falling back to 1920x1080
- created in the same folder as the profile
- named after the source (`RT_SceneDepth`, `RT_WorldNormal`, ...) with unique-name handling
- black clear color, no mips, clamped addressing
- assigned through the property handle, so it is undoable

This exists because the object picker's own "Render Target" entry makes an unconfigured default,
which is wrong for almost every source here.

### Inline format warning

When the assigned target cannot represent the selected Source, a warning appears directly under the
entry. Red means the output will be visibly broken, amber means it works but you are losing
something. The rules live in `ScenePassCaptureFormatRules.cpp`, which the button, this warning, and
the Cost and Validation pane all share, so they cannot drift apart.

- **Details** is the usual property panel for the pass list.
- **Targets** is a live preview wall. Every enabled entry gets a tile showing its render target
  updating in real time, with its resolution and asset name underneath. Start capture and watch the
  buffers fill in.
- **Cost and Validation** shows the active pass count, total render target memory, bytes moved per
  captured frame, and an estimated GPU cost at 360 GB/s and 1 TB/s so you can see immediately what
  the profile is going to cost.

The validation list catches the mistakes that are otherwise invisible until the output looks wrong:

- raw world depth aimed at a non-float target, where everything past 1 cm clamps to white
- decoded normals aimed at a target that cannot store negatives
- an HDR source aimed at an LDR target
- GBuffer sources while Substrate is enabled
- custom depth or stencil sources while the corresponding project setting is off
- two entries writing to the same render target
- velocity and ambient occlusion, which are only written on frames where those passes ran

The toolbar drives capture directly: **Capture** toggles continuous capture, **Single Frame** grabs
one frame, and **Refresh** rebuilds the tiles and re-runs validation. It targets the PIE world when
one is running, otherwise the editor viewport, so you can preview passes without entering play.

## Sources

| Source | Buffer | Hook point |
| --- | --- | --- |
| Scene Color (Pre-Translucency, HDR) | scene color | post-opaque |
| Scene Color (Before Bloom, HDR) | scene color | post chain, `BL_SceneColorBeforeBloom` |
| Scene Color (After Tonemap) | scene color | post chain, `BL_SceneColorAfterTonemapping` |
| Scene Depth (World Units) | depth, linearized | post-opaque |
| Scene Depth (Raw Device Z) | depth, untouched | post-opaque |
| World Normal | GBufferA | post-opaque |
| Base Color | GBufferC | post-opaque |
| Metallic / Specular / Roughness | GBufferB R / G / B | post-opaque |
| Screen Velocity | GBuffer velocity | post-opaque |
| Ambient Occlusion | SSAO buffer | post-opaque |
| Custom Depth (World Units) | custom depth, linearized | post-opaque |
| Custom Stencil | custom stencil | post-opaque |
| Separate Translucency (HDR) | translucency layer alone | post chain, before bloom |
| GBuffer D (Custom Data) | GBufferD | post-opaque |
| GBuffer E (Precomputed Shadows) | GBufferE | post-opaque |
| GBuffer F (Anisotropy / Tangent) | GBufferF | post-opaque |
| Scene Partial Depth | partial depth, raw device Z | post-opaque |

GBuffer D carries different data per shading model (subsurface color, clear coat, hair). GBuffer F
only exists when anisotropic materials are enabled for the project.

### Screen-space Lumen

| Source | Buffer |
| --- | --- |
| Lumen: Diffuse Indirect (Screen) | `ScreenProbeGatherState.DiffuseIndirectHistoryRT` |
| Lumen: Rough Specular Indirect (Screen) | `RoughSpecularIndirectHistoryRT` |
| Lumen: Short Range AO (Screen) | `ShortRangeAOHistoryRT` |
| Lumen: Backface Diffuse Indirect (Screen) | `BackfaceDiffuseIndirectHistoryRT` |
| Lumen: Reflections (Screen) | `ReflectionState.SpecularAndSecondMomentHistory` |

**Short Range AO is the AO source to use when Lumen is on.** The plain Ambient Occlusion source
reads the SSAO buffer, which the renderer never fills under Lumen.

These are Lumen's **denoised temporal history** buffers, which carries two consequences. They are the
previous frame's result reprojected, so they lag by a frame. And they can be at a downsampled
resolution depending on the screen probe gather settings, so the blit rescales them. Each one is
null whenever its Lumen feature is off, in which case the target is left untouched.

Reflections pack specular in RGB and the denoiser's second moment in alpha.

**These sources are the one part of the plugin that is not built on public engine API.** Lumen
exposes nothing publicly, so `ScenePassCaptureLumen.cpp` includes Renderer private headers, enabled
by private include paths in `ScenePassCapture.Build.cs`. Everything Lumen is confined to that one
translation unit and gated on `SCENEPASSCAPTURE_LUMEN`. If an engine upgrade breaks it:

```csharp
private const bool bEnableLumenSources = false;
```

That drops the private include paths and the whole file. The rest of the plugin keeps building, the
enum entries stay valid so existing profiles do not corrupt, and the validation pane reports that
Lumen sources were compiled out.

The reason this works from the post-opaque hook at all is that the history buffers are persistent
pooled render targets on the view state rather than transient RDG textures, so they outlive the
passes that produce them. Note also that the code reads `View->ViewState->Lumen` directly: the
tidier `FScene::GetLumenSceneData()` accessor is inline but calls two functions the Renderer module
does not export, so it fails to link from outside.

## Limitations

- **Deferred renderer only.** The forward and mobile renderers have no GBuffer, and the plugin
  detects the mobile path and skips rather than reading garbage.
- **Substrate.** With `r.Substrate=1` the classic GBufferA/B/C layout no longer holds, so the
  normal, base color, metallic, specular and roughness sources will be wrong. Depth, velocity,
  custom depth/stencil and all the scene color sources stay valid either way. Substrate is off in
  this project.
- **Main viewport only.** Capture is restricted to real viewports, so SceneCaptureComponents,
  thumbnail renders and material previews are ignored. Only the first view of a family is captured,
  so split screen and stereo take the left eye / first player.
- **Ambient Occlusion is unavailable under Lumen.** When `r.DynamicGlobalIlluminationMethod` is 1,
  the renderer never runs the separate SSAO pass and substitutes a white dummy texture, so the
  capture is solid white. Lumen does its own occlusion inside the GI solve and does not expose it as
  a buffer. The validation pane flags this.
- **Velocity, Anisotropy and Scene Partial Depth can be absent** on frames where those passes did
  not run. The entry is skipped rather than writing stale data.
- **Single channel targets preview as red, not greyscale.** An R8 or R32f target only stores the red
  channel, so the editor draws it red. That is the format, not a broken capture. The preview tiles
  name the format for this reason.
- **GPU side only.** Getting the pixels onto the CPU is a separate problem: use
  `FRHIGPUTextureReadback` with a few frames of latency, and expect roughly 1.5 ms of PCIe transfer
  per 1080p RGBA16f pass.

## Layout

```
ScenePassCapture.uplugin
Shaders/Private/ScenePassCaptureCopy.usf     blit + decode (channel, normal, depth, stencil)
Source/ScenePassCapture/
  Public/ScenePassCaptureTypes.h             source enum + per-pass entry struct
  Public/ScenePassCaptureProfile.h           the data asset
  Public/ScenePassCaptureSubsystem.h         per-world control surface, Blueprint exposed
  Private/ScenePassCaptureViewExtension.*    the render graph hooks, all the real work
  Private/ScenePassCaptureShaders.*          global shader declarations
  Private/ScenePassCaptureModule.cpp         virtual shader path mapping
Source/ScenePassCaptureEditor/
  Private/AssetDefinition_*                  asset type registration
  Private/ScenePassCaptureProfileFactory.*   content browser creation
  Private/ScenePassCaptureProfileEditor.*    the three-pane asset editor
  Private/ScenePassCaptureAnalysis.*         cost model and validation rules
```

The module loads at `PostConfigInit` because the shader directory mapping has to be registered
before shader compilation starts.

## Implementation notes

Two hooks are used, both on public engine API so this should survive engine upgrades reasonably well:

- `IRendererModule::RegisterPostOpaqueRenderDelegate` hands over `FPostOpaqueRenderParameters`, which
  carries the full `FSceneTextureUniformParameters` (every GBuffer, depth, custom depth/stencil,
  SSAO) plus the pre-translucency scene color. That is where all the geometry passes come from.
- `ISceneViewExtension::SubscribeToPostProcessingPass` inserts a pass-through callback into the post
  chain for the two scene color taps. It returns the scene color it was handed, and honours
  `OverrideOutput` via `FScreenPassTexture::CopyFromSlice` so it stays correct on the frames where it
  ends up last in the chain.

The post-opaque delegate fires for every scene render in the engine, including ones we do not want.
`PreRenderViewFamily_RenderThread` and `PostRenderViewFamily_RenderThread` bracket the family the
view extension opted into, and the delegate early-outs outside that bracket.

Render targets are resolved to raw RHI pointers on the game thread in `BeginRenderViewFamily`, packed
into an immutable snapshot, and handed to the render thread through a render command. The render
thread never touches a UObject.

`Shutdown()` must be called on the game thread before the last reference to the view extension is
dropped. It removes the renderer delegate and flushes rendering commands, which is what keeps the
raw `this` captured by those callbacks safe. The subsystem does this in `Deinitialize`.
