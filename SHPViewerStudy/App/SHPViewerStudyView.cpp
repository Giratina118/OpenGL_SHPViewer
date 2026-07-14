#include "pch.h"
#include "framework.h"

// SHARED_HANDLERS는 미리 보기, 축소판 그림 및 검색 필터 처리기를 구현하는 ATL 프로젝트에서 정의할 수 있으며 해당 프로젝트와 문서 코드를 공유하도록 해 줍니다.
#ifndef SHARED_HANDLERS
#include "SHPViewerStudy.h"
#endif

#include "SHPViewerStudyDoc.h"
#include "SHPViewerStudyView.h"
#include <filesystem>
#include <limits>
#include <glm/gtc/type_ptr.hpp>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CSHPViewerStudyView, CView)

BEGIN_MESSAGE_MAP(CSHPViewerStudyView, CView)
	ON_WM_CREATE()		// OnCreate
	ON_WM_TIMER()		// OnTimer
	ON_WM_SIZE()		// OnSize
	ON_WM_ERASEBKGND()	// OnEraseBkgnd
	ON_WM_DESTROY()		// OnDestroy
	ON_WM_LBUTTONDOWN()	// OnLButtonDown
	ON_WM_LBUTTONUP()	// OnLButtonUp
	ON_WM_RBUTTONDOWN()	// OnRButtonDown
	ON_WM_RBUTTONUP()	// OnRButtonUp
	ON_WM_MOUSEMOVE()	// OnMouseMove
	ON_WM_MOUSEWHEEL()	// OnMouseWheel
	ON_WM_KEYDOWN()		// OnKeyDown
	ON_WM_KEYUP()		// OnKeyUp
	ON_WM_DROPFILES()	// OnDropFiles
	ON_COMMAND(ID_FILE_OPEN_SHP,    &CSHPViewerStudyView::OnFileOpenShp)
	ON_COMMAND(ID_FILE_OPEN_FOLDER, &CSHPViewerStudyView::OnFileOpenFolder)
END_MESSAGE_MAP()


// 뷰와 연결되어 있는 Doc의 포인터 얻어오는 함수
CSHPViewerStudyDoc* CSHPViewerStudyView::GetDocument() const
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CSHPViewerStudyDoc)));
	return (CSHPViewerStudyDoc*)m_pDocument;
}

// 윈도우 스타일 지정
BOOL CSHPViewerStudyView::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.style |= WS_CLIPCHILDREN;
	return CView::PreCreateWindow(cs);
}

// CSHPViewerStudyView 그리기
void CSHPViewerStudyView::OnDraw(CDC* pDc)
{
	CSHPViewerStudyDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc) return;

	//CRect rect;
	//GetClientRect(&rect);
	m_layerManager.Render(m_camera, m_uiSize, m_hitPoint);
	//m_renderer.Render(m_camera, m_layerManager, m_uiState);
}

void CSHPViewerStudyView::LinkCallbacksToUI()
{
	LeftPanelCallbacks callback;
	callback.controlCallbacks.onDeleteLayer    = [this](bool value) { 
		TCHAR buf[256];
		_stprintf_s(buf, _T("View.cpp 반응\n"));
		OutputDebugString(buf);
		m_panelLeft.m_pageControl.RefreshLayerList(m_layerManager); m_layerManager.ReDraw(); SetFocus(); 
	};

	callback.visibilityCallbacks.onObjectMBR   = [this](bool value) { m_uiState.isShowObjectMBR   = value; m_layerManager.ReDraw(); SetFocus(); };
	callback.visibilityCallbacks.onNodeMBR     = [this](bool value) { m_uiState.isShowNodeMBR     = value; m_layerManager.ReDraw(); SetFocus(); };
	callback.visibilityCallbacks.onLevelColor  = [this](bool value) { m_uiState.isShowLevelColor  = value; m_layerManager.ApplyObjectColorWithLevel(); m_layerManager.ReDraw(); SetFocus(); };
	callback.visibilityCallbacks.onFrustumView = [this](bool value) { m_uiState.isShowFrustumView = value; m_layerManager.layers[0]->m_renderer->SetDrawFrustum(!value);  m_layerManager.ReDraw(); SetFocus(); };
	callback.visibilityCallbacks.onFakeObject  = [this](bool value) { m_uiState.isShowFakeObject  = value; m_layerManager.ReDraw(); SetFocus(); };
	callback.visibilityCallbacks.onBuilding    = [this](bool value) { m_uiState.isShowBuilding    = value; m_layerManager.ReDraw(); SetFocus(); }; 
	callback.visibilityCallbacks.onMap         = [this](bool value) { m_layerManager.ReDraw(); SetFocus(); }; // TODO: 버튼 기능 추가하기
	callback.visibilityCallbacks.onViewRange   = [this](int  value) { m_camera.SetViewRange(value);      m_layerManager.ReDraw(); SetFocus(); };

	callback.pickingCallbacks.onPicking        = [this](bool value) { m_isPickingMode     = value; SetFocus(); };
	callback.pickingCallbacks.onPicking        = [this](bool value) { m_isPickingMode     = value; SetFocus(); };
	callback.pickingCallbacks.onThirdMode      = [this](bool value) { m_isCameraThirdMode = value; SetFocus(); };
	m_panelLeft.SetCallbacks(callback);
}

