#include <pch.h>
#include "VWorldTexture.h"
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

GLuint VWorldTexture::Create(const std::vector<uint8_t>& imageData)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    unsigned char* pixels = stbi_load_from_memory(imageData.data(), static_cast<int>(imageData.size()), &width, &height, &channels, 4);

    if (pixels == nullptr)
    {
        OutputDebugStringA("[VWORLD] PNG Decode Failed\n");
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    OutputDebugStringA(("[VWORLD] Texture Created : " + std::to_string(width) + " x " + std::to_string(height) + "\n").c_str());

    return texture;
}