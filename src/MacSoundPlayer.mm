#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>

#include "MacSoundPlayer.h"
#include <algorithm>

namespace
{
NSMutableArray<AVAudioPlayer*>* gPlayers = nil;
AVAudioPlayer* gMusicPlayer = nil;
AVAudioPlayer* gAmbiencePlayer = nil;
AVAudioPlayer* gVoicePlayer = nil;

AVAudioPlayer*& PlayerForChannel(MacSoundChannel channel)
{
    switch (channel)
    {
        case MacSoundChannel::Music: return gMusicPlayer;
        case MacSoundChannel::Ambience: return gAmbiencePlayer;
        case MacSoundChannel::Voice: return gVoicePlayer;
    }
    return gVoicePlayer;
}

void PrunePlayers()
{
    if (!gPlayers)
        return;
    for (NSInteger i = (NSInteger)gPlayers.count - 1; i >= 0; --i)
    {
        AVAudioPlayer* player = [gPlayers objectAtIndex:(NSUInteger)i];
        if (![player isPlaying])
            [gPlayers removeObjectAtIndex:(NSUInteger)i];
    }
}

AVAudioPlayer* CreatePlayer(const void* data, std::size_t dataSize, float volume)
{
    if (!data || dataSize == 0)
        return nil;
    NSData* audioData = [NSData dataWithBytes:data length:dataSize];
    NSError* error = nil;
    AVAudioPlayer* player = [[AVAudioPlayer alloc] initWithData:audioData error:&error];
    if (!player)
    {
        NSLog(@"DK2 audio decode failed: %@", error);
        return nil;
    }
    player.volume = std::max(0.0f, std::min(1.0f, volume));
    [player prepareToPlay];
    return player;
}
} // namespace

bool MacPlaySoundData(const void* data, std::size_t dataSize, float volume)
{
    @autoreleasepool
    {
        PrunePlayers();
        AVAudioPlayer* player = CreatePlayer(data, dataSize, volume);
        if (!player)
            return false;
        if (!gPlayers)
            gPlayers = [[NSMutableArray alloc] init];
        [gPlayers addObject:player];
        const BOOL started = [player play];
#if !__has_feature(objc_arc)
        [player release];
#endif
        return started == YES;
    }
}

bool MacPlaySoundDataBlocking(const void* data, std::size_t dataSize, float volume)
{
    @autoreleasepool
    {
        AVAudioPlayer* player = CreatePlayer(data, dataSize, volume);
        if (!player)
            return false;
        const BOOL started = [player play];
        if (started)
        {
            while ([player isPlaying])
            {
                [[NSRunLoop currentRunLoop]
                    runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
            }
        }
#if !__has_feature(objc_arc)
        [player release];
#endif
        return started == YES;
    }
}

bool MacPlaySoundChannelData(MacSoundChannel channel, const void* data,
    std::size_t dataSize, float volume, bool loop)
{
    @autoreleasepool
    {
        AVAudioPlayer*& slot = PlayerForChannel(channel);
        if (slot)
        {
            [slot stop];
#if !__has_feature(objc_arc)
            [slot release];
#endif
            slot = nil;
        }

        AVAudioPlayer* player = CreatePlayer(data, dataSize, volume);
        if (!player)
            return false;
        player.numberOfLoops = loop ? -1 : 0;
        slot = player;
        return [slot play] == YES;
    }
}

bool MacIsSoundChannelPlaying(MacSoundChannel channel)
{
    @autoreleasepool
    {
        AVAudioPlayer* player = PlayerForChannel(channel);
        return player && [player isPlaying];
    }
}

void MacStopSoundChannel(MacSoundChannel channel)
{
    @autoreleasepool
    {
        AVAudioPlayer*& slot = PlayerForChannel(channel);
        if (!slot)
            return;
        [slot stop];
#if !__has_feature(objc_arc)
        [slot release];
#endif
        slot = nil;
    }
}

void MacStopAllSounds()
{
    @autoreleasepool
    {
        if (gPlayers)
        {
            for (AVAudioPlayer* player in gPlayers)
                [player stop];
            [gPlayers removeAllObjects];
        }
        MacStopSoundChannel(MacSoundChannel::Music);
        MacStopSoundChannel(MacSoundChannel::Ambience);
        MacStopSoundChannel(MacSoundChannel::Voice);
    }
}
