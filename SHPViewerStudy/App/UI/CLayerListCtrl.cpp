#include "pch.h"
#include "CLayerListCtrl.h"
#include "Layer.h"

BEGIN_MESSAGE_MAP(CLayerListCtrl, CListCtrl)
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

// 초기화
void CLayerListCtrl::Init()
{
    InsertColumn(0, _T(""), LVCFMT_LEFT, 2000); // 컬럼 하나만
    SetExtendedStyle(LVS_EX_FULLROWSELECT);
}

// 아이템 추가
void CLayerListCtrl::AddLayer(const CString& name, int32_t iconType, bool isVisible, int32_t layerId)
{
    int32_t index = static_cast<int32_t>(m_items.size());

    LayerItemData itemData;
    itemData.name      = name;
	itemData.layerId   = layerId; // 임시로 인덱스를 레이어 ID로 사용
    itemData.iconType  = iconType;
    itemData.isVisible = isVisible;
    m_items.push_back(itemData);

    // CListCtrl에 빈 아이템 삽입 (실제 그리기는 DrawItem에서)
    LVITEM lvItem  = {};
    lvItem.mask    = LVIF_TEXT;
    lvItem.iItem   = index;
    lvItem.pszText = (LPTSTR)(LPCTSTR)name;
    InsertItem(&lvItem);

    RedrawItems(index, index);
    UpdateWindow();

    TCHAR buf[256];
    _stprintf_s(buf, _T("[visible] = %d\n"), itemData.isVisible);
    OutputDebugString(buf);
}

// 레이어 체크 상태 변경
void CLayerListCtrl::SetLayerVisible(int32_t index, bool isVisible)
{
    if (index < 0 || index >= static_cast<int32_t>(m_items.size())) return;
    m_items[index].isVisible = isVisible;
    RedrawItems(index, index);
}

bool CLayerListCtrl::GetLayerVisible(int32_t index) const
{
    if (index < 0 || index >= static_cast<int32_t>(m_items.size())) return false;
    return m_items[index].isVisible;
}

void CLayerListCtrl::DeleteLayerItem(int32_t layerId)
{
    if (m_hitItemIndex < 0) return; 
    m_items.erase(m_items.begin() + m_hitItemIndex); 
    m_hitItemIndex = -1;

    m_layerManager->DeleteLayer(layerId);
}

// 아이템 높이 설정
void CLayerListCtrl::MeasureItem(LPMEASUREITEMSTRUCT lpMIS)
{
    lpMIS->itemHeight = ITEM_HEIGHT;
}

// 영역 계산 헬퍼
CRect CLayerListCtrl::GetCheckRect(const CRect& rect) const
{
    // 왼쪽 끝에 정사각형 체크박스
    int32_t size = 18;
    int32_t cx   = rect.left + 10;
    int32_t cy   = rect.top + (rect.Height() - size) / 2;
    return CRect(cx, cy, cx + size, cy + size);
}

CRect CLayerListCtrl::GetIconRect(const CRect& rect) const
{
    // 체크박스 오른쪽에 아이콘
    CRect checkRect = GetCheckRect(rect);
    int32_t size = 20;
    int32_t cx   = checkRect.right + 8;
    int32_t cy   = rect.top + (rect.Height() - size) / 2;
    return CRect(cx, cy, cx + size, cy + size);
}

CRect CLayerListCtrl::GetTextRect(const CRect& rect) const
{
    // 아이콘 오른쪽에 텍스트
    CRect iconRect = GetIconRect(rect);
    return CRect(iconRect.right + 8, rect.top, rect.right - 4, rect.bottom);
}

COLORREF CLayerListCtrl::GetIconColor(int32_t iconType) const
{
    switch (iconType) {
    case 0:  return RGB(30,  144, 255); // Point   파랑
    case 1:  return RGB(60,  179, 113); // Line    초록
    case 2:  return RGB(220, 100, 60);  // Polygon 주황
    default: return RGB(150, 150, 150);
    }
}

CString CLayerListCtrl::GetIconLabel(int32_t iconType) const
{
    switch (iconType) {
	case 0:  return _T(".");  // Point
	case 1:  return _T("/");  // Line
	case 2:  return _T("Δ"); // Polygon 
    default: return _T("?");
    }
}

