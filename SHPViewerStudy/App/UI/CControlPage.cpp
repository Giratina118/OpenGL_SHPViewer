#include <pch.h>
#include "CControlPage.h"
#include "Layer.h"
#include <resource.h>
#include <algorithm>

BEGIN_MESSAGE_MAP(CControlPage, CWnd)
    ON_BN_CLICKED(ID_BTN_DELETE_LAYER, &CControlPage::OnBtnDeleteLayer)
END_MESSAGE_MAP()

bool CControlPage::Create(CWnd* pParent, UINT nID)
{
    return CWnd::Create(AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1)), _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, CRect(0, 0, 10, 10), pParent, nID) == TRUE;
}

void CControlPage::CreateTabControls()
{
    m_staticChangeInfo.Create(_T("데이터 대기중..."), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(0, 0, 10, 10), this);
    m_staticInfo.Create(_T("이동: WASD, 좌클릭\n줌: R/F, 휠\n회전: 방향키, Q/E, 우클릭"), WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(0, 0, 10, 10), this);
    m_buttonDeleteLayer.Create(_T("Delete"), WS_CHILD | WS_VISIBLE, CRect(0, 0, 10, 10), this, ID_BTN_DELETE_LAYER);

    // LVS_OWNERDRAWFIXED 필수, 스크롤은 WS_VSCROLL
    m_listCtrlLayer.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | LVS_REPORT | LVS_OWNERDRAWFIXED | LVS_SINGLESEL | LVS_NOCOLUMNHEADER, CRect(0, 0, 10, 10), this, IDC_LAYER_LIST);
    m_listCtrlLayer.Init();
}

void CControlPage::Resize(UISize& uiSize)
{
    // 탭1 내부
    m_staticChangeInfo.MoveWindow (0, 0,                        uiSize.buttonWidth, uiSize.buttonHeight * 4);
    m_staticInfo.MoveWindow       (0, uiSize.buttonHeight * 4,  uiSize.buttonWidth, uiSize.buttonHeight * 2);
	m_listCtrlLayer.MoveWindow    (0, uiSize.buttonHeight * 7,  uiSize.buttonWidth, uiSize.buttonHeight * 5);
    m_buttonDeleteLayer.MoveWindow(0, uiSize.buttonHeight * 12, uiSize.buttonWidth, uiSize.buttonHeight);

    // 폰트
    if (uiSize.isFontChanged) {
        auto applyFont = [&](CWnd& w) { if (w.GetSafeHwnd()) w.SetFont(&uiSize.font); };
        applyFont(m_staticChangeInfo);
        applyFont(m_staticInfo);
        applyFont(m_listCtrlLayer);
        applyFont(m_buttonDeleteLayer);
    }

    m_listCtrlLayer.Resize(uiSize);
}

void CControlPage::UpdateInfo(float fps, int32_t total, int32_t rendered, int32_t fake, int32_t cameraAltitude)
{
    CString text;
    text.Format(_T("FPS : %.1f\r\n전체 객체: %d\r\n렌더 객체: %d\r\n가상 객체: %d\r\n현재 고도: %dm\r\n"), fps, total, rendered, fake, cameraAltitude);
    if (m_staticChangeInfo.GetSafeHwnd()) {
        m_staticChangeInfo.SetWindowText(text);
        m_staticChangeInfo.Invalidate(FALSE);
    }
}

// View나 Panel에서
void CControlPage::RefreshLayerList(LayerManager& layerManager)
{
	m_listCtrlLayer.ClearItems(&layerManager);
    // m_items도 비워야 하므로 CLayerListCtrl에 Clear 함수 추가 권장

    for (const auto& layer : layerManager.layers) {
        int32_t iconType = 0;
        if      (layer->m_shapeType == 3) iconType = 1; // Line
        else if (layer->m_shapeType == 5) iconType = 2; // Polygon

        TCHAR buf[256];
        _stprintf_s(buf, _T("[LayerList] 이름 = %hs,  타입 = %d,  아이콘 = %d\n"), layer->m_name.c_str(), layer->m_shapeType, iconType);
        OutputDebugString(buf);

        CString name(layer->m_name.c_str());
        m_listCtrlLayer.AddLayer(name, iconType, layer->m_isVisible, layer->m_id);
    }
}

void CControlPage::OnBtnDeleteLayer()
{
    if (!m_callback.onDeleteLayer) return;
    int32_t hitLayerId = m_listCtrlLayer.GetHitLayerId();
    if (hitLayerId < 0) return;

    TCHAR buf[256];
    _stprintf_s(buf, _T("Layer List 반응\n"));
    OutputDebugString(buf);

    m_listCtrlLayer.DeleteLayerItem(hitLayerId); // 레이어 아이템 지우기

    m_callback.onDeleteLayer(hitLayerId);
}