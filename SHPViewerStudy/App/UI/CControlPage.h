#pragma once
#include "framework.h"
#include "CLayerListCtrl.h"
#include "UIState.h"

class LayerManager;

class CControlPage : public CWnd
{
public:
    bool Create(CWnd* parent, UINT id);
    void CreateTabControls();
    void Resize(UISize& uiSize);
    void UpdateInfo(float fps, int32_t total, int32_t rendered, int32_t fake, int32_t cameraAltitude, double scalePerCm); // View에서 호출 -> 텍스트 갱신
    void RefreshLayerList(const LayerManager& layerManager);
private:
    CLayerListCtrl m_listCtrlLayer;
    CStatic m_staticChangeInfo;
    CStatic m_staticInfo;
    CFont   m_font;
};