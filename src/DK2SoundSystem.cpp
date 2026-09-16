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
    mSpeechQueue.clear();
    mMusicSequence = {};
    mAmbienceSequence = {};
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
    mSpeechQueue.clear();
    mMusicSequence = {};
    mAmbienceSequence = {};
    mSfxRoot.clear();
}

const std::vector<unsigned char>* DK2SoundSystem::ResolveEvent(const std::string& category,
    unsigned int eventId)
{
    return ResolveEventClip(category, eventId, 0);
}

const std::vector<unsigned char>* DK2SoundSystem::ResolveEventClip(const std::string& category,
    unsigned int eventId, std::size_t clipIndex)
{
    if (mSfxRoot.empty())
        return nullptr;

    const std::string key = cxx::lower_string(category) + ":" +
        std::to_string(eventId) + ":" + std::to_string(clipIndex) +
        (mEnhanced ? ":hd" : ":hw");
    auto found = mClipCache.find(key);
    if (found == mClipCache.end())
    {
        std::vector<unsigned char> bytes;
        std::string source;
        if (!DK2LoadSoundEventClip(mSfxRoot, category, eventId, mEnhanced,
                clipIndex, bytes, &source))
        {
            return nullptr;
        }
        gConsole.LogMessage(eLogLevel_Debug,
            "Resolved DK2 sound event %s:%u clip %zu -> %s",
            category.c_str(), eventId, clipIndex, source.c_str());
        found = mClipCache.emplace(key, std::move(bytes)).first;
    }
    return &found->second;
}

bool DK2SoundSystem::PlayEvent(const std::string& category, unsigned int eventId, float volume)
{
    const auto* clip = ResolveEvent(category, eventId);
    if (!clip)
    {
        gConsole.LogMessage(eLogLevel_Warning,
            "Cannot resolve DK2 sound event %s:%u", category.c_str(), eventId);
        return false;
    }
#ifdef __APPLE__
    return MacPlaySoundData(clip->data(), clip->size(), volume);
#else
    return false;
#endif
}

bool DK2SoundSystem::BuildJoinedBackground(BackgroundSequence& sequence)
{
    sequence.mJoinedData.clear();
    sequence.mJoinedMpeg = false;
    if (sequence.mClipCount < 2)
        return false;

    // DK2 music/ambience banks are split into short MPEG frame runs.  Opening
    // a fresh AVAudioPlayer for each run creates an audible gap.  MPEG audio
    // frames are independently decodable, so concatenate the original payloads
    // in SFX.map order and present AVFoundation with one continuous stream.
    for (std::size_t clipIndex = 0; clipIndex < sequence.mClipCount; ++clipIndex)
    {
        const auto* clip = ResolveEventClip(sequence.mCategory, sequence.mEventId, clipIndex);
        if (!clip || clip->size() < 2 || (*clip)[0] != 0xff || (((*clip)[1] & 0xe0) != 0xe0))
        {
            sequence.mJoinedData.clear();
            return false;
        }
        sequence.mJoinedData.insert(sequence.mJoinedData.end(), clip->begin(), clip->end());
    }

    sequence.mJoinedMpeg = !sequence.mJoinedData.empty();
    if (sequence.mJoinedMpeg)
        gConsole.LogMessage(eLogLevel_Info,
            "Joined DK2 background event %s:%u into one MPEG stream (%zu clips, %zu bytes)",
            sequence.mCategory.c_str(), sequence.mEventId, sequence.mClipCount,
            sequence.mJoinedData.size());
    return sequence.mJoinedMpeg;
}

bool DK2SoundSystem::StartBackground(BackgroundSequence& sequence, bool musicChannel)
{
#ifdef __APPLE__
    if (!sequence.mActive)
        return false;
    const MacSoundChannel channel = musicChannel ?
        MacSoundChannel::Music : MacSoundChannel::Ambience;
    if (sequence.mJoinedMpeg)
        return MacPlaySoundChannelData(channel, sequence.mJoinedData.data(),
            sequence.mJoinedData.size(), sequence.mVolume, sequence.mCycle);
#endif
    return StartNextBackground(sequence, musicChannel);
}

