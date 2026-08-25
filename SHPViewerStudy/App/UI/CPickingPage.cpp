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
    ON_BN_CLICKED(ID_BTN_CREATE_CIRCLE,      &CPickingPage::OnBtnCreateCircle)
    ON_BN_CLICKED(ID_BTN_CREATE_RECTANGLE,   &CPickingPage::OnBtnCreateRectangle)
    ON_BN_CLICKED(ID_BTN_CREATE_TRIANGLE,    &CPickingPage::OnBtnCreateTriangle)
    ON_BN_CLICKED(ID_BTN_DELETE_OBJECT,      &CPickingPage::OnBtnDeleteObject)
END_MESSAGE_MAP()

bool CPickingPage::Create(CWnd* pParent, UINT nID)
{
    //m_bgBrush.CreateSolidBrush(RGB(240, 240, 240));
    return CWnd::Create(AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1)), _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, CRect(0, 0, 10, 10), pParent, nID) == TRUE;
}

void CPickingPage::CreateTabControls()
{
    const DWORD btnToggle   = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE;
    const DWORD btnRadio    = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    const DWORD btnPush     = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
    const DWORD staticBasic = WS_CHILD | WS_VISIBLE | SS_LEFT;
    m_buttonPicking.Create(_T("피킹 ON/OFF"), btnToggle, CRect(0, 0, 10, 10), this, ID_BTN_PICKING);
    m_radioFirstPerson.Create(_T("1인칭"),    btnRadio,  CRect(0, 0, 10, 10), this, ID_BTN_FIRST_PERSON_VIEW);
    m_radioThirdPerson.Create(_T("3인칭"),    btnRadio,  CRect(0, 0, 10, 10), this, ID_BTN_THIRD_PERSON_VIEW);
    m_radioFirstPerson.SetCheck(BST_CHECKED); // 기본값
    m_staticPickingInfo.Create(_T(""), staticBasic, CRect(0, 0, 10, 10), this);
    m_buttonEditObjectMode.Create(_T("편집 모드 ON/OFF"), btnToggle, CRect(0, 0, 10, 10), this, ID_BTN_EDIT_OBJECT);
    m_buttonEditObjectSave.Create(_T("편집 저장"),        btnPush,   CRect(0, 0, 10, 10), this, ID_BTN_EDIT_OBJECT_SAVE);
    m_buttonEditObjectCancle.Create(_T("편집 취소"),      btnPush,   CRect(0, 0, 10, 10), this, ID_BTN_EDIT_OBJECT_CANCLE);
    m_staticCreateObject.Create(_T("객체 생성"), staticBasic, CRect(0, 0, 10, 10), this);
    m_buttonCreateCircle.Create(_T("○"),        btnPush,     CRect(0, 0, 10, 10), this, ID_BTN_CREATE_CIRCLE);
    m_buttonCreateRectangle.Create(_T("□"),     btnPush,     CRect(0, 0, 10, 10), this, ID_BTN_CREATE_RECTANGLE);
    m_buttonCreateTriangle.Create(_T("△"),      btnPush,     CRect(0, 0, 10, 10), this, ID_BTN_CREATE_TRIANGLE);
    m_buttonDeleteObject.Create(_T("객체 삭제"), btnPush,     CRect(0, 0, 10, 10), this, ID_BTN_DELETE_OBJECT);
}

