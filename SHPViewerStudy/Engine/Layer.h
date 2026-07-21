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
	double	    m_objSize = 0.01; // 객체 크기   (선 객체 -> 너비, 점 객체 -> 반지름)
	int32_t     m_id      = -1;   // 레이어 인덱스 (LayerManager에서 관리하는 layers 벡터의 인덱스)

	std::unique_ptr<Renderer> m_renderer;
	std::unique_ptr<QuadTree> m_quadTree;
	BoundingBox               m_boundingBox; // 레이어 전체 MBR, 루트노드 생성 시 사용
	DBFTable                  m_dbfTable;    // dbf 데이터 테이블

	bool m_isVisible  = false; // 레이어 가시 상태
	bool m_isBuilding = false; // 건물인지 여부
	bool m_hasShx     = false; // .shx 파일 존재 여부
	bool m_hasDbf     = false; // .dbf 파일 존재 여부

	std::vector<PointObject>      pointObjects;      // Point      객체 배열
	std::vector<PolyObject>       polyLineObjects;   // PolyLine   객체 배열
	std::vector<PolyObject>       polygonObjects;    // Polygon    객체 배열
	std::vector<MultiPointObject> multiPointObjects; // MultiPoint 객체 배열
	std::vector<MultiPatchObject> multiPatchObjects; // MultiPatch 객체 배열

	void SetMBRBox(glm::dvec2& min, glm::dvec2& max) { m_boundingBox.minX = min.x; m_boundingBox.minY = min.y; m_boundingBox.maxX = max.x; m_boundingBox.maxY = max.y; }
};

class LayerManager
{
public:
	std::vector<std::unique_ptr<Layer>>  layers; // 레이어 목록
	std::unordered_map<int32_t, int32_t> m_layerIdToIndex; // 레이어 아이디(생성 순서)와 현재 벡터 번호(다른 레이어 삭제에 따라 당겨짐) 연결
	int32_t m_nextLayerId =  0; // 다음 생성될 레이어의 id
	int32_t m_hitLayerId  = -1; // 현재 UI에서 선택한 레이어 id
	bool    m_drawedFrustum = false; // 절투체 표현 여부

	// 셰이더
	Shader m_shader;
	GLint  m_viewProjectionLocation  = -1; // u_viewProjection uniform 슬롯 ID
	GLint  m_colorMultiplierLocation = -1; // 라인 어둡게: 1.0 → 0.6
	GLuint m_debugVAO = 0, m_debugVBO = 0;
	std::vector<Vertex> m_frustumLineVertices;

	// EGL
	EGLDisplay m_display = EGL_NO_DISPLAY; // GPU 드라이버 연결 핸들
	EGLSurface m_surface = EGL_NO_SURFACE; // 그림이 실제로 그려질 표면 (윈도우와 연결)
	EGLContext m_context = EGL_NO_CONTEXT; // EGL 컨텍스트 핸들
	HDC  m_deviceContext = nullptr; // 윈도우 디바이스 컨텍스트 핸들

	bool m_needRedraw    = true;    // true면 다음 Render에서 컬링/업로드 수행
	UIState* m_uiState   = nullptr; // UI 버튼 토글 상태
	int32_t pickingDataId  = -1;    // 피킹한 데이터 아이디, 이전 피킹과 현재 피킹 객체가 같은지 아닌지 판별 시 사용하기 위해 저장
	int32_t pickingLayerId = -1;    // 피킹한 데이터의 레이어 아이디

	Layer& CreateLayer(std::string name, uint32_t shpType, BoundingBox& layerBox); // 레이어 생성
	void DeleteLayer(int32_t layerId); // 레이어 삭제
	bool InitRenderer(HWND hWnd, UIState* uiState); // 생성 직후 렌더 초기화
	bool InitEGL(HWND hwnd);  // EGL 초기화
	void Shutdown(HWND hWnd); // 메모리 해제
	void Render(CameraController& camera, UISize& uiSize, glm::dvec3 hitPoint); // 일괄 그리기 명령
	void Resize(int32_t width, int32_t height, int32_t panelWidthLeft); // 창 크기 변경
	void CountObject(int32_t& totalObjCount, int32_t& renderObjCount, int32_t& fakeObjCount); // 객체 개수 세기
	void Picking(glm::dvec3& rayStart, glm::dvec3& rayDir, CRightPanel& rightPanel); // 피킹
	void ApplyObjectColorWithLevel(); // 객체 색상 설정

	void DrawCameraFrustum(CameraController& camera); // 카메라 절두체 시각화
	void DrawDebugRect(const glm::dvec3& center, float size); // 피킹 지점 사각형 표시
	void DrawDebugPrimitives(const std::vector<Vertex>& vertices, GLenum drawMode); // 절두체, 피킹 지점 사각형 그리기

	void ReDraw() { m_needRedraw = true; } // 화면 상태가 바뀔 시 그리기 실행
	void SetDrawFrustum(bool isDrawed) { m_drawedFrustum = isDrawed; } // 프러스텀 그리기 토글
};