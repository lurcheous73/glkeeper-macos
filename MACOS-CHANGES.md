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

## 14 September 2026 — named texture export milestone

- Added `--export-textures <directory>` utility mode to the native DK2 engine.
- The exporter reads the user's `DK2TextureCache` through GLKeeper's native decoder.
- Mipmap entries are grouped by stable DK2 texture name and exported as PNG base textures.
- Verified against the GOG data: 5,767 cache entries resolve to 1,911 unique textures.
- Validation result: 1,911/1,911 PNGs exported successfully with zero failures.
- Output preserves stable texture names/subdirectories so Enhanced overrides map back directly.
- Generated textures are derived from the user's game media and are intentionally not committed to Git.
- Enhanced runtime already checks `enhanced/textures/<stable-name>.png` before falling back to original data.

## 14 September 2026 — native sound-bank export milestone

- Added a native C++ reader/exporter for Dungeon Keeper II `.sdt` sound banks.
- Added `--export-sounds <directory>` utility mode; no Java/OpenKeeper runtime is required.
- PCM entries receive standard WAV headers; original MPEG audio payloads are preserved losslessly.
- Export preserves the original bank/category hierarchy and safely handles duplicate clip names.
- Verified across the GOG sound tree: 142/142 banks read with zero failures.
- 8,959 SDT entries were scanned: 7,960 playable clips exported and 999 blank/unsupported entries skipped.
- Export result contains 211 PCM WAV clips and 7,749 original MPEG-audio clips (~209 MB total).
- Representative frontend, mentor-speech and PCM outputs were independently probed successfully.
- Some historic `.mp2`-labelled DK2 payloads identify as MPEG Layer I; runtime audio must inspect content, not assume Layer II from the suffix.
- Generated sound files remain derived user game media and are intentionally excluded from Git.

## 14 September 2026 — native runtime sound milestone

- Added native parsing of DK2 `SFX.map` and `BANK.map` event indexes.
- Logical DK2 sound event IDs now resolve to their original SDT archive and clip.
- Added in-memory PCM/MPEG extraction; no temporary sound files are required at runtime.
- Added a native macOS AVFoundation/AVAudioPlayer backend for asynchronous game SFX.
- Enhanced mode prefers HD sound banks and falls back to HW; Original prefers HW then falls back to HD.
- Added a small runtime sound cache so repeated UI sounds are not reparsed from disk.
- Wired authentic frontend click event 778 and frontend glow/hover event 783 into the UI button path.
- Added `--test-sound <category> <event-id>` for deterministic sound-resolution/playback tests.
- Verified event 778 resolves to `GuiHD.sdt#8`; event 783 resolves to `FrontEndHD.sdt#1`.
- Verified both events play successfully on the Universal `x86_64 arm64` native binary with exit code 0.

## Native background audio sequencing

- Reworked DK2 music and ambience playback to follow every clip mapped by `SFX.map` instead of looping the first resolved fragment.
- Added indexed sound-event access so background categories advance in original map order and only wrap after the full mapped playlist.
- Added independent native macOS channels for music, ambience and voice; UI/effects remain overlapping one-shots.
- Added queued voice playback so mentor/speech clips can play over background audio without interrupting music or ambience.
- Verified frontend music event 343, gameplay music 345, ambience 341, options music 838 and mentor speech against the original GOG sound maps.
- Verified Universal x86_64 + arm64 build and existing mapped sound diagnostics after the sequencing change.

## Native KWD trigger execution
- Parse DKLD_TRIGGERS into typed generic/action nodes instead of discarding the section.
- Preserve each player's root trigger id from the Players KLD data.
- Added a native trigger evaluator for level-time, flag, timer, GUI-transition, gold and mana conditions.
- Added flag/timer actions plus PLAY_SPEECH dispatch into the independent mentor voice queue.
- Added hidden `KEEPER_START_LEVEL` and `--test-level-triggers` diagnostics for deterministic port testing.
- Verified Level 1 exactly: 682 nodes = 356 generic + 326 action, including 48 speech actions; Keeper 1 root is trigger 1.
- Live Level 1 execution proved the original Bullfrog graph activates and dispatches mentor speech ids 2, 3 and 5.
- Universal x86_64 + arm64 build reproduces the same Level 1 trigger inventory.
