#pragma once

#include <string>
#include "UI/CPanelLeft.h"
#include "UI/CPanelRight.h"

#include "Layer.h"
#include "Render/Renderer.h"
#include "Parser/SHPLoader.h"
#include "CameraController.h"

// 키보드 버튼 눌림 상태 관리
struct KeyState
{
	bool keyW = false, keyA = false, keyS = false, keyD = false; // 로컬 이동
	bool keyR = false, keyF = false; // 로컬 줌인 줌아웃
	bool keyQ = false, keyE = false; // 로절 회전
	bool keyUp = false, keyDown = false, keyLeft = false, keyRight = false; // 로컬 회전

	bool keyI = false, keyJ = false, keyK = false, keyL = false; // 월드 이동
	bool keyU = false, keyO = false; // 월드 줌인 줌아웃
	bool numPad7 = false, numPad9 = false; // 월드 회전
	bool numPad8 = false, numPad4 = false, numPad5 = false, numPad6 = false; // 월드 회전
};

class CSHPViewerStudyDoc;

class CSHPViewerStudyView : public CView
{
private:
	UISize		     m_uiSize;       // ui 크기 정보
	UIState          m_uiState;      // ui버튼 눌림 상태
	KeyState         m_keyState;     // 키보드 눌림 상태
	CLeftPanel       m_panelLeft;    // 좌측 패널
	CRightPanel      m_panelRight;   // 우측 패널
	LayerManager	 m_layerManager; // 레이어 클래스
	SHPLoader        m_shpLoader;    // shp로더 클래스
	CameraController m_camera;       // 카메라 클래스

	// 업데이트 주기 조절
	std::chrono::steady_clock::time_point m_lastTime; // 측정 시간
	float   m_updatePeriod   = 0.5f; // 업데이트 주기 (fpa, 렌더 객체 수 등)
	float   m_deltaTimeStack = 0.0f; // 델타타임 누적
	int32_t m_frameCount     = 0;    // 프레임 카운트

	// 화면 길이 정보
	//int32_t m_clientWidth  = 1; // 윈도우 전체 좌우 길이
	//int32_t m_clientHeight = 1; // 윈도우 전체 상하 길이

	// 마우스, 좌표 정보
	bool    m_isCameraThirdMode = false; // 카메라 모드, false: 1인칭(기본), true: 3인칭
	bool    m_isPickingMode     = false; // 피킹 모드, 버튼으로 on/off
	int32_t pickingDataId       = -1;    // 피킹한 데이터 아이디, 이전 피킹과 현재 피킹 객체가 같은지 아닌지 판별 시 사용하기 위해 저장
	glm::dvec3 m_rayStart;
	glm::dvec3 m_rayDir;

	bool    m_isLButtonDragging = false; // 마우스 좌클릭 눌림 상태
	bool    m_isRButtonDragging = false; // 마우스 우클릭 눌림 상캐
	CPoint  m_mouseClientPos;		     // 마우스 스크린 좌표
	glm::dvec3 m_hitPoint;               // 스크린 -> 월드 좌표


	// 성능 측정, 디버그 용도
	bool   m_autoPanning = false;
	double m_panTime = 0.0;     // 경과 시간(초)
	double m_panDuration = 8.0; // 왕복 총 시간(초)
	double m_panRangeX = 0.0;   // 좌우 이동 폭(미터)
	glm::dvec3 m_panCenter;     // 왕복 중심 위치

	void TestTimeAboutAltitude(double altitude); // 카메라 자동 이동 (동일 환경에서 디버그 데이터 수집)

protected:
	CSHPViewerStudyView() noexcept : m_panelLeft(m_uiSize), m_panelRight(m_uiSize) {}
	DECLARE_DYNCREATE(CSHPViewerStudyView)

	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);               // 초기 생성
	afx_msg void OnSize(UINT nType, int clientWidth, int clientHeight); // 창 크기 변경
	afx_msg void OnDestroy();				 // 창 닫기
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);	 // 깜빡임 제거, 다시 지웠다 그리기 없애기
	afx_msg void OnTimer(UINT_PTR nIDEvent); // 타이머, 반복 실행

	afx_msg void OnLButtonDown(UINT nFlags, CPoint point); // 마우스 좌클릭 누르기
	afx_msg void OnLButtonUp  (UINT nFlags, CPoint point); // 마우스 좌클릭 떼기
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point); // 마우스 우클릭 누르기
	afx_msg void OnRButtonUp  (UINT nFlags, CPoint point); // 마우스 우클릭 떼기
	afx_msg void OnMouseMove  (UINT nFlags, CPoint point); // 마우스 이동
	afx_msg BOOL OnMouseWheel (UINT nFlags, short zDelta, CPoint point); // 마우스 휠

	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags); // 키보드 키 누르기
	afx_msg void OnKeyUp  (UINT nChar, UINT nRepCnt, UINT nFlags); // 키보드 키 떼기

	afx_msg void OnDropFiles(HDROP hDropInfo); // 파일 끌어다 놓기
	afx_msg void OnFileOpenFolder();           // 폴더 열기
	afx_msg void OnFileOpenShp();	           // 파일 열기
	DECLARE_MESSAGE_MAP()

public:
	CSHPViewerStudyDoc* GetDocument() const;
	virtual ~CSHPViewerStudyView() {}
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnDraw(CDC* pDC);
	
	void LinkCallbacksToUI();  // 좌측 패널 콜백 연결
	void RefreshMap();                             // 파일 열기 시 화면 갱신
	void InputKey(float deltaTime);                // 키 입력
	glm::dvec3 ClientToWorldPos(CPoint clientPos); // 피킹 위치
	glm::dvec3 PickingObj(CPoint clientPos);       // 피킹 객체
	bool IsInUIPanel(const CPoint& mousePoint) const { return mousePoint.x < m_panelLeft.GetWidth() || mousePoint.x > m_uiSize.clientWidth - m_panelRight.GetWidth(); } // UI 마우스 이벤트가 3D 영역인지 판별
};

/*
#ifndef _DEBUG   SHPViewerStudyView.cpp의 디버그 버전
inline CSHPViewerStudyDoc* CSHPViewerStudyView::GetDocument() const
   { return reinterpret_cast<CSHPViewerStudyDoc*>(m_pDocument); }
#endif
*/ // 프로그램 생성 시 기본으로 있던 부분