#pragma once
#include "framework.h"

class LayerManager;

// 레이어 리스트 한 아이템의 데이터
struct LayerItemData
{
    CString  name;      // 레이어 이름
    int32_t  layerId;
    int32_t  iconType;  // 0=Point, 1=Line, 2=Polygon
    bool     isVisible; // 토글 체크 상태
};

class CLayerListCtrl : public CListCtrl
{
public:
    void Init();
    void AddLayer(const CString& name, int32_t iconType, bool isVisible, int32_t layerId);
    void SetLayerVisible(int32_t index, bool isVisible);
    bool GetLayerVisible(int32_t index) const;
    void ClearItems(LayerManager* layerManager) { m_items.clear(); DeleteAllItems(); m_layerManager = layerManager; }
	int32_t GetHitLayerId() const { return (m_hitItemIndex < 0 || m_hitItemIndex >= m_items.size()) ? -1 : m_items[m_hitItemIndex].layerId; } // 체크박스 클릭 시 인덱스 반환
    void DeleteLayerItem(int32_t layerId);

    // 아이템 높이 (MeasureItem용)
    static constexpr int32_t ITEM_HEIGHT = 36;

protected:
    // Owner Draw 핵심 두 함수
    afx_msg void DrawItem(LPDRAWITEMSTRUCT lpDIS);
    afx_msg void MeasureItem(LPMEASUREITEMSTRUCT lpMIS);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP()

private:
    std::vector<LayerItemData> m_items;
    LayerManager* m_layerManager;
    int32_t m_hitItemIndex = -1; // 마우스 클릭 시 체크박스 영역에 있는 아이템 인덱스

    // 체크박스, 아이콘 영역 계산
    CRect GetCheckRect(const CRect& itemRect) const;
    CRect GetIconRect (const CRect& itemRect) const;
    CRect GetTextRect (const CRect& itemRect) const;

    // 아이콘 타입별 색상
    COLORREF GetIconColor(int32_t iconType) const;
    CString  GetIconLabel(int32_t iconType) const;
};