#pragma once

// The game streams music from its timer thread (0x410B00): it tests the DirectSound stream buffer
// (0x4C2D74) and then refills it, while the main thread stops and releases that buffer when the
// music changes (StopMusic 0x41165C, PlayMusic 0x4114B8, e.g. when a mission starts). A release
// between the test and the refill crashes the game (faults at 0x4109BE / 0x410AA3). This makes the
// thread's streaming section and the two release paths mutually exclusive.
void MusicInstall();
