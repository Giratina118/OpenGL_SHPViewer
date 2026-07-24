#pragma once

#include "framework.h"
#include "UIState.h"
#include <functional>

struct PickingCallbacks
{
    std::function<void(bool)> onPicking;
    std::function<void(bool)> onThirdMode;
    std::function<void(bool)> onEditObjectMode;
    std::function<void(bool)> onEditObjectSave;
    std::function<void(bool)> onEditObjectCancle;
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
    CButton m_buttonEditObjectMode;
    CButton m_buttonEditObjectSave;
    CButton m_buttonEditObjectCancle;
    CButton m_radioFirstPerson;
    CButton m_radioThirdPerson;
    CStatic m_staticPickingInfo;

    //afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnBtnPicking();
    afx_msg void OnBtnFirstPerson();
    afx_msg void OnBtnThirdPerson();
    afx_msg void OnBtnEditObjectMode();
    afx_msg void OnBtnEditObjectSave();
    afx_msg void OnBtnEditObjectCancle();

    DECLARE_MESSAGE_MAP()
};