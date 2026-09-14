#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>
#import <AVKit/AVKit.h>

#include "MacMoviePlayer.h"

@interface DKMovieWindow : NSWindow
@end

@implementation DKMovieWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
- (void)keyDown:(NSEvent*)event
{
    if (event.keyCode == 53) {
        [NSApp stop:nil];
        return;
    }
    [super keyDown:event];
}
@end

@interface DKMovieDelegate : NSObject
@property(nonatomic, strong) AVPlayer* player;
@property(nonatomic, strong) id endObserver;
@property(nonatomic, strong) id failObserver;
@end
@implementation DKMovieDelegate
- (void)dealloc
{
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    if (_endObserver) [center removeObserver:_endObserver];
    if (_failObserver) [center removeObserver:_failObserver];
}
@end

bool MacPlayMovie(const char* moviePath)
{
    if (!moviePath || !moviePath[0])
        return false;

    NSString* path = [NSString stringWithUTF8String:moviePath];
    if (![[NSFileManager defaultManager] fileExistsAtPath:path])
        return false;

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSScreen* screen = [NSScreen mainScreen];
    if (!screen)
        return false;

    DKMovieWindow* window = [[DKMovieWindow alloc]
        initWithContentRect:screen.frame
        styleMask:NSWindowStyleMaskBorderless
        backing:NSBackingStoreBuffered defer:NO];
    window.backgroundColor = NSColor.blackColor;
    window.opaque = YES;
    window.level = NSMainMenuWindowLevel + 1;
    window.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                                NSWindowCollectionBehaviorFullScreenAuxiliary;

    NSURL* url = [NSURL fileURLWithPath:path];
    AVPlayerItem* item = [AVPlayerItem playerItemWithURL:url];
    AVPlayer* player = [AVPlayer playerWithPlayerItem:item];

    AVPlayerView* playerView = [[AVPlayerView alloc] initWithFrame:window.contentView.bounds];
    playerView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    playerView.controlsStyle = AVPlayerViewControlsStyleNone;
    playerView.videoGravity = AVLayerVideoGravityResizeAspect;
    playerView.player = player;
    [window setContentView:playerView];

    DKMovieDelegate* delegate = [DKMovieDelegate new];
    delegate.player = player;
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    delegate.endObserver = [center addObserverForName:AVPlayerItemDidPlayToEndTimeNotification
        object:item queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) {
            [NSApp stop:nil];
        }];
    delegate.failObserver = [center addObserverForName:AVPlayerItemFailedToPlayToEndTimeNotification
        object:item queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification*) {
            [NSApp stop:nil];
        }];

    NSApplicationPresentationOptions oldOptions = NSApp.presentationOptions;
    NSApp.presentationOptions = NSApplicationPresentationHideDock |
                                NSApplicationPresentationHideMenuBar;

    [window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [player play];
    [NSApp run];

    [player pause];
    [window orderOut:nil];
    NSApp.presentationOptions = oldOptions;
    (void)delegate;
    return true;
}
