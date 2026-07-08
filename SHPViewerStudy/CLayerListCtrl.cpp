#include "pch.h"
#include "CLayerListCtrl.h"

BEGIN_MESSAGE_MAP(CLayerListCtrl, CListCtrl)
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

// 초기화
void CLayerListCtrl::Init()
{
    // 컬럼 하나만 (Owner Draw라 헤더는 숨겨도 됨)
    InsertColumn(0, _T(""), LVCFMT_LEFT, 2000);

    // 헤더 숨기기 (선택사항)
    // CHeaderCtrl* pHeader = GetHeaderCtrl();
    // if (pHeader) pHeader->ShowWindow(SW_HIDE);

    SetExtendedStyle(LVS_EX_FULLROWSELECT);
}

// 아이템 추가
void CLayerListCtrl::AddLayer(const CString& name, int iconType, bool isVisible)
{
    int index = (int)m_items.size();

    LayerItemData data;
    data.name = name;
    data.iconType = iconType;
    data.isVisible = isVisible;
    m_items.push_back(data);

    // CListCtrl에 빈 아이템 삽입 (실제 그리기는 DrawItem에서)
    LVITEM lvi = {};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = index;
    lvi.pszText = (LPTSTR)(LPCTSTR)name;
    InsertItem(&lvi);
}

void CLayerListCtrl::SetLayerVisible(int index, bool isVisible)
{
    if (index < 0 || index >= (int)m_items.size()) return;
    m_items[index].isVisible = isVisible;
    RedrawItems(index, index);
}

bool CLayerListCtrl::GetLayerVisible(int index) const
{
    if (index < 0 || index >= (int)m_items.size()) return false;
    return m_items[index].isVisible;
}

// 아이템 높이 설정
void CLayerListCtrl::MeasureItem(LPMEASUREITEMSTRUCT lpMIS)
{
    lpMIS->itemHeight = ITEM_HEIGHT;
}

// 영역 계산 헬퍼
CRect CLayerListCtrl::GetCheckRect(const CRect& r) const
{
    // 왼쪽 끝에 정사각형 체크박스
    int size = 18;
    int cx = r.left + 10;
    int cy = r.top + (r.Height() - size) / 2;
    return CRect(cx, cy, cx + size, cy + size);
}

CRect CLayerListCtrl::GetIconRect(const CRect& r) const
{
    // 체크박스 오른쪽에 아이콘
    CRect checkRect = GetCheckRect(r);
    int size = 20;
    int cx = checkRect.right + 8;
    int cy = r.top + (r.Height() - size) / 2;
    return CRect(cx, cy, cx + size, cy + size);
}

CRect CLayerListCtrl::GetTextRect(const CRect& r) const
{
    // 아이콘 오른쪽에 텍스트
    CRect iconRect = GetIconRect(r);
    return CRect(iconRect.right + 8, r.top, r.right - 4, r.bottom);
}

COLORREF CLayerListCtrl::GetIconColor(int iconType) const
{
    switch (iconType) {
    case 0:  return RGB(30, 144, 255);  // Point   파랑
    case 1:  return RGB(60, 179, 113);  // Line    초록
    case 2:  return RGB(220, 100, 60);  // Polygon 주황
    default: return RGB(150, 150, 150);
    }
}

CString CLayerListCtrl::GetIconLabel(int iconType) const
{
    switch (iconType) {
    case 0:  return _T("P");
    case 1:  return _T("L");
    case 2:  return _T("A");  // Area
    default: return _T("?");
    }
}