bool DK2SoundSystem::StartNextBackground(BackgroundSequence& sequence, bool musicChannel)
{
#ifdef __APPLE__
    if (!sequence.mActive || sequence.mClipCount == 0)
        return false;

    for (std::size_t attempt = 0; attempt < sequence.mClipCount; ++attempt)
    {
        if (sequence.mNextIndex >= sequence.mClipCount)
        {
            if (!sequence.mCycle)
            {
                sequence.mActive = false;
                return false;
            }
            sequence.mNextIndex = 0;
        }

        const std::size_t clipIndex = sequence.mNextIndex++;
        const auto* clip = ResolveEventClip(sequence.mCategory,
            sequence.mEventId, clipIndex);
        if (!clip)
            continue;

        const MacSoundChannel channel = musicChannel ?
            MacSoundChannel::Music : MacSoundChannel::Ambience;
        return MacPlaySoundChannelData(channel, clip->data(), clip->size(),
            sequence.mVolume, false);
    }

    sequence.mActive = false;
#endif
    return false;
}

bool DK2SoundSystem::PlayMusic(const std::string& category, unsigned int eventId,
    float volume, bool cyclePlaylist)
{
    mMusicSequence = {};
    mMusicSequence.mCategory = category;
    mMusicSequence.mEventId = eventId;
    mMusicSequence.mVolume = volume;
    mMusicSequence.mClipCount = DK2GetSoundEventClipCount(mSfxRoot, category, eventId);
    mMusicSequence.mActive = mMusicSequence.mClipCount > 0;
    mMusicSequence.mCycle = cyclePlaylist;
    BuildJoinedBackground(mMusicSequence);
#ifdef __APPLE__
    MacStopSoundChannel(MacSoundChannel::Music);
#endif
    return StartBackground(mMusicSequence, true);
}

bool DK2SoundSystem::PlayAmbience(const std::string& category, unsigned int eventId,
    float volume, bool cyclePlaylist)
{
    mAmbienceSequence = {};
    mAmbienceSequence.mCategory = category;
    mAmbienceSequence.mEventId = eventId;
    mAmbienceSequence.mVolume = volume;
    mAmbienceSequence.mClipCount = DK2GetSoundEventClipCount(mSfxRoot, category, eventId);
    mAmbienceSequence.mActive = mAmbienceSequence.mClipCount > 0;
    mAmbienceSequence.mCycle = cyclePlaylist;
    BuildJoinedBackground(mAmbienceSequence);
#ifdef __APPLE__
    MacStopSoundChannel(MacSoundChannel::Ambience);
#endif
    return StartBackground(mAmbienceSequence, false);
}

void DK2SoundSystem::QueueSpeech(const std::string& category, unsigned int eventId, float volume)
{
    mSpeechQueue.push_back({category, eventId, volume});
}

void DK2SoundSystem::Update()
{
#ifdef __APPLE__
    if (mMusicSequence.mActive && !mMusicSequence.mJoinedMpeg &&
        !MacIsSoundChannelPlaying(MacSoundChannel::Music))
        StartNextBackground(mMusicSequence, true);
    if (mAmbienceSequence.mActive && !mAmbienceSequence.mJoinedMpeg &&
        !MacIsSoundChannelPlaying(MacSoundChannel::Ambience))
        StartNextBackground(mAmbienceSequence, false);

    if (!mSpeechQueue.empty() && !MacIsSoundChannelPlaying(MacSoundChannel::Voice))
    {
        const PendingSpeech speech = mSpeechQueue.front();
        mSpeechQueue.pop_front();
        const auto* clip = ResolveEvent(speech.mCategory, speech.mEventId);
        if (clip)
            MacPlaySoundChannelData(MacSoundChannel::Voice, clip->data(), clip->size(),
                speech.mVolume, false);
    }
#endif
}

void DK2SoundSystem::StopBackground()
{
    mMusicSequence.mActive = false;
    mAmbienceSequence.mActive = false;
#ifdef __APPLE__
    MacStopSoundChannel(MacSoundChannel::Music);
    MacStopSoundChannel(MacSoundChannel::Ambience);
#endif
}
