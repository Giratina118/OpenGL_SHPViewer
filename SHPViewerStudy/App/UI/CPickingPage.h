#pragma once

#include "framework.h"
#include "UIState.h"
#include <functional>

struct PickingCallbacks
{
    std::function<void(bool)> onPicking;
    std::function<void(bool)> onThirdMode;
};

class CPickingPage : public CWnd
{
public:
    bool Create(CWnd* parent, UINT id);
    void CreateTabControls();
    void Resize(UISize& uiSize);
    void SetCallbacks(const PickingCallbacks& callback) { m_callback = callback; }
    void UpdatePickingInfo(double pointX, double pointY);

private:
    PickingCallbacks m_callback;

    CButton m_buttonPicking;
    CButton m_radioFirstPerson;
    CButton m_radioThirdPerson;

    CStatic m_staticPickingInfo;
    CFont m_font;

    //afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnBtnPicking();
    afx_msg void OnBtnFirstPerson();
    afx_msg void OnBtnThirdPerson();

    DECLARE_MESSAGE_MAP()
};