// 핵심: 아이템 그리기
void CLayerListCtrl::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
    int32_t index = lpDIS->itemID;
    if (index < 0 || index >= static_cast<int32_t>(m_items.size())) return;

    CDC* pDC = CDC::FromHandle(lpDIS->hDC);
    CRect rectItem = lpDIS->rcItem;
    const LayerItemData& item = m_items[index];

    bool isSelected = (lpDIS->itemState & ODS_SELECTED) != 0;

    // 1. 배경
    COLORREF bgColor = isSelected ? RGB(200, 220, 255) : RGB(245, 245, 245);
    if (index % 2 == 1 && !isSelected)
        bgColor = RGB(255, 255, 255); // 줄무늬 배경
    pDC->FillSolidRect(rectItem, bgColor);

    // 2. 체크박스
    CRect rectCheck = GetCheckRect(rectItem);

    // 체크박스 테두리
    pDC->Draw3dRect(rectCheck, RGB(120, 120, 120), RGB(120, 120, 120));

    if (item.isVisible) {
        // 체크 내부 채우기
        CRect rectCheckInner = rectCheck;
        rectCheckInner.DeflateRect(2, 2);
        pDC->FillSolidRect(rectCheckInner, RGB(70, 130, 220));

        // 체크 표시 - 선으로 직접 그리기
        CPen checkPen(PS_SOLID, 2, RGB(255, 255, 255));
        CPen* pOldPen = pDC->SelectObject(&checkPen);
        int32_t cx = rectCheck.left, cy = rectCheck.top;
        pDC->MoveTo(cx + 4,  cy + 9);
        pDC->LineTo(cx + 7,  cy + 12);
        pDC->LineTo(cx + 14, cy + 5);
        pDC->SelectObject(pOldPen);
    }

    // 3. 타입 아이콘
    CRect    rectIcon  = GetIconRect(rectItem);
    COLORREF iconColor = GetIconColor(item.iconType);

    // 아이콘 배경 (둥근 사각형 효과를 직사각형으로)
    pDC->FillSolidRect(rectIcon, iconColor);

    // 아이콘 레이블 (P / L / A)
    CString iconLabel = GetIconLabel(item.iconType);
    pDC->SetTextColor(RGB(255, 255, 255));
    pDC->SetBkMode(TRANSPARENT);

    CFont iconFont;
    iconFont.CreateFont(-(rectIcon.Height() - 4), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
    CFont* pOldFont = pDC->SelectObject(&iconFont);
    pDC->DrawText(iconLabel, rectIcon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    pDC->SelectObject(pOldFont);

    // 4. 레이어 이름
    CRect rectText = GetTextRect(rectItem);
    pDC->SetTextColor(item.isVisible ? RGB(30, 30, 30) : RGB(160, 160, 160));

    // 현재 컨트롤에 설정된 폰트 사용
    pDC->DrawText(item.name, rectText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // 5. 구분선
    CPen linePen(PS_SOLID, 1, RGB(210, 210, 210));
    CPen* pOldPen = pDC->SelectObject(&linePen);
    pDC->MoveTo(rectItem.left,  rectItem.bottom - 1);
    pDC->LineTo(rectItem.right, rectItem.bottom - 1);
    pDC->SelectObject(pOldPen);

    // 6. 포커스 사각형
    if (lpDIS->itemState & ODS_FOCUS)
        pDC->DrawFocusRect(rectItem);
}

// 클릭: 체크박스 영역 클릭 시 토글
void CLayerListCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
    // 클릭한 아이템 인덱스 찾기
    LVHITTESTINFO ht = {};
    ht.pt = point;
    int32_t index = HitTest(&ht);

    if (index >= 0 && index < static_cast<int32_t>(m_items.size())) {
		m_hitItemIndex = index;

        // 해당 아이템의 체크박스 영역 계산
        CRect rectItem;
        GetItemRect(index, &rectItem, LVIR_BOUNDS);
        CRect rcCheck = GetCheckRect(rectItem);

        if (rcCheck.PtInRect(point)) {
            // 체크박스 클릭 → 토글
            m_items[index].isVisible = !m_items[index].isVisible;
			m_layerManager->layers[m_layerManager->m_layerIdToIndex[m_items[index].layerId]]->m_isVisible = m_items[index].isVisible; // LayerManager에도 반영
            m_layerManager->ReDraw();

            RedrawItems(index, index);
            UpdateWindow();

            // 부모에게 알리기 (필요 시)
            GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), LVN_ITEMCHANGED), (LPARAM)GetSafeHwnd());
            return;
        }
    }

    CListCtrl::OnLButtonDown(nFlags, point);
}