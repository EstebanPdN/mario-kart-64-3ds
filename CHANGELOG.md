# Changelog

## v1.2-E11 — changes since v1.1

- Removed the 400/800-pixel toggle from the Screen menu while retaining
  compatible top-screen output handling.
- Added conservative Old and New Nintendo 3DS performance, memory, audio, HUD,
  and presentation profiles.
- Fixed shutdown, diagnostics, repeated menu/demo/race, results, camera, object,
  resource, texture, and renderer-pressure crashes.
- Restored the proven Citro3D display-target formats after the E2 top-screen
  startup regression.
- Bounded resource, texture, actor, object, display-list, collision, and vertex
  storage, with automatic renderer recovery where a frame can be retried safely.
- Added guarded Fast3D culling and removed duplicate or unnecessary scene,
  transform, camera, collision, traffic, object, and HUD work.
- Preserved the original 30 Hz simulation while improving overload recovery and
  adaptive New 3DS midpoint presentation.
- Fixed menu rectangle bounds and GPU cache coherency across menu, race, and
  texture-streaming transitions.
- Restored the complete title screen and textured menu presentation.
- Moved audio synthesis to reusable direct NDSP buffers and overlapped it on an
  auxiliary CPU core when the hardware can provide one safely.
- Reduced the Fast3D vertex stream and transfer traffic by 25%, added a compact
  combiner path, and limited broad cache maintenance to Citro2D frames.
- Expanded renderer, presentation, memory, resource, and timing diagnostics and
  regression probes.
- Added persistent Low, Medium, and High internal resolutions plus Bilinear,
  Blur, and CRT display filters.
- Moved CPU-to-PICA200 and CPU-to-NDSP cache cleaning off blocking service IPC
  when the direct ARM11 path is available, with an automatic compatibility
  fallback.
- Limited Citro2D cache cleaning to its captured vertex/index allocation span,
  retained unchanged filter-presentation geometry between frames, and exposed
  the active cache-clean path in diagnostic dumps.

Mario Kart 64's game simulation remains 30 Hz. Sustained frame-rate results
require representative measurements on physical Old and New Nintendo 3DS
hardware.
