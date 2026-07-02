#pragma once

#include <string>
#include <GLES3/gl3.h>
#include <EGL/egl.h>

// Engine
#include "ShaderLoadAndUse.h"
#include "CameraController.h"
#include "Triangulate.h"
#include "UI/Palette.h"

class Layer;
class QuadTree;
class QuadTreeNode;

// 정점 - 색상 포함
struct Vertex
{
	float x, y, z;
	unsigned char r, g, b, a;
};

// 각 폴리곤별 IBO 위치 정보를 담을 구조체
struct DrawInfo
{
	uint32_t indexOffset;  // 인덱스 시작 위치
	uint32_t indexCount;   // 그려야 할 인덱스 개수
	uint32_t vertexOffset; // 인덱스 시작 위치
	uint32_t vertexCount;  // 그려야 할 인덱스 개수
};

// UI 토글 상태 관리
struct UIState
{
	bool isShowObjectMBR   = false;
	bool isShowNodeMBR     = false;
	bool isShowLevelColor  = false;
	bool isShowFrustumView = false;
	bool isShowColorLegend = false;
	bool isShowFakeObject  = false;
};

class Renderer
{
private:

	// 셰이더
	Shader m_shader;
	GLint  m_viewProjectionLocation  = -1; // u_viewProjection uniform 슬롯 ID
	GLint  m_colorMultiplierLocation = -1; // 라인 어둡게: 1.0 → 0.6

	Triangulate m_triangulate; // 삼각분할

	// 면 메쉬 (들로네 삼각분할)
	GLuint m_polygonVAO = 0;
	GLuint m_polygonVBO = 0;
	GLuint m_polygonIBO = 0;
	GLuint m_polygonIBOVisible = 0;
	std::vector<Vertex>   m_polygonVertices;	   // 면 vertex (BuildMapMesh에서 만든 라인 vertex와 별개)
	std::vector<uint32_t> m_polygonIndices;		   // 면 삼각형 인덱스
	std::vector<DrawInfo> m_polygonDrawInfos;	   // 객체별 인덱스 범위 (indexOffset, indexCount)
	std::vector<uint32_t> m_polygonVisibleIndices; // 가시 인덱스 임시 버퍼

	// 라인 메쉬
	GLuint m_lineIBOVisible = 0; // 가시 인덱스 전용 IBO, 매번 컬링 결과로 덮어씀
	std::vector<uint32_t> m_lineIndices;		// CPU 측 전체 인덱스 원본
	std::vector<DrawInfo> m_lineDrawInfos;	    // 객체별 인덱스 범위 (offset, count)
	std::vector<uint32_t> m_lineVisibleIndices; // 가시 인덱스 임시 버퍼

	// 가상 객체 메쉬 (쿼드트리 노드별 간략화된 메쉬)
	GLuint m_fakeIBO = 0;
	GLuint m_fakeIBOVisible = 0;
	std::vector<uint32_t> m_fakeIndices; // m_polygonVertices 참조하는 인덱스
	std::vector<uint32_t> m_fakeVisibleIndices;

	int32_t m_selectedObjectId = -1; // 피킹한 객체 아이디

	// 렌더링 상태
	std::vector<int32_t> m_renderObjectIds; // 컬링 통과 객체 ID 목록
	bool m_drawedFrustum = false;

	// MBR 시각화 (객체 MBR, 노드 MBR, 절두체 라인을 공용으로 그리는 버퍼)
	GLuint m_mbrVAO = 0, m_mbrVBO = 0;
	std::vector<Vertex> m_objMbrBoxVertices;   // 가시 객체 MBR 재구성
	std::vector<Vertex> m_nodeMbrBoxVertices;  // 가시 노드 MBR 재구성
	std::vector<Vertex> m_frustumLineVertices; // 카메라 절두체 라인 재구성

	// 화면 크기
	//int32_t m_viewportWidth    = 0;  // 클라이언트 영역 전체 너비 
	//int32_t m_viewportHeight   = 0;  // 클라이언트 영역 전체 높이
	//int32_t m_uiPanelWidthLeft = 0;  // UI 좌측 패널 너비 (이 영역은 3D 렌더 안 함)


