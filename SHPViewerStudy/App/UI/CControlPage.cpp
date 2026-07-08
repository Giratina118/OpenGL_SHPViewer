#include <pch.h>
#include "CControlPage.h"
#include "CLayerListCtrl.h" // 위치 잘 찾아서 다시 만들기
#include <resource.h>
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

    // LVS_OWNERDRAWFIXED 필수, 스크롤은 WS_VSCROLL 또는 자동
    m_listCtrlLayer.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | LVS_REPORT | LVS_OWNERDRAWFIXED | LVS_SINGLESEL | LVS_NOCOLUMNHEADER, CRect(0, 0, 10, 10), this, IDC_LAYER_LIST);

    m_listCtrlLayer.Init();

    // 테스트 아이템 추가 (실제로는 LayerManager에서 받아와야 함)
    m_listCtrlLayer.AddLayer(_T("부산 건물"), 2, true);  // Polygon
    m_listCtrlLayer.AddLayer(_T("도로 네트워크"), 1, true);  // Line
    m_listCtrlLayer.AddLayer(_T("버스 정류장"), 0, false); // Point

}

void CControlPage::Resize(int width, int height)
{
    int marginX = width * 0.05, buttonWidth = width - marginX * 2;
    int buttonHeight = height * 0.07, gapHeight = height * 0.01;

    // 탭1 내부
    m_staticChangeInfo.MoveWindow(0, 0, buttonWidth, buttonHeight * 3.5);
    m_staticInfo.MoveWindow(0, buttonHeight * 4 + gapHeight, buttonWidth, buttonHeight * 2);
	m_listCtrlLayer.MoveWindow(0, buttonHeight * 7 + gapHeight * 2, buttonWidth, buttonHeight * 6);


    // 폰트
    int fontSize = std::max(10, height / 32);
    m_font.DeleteObject();
    m_font.CreateFont(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));


    auto applyFont = [&](CWnd& w) { if (w.GetSafeHwnd()) w.SetFont(&m_font); };
    applyFont(m_staticChangeInfo); 
    applyFont(m_staticInfo);
    applyFont(m_listCtrlLayer);
}

void CControlPage::UpdateInfo(float fps, int total, int rendered, int fake, int cameraAltitude, double scalePerCm)
{
    CString text;
    text.Format(_T("FPS : %.1f\r\n전체 객체: %d\r\n렌더 객체: %d\r\n가상 객체: %d\r\n현재 고도: %dm\r\n"), fps, total, rendered, fake, cameraAltitude);
    if (m_staticChangeInfo.GetSafeHwnd()) {
        m_staticChangeInfo.SetWindowText(text);
        m_staticChangeInfo.Invalidate(FALSE);
    }
}

// View나 Panel에서
void CControlPage::RefreshLayerList(const LayerManager& layerManager)
{
    m_listCtrlLayer.DeleteAllItems();
    // m_items도 비워야 하므로 CLayerListCtrl에 Clear 함수 추가 권장

    for (const auto& layer : layerManager.layers) {
        int iconType = 0;
        if (layer->m_shapeType == 3) iconType = 1;      // Line
        else if (layer->m_shapeType == 5) iconType = 2; // Polygon

        CString name(layer->m_name.c_str());
        m_listCtrlLayer.AddLayer(name, iconType, layer->m_isVisible);
    }
}