void CPickingPage::Resize(UISize& uiSize)
{
	int32_t halfBtnWidth  = uiSize.buttonWidth / 2;
	int32_t thirdBtnWidth = uiSize.buttonWidth / 3;
    int32_t btnHeightGap  = uiSize.buttonHeight + uiSize.marginY;

    // 탭3 내부
    m_buttonPicking.MoveWindow         (0,                 0,                 uiSize.buttonWidth, uiSize.buttonHeight);
    m_radioFirstPerson.MoveWindow      (0,                 btnHeightGap,      halfBtnWidth,       uiSize.buttonHeight);
    m_radioThirdPerson.MoveWindow      (halfBtnWidth,      btnHeightGap,      halfBtnWidth,       uiSize.buttonHeight);
    m_staticPickingInfo.MoveWindow     (0,                 btnHeightGap * 2,  uiSize.buttonWidth, uiSize.buttonHeight * 3);
    m_buttonEditObjectMode.MoveWindow  (0,                 btnHeightGap * 5,  uiSize.buttonWidth, uiSize.buttonHeight);
    m_buttonEditObjectSave.MoveWindow  (0,                 btnHeightGap * 6,  halfBtnWidth,       uiSize.buttonHeight);
    m_buttonEditObjectCancle.MoveWindow(halfBtnWidth,      btnHeightGap * 6,  halfBtnWidth,       uiSize.buttonHeight);
    m_buttonDeleteObject.MoveWindow    (0,                 btnHeightGap * 7,  uiSize.buttonWidth, uiSize.buttonHeight);
    m_staticCreateObject.MoveWindow    (0,                 btnHeightGap * 9,  uiSize.buttonWidth, uiSize.buttonHeight);
    m_buttonCreateCircle.MoveWindow    (0,                 btnHeightGap * 10, thirdBtnWidth,      uiSize.buttonHeight * 2);
    m_buttonCreateRectangle.MoveWindow (thirdBtnWidth,     btnHeightGap * 10, thirdBtnWidth,      uiSize.buttonHeight * 2);
    m_buttonCreateTriangle.MoveWindow  (thirdBtnWidth * 2, btnHeightGap * 10, thirdBtnWidth,      uiSize.buttonHeight * 2);

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
        applyFont(m_staticCreateObject);
        applyFont(m_buttonDeleteObject);
        
        m_fontIcon.CreateFont(-(uiSize.fontSize * 2), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("맑은 고딕"));
        auto applyFontIcon = [&](CWnd& w) { if (w.GetSafeHwnd()) w.SetFont(&m_fontIcon); };
        applyFontIcon(m_buttonCreateCircle);
        applyFontIcon(m_buttonCreateRectangle);
        applyFontIcon(m_buttonCreateTriangle);
    }
}

void CPickingPage::UpdatePickingInfo(double pointX, double pointY)
{
    CString text;
    text.Format(_T("선택 좌표\nx :  %.6f\r\ny :  %.6f\r\n"), pointX, pointY);
    if (m_staticPickingInfo.GetSafeHwnd())
        m_staticPickingInfo.SetWindowText(text);
}

// 콜백 전달
void CPickingPage::OnBtnPicking()
{
    bool isPicking = (m_buttonPicking.GetCheck() == BST_CHECKED);

    if (m_callback.onPicking) {
        m_callback.onPicking(isPicking);
    }

    // 피킹 모드를 켤 때만 편집 모드 버튼과 플래그를 해제
    if (isPicking) {
        m_buttonEditObjectMode.SetCheck(BST_UNCHECKED);
        if (m_callback.onEditObjectMode) {
            m_callback.onEditObjectMode(false);
        }
    }
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
    if (m_callback.onEditObjectMode) {
        m_callback.onEditObjectMode(m_buttonEditObjectMode.GetCheck() == BST_CHECKED);
        m_buttonPicking.SetCheck(BST_UNCHECKED);
        m_callback.onPicking(false);
    }
}

void CPickingPage::OnBtnEditObjectSave()
{
    if (m_callback.onEditObjectSave) {
        m_callback.onEditObjectSave(true);
        //m_buttonEditObjectMode.SetCheck(BST_UNCHECKED);
        //m_callback.onEditObjectMode(false);
    }
}

void CPickingPage::OnBtnEditObjectCancle()
{
    if (m_callback.onEditObjectCancle) {
        m_callback.onEditObjectCancle(true);
		//m_buttonEditObjectMode.SetCheck(BST_UNCHECKED);
        //m_callback.onEditObjectMode(false);
    }
}

void CPickingPage::OnBtnDeleteObject()
{
    if (m_callback.onDeleteObject) {
        m_callback.onDeleteObject(true);

        TCHAR buf[256]; _stprintf_s(buf, _T("객체 삭제\n")); OutputDebugString(buf);
    }
}

void CPickingPage::OnBtnCreateCircle()
{
    if (m_callback.onCreateCircle) {
        m_callback.onCreateCircle(true);

        TCHAR buf[256]; _stprintf_s(buf, _T("원\n")); OutputDebugString(buf);
    }
}

void CPickingPage::OnBtnCreateRectangle()
{
    if (m_callback.onCreateRectangle) {
        m_callback.onCreateRectangle(true);

        TCHAR buf[256]; _stprintf_s(buf, _T("사각형\n")); OutputDebugString(buf);
    }
}

void CPickingPage::OnBtnCreateTriangle()
{
    if (m_callback.onCreateTriangle) {
        m_callback.onCreateTriangle(true);

        TCHAR buf[256]; _stprintf_s(buf, _T("삼각형\n")); OutputDebugString(buf);
    }
}