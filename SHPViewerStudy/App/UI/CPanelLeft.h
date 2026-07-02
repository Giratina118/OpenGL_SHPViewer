#pragma once
#include "framework.h"
#include "CControlPage.h"
#include "CVisibilityPage.h"
#include "CPickingPage.h"
#include <functional>

// View → 패널에 전달할 콜백 묶음
struct LeftPanelCallbacks
{
    VisibilityCallbacks visibilityCallbacks;
    PickingCallbacks    pickingCallbacks;
};

class CLeftPanel : public CWnd
{
public:
    CLeftPanel() = default;
    ~CLeftPanel() = default;

    bool Create(CWnd* pParent, UINT nID, CRect& rect);
    void Resize(int panelWidth, int panelHeight);
    void SetCallbacks(const LeftPanelCallbacks& callbak);

    // View에서 호출 -> 텍스트 갱신
    void UpdateInfo(float fps, int total, int rendered, int fake, int cameraAltitude, double scalePerCm) { m_pageControl.UpdateInfo(fps, total, rendered, fake, cameraAltitude, scalePerCm); }
    void UpdatePickingInfo(glm::dvec3 m_hitPoint) { m_pagePicking.UpdatePickingInfo(m_hitPoint.x, m_hitPoint.y); }
    int32_t GetWidth() const { return static_cast<int32_t>(m_clientWidth * m_panelRate); }

private:
    LeftPanelCallbacks m_callback;
    CTabCtrl  m_tab;

    // 탭별 컨테이너 (CWnd)
    CControlPage    m_pageControl;    // 조작 정보
    CVisibilityPage m_pageVisibility; // 가시 모드
    CPickingPage    m_pagePicking;    // 피킹

    CBrush m_bgBrush;
    CFont  m_font;

    int32_t m_clientWidth  = 1;
    int32_t m_clientHeight = 1;
    float   m_panelRate    = 0.2f;

    void ShowPage(int tabIndex);

    // 메시지 맵
    afx_msg int  OnCreate(LPCREATESTRUCT lp);
    afx_msg void OnTcnSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    //afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    DECLARE_MESSAGE_MAP()
};