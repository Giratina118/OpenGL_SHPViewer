#pragma once
#include "framework.h"

class CControlPage : public CWnd
{
public:
    bool Create(CWnd* parent, UINT id);
    void CreateTabControls();
    void Resize(int width, int height);
    void UpdateInfo(float fps, int total, int rendered, int fake, int cameraAltitude, double scalePerCm); // View에서 호출 -> 텍스트 갱신

private:
    CStatic m_staticChangeInfo;
    CStatic m_staticInfo;
    CFont m_font;

    //afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    //DECLARE_MESSAGE_MAP()
};