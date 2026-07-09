#pragma once
#include "framework.h"
#include "UIState.h"

class CRightPanel : public CWnd
{
public:
    CRightPanel(UISize& uiSize) : m_uiSize(uiSize) {};
    ~CRightPanel() {}

    bool Create(CWnd* pParent, UINT nID, CRect& rect);
    void Resize();
    void SetPickingInfo(const CString& text);
    void Show(bool show); // ¿­±â/´Ý±â
    int32_t GetWidth() const { return m_isShowPanel ? static_cast<int32_t>(m_uiSize.panelWidth) : 0; }

private:
    CStatic m_staticInfo;
    CFont   m_font;

    bool    m_isShowPanel = false;
    UISize& m_uiSize;
    float   m_panelRate   = 0.2f;

    afx_msg int OnCreate(LPCREATESTRUCT lp);
    DECLARE_MESSAGE_MAP()
};