#pragma once

#include <cstdint>
#include <vector>
#include <GLES3/gl3.h>

class VWorldTexture
{
public:
    static GLuint Create(const std::vector<uint8_t>& imageData);
};