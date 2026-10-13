#pragma once
// Declaration-only adapter for the Windows persistence branch. No OS emulation.
#ifndef NOMINMAX
#define min(a,b) WINDOWS_MIN_MACRO_MUST_NOT_REACH_CPP(a,b)
#define max(a,b) WINDOWS_MAX_MACRO_MUST_NOT_REACH_CPP(a,b)
#endif
constexpr unsigned MOVEFILE_REPLACE_EXISTING=1, MOVEFILE_WRITE_THROUGH=8;
extern "C" int MoveFileExA(const char *,const char *,unsigned);
