#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

class DK2SoundSystem
{
public:
    bool Initialize();
    void Shutdown();

    bool PlayEvent(const std::string& category, unsigned int eventId, float volume = 1.0f);
    bool PlayMusic(const std::string& category, unsigned int eventId,
        float volume = 0.65f, bool cyclePlaylist = true);
    bool PlayAmbience(const std::string& category, unsigned int eventId,
        float volume = 0.35f, bool cyclePlaylist = true);
    void QueueSpeech(const std::string& category, unsigned int eventId, float volume = 1.0f);
    void Update();
    void StopBackground();

private:
    struct PendingSpeech
    {
        std::string mCategory;
        unsigned int mEventId = 0;
        float mVolume = 1.0f;
    };

    struct BackgroundSequence
    {
        std::string mCategory;
        unsigned int mEventId = 0;
        float mVolume = 1.0f;
        std::size_t mNextIndex = 0;
        std::size_t mClipCount = 0;
        std::vector<unsigned char> mJoinedData;
        bool mJoinedMpeg = false;
        bool mActive = false;
        bool mCycle = true;
    };

    const std::vector<unsigned char>* ResolveEvent(const std::string& category,
        unsigned int eventId);
    const std::vector<unsigned char>* ResolveEventClip(const std::string& category,
        unsigned int eventId, std::size_t clipIndex);
    bool BuildJoinedBackground(BackgroundSequence& sequence);
    bool StartNextBackground(BackgroundSequence& sequence, bool musicChannel);
    bool StartBackground(BackgroundSequence& sequence, bool musicChannel);

    std::string mSfxRoot;
    bool mEnhanced = false;
    std::unordered_map<std::string, std::vector<unsigned char>> mClipCache;
    std::deque<PendingSpeech> mSpeechQueue;
    BackgroundSequence mMusicSequence;
    BackgroundSequence mAmbienceSequence;
};

extern DK2SoundSystem gDK2SoundSystem;