// 파일 열리면 화면 갱신
void CSHPViewerStudyView::RefreshMap()
{
	//if (m_layerManager.polygonObjects.empty() && m_layerManager.polyLineObjects.empty() && m_layerManager.pointObjects.empty()) return;

	CRect rect;
	GetClientRect(&rect);
	m_camera.Init(m_layerManager.m_boundingBox, rect.Width() - m_panelLeft.GetWidth() - m_panelRight.GetWidth(), rect.Height());

	m_layerManager.Refresh();
	//m_renderer.BuildMesh(m_layerManager);
	//m_renderer.RebuildQuadTree(m_layerManager);
	//m_renderer.ReDraw();
}

// 키 입력
void CSHPViewerStudyView::InputKey(float deltaTime)
{
	float moveSpeed   = 300.0f * deltaTime;
	float rotateSpeed = 900.0f * deltaTime;
	float moveX = 0.0f, moveY = 0.0f;

	// WASD (로컬 좌표 이동)
	if (m_keyState.keyW) moveY += moveSpeed;
	if (m_keyState.keyS) moveY -= moveSpeed;
	if (m_keyState.keyA) moveX += moveSpeed;
	if (m_keyState.keyD) moveX -= moveSpeed;
	if (moveX != 0.0f || moveY != 0.0f) m_camera.MoveLocal(moveX, moveY);

	// IJKL (월드 좌표 이동)
	moveX = moveY = 0.0f;
	if (m_keyState.keyI) moveY += moveSpeed;
	if (m_keyState.keyK) moveY -= moveSpeed;
	if (m_keyState.keyJ) moveX += moveSpeed;
	if (m_keyState.keyL) moveX -= moveSpeed;
	if (moveX != 0.0f || moveY != 0.0f) m_camera.MoveWorld(moveX, moveY);

	// RF (로컬 좌표 줌)
	if (m_keyState.keyR) m_camera.ZoomLocal(1.0);
	if (m_keyState.keyF) m_camera.ZoomLocal(-1.0);

	// UO (월드 좌표 줌)
	if (m_keyState.keyU) m_camera.ZoomWorld(1.0);
	if (m_keyState.keyO) m_camera.ZoomWorld(-1.0);

	// QE, 방향키 (로컬 회전)
	float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
	if (m_keyState.keyE)	 rotZ -= rotateSpeed;
	if (m_keyState.keyQ)	 rotZ += rotateSpeed;
	if (m_keyState.keyLeft)  rotX -= rotateSpeed;
	if (m_keyState.keyRight) rotX += rotateSpeed;
	if (m_keyState.keyUp)    rotY -= rotateSpeed;
	if (m_keyState.keyDown)  rotY += rotateSpeed;
	if (rotX != 0.0f || rotY != 0.0f || rotZ != 0.0f) m_camera.RotateLocal(rotX, rotY, rotZ);

	// 숫자패드 (월드 회전)
	rotX = rotY = rotZ = 0.0f;
	if (m_keyState.numPad9)  rotZ -= rotateSpeed;
	if (m_keyState.numPad7)	 rotZ += rotateSpeed;
	if (m_keyState.numPad4)  rotX -= rotateSpeed;
	if (m_keyState.numPad6)  rotX += rotateSpeed;
	if (m_keyState.numPad8)  rotY -= rotateSpeed;
	if (m_keyState.numPad5)  rotY += rotateSpeed;
	if (rotX != 0.0f || rotY != 0.0f || rotZ != 0.0f) m_camera.RotateWorld(rotX, rotY, rotZ);
}

