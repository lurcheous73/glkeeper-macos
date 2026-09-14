#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct DK2SoundExportStats
{
    std::size_t mBanks = 0;
    std::size_t mEntries = 0;
    std::size_t mExported = 0;
    std::size_t mSkipped = 0;
    std::size_t mFailed = 0;
};

// Export Dungeon Keeper 2 SDT sound banks to ordinary WAV/MP2 files.
// The input folder is Data/Sound/Sfx from a user-owned game install.
bool DK2ExportSoundBanks(const std::string& inputRoot,
    const std::string& outputRoot, DK2SoundExportStats& stats);

// Resolve a logical DK2 sound event through <category>SFX.map and
// <category>BANK.map and return a directly playable WAV/MPEG payload.
// Enhanced mode prefers the HD bank and falls back to HW; Original does
// the reverse, without modifying either original bank.
bool DK2LoadSoundEvent(const std::string& sfxRoot, const std::string& category,
    unsigned int eventId, bool preferHD, std::vector<unsigned char>& outputData,
    std::string* outputSource = nullptr);
