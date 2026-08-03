#include <pch.h>
#include "CPickingPage.h"
#include "resource.h"

BEGIN_MESSAGE_MAP(CPickingPage, CWnd)
    ON_BN_CLICKED(ID_BTN_PICKING,            &CPickingPage::OnBtnPicking)
    ON_BN_CLICKED(ID_BTN_FIRST_PERSON_VIEW,  &CPickingPage::OnBtnFirstPerson)
    ON_BN_CLICKED(ID_BTN_THIRD_PERSON_VIEW,  &CPickingPage::OnBtnThirdPerson)
    ON_BN_CLICKED(ID_BTN_EDIT_OBJECT,        &CPickingPage::OnBtnEditObjectMode)
    ON_BN_CLICKED(ID_BTN_EDIT_OBJECT_SAVE,   &CPickingPage::OnBtnEditObjectSave)
    ON_BN_CLICKED(ID_BTN_EDIT_OBJECT_CANCLE, &CPickingPage::OnBtnEditObjectCancle)
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
    m_staticPickingInfo.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_LEFT,     CRect(0, 0, 10, 10), this);
    m_buttonEditObjectMode.Create(_T("객체 편집 모드 ON/OFF"), tog,         CRect(0, 0, 10, 10), this, ID_BTN_EDIT_OBJECT);
    m_buttonEditObjectSave.Create(_T("편집 저장"),   WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), this, ID_BTN_EDIT_OBJECT_SAVE);
    m_buttonEditObjectCancle.Create(_T("편집 취소"), WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), this, ID_BTN_EDIT_OBJECT_CANCLE);
}

void CPickingPage::Resize(UISize& uiSize)
{
	int32_t halfBtnWidth = uiSize.buttonWidth / 2;
    int32_t btnHeightGap = uiSize.buttonHeight    + uiSize.marginY;

    // 탭3 내부
    m_buttonPicking.MoveWindow         (0,            0,                uiSize.buttonWidth, uiSize.buttonHeight);
    m_radioFirstPerson.MoveWindow      (0,            btnHeightGap,     halfBtnWidth,       uiSize.buttonHeight);
    m_radioThirdPerson.MoveWindow      (halfBtnWidth, btnHeightGap,     halfBtnWidth,       uiSize.buttonHeight);
    m_staticPickingInfo.MoveWindow     (0,            btnHeightGap * 2, uiSize.buttonWidth, uiSize.buttonHeight * 3);
    m_buttonEditObjectMode.MoveWindow  (0,            btnHeightGap * 5, uiSize.buttonWidth, uiSize.buttonHeight);
    m_buttonEditObjectSave.MoveWindow  (0,            btnHeightGap * 6, halfBtnWidth,       uiSize.buttonHeight);
    m_buttonEditObjectCancle.MoveWindow(halfBtnWidth, btnHeightGap * 6, halfBtnWidth,       uiSize.buttonHeight);

    // 폰트
    if (uiSize.isFontChanged) {
        auto applyFont = [&](CWnd& w) { if (w.GetSafeHwnd()) w.SetFont(&uiSize.font); };
        applyFont(m_buttonPicking);
        applyFont(m_radioFirstPerson);
        applyFont(m_radioThirdPerson);
        applyFont(m_staticPickingInfo);
        applyFont(m_buttonEditObjectMode);
        applyFont(m_buttonEditObjectSave);
        applyFont(m_buttonEditObjectCancle);
    }
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

void CPickingPage::OnBtnEditObjectMode()
{
    if (m_callback.onEditObjectMode)
        m_callback.onEditObjectMode(m_buttonEditObjectMode.GetCheck() == BST_CHECKED);
}

void CPickingPage::OnBtnEditObjectSave()
{
    if (m_callback.onEditObjectSave)
        m_callback.onEditObjectSave(true);
}

void CPickingPage::OnBtnEditObjectCancle()
{
    if (m_callback.onEditObjectCancle)
        m_callback.onEditObjectCancle(true);
}

void CPickingPage::UpdatePickingInfo(double pointX, double pointY)
{
    CString text;
    text.Format(_T("선택 좌표\nx :  %.6f\r\ny :  %.6f\r\n"), pointX, pointY);
    if (m_staticPickingInfo.GetSafeHwnd())
        m_staticPickingInfo.SetWindowText(text);
}