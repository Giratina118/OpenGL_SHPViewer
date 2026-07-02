#pragma once
#include "QuadTree.h"
#include "Render/Renderer.h"
#include <string>
#include <variant>

// 레이어 수가 늘어도 유연하게 대응하기 위해 레이어별 데이터 위치 정보를 담는 구조체 (어느 벡터에 어디부터 어디까지 들어있는지)
// 현재는 하나의 레이어만 들어가기에 layerDatas[0]에만 데이터가 들어있다.
// TODO: 아직 멀티 레이어 미구현
class Layer
{
	// shapeType에 따른 objects벡터에서 startIndex부터 length만큼이 해당 레이어의 objects(객체)
public:
	std::string m_name;        // 레이어 이름 (파일명과 동일)
	uint32_t    m_shapeType;   // 객체 타입   (1: Point, 3: PolyLine, 5: Polygon, 8: MultiPoint, 31: MultiPatch)
	int32_t     m_startIndex;  // 시작 인덱스 (pointObjects, polyLineObjects, polygonObjects, multiPointObjects, multiPatchObjects 중 하나의 벡터에서)
	int32_t     m_length;      // 데이터 개수 (startIndex부터 m_length 개의 데이터가 해당 레이어에 속함)
	std::unique_ptr<Renderer> m_renderer;
	std::unique_ptr<QuadTree> m_quadTree;
	BoundingBox m_boundingBox; // 레이어 전체 MBR
	bool m_isVisible  = false; // 레이어 가시 상태
	bool m_isBuilding = false;

	std::vector<PointObject>      pointObjects;      // Point      객체 배열
	std::vector<PolyObject>       polyLineObjects;   // PolyLine   객체 배열
	std::vector<PolyObject>       polygonObjects;    // Polygon    객체 배열
	std::vector<MultiPointObject> multiPointObjects; // MultiPoint 객체 배열
	std::vector<MultiPatchObject> multiPatchObjects; // MultiPatch 객체 배열

	DBFTable    dbfTable;

	//void CleanPoints();
};

class LayerManager
{
public:
	std::vector<Layer>    layers;        // 어느 벡터에 어디부터 어디까지 들어있는지
	std::vector<int32_t>  visibleLayers; // 현재 보이는 레이어들

	/*
	std::vector<PointObject>      pointObjects;      // Point      객체 배열
	std::vector<PolyObject>       polyLineObjects;   // PolyLine   객체 배열
	std::vector<PolyObject>       polygonObjects;    // Polygon    객체 배열
	std::vector<MultiPointObject> multiPointObjects; // MultiPoint 객체 배열
	std::vector<MultiPatchObject> multiPatchObjects; // MultiPatch 객체 배열
	*/

	// EGL
	EGLDisplay m_display = EGL_NO_DISPLAY; // GPU 드라이버 연결 핸들
	EGLSurface m_surface = EGL_NO_SURFACE; // 그림이 실제로 그려질 표면 (윈도우와 연결)
	EGLContext m_context = EGL_NO_CONTEXT; // EGL 컨텍스트 핸들
	HDC m_deviceContext  = nullptr; // 윈도우 디바이스 컨텍스트 핸들

	bool m_needRedraw = true; // true면 다음 Render에서 컬링/업로드 수행
	BoundingBox boundingBox; // 전체 MBR

	Layer& CreateLayer(std::string name, uint32_t shpType, BoundingBox& layerBox);
	bool InitRenderer(HWND hWnd);
	bool InitEGL(HWND hwnd);
	void Shutdown(HWND hWnd);
	void Render(CameraController& camera, UIState& uiState, int32_t screenWidth, int32_t screenHeight, int32_t panelWidthLeft);
	void Resize(int32_t width, int32_t height, int32_t panelWidthLeft);
	void Refresh();
	void ReDraw() { m_needRedraw = true; } // 토글 시 캐시 강제 무효화용

	void ApplyObjectColorWithLevel(bool useLevelColor);
	//void CleanPoints();
};