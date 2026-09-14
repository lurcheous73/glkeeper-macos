#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class DK2SoundSystem
{
public:
    bool Initialize();
    void Shutdown();

    bool PlayEvent(const std::string& category, unsigned int eventId, float volume = 1.0f);

private:
    std::string mSfxRoot;
    bool mEnhanced = false;
    std::unordered_map<std::string, std::vector<unsigned char>> mClipCache;
};

extern DK2SoundSystem gDK2SoundSystem;
