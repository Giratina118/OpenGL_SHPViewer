#pragma once
#include "framework.h"
#include "CControlPage.h"
#include "CVisibilityPage.h"
#include "CPickingPage.h"
#include "UIState.h"
#include <functional>

// View → 패널에 전달할 콜백 묶음
struct LeftPanelCallbacks
{
	ControllCallbacks   controlCallbacks;
    VisibilityCallbacks visibilityCallbacks;
    PickingCallbacks    pickingCallbacks;
};

class CLeftPanel : public CWnd
{
public:
    CLeftPanel(UISize& uiSize) : m_uiSize(uiSize) {};
    ~CLeftPanel() {}

    bool Create(CWnd* pParent, UINT nID, CRect& rect);
    void Resize();
    void SetCallbacks(const LeftPanelCallbacks& callbak);

    // View에서 호출 -> 텍스트 갱신
    void UpdateInfo(float fps, int32_t total, int32_t rendered, int32_t fake, int32_t cameraAltitude) { m_pageControl.UpdateInfo(fps, total, rendered, fake, cameraAltitude); }
    void UpdatePickingInfo(glm::dvec3 m_hitPoint) { m_pagePicking.UpdatePickingInfo(m_hitPoint.x, m_hitPoint.y); }
    int32_t GetWidth() const { return static_cast<int32_t>(m_uiSize.clientWidth * m_panelRate); }

    // 탭별 컨테이너 (CWnd)
    CControlPage    m_pageControl;    // 조작 정보
    CVisibilityPage m_pageVisibility; // 가시 모드
    CPickingPage    m_pagePicking;    // 피킹

private:
    LeftPanelCallbacks m_callback;
    CTabCtrl m_tab;

    CBrush  m_bgBrush;
    CFont   m_font;
	UISize& m_uiSize;
    float   m_panelRate = 0.2f;

    void ShowPage(int32_t tabIndex);

    // 메시지 맵
    afx_msg int  OnCreate(LPCREATESTRUCT lp);
    afx_msg void OnTcnSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()
};