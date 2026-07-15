#pragma once
#include "QuadTree.h"
#include "Render/Renderer.h"
#include <string>
#include <variant>
#include <unordered_map>

class CRightPanel;

// 레이어 클래스, 하나의 레이어 클래스가 하나의 shp파일을 담당
class Layer
{ // m_shapeType에 따른 objects벡터에서 m_startIndex부터 m_length만큼이 해당 레이어의 objects(객체)
public:
	std::string m_name;        // 레이어 이름 (파일명과 동일)
	uint32_t    m_shapeType;   // 객체 타입   (1: Point, 3: PolyLine, 5: Polygon, 8: MultiPoint, 31: MultiPatch)
	int32_t     m_startIndex;  // 시작 인덱스 (pointObjects, polyLineObjects, polygonObjects, multiPointObjects, multiPatchObjects 중 하나의 벡터에서)
	int32_t     m_length;      // 데이터 개수 (startIndex부터 m_length 개의 데이터가 해당 레이어에 속함)
	float	    m_objSize = 0.01f; // 객체 크기   (선 객체 -> 너비, 점 객체 -> 반지름)
	int32_t     m_id      = -1;    // 레이어 인덱스 (LayerManager에서 관리하는 layers 벡터의 인덱스)
	std::unique_ptr<Renderer> m_renderer;
	std::unique_ptr<QuadTree> m_quadTree;
	BoundingBox m_boundingBox; // 레이어 전체 MBR
	bool m_isVisible  = false; // 레이어 가시 상태
	bool m_isBuilding = false;
	bool m_hasShx     = false; // .shx 파일 존재 여부
	bool m_hasDbf     = false; // .dbf 파일 존재 여부

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
	std::vector<std::unique_ptr<Layer>> layers;
	std::unordered_map<int32_t, int32_t> m_layerIdToIndex;
	int32_t m_nextLayerId   =  0;
	int32_t m_hitLayerId    = -1;
	bool    m_drawedFrustum = false;

	// 셰이더
	Shader m_shader;
	GLint  m_viewProjectionLocation  = -1; // u_viewProjection uniform 슬롯 ID
	GLint  m_colorMultiplierLocation = -1; // 라인 어둡게: 1.0 → 0.6
	GLuint m_debugVAO = 0;
	GLuint m_debugVBO = 0;
	std::vector<Vertex> m_frustumLineVertices;
	//std::vector<Vertex> m_frustumLineVertices; // 카메라 절두체 라인 재구성

	// EGL
	EGLDisplay m_display = EGL_NO_DISPLAY; // GPU 드라이버 연결 핸들
	EGLSurface m_surface = EGL_NO_SURFACE; // 그림이 실제로 그려질 표면 (윈도우와 연결)
	EGLContext m_context = EGL_NO_CONTEXT; // EGL 컨텍스트 핸들
	HDC  m_deviceContext = nullptr; // 윈도우 디바이스 컨텍스트 핸들
	bool m_needRedraw    = true;    // true면 다음 Render에서 컬링/업로드 수행
	BoundingBox m_boundingBox;      // 전체 MBR
	UIState* m_uiState   = nullptr; // UI 버튼 토글 상태
	std::unique_ptr<Renderer> m_renderer;
	int32_t pickingDataId  = -1;    // 피킹한 데이터 아이디, 이전 피킹과 현재 피킹 객체가 같은지 아닌지 판별 시 사용하기 위해 저장
	int32_t pickingLayerId = -1;

	Layer& CreateLayer(std::string name, uint32_t shpType, BoundingBox& layerBox);
	void DeleteLayer(int32_t layerId);
	bool InitRenderer(HWND hWnd, UIState* uiState);
	bool InitEGL(HWND hwnd);
	void Shutdown(HWND hWnd);
	void Render(CameraController& camera, UISize& uiSize, glm::dvec3 hitPoint);
	void Resize(int32_t width, int32_t height, int32_t panelWidthLeft);
	void ReDraw() { m_needRedraw = true; } // 토글 시 캐시 강제 무효화용
	void CountObject(int32_t& totalObjCount, int32_t& renderObjCount, int32_t& fakeObjCount);
	glm::dvec3 Picking(glm::dvec3& hitPoint, glm::dvec3& rayStart, glm::dvec3& rayDir, CRightPanel& rightPanel);
	
	void DrawDebugPrimitives(const std::vector<Vertex>& vertices, GLenum drawMode);
	void DrawCameraFrustum(CameraController& camera); // 카메라 절두체 시각화
	void SetDrawFrustum(bool isDrawed) { m_drawedFrustum = isDrawed; } // 프러스텀 그리기 토글
	void ApplyObjectColorWithLevel();

	// 디버그용
	void DrawDebugRect(const glm::dvec3& center, float size);
	//void CleanPoints();
};