// 핵심: 아이템 그리기
void CLayerListCtrl::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
    int   index = lpDIS->itemID;
    if (index < 0 || index >= (int)m_items.size()) return;

    CDC* pDC = CDC::FromHandle(lpDIS->hDC);
    CRect rcItem = lpDIS->rcItem;
    const LayerItemData& item = m_items[index];

    bool isSelected = (lpDIS->itemState & ODS_SELECTED) != 0;

    // 1. 배경
    COLORREF bgColor = isSelected ? RGB(200, 220, 255) : RGB(245, 245, 245);
    if (index % 2 == 1 && !isSelected)
        bgColor = RGB(255, 255, 255); // 줄무늬 배경
    pDC->FillSolidRect(rcItem, bgColor);

    // 2. 체크박스
    CRect rcCheck = GetCheckRect(rcItem);

    // 체크박스 테두리
    pDC->Draw3dRect(rcCheck, RGB(120, 120, 120), RGB(120, 120, 120));

    if (item.isVisible) {
        // 체크 내부 채우기
        CRect rcCheckInner = rcCheck;
        rcCheckInner.DeflateRect(2, 2);
        pDC->FillSolidRect(rcCheckInner, RGB(70, 130, 220));

        // 체크 표시 - 선으로 직접 그리기
        CPen checkPen(PS_SOLID, 2, RGB(255, 255, 255));
        CPen* pOldPen = pDC->SelectObject(&checkPen);
        int cx = rcCheck.left, cy = rcCheck.top;
        pDC->MoveTo(cx + 4, cy + 9);
        pDC->LineTo(cx + 7, cy + 12);
        pDC->LineTo(cx + 14, cy + 5);
        pDC->SelectObject(pOldPen);
    }

    // 3. 타입 아이콘
    CRect rcIcon = GetIconRect(rcItem);
    COLORREF iconColor = GetIconColor(item.iconType);

    // 아이콘 배경 (둥근 사각형 효과를 직사각형으로)
    pDC->FillSolidRect(rcIcon, iconColor);

    // 아이콘 레이블 (P / L / A)
    CString iconLabel = GetIconLabel(item.iconType);
    pDC->SetTextColor(RGB(255, 255, 255));
    pDC->SetBkMode(TRANSPARENT);

    CFont iconFont;
    iconFont.CreateFont(
        -(rcIcon.Height() - 4), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
    CFont* pOldFont = pDC->SelectObject(&iconFont);
    pDC->DrawText(iconLabel, rcIcon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    pDC->SelectObject(pOldFont);

    // 4. 레이어 이름
    CRect rcText = GetTextRect(rcItem);
    pDC->SetTextColor(item.isVisible ? RGB(30, 30, 30) : RGB(160, 160, 160));

    // 현재 컨트롤에 설정된 폰트 사용
    pDC->DrawText(item.name, rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // 5. 구분선
    CPen linePen(PS_SOLID, 1, RGB(210, 210, 210));
    CPen* pOldPen = pDC->SelectObject(&linePen);
    pDC->MoveTo(rcItem.left, rcItem.bottom - 1);
    pDC->LineTo(rcItem.right, rcItem.bottom - 1);
    pDC->SelectObject(pOldPen);

    // 6. 포커스 사각형
    if (lpDIS->itemState & ODS_FOCUS)
        pDC->DrawFocusRect(rcItem);
}

// 클릭: 체크박스 영역 클릭 시 토글
void CLayerListCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
    // 클릭한 아이템 인덱스 찾기
    LVHITTESTINFO ht = {};
    ht.pt = point;
    int index = HitTest(&ht);

    if (index >= 0 && index < (int)m_items.size()) {
        // 해당 아이템의 체크박스 영역 계산
        CRect rcItem;
        GetItemRect(index, &rcItem, LVIR_BOUNDS);
        CRect rcCheck = GetCheckRect(rcItem);

        if (rcCheck.PtInRect(point)) {
            // 체크박스 클릭 → 토글
            m_items[index].isVisible = !m_items[index].isVisible;
            RedrawItems(index, index);
            UpdateWindow();

            // 부모에게 알리기 (필요 시)
            GetParent()->SendMessage(WM_COMMAND,
                MAKEWPARAM(GetDlgCtrlID(), LVN_ITEMCHANGED),
                (LPARAM)GetSafeHwnd());
            return;
        }
    }

    CListCtrl::OnLButtonDown(nFlags, point);
}