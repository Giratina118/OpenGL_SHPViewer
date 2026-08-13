#pragma once
#include "EditObject.h"
#include "Layer.h"
#include <variant>
#include <unordered_map>

class LayerManager
{
public:
	std::vector<std::unique_ptr<Layer>>  m_layers; // 레이어 목록
	std::unordered_map<int32_t, int32_t> m_layerIdToIndex; // 레이어 아이디(생성 순서)와 현재 벡터 번호(다른 레이어 삭제에 따라 당겨짐) 연결
	std::vector<int32_t> m_layerOrder; // 레이어 순서, UI에서 레이어 순서 변경 시 이 벡터를 재정렬하고 Render에서 이 순서대로 그리기 수행
	int32_t m_nextLayerId   =  0;      // 다음 생성될 레이어의 id
	int32_t m_hitLayerId    = -1;      // 현재 UI에서 선택한 레이어 id
	bool    m_drawedFrustum = false;   // 절투체 표현 여부

	EditObject m_editObject; // 객체 편집 클래스

	// 셰이더
	Shader m_shader;
	GLint  m_viewProjectionLocation  = -1; // u_viewProjection uniform 슬롯 ID
	GLint  m_colorMultiplierLocation = -1; // 라인 어둡게: 1.0 → 0.6

	// 기존 debugVAO/VBO, mbrVAO/VBO, transformVAO/VBO/IBO/lineIBO, hoverVAO/VBO/IBO/lineIBO, frustumLineVertices 등 raw GL 핸들 전부를 아래 5개로 대체
	DebugVertexBuffer m_debugRectBuffer; // 피킹 지점 사각형
	DebugVertexBuffer m_mbrBuffer;       // 객체 MBR / 노드 MBR 공용 (순차 업로드-그리기)
	DebugVertexBuffer m_frustumBuffer;   // 절두체 라인 (스냅샷 시점에만 업로드)
	OverlayMesh       m_hoverOverlay;    // 호버 오브젝트

	std::vector<Vertex> m_frustumLineVertices; // 절두체 정점 계산용 (CPU 임시 버퍼, 유지)

	// Hover(연속 피킹) 전용 변수
	bool m_isHovering = false;
	std::vector<Vertex>   m_hoverVertices;
	std::vector<uint32_t> m_hoverPolygonIndices;
	std::vector<uint32_t> m_hoverLineIndices;

	// EGL
	HWND       m_hWnd    = nullptr;
	EGLDisplay m_display = EGL_NO_DISPLAY; // GPU 드라이버 연결 핸들
	EGLSurface m_surface = EGL_NO_SURFACE; // 그림이 실제로 그려질 표면 (윈도우와 연결)
	EGLContext m_context = EGL_NO_CONTEXT; // EGL 컨텍스트 핸들
	HDC  m_deviceContext = nullptr; // 윈도우 디바이스 컨텍스트 핸들

	bool m_needRedraw  = true;    // true면 다음 Render에서 컬링/업로드 수행
	UIState* m_uiState = nullptr; // UI 버튼 토글 상태
	int32_t m_pickingLayerId  = -1; // 피킹한 객체의 레이어 아이디
	int32_t m_pickingObjectId = -1; // 피킹한 객체의 아이디, 이전 피킹과 현재 피킹 객체가 같은지 아닌지 판별 시 사용하기 위해 저장
	int32_t m_pickingNodeId   = -1; // 피킹한 객체의 노드 아이디

public:
	LayerManager() = default;
	~LayerManager() { Shutdown(); }

	Layer& CreateLayer(std::string name, uint32_t shpType, BoundingBox& layerBox); // 레이어 생성
	void DeleteLayer(int32_t layerId); // 레이어 삭제
	bool InitRenderer(HWND hWnd, UIState* uiState); // 생성 직후 렌더 초기화
	bool InitEGL(HWND hwnd);  // EGL 초기화
	void Shutdown(); // 메모리 해제
	void Render(CameraController& camera, UISize& uiSize, glm::dvec3 hitPoint); // 일괄 그리기 명령
	void Resize(int32_t width, int32_t height, int32_t panelWidthLeft); // 창 크기 변경
	void CountObject(int32_t& totalObjCount, int32_t& renderObjCount, int32_t& fakeObjCount); // 객체 개수 세기
	glm::dvec3 Picking(glm::dvec3& rayStart, glm::dvec3& rayDir, CRightPanel& rightPanel); // 피킹
	void ApplyObjectColorWithLevel(); // 객체 색상 설정

	Layer* GetLayerById(int32_t layerId) { return ResolveLayerById(m_layers, m_layerIdToIndex, layerId); }

	void DrawCameraFrustum(CameraController& camera);         // 카메라 절두체 시각화
	void DrawDebugRect(const glm::dvec3& center, float size); // 피킹 지점 사각형 표시

	void ReDraw() { m_needRedraw = true; } // 화면 상태가 바뀔 시 그리기 실행
	void SetDrawFrustum(bool isDrawed) { m_drawedFrustum = isDrawed; } // 프러스텀 그리기 토글
	void ReOrderLayer(std::vector<LayerItemData>& items);

	void SetHoverObject();
};