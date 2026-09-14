#pragma once

#include <cstddef>

#ifdef __APPLE__

// Play encoded DK2 audio held in memory. AVFoundation sniffs the actual
// MPEG layer, so historic .mp2-labelled Layer-I payloads also work.
bool MacPlaySoundData(const void* data, std::size_t dataSize, float volume = 1.0f);

// Diagnostic helper used by --test-sound; waits for the clip to finish.
bool MacPlaySoundDataBlocking(const void* data, std::size_t dataSize, float volume = 1.0f);

void MacStopAllSounds();

#endif