// 마우스 커서 world 좌표 획득
glm::dvec3 CSHPViewerStudyView::ClientToWorldPos(CPoint clientPos)
{
	glm::dmat4 inverseViewProjection = glm::inverse(m_camera.GetMatrix());
	CRect rect;
	GetClientRect(&rect);

	// NDC 변환 (윈도우 전체를 비율에 맞춰 -1 ~ 1의 범위로 축소, Y축 반전 필수)
	float ndcX =  (float)(clientPos.x - m_panelLeft.GetWidth()) / (rect.Width() - m_panelLeft.GetWidth())  * 2.0f - 1.0f;
	float ndcY = -((float)clientPos.y / rect.Height() * 2.0f - 1.0f);

	// inverse 변환 후 w로 나누기
	glm::vec4 nearWorld = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
	glm::vec4 farWorld  = inverseViewProjection * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
	nearWorld /= nearWorld.w;
	farWorld  /= farWorld.w;

	m_rayStart = glm::dvec3(nearWorld);
	m_rayDir   = glm::normalize(glm::dvec3(farWorld) - m_rayStart);

	if (std::abs(m_rayDir.z) < 1e-6)
		return glm::dvec3(std::numeric_limits<double>::quiet_NaN()); // 수평 방향이면 교차 없음

	// rayOrigin에서 rayDir 방향으로 쏘았을 때 * correctionValue만큼 가면 z = 0에 도달,  origin.z + dir.z * cor = 0 -> cor = -origin.z / dir.z
	double correctionValue = -m_rayStart.z / m_rayDir.z;

	// 교차점이 카메라 뒤에 있으면 유효하지 않음
	if (correctionValue < 0.0)
		return glm::dvec3(std::numeric_limits<double>::quiet_NaN());

	glm::dvec3 hit = m_rayStart + correctionValue * m_rayDir;
	return hit;
}

// 객체 선택 (픽킹)
// TODO: 현재는 0번 레이어만 피킹 체크 중
glm::dvec3 CSHPViewerStudyView::PickingObj(CPoint clientPos)
{
	int32_t beforePickingId = pickingDataId;
	m_hitPoint = ClientToWorldPos(clientPos);
	glm::dvec3 hit = m_hitPoint;

	// 레이와 객체의 충돌 검사, 쿼드트리를 이용한 피킹
	double collisionDistance = std::numeric_limits<double>::max();
	pickingDataId = m_layerManager.layers[0]->m_quadTree->SearchPickingData(m_rayStart, m_rayDir, 0, collisionDistance, m_layerManager.layers[0]->m_renderer->GetPolygonDrawInfo(), m_layerManager.layers[0]->m_renderer->GetPolygonIndices(), m_layerManager.layers[0]->m_renderer->GetPolygonVertices(), hit);

	if (pickingDataId   == -1)            { m_layerManager.layers[0]->m_renderer->SetSelectedObject(-1, m_uiState);  return hit; } // 객체가 없는 빈 공간 선택
	if (beforePickingId == pickingDataId) { m_panelRight.Show(false); pickingDataId = -1; return hit; } // 이전과 같은 객체 선택

	// 피킹된 객체 정보 얻기, TODO: 폴리곤 데이터만 일단 적용
	if (m_layerManager.layers[0]->polygonObjects.size() < 1) return hit;

	m_layerManager.layers[0]->m_renderer->SetSelectedObject(pickingDataId, m_uiState); // 선택 객체 색상 적용
	m_panelRight.SetPickingInfo(m_layerManager.layers[0]->dbfTable.PrintAttribute(pickingDataId)); // 선택 객체 dbf 정보 출력

	m_layerManager.ReDraw();
	Invalidate(FALSE);

	return hit;
}

