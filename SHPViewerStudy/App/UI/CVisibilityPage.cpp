#include <pch.h>
#include "CVisibilityPage.h"
#include "resource.h"
#include <array>

BEGIN_MESSAGE_MAP(CVisibilityPage, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_CTLCOLOR()
    ON_WM_HSCROLL()
    ON_WM_CTLCOLOR()
    ON_BN_CLICKED(ID_BTN_OBJECT_MBR,   &CVisibilityPage::OnBtnObjectMBR)
    ON_BN_CLICKED(ID_BTN_NODE_MBR,     &CVisibilityPage::OnBtnNodeMBR)
    ON_BN_CLICKED(ID_BTN_LEVEL_COLOR,  &CVisibilityPage::OnBtnLevelColor)
    ON_BN_CLICKED(ID_BTN_FRUSTUM_VIEW, &CVisibilityPage::OnBtnFrustumView)
    ON_BN_CLICKED(ID_BTN_FAKE_OBJECT,  &CVisibilityPage::OnBtnFakeObject)
    ON_BN_CLICKED(ID_BTN_BUILDING,     &CVisibilityPage::OnBtnBuilding)
    ON_BN_CLICKED(ID_BTN_MAP,          &CVisibilityPage::OnBtnMap)
    ON_BN_CLICKED(ID_BTN_TRIANGULATE_CDT, &CVisibilityPage::OnBtnCDT)
    ON_BN_CLICKED(ID_BTN_TRIANGULATE_EAR, &CVisibilityPage::OnBtnCDT)
END_MESSAGE_MAP()

bool CVisibilityPage::Create(CWnd* pParent, UINT nID)
{
    //m_bgBrush.CreateSolidBrush(RGB(240, 240, 240));
    return CWnd::Create(AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1)), _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, CRect(0, 0, 10, 10), pParent, nID) == TRUE;
}

void CVisibilityPage::CreateTabControls()
{
    const DWORD tog = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE;
    const DWORD rad = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    m_buttonObjectMBR.Create  (_T("객체 MBR"),    tog, CRect(0, 0, 10, 10), this, ID_BTN_OBJECT_MBR);
    m_buttonNodeMBR.Create    (_T("노드 MBR"),    tog, CRect(0, 0, 10, 10), this, ID_BTN_NODE_MBR);
    m_buttonLevelColor.Create (_T("객체 색상"),   tog, CRect(0, 0, 10, 10), this, ID_BTN_LEVEL_COLOR);
    m_buttonFrustumView.Create(_T("절두체 라인"), tog, CRect(0, 0, 10, 10), this, ID_BTN_FRUSTUM_VIEW);
    m_buttonFakeObject.Create (_T("가상 객체"),   tog, CRect(0, 0, 10, 10), this, ID_BTN_FAKE_OBJECT);
    m_buttonBuilding.Create   (_T("건물"),        tog, CRect(0, 0, 10, 10), this, ID_BTN_BUILDING);
    m_buttonMap.Create        (_T("지도"),        tog, CRect(0, 0, 10, 10), this, ID_BTN_MAP);
    //m_radioCDT.Create         (_T("CDT"),         rad, CRect(0, 0, 10, 10), this, ID_BTN_TRIANGULATE_CDT);
    //m_radioEarClipping.Create (_T("Ear"),         rad, CRect(0, 0, 10, 10), this, ID_BTN_TRIANGULATE_EAR);
    m_radioCDT.GetCheck();

    m_staticViewRange.Create  (_T("가시거리 배율: 40"), WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), this);
    m_sliderViewRange.Create  (WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, CRect(0, 0, 10, 10), this, ID_SLIDER_VIEW_RANGE);

    int32_t sliderMinValue = 4;
    int32_t sliderMaxValue = 100;
    int32_t sliderTicValue = 8;
    m_sliderViewRange.SetRange(sliderMinValue, sliderMaxValue);
    m_sliderViewRange.SetPos(40);
    m_sliderViewRange.SetTicFreq(sliderTicValue);

    // 슬라이더 간격 숫자 표시
    CString strText;
    strText.AppendFormat(_T(" %d  "), sliderMinValue);
    m_staticSliderValueMin.Create(strText, WS_CHILD | WS_VISIBLE | SS_LEFT,   CRect(0, 0, 10, 10), this);

    strText = "";
    strText.AppendFormat(_T("%d  "), (sliderMinValue + sliderMaxValue) / 2);
    m_staticSliderValueMid.Create(strText, WS_CHILD | WS_VISIBLE | SS_CENTER, CRect(0, 0, 10, 10), this);

    strText = "";
    strText.AppendFormat(_T("%d  "), sliderMaxValue);
    m_staticSliderValueMax.Create(strText, WS_CHILD | WS_VISIBLE | SS_RIGHT,  CRect(0, 0, 10, 10), this);

    // 색상 범례 생성
    m_staticColorInfo.Create(_T("색상범례 (Lv 0 -> Lv 8)"), WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), this);
    for (int i = 0; i < kLegendCount; i++)
        m_legendSwatch[i].Create(_T(""), WS_CHILD | WS_VISIBLE | SS_NOTIFY, CRect(0, 0, 10, 10), this);
}

