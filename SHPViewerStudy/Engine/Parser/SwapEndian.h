#pragma once

#include <cstdint>

// 빅 엔디언(32비트)을 리틀 엔디언으로 변환하는 함수
inline int32_t SwapEndian(int32_t val) {
    return ((val << 24) & 0xff000000) | ((val << 8) & 0x00ff0000) |
            ((val >> 8) & 0x0000ff00) | ((val >> 24) & 0x000000ff);
}