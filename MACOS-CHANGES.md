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
