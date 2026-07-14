#pragma once

#include "framework.h"
#include "UIState.h"
#include <functional>

struct VisibilityCallbacks
{
    std::function<void(bool)> onObjectMBR;
    std::function<void(bool)> onNodeMBR;
    std::function<void(bool)> onLevelColor;
    std::function<void(bool)> onFrustumView;
    std::function<void(bool)> onFakeObject;
    std::function<void(bool)> onBuilding;
    std::function<void(bool)> onMap;
    std::function<void(int)>  onViewRange;
};

class CVisibilityPage : public CWnd
{
public:
    bool Create(CWnd* parent, UINT id);
    void CreateTabControls();
    void Resize(UISize& uiSize);
    void SetCallbacks(const VisibilityCallbacks& callback) { m_callback = callback; }
    void SetLegendColors();

private:
    VisibilityCallbacks m_callback;

    CButton m_buttonObjectMBR;
    CButton m_buttonNodeMBR;
    CButton m_buttonLevelColor;
    CButton m_buttonFrustumView;
    CButton m_buttonFakeObject;
    CButton m_buttonBuilding;
    CButton m_buttonMap;
    CButton m_radioCDT;
    CButton m_radioEarClipping;

    CStatic m_staticViewRange;
    CStatic m_staticSliderValueMin;
    CStatic m_staticSliderValueMid;
    CStatic m_staticSliderValueMax;
    CSliderCtrl m_sliderViewRange;
    //CFont m_font;
    CFont m_fontHalf;

    // 색상 범례 브러시
    static constexpr int32_t kLegendCount = 9;
    CBrush  m_legendBrush[kLegendCount];
    CStatic m_legendSwatch[kLegendCount];
    CStatic m_staticColorInfo;

    //afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnBtnObjectMBR();
    afx_msg void OnBtnNodeMBR();
    afx_msg void OnBtnLevelColor();
    afx_msg void OnBtnFrustumView();
    afx_msg void OnBtnFakeObject();
    afx_msg void OnBtnBuilding();
    afx_msg void OnBtnMap();
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    DECLARE_MESSAGE_MAP()
};