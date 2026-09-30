#pragma once

// Applies every "*.wwp" patch file in dir except "feature-*.wwp" (features apply
// those themselves when enabled). Returns the number of files applied.
//
// File format, one directive per line ('#' starts a comment):
//   verify <hexaddr> <hex bytes...>   bytes that must already be present
//   patch  <hexaddr> <hex bytes...>   bytes to write
// A file is applied atomically: if any verify or patch-original check fails,
// nothing from that file is written.
int PatchApplyDirectory(const char* dir);

// Applies one patch file (same format). Returns false if it was skipped or missing.
bool PatchApplyFile(const char* path);

bool PatchWrite(DWORD addr, const void* bytes, size_t len);
