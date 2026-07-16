#include "pch.h"
#include "CPanelLeft.h"
#include "resource.h"

BEGIN_MESSAGE_MAP(CLeftPanel, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_HSCROLL()
    ON_NOTIFY(TCN_SELCHANGE, ID_TAB_LEFT, &CLeftPanel::OnTcnSelChange)
END_MESSAGE_MAP()

bool CLeftPanel::Create(CWnd* pParent, UINT nID, CRect& rect)
{
    m_bgBrush.CreateSolidBrush(RGB(240, 240, 240));
    return CWnd::Create(AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1)), _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, CRect(0, 0, 10, 10), pParent, nID) == TRUE;
}

int CLeftPanel::OnCreate(LPCREATESTRUCT lp)
{
    if (CWnd::OnCreate(lp) == -1) return -1;

    // 탭 컨트롤 생성
    m_tab.Create(WS_CHILD | WS_VISIBLE | TCS_TABS, CRect(0, 0, 10, 10), this, ID_TAB_LEFT);
    m_tab.InsertItem(0, _T("조작"));
    m_tab.InsertItem(1, _T("가시"));
    m_tab.InsertItem(2, _T("피킹"));

    // 탭별 페이지 컨테이너
    DWORD pageStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
    m_pageControl.Create   (this, ID_PANNEL_LEFT_PAGE1);
    m_pageVisibility.Create(this, ID_PANNEL_LEFT_PAGE2);
    m_pagePicking.Create   (this, ID_PANNEL_LEFT_PAGE3);

    m_pageControl.CreateTabControls();
    m_pageVisibility.CreateTabControls();
    m_pagePicking.CreateTabControls();

    ShowPage(0); // 탭1이 기본
    return 0;
}

void CLeftPanel::ShowPage(int32_t idx)
{
    m_pageControl.ShowWindow   (idx == 0 ? SW_SHOW : SW_HIDE);
    m_pageVisibility.ShowWindow(idx == 1 ? SW_SHOW : SW_HIDE);
    m_pagePicking.ShowWindow   (idx == 2 ? SW_SHOW : SW_HIDE);
}

void CLeftPanel::OnTcnSelChange(NMHDR*, LRESULT* pResult)
{
    ShowPage(m_tab.GetCurSel());
    *pResult = 0;
}

void CLeftPanel::Resize()
{
    MoveWindow(0, 0, m_uiSize.panelWidth, m_uiSize.clientHeight);

    int32_t tabHeight  = std::max(10, m_uiSize.clientHeight / 16);
    int32_t pageTop    = tabHeight + 4;
    int32_t pageHeight = m_uiSize.clientHeight - pageTop;

    m_tab.MoveWindow(0, 0, m_uiSize.panelWidth, tabHeight + pageHeight);
    m_tab.SetItemSize(CSize(m_uiSize.panelWidth, tabHeight));

    // 폰트
    if (m_uiSize.isFontChanged) {
        int32_t fontSize = std::max(10, m_uiSize.clientHeight / 36);
        m_font.DeleteObject();
        m_font.CreateFont(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

        auto applyFont = [&](CWnd& w) { if (w.GetSafeHwnd()) w.SetFont(&m_font); };
        applyFont(m_tab);
    }

    // 세 페이지 모두 같은 위치에 겹쳐 배치 (탭이 전환할 때 ShowWindow로 교체)
    CRect pageRect(m_uiSize.marginX, pageTop + m_uiSize.marginY, m_uiSize.panelWidth - m_uiSize.marginX, pageTop + pageHeight - m_uiSize.marginY);
    m_pageControl.MoveWindow(pageRect);
    m_pageVisibility.MoveWindow(pageRect);
    m_pagePicking.MoveWindow(pageRect);

    m_pageControl.Resize(m_uiSize);
    m_pageVisibility.Resize(m_uiSize);
    m_pagePicking.Resize(m_uiSize);
}

void CLeftPanel::SetCallbacks(const LeftPanelCallbacks& callbak)
{
    m_callback = callbak;
	m_pageControl.SetCallbacks   (m_callback.controlCallbacks);
    m_pageVisibility.SetCallbacks(m_callback.visibilityCallbacks);
    m_pagePicking.SetCallbacks   (m_callback.pickingCallbacks);
}