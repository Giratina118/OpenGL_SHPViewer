#pragma once

#include <cstdint>
#include <vector>
#include <FeatureObject.h>
#include <glm/glm.hpp>

class CameraController;
class Layer;
struct DrawInfo;
struct Vertex;

// 자식 노드 위치 타입
enum class NodeChild {
	NorthWest = 0,
	NorthEast = 1,
	SouthWest = 2,
	SouthEast = 3
};

// 쿼드트리 노드 클래스
class QuadTreeNode
{
public:
	QuadTreeNode(int32_t level, BoundingBox& mbrBox);

	int32_t m_level; // 깊이
	int32_t m_childNodes[4] = { -1, -1, -1, -1 }; // 자식노드의 ID (좌상, 우상, 좌하, 우하), -1: 자식 없음 의미
	std::vector<int32_t> m_objectIds; // 이 노드에 들어있는 객체ID 목록
	BoundingBox m_boundingBox; // 노드 영역 사각형

	// 이 노드의 LOD 대표 메쉬 위치 Renderer의 m_lodVertices/m_lodIndices 배열에서 어디서 몇 개인지
	uint32_t m_lodVertexOffset = 0;
	uint32_t m_lodVertexCount  = 0;
	uint32_t m_lodIndexOffset  = 0;
	uint32_t m_lodIndexCount   = 0;
	// m_lodIndexCount == 0 이면 LOD 메쉬 없음

	int32_t m_parentNodeId  = -1; // 부모노드 ID (루트노드는 0)
	bool    m_isVisibleNode = false;
	bool    m_isVisibleFake = false;
};

// 쿼드트리 클래스
class QuadTree
{
public:
	QuadTree(Layer& layer) : m_layer(layer) {};

	int32_t                   m_maxLevel = 1;   // 최대 깊이
	std::vector<QuadTreeNode> m_nodes;          // 트리 노드
	std::vector<int32_t>	  m_objectLevels;   // 객체ID -> 트리 레벨 매핑 (레벨 색상, MBR 색상에 사용)
	std::vector<int32_t>      m_visibleNodeIds; // 마지막 검색에서 통과한 노드들
	std::vector<int32_t>      m_visibleNodeFakeObjIds; // 마지막 검색에서 통과한 노드들
	Layer& m_layer;    // 레이어

	double m_limitSizeRate = 0.25;  // 노드 길이 * m_limitSizeRate 보다 객체의 길이가 크다면 자식 노드에 넣기에 크다고 판단하여 자식 노드에 넣지 않고 현재 노드에 넣는다.
	double m_looseBoxRate  = 0.075; // 느슨한 박스 = 노드 길이 * m_looseBoxRate 만큼 확장
	double m_minNodeLength = 500.0;   // 노드 길이가 이보다 작으면 더 이상 분할하지 않는다.

	// 성능 측정용 카운터 (매 탐색마다 리셋)
	//int64_t m_visitedNodeCount = 0; // SearchRenderingData가 진입한 노드 수
	//int64_t m_frustumTestCount = 0; // GetFrustumState 호출 횟수 (노드 + 객체)
	//int64_t m_objectTestCount  = 0; // 객체 단위 절두체 검사 횟수
	//int64_t m_objectnotCullCount = 0; // 객체 단위 절두체 검사 횟수

public:
	void BuildQuadTree(); // 트리 빌드(생성)
	void CreateNode(int32_t level, BoundingBox& boundingBox) { m_nodes.emplace_back(level, boundingBox); } // 노드 생성
	void InsertData(int32_t currentNodeId, int32_t dataId, BoundingBox& dataMbrBox, bool isBuilding); // 노드 삽입
	int32_t DivideNode  (int32_t currentNodeId, int32_t childNodeType); // 노드 분할
	double SettingHeight(int32_t currentNodeId);

	// LOD: minScreenPixels보다 작게 그려질 노드/객체는 컬링
	void SearchRenderingData  (std::vector<int32_t>& renderObjectIds, int32_t currentNodeId, const CameraController& camera, const glm::dvec3& cameraPos, double worldToScreenScale);
	void InputRenderingDataAll(std::vector<int32_t>& renderObjectIds, int32_t currentNodeId, const CameraController& camera, const glm::dvec3& cameraPos, double worldToScreenScale);
	
	int32_t SearchPickingData(glm::dvec3& cameraPos, glm::dvec3& rayDir, int32_t currentNodeId, double& minDistance, std::vector<DrawInfo>& polygonDrawInfos, std::vector<uint32_t>& polygonIndices, std::vector<Vertex>& polygonVertices); // 피킹 데이터 탐색 및 반환
	double OnCollisionRayTriangle(glm::dvec3& rayStart, glm::dvec3& rayDir, glm::dvec3& trianglePoint1, glm::dvec3& trianglePoint2, glm::dvec3& trianglePoint3);
	double RayTriangle(const glm::dvec3& rayStart, const glm::dvec3& rayDir, const glm::dvec3& trianglePoint1, const glm::dvec3& trianglePoint2, const glm::dvec3& trianglePoint3);
};