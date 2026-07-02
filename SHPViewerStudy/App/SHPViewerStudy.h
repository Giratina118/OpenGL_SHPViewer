// SHPViewerStudy.h: SHPViewerStudy 애플리케이션의 기본 헤더 파일
#pragma once

#ifndef __AFXWIN_H__
	#error "PCH에 대해 이 파일을 포함하기 전에 'pch.h'를 포함합니다."
#endif

#include "resource.h"

class CSHPViewerStudyApp : public CWinApp
{
public:
	CSHPViewerStudyApp() noexcept;

// 재정의
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// 구현
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
};

extern CSHPViewerStudyApp theApp;
