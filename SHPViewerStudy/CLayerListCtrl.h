#pragma once
#include "framework.h"

// 레이어 리스트 한 아이템의 데이터
struct LayerItemData
{
    CString  name;          // 레이어 이름
    int      iconType;      // 0=Point, 1=Line, 2=Polygon
    bool     isVisible;     // 토글 체크 상태
};

class CLayerListCtrl : public CListCtrl
{
public:
    void    Init();
    void    AddLayer(const CString& name, int iconType, bool isVisible = true);
    void    SetLayerVisible(int index, bool isVisible);
    bool    GetLayerVisible(int index) const;

    // 아이템 높이 (MeasureItem용)
    static constexpr int ITEM_HEIGHT = 36;

protected:
    // Owner Draw 핵심 두 함수
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDIS) override;
    virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMIS) override;

    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    std::vector<LayerItemData> m_items;

    // 체크박스, 아이콘 영역 계산
    CRect GetCheckRect(const CRect& itemRect) const;
    CRect GetIconRect(const CRect& itemRect) const;
    CRect GetTextRect(const CRect& itemRect) const;

    // 아이콘 타입별 색상
    COLORREF GetIconColor(int iconType) const;
    CString  GetIconLabel(int iconType) const;
};