	// 디버깅 쿼드트리 성능 체크
	double  m_cullMicrosSum   = 0.0;
	int32_t m_cullSampleCount = 0;
	int32_t fakeObjCount      = 0; // 가짜 객체 수

public:
	QuadTree& m_quadTree; // 쿼드트리
	Layer&    m_layer;    // 레이어

	bool    m_isCDTTriangluate       = true; // 삼각분할, true: cdt, false: ear clipping
	int32_t m_currentRenderCount     = 0; // FPS UI에 표시할 렌더링 객체 수
	int32_t m_currentRenderFakeCount = 0; 

	// 디버그용 시간 저장
	int32_t timeCheckCount  = 0;
	int32_t renderObjSum    = 0;
	int32_t fakeObjSum      = 0;
	double  microSecSum     = 0.0;
	double  fakeMicroSecSum = 0.0;

public:
	Renderer(HWND hWnd, Layer& layer, QuadTree& quadtree) : m_layer(layer), m_quadTree(quadtree) { Initialize(hWnd); }
	~Renderer() {}

	bool Initialize(HWND hWnd);  // 전체 초기화 진입점, EGL/셰이더/버퍼/상태/쿼드트리를 준비
	//bool InitializeEGL(HWND hwnd); // EGL 컨텍스트 생성
	void InitBuffer(GLuint& vao, GLuint& vbo, GLuint* ibo); // 버퍼 초기화
	void UploadAndDraw(GLuint& vao, GLuint& vbo, std::vector<Vertex>& vertices, int drawType);
	void Shutdown(HWND hWnd);	   // GPU 리소스 해제

	void Render(CameraController& camera, UIState& uiState, int32_t screenWidth, int32_t screenHeight, int32_t panelWidthLeft); // 메인 렌더 함수
	//void Resize(int32_t width, int32_t height, int32_t panelWidthLeft); // 윈도우 크기 변경 시 viewport 갱신

	// 그리기 정보 빌드, 파일 열때 호출되는 함수들
	void BuildMesh();        // 라인 메쉬 빌드
	void BuildPolygonMesh(); // 면 메쉬 빌드
	void RebuildQuadTree();  // 쿼드트리 재빌드 (파일 새로 로딩 시 호출)
	void BuildFakeMeshes();  // LOD용 간략화된 메쉬 빌드
	void BuildConvexHullNode(QuadTreeNode& node); // 노드 대표 메쉬, 볼록 껍질 빌드
	
	// UI 버튼 클릭으로 mbr그리기, 색상 표현
	void DrawObjectMBR(); // 객체 MBR 박스 그리기
	void DrawQuadTreeNodeMBR();		  // 노드 MBR 박스 그리기
	void DrawCameraFrustum(CameraController& camera); // 카메라 절두체 시각화

	void PushBoundingBoxLine(const BoundingBox& boundingBox, std::vector<Vertex>& vertices, unsigned char r, unsigned char g, unsigned char b, bool hasHeight); // mbr 정점 버퍼 삽입
	void ApplyLevelColors(bool useLevelColor); // 라인/면 vertex 색상 다시 칠하기
	void GetLevelColor(int32_t level, unsigned char& r, unsigned char& g, unsigned char& b); // 객체 레벨에 따른 색상 계산
	
	void SetDrawFrustum(bool isDrawed) { m_drawedFrustum = isDrawed; } // 프러스텀 그리기 토글

	void SetSelectedObject(int32_t objectId, UIState& uiState); // 선택된 객체(피킹) 강조 표시
	void HighlightObjectColor(int32_t objectId); // 피킹 객체 색상 강조
	void RestoreObjectColor(int32_t objectId, UIState& uiState); // 강조했던 색상 복구
	
	std::vector<DrawInfo>& GetPolygonDrawInfo() { return m_polygonDrawInfos; } // 면 객체별 인덱스 범위 반환
	std::vector<uint32_t>& GetPolygonIndices()  { return m_polygonIndices;   } // 면 인덱스 반환
	std::vector<Vertex>&   GetPolygonVertices() { return m_polygonVertices;  } // 면 버텍스 반환

	// 디버그용
	void DrawDebugRect(const glm::dvec3& center, float size);
};