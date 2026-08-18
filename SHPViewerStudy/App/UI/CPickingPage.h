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
    std::function<void(bool)> onDeleteObject;
    std::function<void(bool)> onCreateCircle;
    std::function<void(bool)> onCreateRectangle;
    std::function<void(bool)> onCreateTriangle;
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
    CButton m_buttonDeleteObject;
    CButton m_buttonCreateCircle;
    CButton m_buttonCreateRectangle;
    CButton m_buttonCreateTriangle;
    CButton m_radioFirstPerson;
    CButton m_radioThirdPerson;
    CStatic m_staticPickingInfo;
    CStatic m_staticCreateObject;
    CFont   m_fontIcon;

    //afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnBtnPicking();
    afx_msg void OnBtnFirstPerson();
    afx_msg void OnBtnThirdPerson();
    afx_msg void OnBtnEditObjectMode();
    afx_msg void OnBtnEditObjectSave();
    afx_msg void OnBtnEditObjectCancle();
    afx_msg void OnBtnDeleteObject();
    afx_msg void OnBtnCreateCircle();
    afx_msg void OnBtnCreateRectangle();
    afx_msg void OnBtnCreateTriangle();

    DECLARE_MESSAGE_MAP()
};