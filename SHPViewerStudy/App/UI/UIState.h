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

// UI 토글 상태 관리
struct UIState
{
    bool isShowObjectMBR   = false;
    bool isShowNodeMBR     = false;
    bool isShowLevelColor  = false;
    bool isShowFrustumView = false;
    bool isShowColorLegend = false;
    bool isShowFakeObject  = false;
    bool isShowBuilding    = false;
};

// UI 크기 정보
struct UISize
{
	int32_t clientWidth;
	int32_t clientHeight;
	int32_t panelWidth;

    int32_t marginX;
    int32_t buttonWidth;
    int32_t buttonHeight;
    int32_t gapHeight;

    int32_t fontSize;
    CFont   font;
    bool    isFontChanged = true;
};