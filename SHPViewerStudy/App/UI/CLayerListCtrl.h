#pragma once
#include "framework.h"
#include "UIState.h"

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
private:
    std::vector<LayerItemData> m_items;
    LayerManager* m_layerManager;
    int32_t       m_hitItemIndex = -1; // 마우스 클릭 시 체크박스 영역에 있는 아이템 인덱스
    CImageList    m_dummyImageList;    // 아이템의 높이 설정을 위한 더미 이미지 리스트
    CFont         m_font;

    // 드래그 상태 관리
    bool    m_isDragging = false;
    int32_t m_dragHoldIndex = -1; // 드래그 시작 아이템 인덱스
    int32_t m_dragDropIndex = -1; // 드래그 중인 아이템이 이동할 위치 인덱스
    CPoint  m_dragStartPoint;     // 드래그 시작 지점

    // 체크박스, 아이콘 영역 계산
    CRect GetCheckRect(const CRect& itemRect) const;
    CRect GetIconRect (const CRect& itemRect) const;
    CRect GetTextRect (const CRect& itemRect) const;

    // 아이콘 타입별 색상
    COLORREF GetIconColor(int32_t iconType) const;
    CString  GetIconLabel(int32_t iconType) const;

protected:
    afx_msg void DrawItem(LPDRAWITEMSTRUCT lpDIS); // Owner Draw 함수
    afx_msg void OnMouseMove  (UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp  (UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    DECLARE_MESSAGE_MAP()

public:
    void Init();
    void AddLayer(const CString& name, int32_t iconType, bool isVisible, int32_t layerId);
    void ClearItems(LayerManager* layerManager) { m_items.clear(); DeleteAllItems(); m_layerManager = layerManager; }
	int32_t GetHitLayerId() const { return (m_hitItemIndex < 0 || m_hitItemIndex >= m_items.size()) ? -1 : m_items[m_hitItemIndex].layerId; } // 체크박스 클릭 시 인덱스 반환
    void SetCustomItemHeight(int32_t height);
    void DeleteLayerItem(int32_t layerId);
    void Resize(UISize& uiSize);
};