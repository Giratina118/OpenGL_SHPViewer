#include "pch.h"
#include "CLayerListCtrl.h"
#include "LayerManager.h"

BEGIN_MESSAGE_MAP(CLayerListCtrl, CListCtrl)
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

// 초기화
void CLayerListCtrl::Init()
{
    InsertColumn(0, _T(""), LVCFMT_LEFT, 100);
    SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
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
	m_layerManager->ReOrderLayer(m_items); // 레이어 순서 재정렬
    UpdateWindow();
}

// 아이템 높이 설정
void CLayerListCtrl::SetCustomItemHeight(int32_t height)
{
    height = static_cast<int32_t>(height * 0.8f);
    height = (height < 10) ? 10 : height;

    if (m_dummyImageList.GetSafeHandle()) m_dummyImageList.DeleteImageList(); // 기존 이미지 리스트가 있다면 삭제
    m_dummyImageList.Create(1, height, ILC_COLOR, 1, 1); // 너비 1, 높이 height 짜리 투명한 가짜 이미지 생성
    SetImageList(&m_dummyImageList, LVSIL_SMALL);        // 리스트 컨트롤에 덮어씌우기 (자동으로 행 높이가 height로 쫙 벌어짐)
}

void CLayerListCtrl::DeleteLayerItem(int32_t layerId)
{
    if (m_hitItemIndex < 0) return; 
    m_items.erase(m_items.begin() + m_hitItemIndex); 
    m_hitItemIndex = -1;

    m_layerManager->DeleteLayer(layerId);
}