// WM_CREATE 시 호출, 초기 설정
int CSHPViewerStudyView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1) return -1;
	DragAcceptFiles(TRUE); // 파일 드래그, 드롭 허용



	// shapefile 파싱
	std::filesystem::path busanBuildingPath = std::filesystem::current_path() / "Resource" / "ShpFile" / "BusanBuilding" / "F_FAC_BUILDING_26_202606.shp";
	m_shpLoader.Parse(busanBuildingPath, m_layerManager);

	// UI 패널 생성
	CRect rect;
	GetClientRect(&rect);
	m_panelLeft.Create (this, ID_PANNEL_LEFT_BACKGROUND,  rect);
	m_panelRight.Create(this, ID_PANNEL_RIGHT_BACKGROUND, rect);

	// 콜백 연결 (람다로 View 멤버에 접근)
	// TODO: render에 레이어마다 그려야 하는 것들(객체)만 남기고 전체에 하나만 그리는 것들(절두체)은 밖으로 빼기
	LinkCallbacksToUI();
	
	// 카메라 초기화, UI 패널 부분 남기기
	m_camera.Init(m_layerManager.m_boundingBox, rect.Width() - m_panelLeft.GetWidth() - m_panelRight.GetWidth(), rect.Height());
	m_layerManager.InitRenderer(m_hWnd, &m_uiState); // 렌더 초기화

	m_lastTime = std::chrono::steady_clock::now();
	SetTimer(1, 8, nullptr); // 8ms마다 WM_TIMER -> OnTimer함수 호출
	return 0;
}

// WM_SIZE 시 호출, 화면 사이즈 변화 시
void CSHPViewerStudyView::OnSize(UINT nType, int clientWidth, int clientHeight)
{
	CView::OnSize(nType, clientWidth, clientHeight);
	if (clientWidth <= 0 || clientHeight <= 0) return; // 최소화 방어

	// 새로 변경된 화면 크기 정보
	m_uiSize.clientWidth  = clientWidth;
	m_uiSize.clientHeight = clientHeight;
	m_uiSize.panelWidth   = static_cast<int32_t>(clientWidth  * 0.2);
	m_uiSize.marginX      = static_cast<int32_t>(m_uiSize.panelWidth * 0.05);
	m_uiSize.gapHeight    = static_cast<int32_t>(clientHeight * 0.01);
	m_uiSize.buttonWidth  = m_uiSize.panelWidth - m_uiSize.marginX * 2;
	m_uiSize.buttonHeight = static_cast<int32_t>(clientHeight * 0.07);
	//m_clientWidth  = clientWidth;
	//m_clientHeight = clientHeight;

	// 양쪽 패널 사이즈 재지정
	int32_t fontSize = std::max(10, m_uiSize.clientHeight / 32);
	if (m_uiSize.fontSize != fontSize) {
		m_uiSize.fontSize = fontSize;
		m_uiSize.font.DeleteObject();
		m_uiSize.font.CreateFont(-fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));
		m_uiSize.isFontChanged = true;
	}

	m_panelLeft.Resize ();
	m_panelRight.Resize();
	m_uiSize.isFontChanged = false;

	// 가운데 렌더링 영역 사이즈, 카메라 종횡비 재지정
	m_camera.UpdateAspect(m_uiSize.clientWidth - m_uiSize.panelWidth, m_uiSize.clientHeight);
	m_layerManager.Resize(m_uiSize.clientWidth, m_uiSize.clientHeight, m_uiSize.clientWidth);
}

// WM_DESTROY 시 호출
void CSHPViewerStudyView::OnDestroy()
{
	KillTimer(1); // SetTimer로 생성한 타이머(현재 OnTimer 반복 실행 중) 제거
	m_layerManager.Shutdown(m_hWnd);
	CView::OnDestroy();
}

// 배경 지우기 제거
BOOL CSHPViewerStudyView::OnEraseBkgnd(CDC* pDC)
{
	return false;
}

