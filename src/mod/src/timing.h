#pragma once

// The game clock is a counter advanced by a thread in 20 ms Sleep steps, so the 62 ms frame
// step of every screen (one simulation tick per mission frame) really takes 80 ms or more
// (about 12 frames per second). This keeps the counter on the real millisecond time instead,
// giving the intended 16 frames per second, scaled by GameSpeedPercent (the unmodified game on
// current Windows runs at roughly 60-70% of its designed speed, which players are used to).
void TimingInstall(const char* iniPath);