void CLayerListCtrl::Resize(UISize& uiSize)
{
    if (uiSize.isFontChanged) {
        if (m_font.GetSafeHandle()) m_font.DeleteObject();
        m_font.CreateFont(-uiSize.fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

        SetColumnWidth(0, uiSize.fontSize * 25);
        SetCustomItemHeight(uiSize.fontSize);
    }
}

// 영역 계산
CRect CLayerListCtrl::GetCheckRect(const CRect& rect) const
{
    // 왼쪽 끝에 정사각형 체크박스
    int32_t size   = rect.Height();
    int32_t margin = static_cast<int32_t>(rect.Height() * 0.1f);
    return CRect(rect.left + margin, rect.top + margin, rect.left + size - margin, rect.bottom - margin);
}

CRect CLayerListCtrl::GetIconRect(const CRect& rect) const
{
    // 체크박스 오른쪽에 아이콘
    int32_t size = rect.Height();;
    int32_t margin = static_cast<int32_t>(rect.Height() * 0.1f);
    return CRect(rect.left + size + margin, rect.top + margin, rect.left + size * 2 - margin, rect.bottom - margin);
}

CRect CLayerListCtrl::GetTextRect(const CRect& rect) const
{
    // 아이콘 오른쪽에 텍스트
    return CRect(rect.left + rect.Height() * 2, rect.top, rect.right, rect.bottom);
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

// 아이템 그리기
void CLayerListCtrl::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
    int32_t index = lpDIS->itemID; // 아이템 번호
    if (index < 0 || index >= static_cast<int32_t>(m_items.size())) return;

    CDC*  pDC      = CDC::FromHandle(lpDIS->hDC);
    CRect rectItem = lpDIS->rcItem; // 아이템 영역 크기
    const LayerItemData& item = m_items[index];
    //bool  isSelected = ((lpDIS->itemState & ODS_SELECTED) != 0); // 선택된 아이템과 선택되지 않은 아이템 구분
    //bool  isSelected = (m_hitItemIndex == index); // 선택된 아이템과 선택되지 않은 아이템 구분

    // 배경 설정 (흰색 - 회색 번갈아가며, 선택된 아이템은 하늘색)
    COLORREF bgColor = (m_hitItemIndex == index) ? RGB(200, 220, 255) : ((index % 2) ? RGB(240, 240, 240) : RGB(255, 255, 255));
    pDC->FillSolidRect(rectItem, bgColor);

    CRect rectCheck = GetCheckRect(rectItem); // 체크박스 크기
    pDC->Draw3dRect(rectCheck, RGB(120, 120, 120), RGB(120, 120, 120)); // 체크박스 테두리

    if (item.isVisible) {
        // 체크박스 내부 파란색 채우기
        CRect   rectCheckInner = rectCheck;
        int32_t checkBoxMargin = static_cast<int32_t>(rectCheck.Width() * 0.1f);
        rectCheckInner.DeflateRect(checkBoxMargin, checkBoxMargin);
        pDC->FillSolidRect(rectCheckInner, RGB(70, 130, 220));

        // 체크 표시 흰색 선으로 직접 그리기
        CPen checkPen(PS_SOLID, 2, RGB(255, 255, 255));
        CPen* pOldPen  = pDC->SelectObject(&checkPen);
        checkBoxMargin = static_cast<int32_t>(rectCheck.Width() * 0.25f);
        pDC->MoveTo(rectCheck.left  + checkBoxMargin,               rectCheck.CenterPoint().y);
        pDC->LineTo(rectCheck.CenterPoint().x - checkBoxMargin / 2, rectCheck.bottom - checkBoxMargin);
        pDC->LineTo(rectCheck.right - checkBoxMargin,               rectCheck.top    + checkBoxMargin);
        pDC->SelectObject(pOldPen);
    }

    // Shape Type 아이콘
    CRect    rectIcon  = GetIconRect(rectItem);
	UCharColor color   = m_layerManager->m_layers[m_layerManager->m_layerIdToIndex[item.layerId]]->m_baseColor; // LayerManager에서 색상 가져오기
    COLORREF iconColor = RGB(color.red, color.green, color.blue);

    CString  iconLabel = GetIconLabel(item.iconType); // 아이콘 레이블 (P / L / A)
    pDC->FillSolidRect(rectIcon, iconColor); // 아이콘 배경 (둥근 사각형 효과를 직사각형으로)
    pDC->SetTextColor(RGB(255, 255, 255));
    pDC->SetBkMode(TRANSPARENT);

    //CFont font;
    CFont* pOldFont = pDC->SelectObject(&m_font);
    pDC->DrawText(iconLabel, rectIcon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    pDC->SelectObject(pOldFont);

    // 레이어 이름
    CRect rectText = GetTextRect(rectItem);
    pDC->SetTextColor(item.isVisible ? RGB(30, 30, 30) : RGB(160, 160, 160));
    pDC->DrawText(item.name, rectText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // 구분선
    CPen linePen(PS_SOLID, 1, RGB(210, 210, 210));
    CPen* pOldPen = pDC->SelectObject(&linePen);
    pDC->MoveTo(rectItem.left,  rectItem.bottom - 1);
    pDC->LineTo(rectItem.right, rectItem.bottom - 1);
    pDC->SelectObject(pOldPen);

    // 포커스 사각형
    if (lpDIS->itemState & ODS_FOCUS) pDC->DrawFocusRect(rectItem);


    // 드래그 중 삽입선
    if (m_isDragging && (int)lpDIS->itemID == m_dragDropIndex) {
        CDC* pDC = CDC::FromHandle(lpDIS->hDC);
        CRect rcItem = lpDIS->rcItem;

        CPen insertPen(PS_SOLID, 2, RGB(70, 130, 220));
        CPen* pOldPen = pDC->SelectObject(&insertPen);

        // 삽입 위치 위쪽에 선 그리기
        int lineY = (m_dragDropIndex < m_dragHoldIndex) ? rcItem.top + 1 : rcItem.bottom - 1;

        pDC->MoveTo(rcItem.left,  lineY);
        pDC->LineTo(rcItem.right, lineY);

        // 양 끝 작은 화살표
        pDC->MoveTo(rcItem.left,      lineY - 4);
        pDC->LineTo(rcItem.left,      lineY + 4);
        pDC->MoveTo(rcItem.right - 1, lineY - 4);
        pDC->LineTo(rcItem.right - 1, lineY + 4);

        pDC->SelectObject(pOldPen);
    }
}

// 체크박스 영역 클릭 시 토글
void CLayerListCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
    // 클릭한 아이템 인덱스 찾기
    LVHITTESTINFO hit = {};
    hit.pt = point;
    int32_t index = HitTest(&hit);

    if (index < 0 || index >= static_cast<int32_t>(m_items.size())) return;
    
    // 해당 아이템의 체크박스 영역 계산
    CRect rectItem;
    GetItemRect(index, &rectItem, LVIR_BOUNDS);
    CRect rectCheck = GetCheckRect(rectItem);

    if (rectCheck.PtInRect(point)) { // 체크박스 클릭 -> 토글
        m_items[index].isVisible = !m_items[index].isVisible;
        m_layerManager->m_layers[m_layerManager->m_layerIdToIndex[m_items[index].layerId]]->m_isVisible = m_items[index].isVisible; // LayerManager에도 반영
    }
    else { // 레이어 선택/취소
        m_hitItemIndex = (m_hitItemIndex == index) ? -1 : index;
        m_layerManager->m_hitLayerId = GetHitLayerId(); // 현재 클릭한 레이어 반영 -> 해당 레이어만 노드mbr, 객체mbr 그리기
        m_layerManager->ApplyObjectColorWithLevel();

		// 레이어 드래그 준비
		m_dragHoldIndex  = index;
        m_dragStartPoint = point;
        m_isDragging     = false;
		SetCapture(); // 마우스 캡처 시작
    }

    RedrawItems(index, index);
    m_layerManager->ReDraw();
    UpdateWindow();

    CListCtrl::OnLButtonDown(nFlags, point);
}

void CLayerListCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_dragHoldIndex < 0) { CListCtrl::OnMouseMove(nFlags, point); return; }
     
    // 일정 거리 이상 움직이면 드래그 시작
    if (!m_isDragging) {
        if (abs(point.y - m_dragStartPoint.y) > 10) m_isDragging = true;
        else { CListCtrl::OnMouseMove(nFlags, point); return; }
    }

    // 현재 마우스 위치의 아이템 계산
    LVHITTESTINFO hitTestInfo = {};
    hitTestInfo.pt = point;
    int32_t hitIndex = HitTest(&hitTestInfo);

    // 리스트 위/아래 경계 처리
    if (hitIndex < 0) {
        CRect rectClient;
        GetClientRect(&rectClient);
        if      (point.y <= rectClient.top)    hitIndex = 0;
        else if (point.y >= rectClient.bottom) hitIndex = static_cast<int32_t>(m_items.size()) - 1;
    }

    if (hitIndex >= 0 && hitIndex != m_dragDropIndex) {
        m_dragDropIndex = hitIndex;
        Invalidate(FALSE); // 삽입선 갱신
        UpdateWindow();
    }

    // 커서를 드래그 모양으로
    SetCursor(LoadCursor(nullptr, IDC_SIZENS));
    CListCtrl::OnMouseMove(nFlags, point);
}

void CLayerListCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
    ReleaseCapture();

    if (m_isDragging && m_dragHoldIndex >= 0 && m_dragDropIndex >= 0 && m_dragHoldIndex != m_dragDropIndex) {
        int32_t sourceIndex = m_dragHoldIndex;
        int32_t targetIndex = m_dragDropIndex;
        LayerItemData moved = m_items[sourceIndex];

        m_items.erase(m_items.begin() + sourceIndex);
        m_items.insert(m_items.begin() + targetIndex, moved);

        DeleteAllItems();

        for (int32_t i = 0; i < static_cast<int32_t>(m_items.size()); i++) {
            LVITEM lvi = {};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = i;
            lvi.pszText = (LPTSTR)(LPCTSTR)m_items[i].name;
            InsertItem(&lvi);
        }

        m_layerManager->ReOrderLayer(m_items);
        m_layerManager->ReDraw();

        Invalidate(FALSE);
        UpdateWindow();
    }

    m_isDragging = false;
    m_dragHoldIndex = -1;
    m_dragDropIndex = -1;

    SetCursor(LoadCursor(nullptr, IDC_ARROW));

    CListCtrl::OnLButtonUp(nFlags, point);
}

// 마우스가 리스트 컨트롤 영역을 벗어나면 드래그 상태 초기화
void CLayerListCtrl::OnMouseLeave()
{
    if (m_isDragging && GetCapture() != this) {
        m_isDragging    = false;
        m_dragHoldIndex = -1;
        m_dragDropIndex = -1;
        Invalidate(FALSE);
    }
}