// WM_TIMER 시 호출, 타이머에 따라 주기적으로 호출
void CSHPViewerStudyView::OnTimer(UINT_PTR nIDEvent)
{
	// deltaTime, fps 계산
	std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<float>          deltaTime   = currentTime - m_lastTime;
	m_lastTime        = currentTime;
	m_deltaTimeStack += deltaTime.count();
	m_frameCount++;

	// m_updatePeriod 주기마다 1번씩 갱신 (ui 정보 출력)
	if (m_deltaTimeStack > m_updatePeriod) {
		float fps        = (float)m_frameCount / m_deltaTimeStack; // 평균 fps 계산
		m_deltaTimeStack = 0.0f;
		m_frameCount     = 0;

		// TODO: 모든 레이어 총합 구하기
		//int32_t totalObjCount  = static_cast<int32_t>(m_layerManager.pointObjects.size()) + static_cast<int32_t>(m_layerManager.polyLineObjects.size()) + static_cast<int32_t>(m_layerManager.polygonObjects.size());
		//int32_t renderObjCount = m_renderer.m_currentRenderCount;
		//int32_t fakeObjCount   = m_uiState.isShowFakeObject ? m_renderer.m_currentRenderFakeCount : 0;
		//m_panelLeft.UpdateInfo(fps, totalObjCount, renderObjCount, fakeObjCount, static_cast<int32_t>(m_camera.transform.position.z));
		//m_panelLeft.UpdateInfo(fps, 0, 0, 0, static_cast<int32_t>(m_camera.transform.position.z), 0.0);
		m_panelLeft.UpdatePickingInfo(m_hitPoint);
	}

	InputKey(deltaTime.count()); // 키보드 입력
	Invalidate(FALSE); // 배경을 흰색으로 지우지 않고 다시 그리기 WM_PAINT -> OnDraw
	CView::OnTimer(nIDEvent);
	
	// 측정 부분
	/*
	if (m_autoPanning) {
		m_panTime += deltaTime.count();

		if (m_panTime >= m_panDuration) {
			m_autoPanning = false;
			m_camera.transform.position = m_panCenter; // 정확히 시작점 복귀
			m_camera.UpdateMatrix();
		}
		else {
			// 0 → 2π 한 주기 sin 왕복 (중심에서 시작 → 우 → 좌 → 중심)
			double phase = (m_panTime / m_panDuration) * 2.0 * 3.14159265358979;
			double offsetX = std::sin(phase) * m_panRangeX;

			m_camera.transform.position.x = m_panCenter.x + (float)offsetX;
			m_camera.transform.position.y = m_panCenter.y;
			m_camera.transform.position.z = m_panCenter.z; // 줌 높이 고정
			m_camera.UpdateMatrix(); // m_isCameraChanged=true → Render가 다시 돔
		}
	}
	else {
		InputKey(deltaTime.count());
	}
	Invalidate(FALSE); // 배경을 흰색으로 지우지 않고 다시 그리기 WM_PAINT -> OnDraw
	CView::OnTimer(nIDEvent);
	*/
}


// 마우스 이벤트
// 마우스 좌클릭 누르기
void CSHPViewerStudyView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (IsInUIPanel(point))  return; // UI 패널 위에서는 카메라가 반응하지 않도록 차단
	if (m_isPickingMode)     { PickingObj(point); m_panelLeft.UpdatePickingInfo(m_hitPoint); } // 피킹 모드
	if (m_isCameraThirdMode) { 
		glm::dvec3 hit = ClientToWorldPos(point);
		if (std::isfinite(hit.x) && std::isfinite(hit.y)) { // nan 체크
			m_hitPoint = hit;
			m_camera.m_thirdMovePos = m_camera.transform.position;
		} 
	} // 3인칭 카메라 조작 모드

	m_isLButtonDragging = true;
	m_mouseClientPos    = point;
	SetCapture(); // 마우스가 창 밖으로 나가도 추적
	SetFocus();
	CView::OnLButtonDown(nFlags, point);
}

// 마우스 좌클릭 떼기
void CSHPViewerStudyView::OnLButtonUp(UINT nFlags, CPoint point)
{
	m_isLButtonDragging = false;
	ReleaseCapture(); // SetCapture() 끊기
	CView::OnLButtonUp(nFlags, point);
}

