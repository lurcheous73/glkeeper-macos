#include "stdafx.h"
#include "DK2SoundSystem.h"

#include "DK2SoundBank.h"
#include "FileSystem.h"
#ifdef __APPLE__
#include "MacSoundPlayer.h"
#endif

DK2SoundSystem gDK2SoundSystem;

bool DK2SoundSystem::Initialize()
{
    mClipCache.clear();
    mSfxRoot.clear();
    mEnhanced = false;

    const char* enhanced = std::getenv("KEEPER_ENHANCED");
    mEnhanced = enhanced && std::strcmp(enhanced, "1") == 0;

    if (!gFiles.PathToDirectory("Data/Sound/Sfx", mSfxRoot))
    {
        gConsole.LogMessage(eLogLevel_Warning, "Cannot locate DK2 SFX root");
        return false;
    }

    gConsole.LogMessage(eLogLevel_Info, "DK2 sound root '%s' (%s banks preferred)",
        mSfxRoot.c_str(), mEnhanced ? "HD" : "HW");
    return true;
}
void DK2SoundSystem::Shutdown()
{
#ifdef __APPLE__
    MacStopAllSounds();
#endif
    mClipCache.clear();
    mSfxRoot.clear();
}

bool DK2SoundSystem::PlayEvent(const std::string& category, unsigned int eventId, float volume)
{
    if (mSfxRoot.empty())
        return false;

    const std::string key = cxx::lower_string(category) + ":" +
        std::to_string(eventId) + (mEnhanced ? ":hd" : ":hw");
    auto found = mClipCache.find(key);
    if (found == mClipCache.end())
    {
        std::vector<unsigned char> bytes;
        std::string source;
        if (!DK2LoadSoundEvent(mSfxRoot, category, eventId, mEnhanced, bytes, &source))
        {
            gConsole.LogMessage(eLogLevel_Warning,
                "Cannot resolve DK2 sound event %s:%u", category.c_str(), eventId);
            return false;
        }
        gConsole.LogMessage(eLogLevel_Debug,
            "Resolved DK2 sound event %s:%u -> %s", category.c_str(), eventId, source.c_str());
        found = mClipCache.emplace(key, std::move(bytes)).first;
    }

#ifdef __APPLE__
    return MacPlaySoundData(found->second.data(), found->second.size(), volume);
#else
    return false;
#endif
}
