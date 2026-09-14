#pragma once

#include <cstddef>
#include <string>

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
