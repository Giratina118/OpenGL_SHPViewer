#include "pch.h"
#include "CPanelRight.h"
#include <algorithm>

BEGIN_MESSAGE_MAP(CRightPanel, CWnd)
    ON_WM_CREATE()
    //ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

bool CRightPanel::Create(CWnd* pParent, UINT nID, CRect& rect)
{
    //m_bgBrush.CreateSolidBrush(RGB(240, 240, 240));
    // WS_VISIBLE 없이 시작 = 닫힌 상태
    m_clientWidth  = rect.Width();
    m_clientHeight = rect.Height();
    return CWnd::Create(AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1)), _T(""), WS_CHILD | WS_CLIPCHILDREN, CRect(0, 0, 10, 10), pParent, nID) == TRUE;
}

int CRightPanel::OnCreate(LPCREATESTRUCT lp)
{
    if (CWnd::OnCreate(lp) == -1) return -1;
    m_staticInfo.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(0, 0, 10, 10), this);
    return 0;
}

void CRightPanel::Resize(int32_t screenWidth, int32_t screenHeight)
{
    m_clientWidth  = screenWidth;
    m_clientHeight = screenHeight;

    int panelWidth = GetWidth();
    int marginX = panelWidth * 0.05;
    MoveWindow(screenWidth - panelWidth, 0, panelWidth, screenHeight);
    m_staticInfo.MoveWindow(marginX, marginX, panelWidth - marginX * 2, screenHeight - marginX * 2);

    int fontSize = std::max(10, screenHeight / 40);
    m_font.DeleteObject();
    m_font.CreateFont(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
    m_staticInfo.SetFont(&m_font);
}

void CRightPanel::SetPickingInfo(const CString& text)
{
    m_staticInfo.SetWindowText(text);
    Show(true); // 피킹 정보가 오면 자동으로 열림
}

void CRightPanel::Show(bool show)
{
    if (m_isShowPanel == show) return;

    ShowWindow(show ? SW_SHOW : SW_HIDE);
    m_isShowPanel = show;

    if (show) Resize(m_clientWidth, m_clientHeight);
    else      Resize(m_clientWidth, m_clientHeight);
}

/*
HBRUSH CRightPanel::OnCtlColor(CDC* pDC, CWnd*, UINT)
{
    pDC->SetBkColor(RGB(240, 240, 240));
    return (HBRUSH)m_bgBrush;
}
*/