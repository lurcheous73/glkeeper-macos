#pragma once

#ifdef __APPLE__

// Play a movie in a borderless fullscreen native macOS window.
// Returns true when playback was started; Escape skips cleanly.
bool MacPlayMovie(const char* moviePath);

#endif
