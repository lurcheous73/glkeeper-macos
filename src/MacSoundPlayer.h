#pragma once

#include <cstddef>

#ifdef __APPLE__

// Play encoded DK2 audio held in memory. AVFoundation sniffs the actual
// MPEG layer, so historic .mp2-labelled Layer-I payloads also work.
bool MacPlaySoundData(const void* data, std::size_t dataSize, float volume = 1.0f);

// Diagnostic helper used by --test-sound; waits for the clip to finish.
bool MacPlaySoundDataBlocking(const void* data, std::size_t dataSize, float volume = 1.0f);

enum class MacSoundChannel
{
    Music,
    Ambience,
    Voice
};

// Persistent named channels for looping music/ambience and speech.
bool MacPlaySoundChannelData(MacSoundChannel channel, const void* data,
    std::size_t dataSize, float volume = 1.0f, bool loop = false);
bool MacIsSoundChannelPlaying(MacSoundChannel channel);
void MacStopSoundChannel(MacSoundChannel channel);
void MacStopAllSounds();

#endif