// 마우스 우클릭 누르기
void CSHPViewerStudyView::OnRButtonDown(UINT nFlags, CPoint point)
{
	if (IsInUIPanel(point))  return;
	if (m_isCameraThirdMode) { 
		glm::dvec3 hit = ClientToWorldPos(point);
		if (std::isfinite(hit.x) && std::isfinite(hit.y)) m_hitPoint = hit;
	}

	m_isRButtonDragging = true;
	m_mouseClientPos    = point;
	SetCapture(); // 마우스가 창 밖으로 나가도 추적
	CView::OnRButtonDown(nFlags, point);
}

// 마우스 우클릭 떼기
void CSHPViewerStudyView::OnRButtonUp(UINT nFlags, CPoint point)
{
	m_isRButtonDragging = false;
	ReleaseCapture(); // SetCapture() 끊기
	CView::OnRButtonUp(nFlags, point);
}

// 마우스 이동
void CSHPViewerStudyView::OnMouseMove(UINT nFlags, CPoint point)
{
	//PickingObj(point); // 마우스 이동 시에도 피킹 체크

	if (!m_isLButtonDragging && !m_isRButtonDragging) return;

	// 화면 픽셀 이동량 -> 세계 좌표 이동량으로 변환
	CPoint mouseDelta = point - m_mouseClientPos;
	m_mouseClientPos = point;

	if (m_isLButtonDragging) { // 좌클릭
		if (m_isCameraThirdMode) { // 3인칭 이동
			m_camera.transform.position = m_camera.m_thirdMovePos;
			m_camera.UpdateMatrix();

			glm::dvec3 hit = ClientToWorldPos(point);
			if (!std::isfinite(hit.x) || !std::isfinite(hit.y)) return; // nan이면 이동 안 함
			glm::dvec3 deltaPos = m_hitPoint - hit;
			m_camera.transform.position += deltaPos;
			m_camera.UpdateMatrix();
		}
		else m_camera.MoveLocal(static_cast<float>(mouseDelta.x), static_cast<float>(mouseDelta.y)); // 1인칭 이동
		m_layerManager.ReDraw();
	}
	if (m_isRButtonDragging) { // 우클릭
		if (m_isCameraThirdMode) m_camera.RotateThird(static_cast<float>(mouseDelta.x), static_cast<float>(mouseDelta.y), 0.0f, m_hitPoint); // 3인칭 회전
		else                     m_camera.RotateFirst(static_cast<float>(mouseDelta.x), static_cast<float>(mouseDelta.y), 0.0f);             // 1인칭 회전
		m_layerManager.ReDraw();
	}
}

// 마우스 휠
BOOL CSHPViewerStudyView::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
	// 휠은 화면 좌표가 아닌 스크린 좌표 -> 클라이언트 좌표 변환 후 판정
	ScreenToClient(&point);
	if (IsInUIPanel(point)) return CView::OnMouseWheel(nFlags, zDelta, point);

	// 일반 휠: 줌
	double factor = (zDelta > 0) ? 1.0 : -1.0;

	if (m_isCameraThirdMode) { 
		glm::dvec3 hit = ClientToWorldPos(point);
		if (std::isfinite(hit.x) && std::isfinite(hit.y)) {
			m_hitPoint = hit;
			m_camera.ZoomThird(factor, m_hitPoint);
		}
	}
	else m_camera.ZoomLocal(factor);
	m_layerManager.ReDraw();

	return TRUE;
}


