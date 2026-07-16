#pragma once

#include <string>
#include <GLES3/gl3.h>
#include <EGL/egl.h>

// Engine
#include "ShaderLoadAndUse.h"
#include "CameraController.h"
#include "Triangulate.h"
#include "UI/UIState.h"

class Layer;
class QuadTree;
class QuadTreeNode;

class Renderer
{
private:
	QuadTree&   m_quadTree;    // 쿼드트리 클래스
	Layer&      m_layer;       // 레이어   클래스
	Triangulate m_triangulate; // 삼각분할 클래스

	// 셰이더
	Shader m_shader;
	GLint  m_viewProjectionLocation  = -1; // u_viewProjection uniform 슬롯 ID
	GLint  m_colorMultiplierLocation = -1; // 라인 어둡게: 1.0 → 0.6

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

	// MBR 시각화 (객체 MBR, 노드 MBR 공용으로 그리는 버퍼)
	GLuint m_mbrVAO = 0, m_mbrVBO = 0;
	std::vector<Vertex> m_objMbrBoxVertices;  // 가시 객체 MBR 재구성
	std::vector<Vertex> m_nodeMbrBoxVertices; // 가시 노드 MBR 재구성

	// 가상 객체 메쉬 (쿼드트리 노드별 간략화된 메쉬)
	GLuint m_fakeIBO = 0;
	GLuint m_fakeIBOVisible = 0;
	std::vector<uint32_t> m_fakeIndices; // m_polygonVertices 참조하는 인덱스
	std::vector<uint32_t> m_fakeVisibleIndices;


	std::vector<int32_t> m_renderObjectIds; // 렌더링 할 객체(컬링 통과 객체) ID 목록
	bool m_isCDTTriangluate = true; // 삼각분할, true: cdt, false: ear clipping, TODO: 안 할거면 지우기

public:
	int32_t m_currentRenderCount     = 0; // UI에 표시할 렌더링 객체 수
	int32_t m_currentRenderFakeCount = 0; // UI에 표시할 가상 객체 수

public:
	Renderer(HWND hWnd, Layer& layer, QuadTree& quadtree) : m_layer(layer), m_quadTree(quadtree) { Initialize(hWnd); }
	~Renderer() {}

	bool Initialize(HWND hWnd); // 전체 초기화 진입점
	void InitBuffer(GLuint& vao, GLuint& vbo, GLuint* ibo); // 버퍼 초기화
	void Shutdown(HWND hWnd); // GPU 리소스 해제

	// 객체 메쉬 빌드, 초기화하면서 실행되는 함수들
	void BuildMesh();         // 메쉬 빌드 진입점
	void BuildPointMesh();    // 점 메쉬 빌드
	void BuildPolyLineMesh(); // 선 메쉬 빌드
	void BuildPolygonMesh();  // 면 메쉬 빌드
	void BuildFakeMeshes();   // LOD용 간략화된 메쉬 빌드
	void BuildConvexHullNode(QuadTreeNode& node); // 노드 대표 메쉬, 볼록 껍질 빌드
	
	// 렌더링
	void Render(CameraController& camera, UIState& uiState, UISize& uiSize, bool isSelected); // 메인 렌더 함수
	void DrawObjectMBR();       // 객체 MBR 박스 그리기
	void DrawQuadTreeNodeMBR(); // 노드 MBR 박스 그리기
	void UploadAndDraw(GLuint& vao, GLuint& vbo, std::vector<Vertex>& vertices, int drawType); // 업로드 & 그리기 (객체 mbr, 노드 mbr)

	void PushBoundingBoxLine(const BoundingBox& boundingBox, std::vector<Vertex>& vertices, UCharColor color, bool hasHeight); // mbr 정점 버퍼 삽입
	void ApplyLevelColors(bool useLevelColor); // 라인/면 vertex 색상 다시 칠하기
	void GetLevelColor(int32_t level, UCharColor& color); // 객체 레벨에 따른 색상 계산

	void HighlightObjectColor(int32_t objectId); // 피킹 객체 색상 강조
	void RestoreObjectColor(int32_t objectId, UIState& uiState, bool isSelectedLayer); // 강조했던 색상 복구
	
	std::vector<DrawInfo>& GetPolygonDrawInfo() { return m_polygonDrawInfos; } // 면 객체별 인덱스 범위 반환
	std::vector<uint32_t>& GetPolygonIndices()  { return m_polygonIndices;   } // 면 인덱스 반환
	std::vector<Vertex>&   GetPolygonVertices() { return m_polygonVertices;  } // 면 버텍스 반환
};