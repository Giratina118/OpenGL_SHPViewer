#include <pch.h>
#include "CControlPage.h"
#include <algorithm>

bool CControlPage::Create(CWnd* pParent, UINT nID)
{
    return CWnd::Create(AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1)), _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, CRect(0, 0, 10, 10), pParent, nID) == TRUE;
    //bool bResult = CWnd::Create(AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1)), _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, CRect(0, 0, 10, 10), pParent, nID) == TRUE;
    //if (bResult) CreateTabControls();
    //return bResult;
}

void CControlPage::CreateTabControls()
{
    m_staticChangeInfo.Create(_T("데이터 대기중..."), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(0, 0, 10, 10), this);
    m_staticInfo.Create(_T("이동: WASD, 좌클릭\n줌: R/F, 휠\n회전: 방향키, Q/E, 우클릭"), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(0, 0, 10, 10), this);
}

void CControlPage::Resize(int width, int height)
{
    int marginX = width * 0.05, buttonWidth = width - marginX * 2;
    int buttonHeight = height * 0.07, gapHeight = height * 0.01;

    // 탭1 내부
    m_staticChangeInfo.MoveWindow(0, 0, buttonWidth, buttonHeight * 3.5);
    m_staticInfo.MoveWindow(0, buttonHeight * 4 + gapHeight, buttonWidth, buttonHeight * 2);

    // 폰트
    int fontSize = std::max(10, height / 32);
    m_font.DeleteObject();
    m_font.CreateFont(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

    auto applyFont = [&](CWnd& w) { if (w.GetSafeHwnd()) w.SetFont(&m_font); };
    applyFont(m_staticChangeInfo); 
    applyFont(m_staticInfo);
}

void CControlPage::UpdateInfo(float fps, int total, int rendered, int fake, int cameraAltitude, double scalePerCm)
{
    CString text;
    text.Format(_T("FPS : %.1f\r\n전체 객체: %d\r\n렌더 객체: %d\r\n가상 객체: %d\r\n현재 고도: %dm\r\n축척: 1cm 당 %.1f m"), fps, total, rendered, fake, cameraAltitude, scalePerCm);
    if (m_staticChangeInfo.GetSafeHwnd()) {
        m_staticChangeInfo.SetWindowText(text);
        m_staticChangeInfo.Invalidate(FALSE);
    }
}