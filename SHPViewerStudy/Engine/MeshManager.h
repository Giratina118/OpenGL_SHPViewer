#pragma once
#include <cstdint>
#include "Triangulate.h"
#include "Render/ShaderLoadAndUse.h"

struct Vertex;
struct DrawInfo;
class  Layer;
class  QuadTree;
class  QuadTreeNode;

class MeshManager {
public:
	QuadTree&   m_quadTree;    // 쿼드트리 클래스
	Layer&      m_layer;       // 레이어   클래스
	Triangulate m_triangulate; // 삼각분할 클래스
	
	// 면 메쉬 (들로네 삼각분할)
	GLuint m_polygonVAO = 0;
	GLuint m_polygonVBO = 0;
	std::vector<Vertex>   m_polygonVertices;  // 면 vertex (BuildMapMesh에서 만든 라인 vertex와 별개)
	std::vector<uint32_t> m_polygonIndices;   // 면 삼각형 인덱스
	std::vector<DrawInfo> m_polygonDrawInfos; // 객체별 인덱스 범위 (indexOffset, indexCount)

	// 라인 메쉬
	std::vector<uint32_t> m_lineIndices;   // CPU 측 전체 인덱스 원본
	std::vector<DrawInfo> m_lineDrawInfos; // 객체별 인덱스 범위 (offset, count)

	// 가상 객체 메쉬 (쿼드트리 노드별 간략화된 메쉬)
	std::vector<uint32_t> m_fakeIndices;

	// 순차적으로만 쓰이므로(동시에 필요한 적 없음) 하나로 공유
	GLuint m_visibleIBO = 0;
	std::vector<uint32_t> m_visibleIndices;

	// MBR 시각화 (객체 MBR, 노드 MBR 공용으로 그리는 버퍼)
	std::vector<Vertex> m_mbrBoxVertices;  // 가시 객체 MBR 재구성

public:
	// 객체 메쉬 빌드, 초기화하면서 실행되는 함수들
	MeshManager(Layer& layer, QuadTree& quadtree) : m_layer(layer), m_quadTree(quadtree) {};
	~MeshManager() { Shutdown(); }
	bool InitRenderMesh();    // 메쉬관리 초기화
	void UploadBuffer(GLuint& vao, GLuint& vbo, GLuint* ibo); // GPU 업로드
	void Shutdown(); // GPU 리소스 해제

	void BuildMesh();         // 메쉬 빌드 진입점
	void BuildPointMesh();    // 점 메쉬 빌드
	void BuildPolyLineMesh(); // 선 메쉬 빌드
	void BuildPolygonMesh();  // 면 메쉬 빌드
	void BuildFakeMeshes();   // LOD용 간략화된 메쉬 빌드
	void BuildConvexHullNode(QuadTreeNode& node); // 노드 대표 메쉬, 볼록 껍질 빌드
	void ApplyLevelColors(bool useLevelColor); // 라인/면 vertex 색상 다시 칠하기
	std::vector<glm::dvec2> RamerDouglasPeucker(std::vector<glm::dvec2>& points); // 라인 간략화 알고리즘
	void RDPEpsilon(std::vector<glm::dvec2>& points, std::vector<bool>& isSkipped, int32_t start, int32_t end, double epsilon);

	std::vector<DrawInfo>& GetPolygonDrawInfo() { return m_polygonDrawInfos; } // 면 객체별 인덱스 범위 반환
	std::vector<uint32_t>& GetPolygonIndices()  { return m_polygonIndices;   } // 면 인덱스 반환
	std::vector<Vertex>&   GetPolygonVertices() { return m_polygonVertices;  } // 면 버텍스 반환
};