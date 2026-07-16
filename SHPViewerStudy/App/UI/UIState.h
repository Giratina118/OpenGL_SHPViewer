#pragma once

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
    bool isCameraThirdMode = false; // 카메라 모드, false: 1인칭(기본), true: 3인칭
    bool isPickingMode     = false; // 피킹 모드, 버튼으로 on/off
};

// UI 크기 정보
struct UISize
{
	int32_t clientWidth;
	int32_t clientHeight;
	int32_t panelWidth;

    int32_t marginX;
    int32_t marginY;
    int32_t buttonWidth;
    int32_t buttonHeight;

    int32_t fontSize;
    CFont   font;
    bool    isFontChanged = true;

    void SetSize(int32_t width, int32_t height) { // 새로 변경된 화면 크기 정보 저장
        clientWidth  = width;
        clientHeight = height;
        panelWidth   = static_cast<int32_t>(clientWidth  * 0.2);
        marginX      = static_cast<int32_t>(panelWidth   * 0.05);
        marginY      = static_cast<int32_t>(clientHeight * 0.01);
        buttonWidth  = panelWidth - marginX * 2;
        buttonHeight = static_cast<int32_t>(clientHeight * 0.07);

        // 양쪽 패널 사이즈 재지정
        int32_t newFontSize = std::max(10, clientHeight / 32);
        if (fontSize != newFontSize) {
            fontSize = newFontSize;
            font.DeleteObject();
            font.CreateFont(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
            isFontChanged = true;
        }
    }
};