// 키보드 이벤트
// 키보트 키 누르기
void CSHPViewerStudyView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	switch (nChar)
	{
	case 'W': m_keyState.keyW = true; break; // 로컬 이동
	case 'A': m_keyState.keyA = true; break;
	case 'S': m_keyState.keyS = true; break;
	case 'D': m_keyState.keyD = true; break;

	case 'R': m_keyState.keyR = true; break; // 로컬 줌인/줌아웃
	case 'F': m_keyState.keyF = true; break;

	case 'Q': m_keyState.keyQ = true; break; // 로컬 회전
	case 'E': m_keyState.keyE = true; break;

	case VK_UP:    m_keyState.keyUp    = true; break; // 로컬 회전
	case VK_DOWN:  m_keyState.keyDown  = true; break;
	case VK_LEFT:  m_keyState.keyLeft  = true; break;
	case VK_RIGHT: m_keyState.keyRight = true; break;

	case 'I': m_keyState.keyI = true; break; // 월드 이동
	case 'J': m_keyState.keyJ = true; break;
	case 'K': m_keyState.keyK = true; break;
	case 'L': m_keyState.keyL = true; break;

	case 'U': m_keyState.keyU = true; break; // 월드 줌인/줌아웃
	case 'O': m_keyState.keyO = true; break;

	case VK_NUMPAD7: m_keyState.numPad7 = true; break; // 월드 회전
	case VK_NUMPAD9: m_keyState.numPad9 = true; break;

	case VK_NUMPAD8: m_keyState.numPad8 = true; break; // 월드 회전
	case VK_NUMPAD5: m_keyState.numPad5 = true; break;
	case VK_NUMPAD4: m_keyState.numPad4 = true; break;
	case VK_NUMPAD6: m_keyState.numPad6 = true; break;

	// 테스트용
	case '1': if (!m_autoPanning) TestTimeAboutAltitude(100000.0);break;
	case '2': if (!m_autoPanning) TestTimeAboutAltitude(50000.0); break;
	case '3': if (!m_autoPanning) TestTimeAboutAltitude(20000.0); break;
	case '4': if (!m_autoPanning) TestTimeAboutAltitude(17500.0); break;
	case '5': if (!m_autoPanning) TestTimeAboutAltitude(15000.0); break;
	case '6': if (!m_autoPanning) TestTimeAboutAltitude(12500.0); break;
	case '7': if (!m_autoPanning) TestTimeAboutAltitude(10000.0); break;
	case '8': if (!m_autoPanning) TestTimeAboutAltitude(7500.0);  break;
	case '9': if (!m_autoPanning) TestTimeAboutAltitude(5000.0);  break;
	case '0': if (!m_autoPanning) TestTimeAboutAltitude(2500.0);  break;
	}

	CView::OnKeyDown(nChar, nRepCnt, nFlags);
}

// 측정용
void CSHPViewerStudyView::TestTimeAboutAltitude(double altitude)
{
	glm::dvec3 pos = m_camera.transform.position;
	pos.z = altitude; // 측정용 고정 줌 높이 (렌더객체 1만~3만 나오던 값으로)

	TCHAR buf[256];
	_stprintf_s(buf, _T("[click P] 높이= %f\n"), pos.z);
	OutputDebugString(buf);

	m_camera.transform.position = pos;

	m_panTime = 0.0;
	m_panCenter = m_camera.transform.position; // 현재 위치를 기준점으로
	m_panRangeX = m_layerManager.m_boundingBox.GetLengthX() * 0.1; // 이동 폭
	m_autoPanning = true;
}

// 키보드 키 떼기
void CSHPViewerStudyView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	switch (nChar)
	{
	case 'W': m_keyState.keyW = false; break; // 로컬 이동
	case 'A': m_keyState.keyA = false; break;
	case 'S': m_keyState.keyS = false; break;
	case 'D': m_keyState.keyD = false; break;

	case 'R': m_keyState.keyR = false; break; // 로컬 줌인/줌아웃
	case 'F': m_keyState.keyF = false; break;

	case 'Q': m_keyState.keyQ = false; break; // 로컬 회전
	case 'E': m_keyState.keyE = false; break;

	case VK_UP:    m_keyState.keyUp    = false; break; // 로컬 회전
	case VK_DOWN:  m_keyState.keyDown  = false; break;
	case VK_LEFT:  m_keyState.keyLeft  = false; break;
	case VK_RIGHT: m_keyState.keyRight = false; break;

	case 'I': m_keyState.keyI = false; break; // 월드 이동
	case 'J': m_keyState.keyJ = false; break;
	case 'K': m_keyState.keyK = false; break;
	case 'L': m_keyState.keyL = false; break;

	case 'U': m_keyState.keyU = false; break; // 월드 줌인/줌아웃
	case 'O': m_keyState.keyO = false; break;

	case VK_NUMPAD7: m_keyState.numPad7 = false; break; // 월드 회전
	case VK_NUMPAD9: m_keyState.numPad9 = false; break;

	case VK_NUMPAD8: m_keyState.numPad8 = false; break; // 월드 회전
	case VK_NUMPAD5: m_keyState.numPad5 = false; break;
	case VK_NUMPAD4: m_keyState.numPad4 = false; break;
	case VK_NUMPAD6: m_keyState.numPad6 = false; break;
	}

	CView::OnKeyUp(nChar, nRepCnt, nFlags);
}


