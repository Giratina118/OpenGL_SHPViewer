#pragma once

struct LevelColor
{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};

constexpr LevelColor palette[9]
{
    {255, 20,  20},
    {255, 150, 20},
    {255, 255, 20},
    {150, 255, 80},
    {20,  255, 130},
    {20,  150, 255},
    {20,  20,  255},
    {150, 20,  255},
    {240, 80,  150},
};