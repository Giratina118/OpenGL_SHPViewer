#pragma once
#include "framework.h"

class CRightPanel : public CWnd
{
public:
    bool Create(CWnd* pParent, UINT nID, CRect& rect);
    void Resize(int32_t screenWidth, int32_t screenHeight);
    void SetPickingInfo(const CString& text);
    void Show(bool show); // ¿­±â/´Ý±â
    int32_t GetWidth() const { return m_isShowPanel ? static_cast<int32_t>(m_clientWidth * m_panelRate) : 0; }

private:
    CStatic m_staticInfo;
    //CBrush  m_bgBrush;
    CFont   m_font;

    bool    m_isShowPanel  = false;
    int32_t m_clientWidth  = 1;
    int32_t m_clientHeight = 1;
    float   m_panelRate    = 0.2f;

    afx_msg int    OnCreate(LPCREATESTRUCT lp);
    //afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    DECLARE_MESSAGE_MAP()
};