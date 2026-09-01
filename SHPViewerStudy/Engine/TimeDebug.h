#pragma once

#include <chrono>
#include <windows.h>

inline void PrintElapsedTime(const wchar_t* name, std::chrono::steady_clock::time_point start)
{
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    double elapsedMilliseconds = std::chrono::duration<double, std::milli>(end - start).count();

    wchar_t debugText[256];
    swprintf_s(debugText, L"[TIME] %s : %.3f ms\n", name, elapsedMilliseconds);
    OutputDebugStringW(debugText);
}