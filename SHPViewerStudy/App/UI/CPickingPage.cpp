#include <pch.h>
#include "CPickingPage.h"
#include "resource.h"

BEGIN_MESSAGE_MAP(CPickingPage, CWnd)
    ON_BN_CLICKED(ID_BTN_PICKING,           &CPickingPage::OnBtnPicking)
    ON_BN_CLICKED(ID_BTN_FIRST_PERSON_VIEW, &CPickingPage::OnBtnFirstPerson)
    ON_BN_CLICKED(ID_BTN_THIRD_PERSON_VIEW, &CPickingPage::OnBtnThirdPerson)
END_MESSAGE_MAP()

bool CPickingPage::Create(CWnd* pParent, UINT nID)
{
    //m_bgBrush.CreateSolidBrush(RGB(240, 240, 240));
    return CWnd::Create(AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1)), _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, CRect(0, 0, 10, 10), pParent, nID) == TRUE;
}

void CPickingPage::CreateTabControls()
{
    const DWORD tog = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE;
    const DWORD rad = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    m_buttonPicking.Create(_T("피킹 ON/OFF"), tog, CRect(0, 0, 10, 10), this, ID_BTN_PICKING);
    m_radioFirstPerson.Create(_T("1인칭"),    rad, CRect(0, 0, 10, 10), this, ID_BTN_FIRST_PERSON_VIEW);
    m_radioThirdPerson.Create(_T("3인칭"),    rad, CRect(0, 0, 10, 10), this, ID_BTN_THIRD_PERSON_VIEW);
    m_radioFirstPerson.SetCheck(BST_CHECKED); // 기본값
    m_staticPickingInfo.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(0, 0, 10, 10), this);
}

void CPickingPage::Resize(int width, int height)
{
    int marginX = width * 0.05, buttonWidth = width - marginX * 2;
    int buttonHeight = height * 0.07, gapHeight = height * 0.01;

    // 탭3 내부
    m_buttonPicking.MoveWindow(0, 0, buttonWidth, buttonHeight);
    m_radioFirstPerson.MoveWindow(0, buttonHeight + gapHeight, buttonWidth / 2 - gapHeight, buttonHeight);
    m_radioThirdPerson.MoveWindow(buttonWidth / 2 + gapHeight, buttonHeight + gapHeight, buttonWidth / 2 - gapHeight, buttonHeight);
    m_staticPickingInfo.MoveWindow(0, (buttonHeight + gapHeight) * 2, buttonWidth, buttonHeight * 3);

    // 폰트
    int fontSize = std::max(10, height / 32);
    m_font.DeleteObject();
    m_font.CreateFont(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

    auto applyFont = [&](CWnd& w) { if (w.GetSafeHwnd()) w.SetFont(&m_font); };
    applyFont(m_buttonPicking);    
    applyFont(m_radioFirstPerson);
    applyFont(m_radioThirdPerson);
    applyFont(m_staticPickingInfo);
}

// 콜백 전달
void CPickingPage::OnBtnPicking()
{
    if (m_callback.onPicking)
        m_callback.onPicking(m_buttonPicking.GetCheck() == BST_CHECKED);
}
void CPickingPage::OnBtnFirstPerson()
{
    if (m_callback.onThirdMode)
        m_callback.onThirdMode(false);
}
void CPickingPage::OnBtnThirdPerson()
{
    if (m_callback.onThirdMode)
        m_callback.onThirdMode(true);
}

void CPickingPage::UpdatePickingInfo(double pointX, double pointY)
{
    CString text;
    text.Format(_T("선택 좌표\nx :  %.1f\r\ny :  %.1f\r\n"), pointX, pointY);
    if (m_staticPickingInfo.GetSafeHwnd())
        m_staticPickingInfo.SetWindowText(text);
}