void CVisibilityPage::Resize(int width, int height)
{
    int marginX = width * 0.05, buttonWidth = width - marginX * 2;
    int buttonHeight = height * 0.07, gapHeight = height * 0.01;

    // 탭2 내부
    for (int i = 0; i < 7; i++) {
        CWnd* btns[] = { &m_buttonObjectMBR,&m_buttonNodeMBR, &m_buttonLevelColor,&m_buttonFrustumView, &m_buttonFakeObject, &m_buttonBuilding, &m_buttonMap };
        btns[i]->MoveWindow(0, i * (buttonHeight + gapHeight), buttonWidth, buttonHeight);
    }
    //m_radioCDT.MoveWindow            (0,                   7.0 * (buttonHeight + gapHeight), buttonWidth / 2, buttonHeight);
    //m_radioEarClipping.MoveWindow    (buttonWidth / 2,     7.0 * (buttonHeight + gapHeight), buttonWidth / 2, buttonHeight);
    m_staticViewRange.MoveWindow     (0, 8.0 * (buttonHeight + gapHeight), buttonWidth, buttonHeight * 0.5);
    m_sliderViewRange.MoveWindow     (0, 8.5 * (buttonHeight + gapHeight), buttonWidth, buttonHeight * 0.5);
    m_staticSliderValueMin.MoveWindow(0,                   9.0 * (buttonHeight + gapHeight), buttonWidth / 3, buttonHeight * 0.5);
    m_staticSliderValueMid.MoveWindow(buttonWidth / 3,     9.0 * (buttonHeight + gapHeight), buttonWidth / 3, buttonHeight * 0.5);
    m_staticSliderValueMax.MoveWindow(buttonWidth / 3 * 2, 9.0 * (buttonHeight + gapHeight), buttonWidth / 3, buttonHeight * 0.5);
    m_staticColorInfo.MoveWindow     (0, 10.0 * (buttonHeight + gapHeight), buttonWidth, buttonHeight * 0.5);

    int legendY = 10.5 * (buttonHeight + gapHeight);
    for (int i = 0; i < kLegendCount; i++) {
        int sizeY = buttonHeight / 2;
        int sizeX = buttonWidth  / kLegendCount;
        m_legendSwatch[i].MoveWindow(i * sizeX, legendY, sizeX, sizeY);
    }
    SetLegendColors();

    // 폰트
    int fontSize = std::max(10, height / 32);
    m_font.DeleteObject();
    m_font.CreateFont(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

    auto applyFont = [&](CWnd& w, CFont& font) { if (w.GetSafeHwnd()) w.SetFont(&font); };
    applyFont(m_buttonObjectMBR,   m_font);
    applyFont(m_buttonNodeMBR,     m_font);
    applyFont(m_buttonLevelColor,  m_font);
    applyFont(m_buttonFrustumView, m_font);
    applyFont(m_buttonFakeObject,  m_font);
    applyFont(m_buttonBuilding,    m_font);
    applyFont(m_buttonMap,         m_font);
    //applyFont(m_radioCDT,          m_font);
    //applyFont(m_radioEarClipping,  m_font);
    applyFont(m_staticViewRange,   m_font);
    applyFont(m_staticColorInfo,   m_font);

    m_fontHalf.DeleteObject();
    m_fontHalf.CreateFont(-fontSize / 2, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
    applyFont(m_staticSliderValueMin, m_fontHalf);
    applyFont(m_staticSliderValueMid, m_fontHalf);
    applyFont(m_staticSliderValueMax, m_fontHalf);
}

// 콜백 전달
void CVisibilityPage::OnBtnObjectMBR()
{
    if (m_callback.onObjectMBR)   m_callback.onObjectMBR(m_buttonObjectMBR.GetCheck() == BST_CHECKED);
}
void CVisibilityPage::OnBtnNodeMBR()
{
    if (m_callback.onNodeMBR)     m_callback.onNodeMBR(m_buttonNodeMBR.GetCheck() == BST_CHECKED);
}
void CVisibilityPage::OnBtnLevelColor()
{
    if (m_callback.onLevelColor)  m_callback.onLevelColor(m_buttonLevelColor.GetCheck() == BST_CHECKED);
}
void CVisibilityPage::OnBtnFrustumView()
{
    if (m_callback.onFrustumView) m_callback.onFrustumView(m_buttonFrustumView.GetCheck() == BST_CHECKED);
}
void CVisibilityPage::OnBtnFakeObject()
{
    if (m_callback.onFakeObject)  m_callback.onFakeObject(m_buttonFakeObject.GetCheck() == BST_CHECKED);
}
void CVisibilityPage::OnBtnBuilding()
{
    if (m_callback.onBuilding)    m_callback.onBuilding(m_buttonBuilding.GetCheck() == BST_CHECKED);
}
void CVisibilityPage::OnBtnMap()
{
    if (m_callback.onMap)         m_callback.onMap(m_buttonMap.GetCheck() == BST_CHECKED);
}
void CVisibilityPage::OnBtnCDT()
{
    if (m_callback.onTriangulate) m_callback.onTriangulate(true);
}
void CVisibilityPage::OnBtnEarClipping()
{
    if (m_callback.onTriangulate) m_callback.onTriangulate(false);
}
void CVisibilityPage::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pSB)
{
    if (pSB->GetSafeHwnd() == m_sliderViewRange.GetSafeHwnd())
        if (m_callback.onViewRange) {
            m_callback.onViewRange(m_sliderViewRange.GetPos());

            CString text;
            text.Format(_T("가시거리 배율: %d"), m_sliderViewRange.GetPos());
            if (m_staticViewRange.GetSafeHwnd()) {
                m_staticViewRange.SetWindowText(text);
                m_staticViewRange.Invalidate(FALSE);
            }
        }
    CWnd::OnHScroll(nSBCode, nPos, pSB);
}

HBRUSH CVisibilityPage::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    for (int i = 0; i < kLegendCount; i++)
        if (pWnd->GetSafeHwnd() == m_legendSwatch[i].GetSafeHwnd())
            return (HBRUSH)m_legendBrush[i];

    return CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CVisibilityPage::SetLegendColors()
{
    for (int i = 0; i < 9; i++) {
        m_legendBrush[i].DeleteObject();
        m_legendBrush[i].CreateSolidBrush(RGB(palette[i].red, palette[i].green, palette[i].blue));
    }
}