#pragma once
#include "framework.h"
#include "CLayerListCtrl.h"
#include "UIState.h"

class LayerManager;

struct ControllCallbacks
{
    std::function<void(int32_t)> onDeleteLayer;
};

class CControlPage : public CWnd
{
public:
    bool Create(CWnd* parent, UINT id);
    void CreateTabControls();
    void Resize(UISize& uiSize);
    void UpdateInfo(float fps, int32_t total, int32_t rendered, int32_t fake, int32_t cameraAltitude, double scalePerCm); // View에서 호출 -> 텍스트 갱신
    void RefreshLayerList(LayerManager& layerManager);
    void SetCallbacks(const ControllCallbacks& callback) { m_callback = callback; }
private:
    CLayerListCtrl    m_listCtrlLayer;
    ControllCallbacks m_callback;
    CStatic m_staticChangeInfo;
    CStatic m_staticInfo;
    CButton m_buttonDeleteLayer;
    CFont   m_font;

    afx_msg void OnBtnDeleteLayer();
    DECLARE_MESSAGE_MAP()
};