// 파일 열기
// 파일 끌어다 놓기
void CSHPViewerStudyView::OnDropFiles(HDROP hDropInfo)
{
	TCHAR filePath[MAX_PATH];
	DragQueryFile(hDropInfo, 0, filePath, MAX_PATH);
	m_shpLoader.Parse(filePath, m_layerManager);

	DragFinish(hDropInfo);

	CRect rect;
	GetClientRect(&rect);
	m_camera.Init(m_layerManager.layers.back()->m_boundingBox, rect.Width() - m_panelLeft.GetWidth() - m_panelRight.GetWidth(), rect.Height());
	m_panelLeft.m_pageControl.RefreshLayerList(m_layerManager);
	m_layerManager.layers.back()->m_renderer = std::make_unique<Renderer>(m_hWnd, *m_layerManager.layers.back(), *m_layerManager.layers.back()->m_quadTree);
	m_layerManager.ReDraw();
}

// SHP 파일 직접 선택
void CSHPViewerStudyView::OnFileOpenShp()
{
	CFileDialog dlg(TRUE, _T("shp"), nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, _T("Shapefile (*.shp)|*.shp||"));
	if (dlg.DoModal() != IDOK) return;

	std::filesystem::path shpPath = dlg.GetPathName().GetString();

	//m_layerManager = LayerManager();
	m_shpLoader.Parse(shpPath, m_layerManager);

	CRect rect;
	GetClientRect(&rect);
	m_camera.Init(m_layerManager.layers.back()->m_boundingBox, rect.Width() - m_panelLeft.GetWidth() - m_panelRight.GetWidth(), rect.Height());
	m_panelLeft.m_pageControl.RefreshLayerList(m_layerManager);
	m_layerManager.layers.back()->m_renderer = std::make_unique<Renderer>(m_hWnd, *m_layerManager.layers.back(), *m_layerManager.layers.back()->m_quadTree);
	m_layerManager.ReDraw();
}

// 폴더 선택 -> 안에 있는 shp 전부 읽기
void CSHPViewerStudyView::OnFileOpenFolder()
{
	// Vista 이상 폴더 선택 다이얼로그
	IFileOpenDialog* pDialog = nullptr;
	CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDialog));

	DWORD options;
	pDialog->GetOptions(&options);
	pDialog->SetOptions(options | FOS_PICKFOLDERS);

	if (FAILED(pDialog->Show(m_hWnd))) { pDialog->Release(); return; }

	IShellItem* pItem = nullptr;
	pDialog->GetResult(&pItem);

	PWSTR folderPath = nullptr;
	pItem->GetDisplayName(SIGDN_FILESYSPATH, &folderPath);

	std::filesystem::path folder = folderPath;
	CoTaskMemFree(folderPath);
	pItem->Release();
	pDialog->Release();

	// 폴더 안 .shp 파일 전부 수집
	//m_layerManager = LayerManager();  // 초기화
	for (const auto& entry : std::filesystem::directory_iterator(folder)) {
		if (entry.path().extension() != ".shp") continue;
		m_shpLoader.Parse(entry.path(), m_layerManager);
	}

	CRect rect;
	GetClientRect(&rect);
	m_camera.Init(m_layerManager.layers.back()->m_boundingBox, rect.Width() - m_panelLeft.GetWidth() - m_panelRight.GetWidth(), rect.Height());
	m_panelLeft.m_pageControl.RefreshLayerList(m_layerManager);
	m_layerManager.layers.back()->m_renderer = std::make_unique<Renderer>(m_hWnd, *m_layerManager.layers.back(), *m_layerManager.layers.back()->m_quadTree);
	m_layerManager.ReDraw();
	//RefreshMap();
}