# GLKeeper macOS port changes

## 14 September 2026 — native Mac milestone

- Added a native CMake build for macOS/Apple Silicon.
- Added macOS executable-path and data-root handling.
- Added explicit UTF-16LE KWD string decoding instead of host `wchar_t` reads.
- This fixes the macOS 32-bit `wchar_t` parser misalignment that caused every KWD map to fail.
- Normalized embedded Windows resource paths (`Data\\Editor\\...`) at the filesystem boundary.
- Preserved real macOS filesystem path case instead of lowercasing search roots.
- DK2 level database now reaches `Ready` and scenario loading progresses beyond the old parser failure.

### Fullscreen and input
- macOS defaults to fullscreen for the native build.
- Disabled Retina framebuffer scaling for GLKeeper so old UI/render coordinates stay 1:1.
- Removed the double-cursor problem by hiding the macOS hardware cursor and using only the in-game cursor.
- Verified the in-game cursor is now correctly aligned.

### Validation
- Native ARM64 build completes successfully.
- Fullscreen smoke test stays alive and reaches the DK2 frontend.
- Original DK2 intro/cutscene playback is not implemented in upstream GLKeeper; this is the next milestone.

### Known repository note
- Some upstream files currently show CRLF/LF churn in the raw diff. Functional changes are much smaller than the raw line count; this will be normalized in a separate mechanical cleanup commit.

## 14 September 2026 — native startup movie milestone

- Added a native macOS AVFoundation/AVKit movie player.
- Startup sequence now plays BullfrogIntro and INTRO before GLKeeper enters the 3D frontend.
- Escape cleanly skips a movie and hands control back to DK2.
- Original TGQ assets are converted locally to square-pixel 640×480 H.264/AAC MP4 for AVFoundation playback.
- The runtime lookup supports separate Original and Enhanced movie directories.
- Enhanced can later substitute AI-restored MP4 assets without changing the startup logic.
- Movie files derived from original game media are intentionally not committed to Git.
- Live validation: Bullfrog movie visible at 3s; DK2 intro visible at 11s; Escape returns to the frontend with the process still running.

## 14 September 2026 — Universal build validation

- Rebuilt the native DK2 engine after the movie-player changes as a Universal macOS binary.
- Verified architectures: `x86_64 arm64`.
- The AVFoundation/AVKit startup-movie code is included in the Universal target.
- Current DK2 engine therefore no longer requires a separate Intel or Apple Silicon executable.
