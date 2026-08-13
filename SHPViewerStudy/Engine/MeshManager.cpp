#include <pch.h>
#include <execution>
#include "MeshManager.h"
#include "FeatureObject.h"
#include "QuadTree.h"
#include "Layer.h"

bool MeshManager::InitRenderMesh()
{
	UploadBuffer(m_polygonVAO, m_polygonVBO, nullptr);

	//glGenBuffers(1, &m_polygonIBOVisible);
	//glGenBuffers(1, &m_lineIBOVisible);
	//glGenBuffers(1, &m_fakeIBO);
	//glGenBuffers(1, &m_fakeIBOVisible);
	glGenBuffers(1, &m_visibleIBO);

	BuildMesh();
	BuildFakeMeshes(); // fake object 활성화

	return true;
}

void MeshManager::UploadBuffer(GLuint& vao, GLuint& vbo, GLuint* ibo)
{
	glGenVertexArrays(1, &vao);			// vao 생성(id를 1개 생성해서 변수에 담기)
	glGenBuffers(1, &vbo);				// vbo id 1개 생성
	glBindVertexArray(vao);				// 조작할 vao 등록
	glBindBuffer(GL_ARRAY_BUFFER, vbo);	// 조작할 vbo 등록

	// location 0: position (vec3)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); // GPU에게 데이터 구조(Vertex) 전달
	glEnableVertexAttribArray(0);		// 방금 세팅한 슬롯(0번)을 활성화

	// location 1: color (vec4)
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)(sizeof(float) * 3));
	glEnableVertexAttribArray(1);

	if (ibo) { // ibo가 있는 경우
		glGenBuffers(1, ibo); // ibo id 1개 생성
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ibo); // 조작할 ibo 등록
	}

	glBindVertexArray(0); // vao 등록 해제
}

// GPU 리소스 해제
void MeshManager::Shutdown()
{
	if (m_visibleIBO) { glDeleteBuffers     (1, &m_visibleIBO); m_visibleIBO = 0; }
	if (m_polygonVBO) { glDeleteBuffers     (1, &m_polygonVBO); m_polygonVBO = 0; }
	if (m_polygonVAO) { glDeleteVertexArrays(1, &m_polygonVAO); m_polygonVAO = 0; }

}


// 메쉬 빌드
// 메쉬 빌드 진입점, 파일이 열리면 실행
void MeshManager::BuildMesh()
{
	// 모든 멤버 클리어
	m_polygonVertices.clear();
	m_polygonIndices.clear();
	m_polygonDrawInfos.clear();
	//m_polygonVisibleIndices.clear();
	m_lineIndices.clear();
	m_lineDrawInfos.clear();
	//m_lineVisibleIndices.clear();

	switch (m_layer.m_shapeType) {
	case 1: BuildPointMesh();    break;
	case 3: BuildPolyLineMesh(); break; // 선 정점/인덱스 빌드, 선을 직사각형의 폴리곤으로 만들어 너비를 설정
	case 5: BuildPolygonMesh();  break; // 면 정점/인덱스 빌드
	}

	// 기본 색상 적용 (레벨 색상 OFF 상태)
	ApplyLevelColors(false);

	// GPU 업로드  VBO 하나에 모든 정점
	glBindVertexArray(m_polygonVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_polygonVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * m_polygonVertices.size(), m_polygonVertices.data(), GL_DYNAMIC_DRAW);
	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_polygonIBO);
	//glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m_polygonIndices.size(), m_polygonIndices.data(), GL_STATIC_DRAW);
	glBindVertexArray(0);

	// 라인 IBO 업로드
	//if (!m_lineIndices.empty()) {
		//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lineIBOVisible);
		//glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m_lineIndices.size(), m_lineIndices.data(), GL_STATIC_DRAW);
	//}

	// GPU 업로드 완료, CPU 사본 해제 (더 이상 안 쓰임)
	m_polygonVertices.shrink_to_fit();
	m_polygonIndices.shrink_to_fit();
	m_lineIndices.shrink_to_fit();
}

void MeshManager::BuildPointMesh()
{
	int32_t pointCount = static_cast<int32_t>(m_layer.pointObjects.size());
	m_polygonDrawInfos.resize(pointCount);
	m_lineDrawInfos.resize(pointCount);
	m_polygonVertices.reserve(pointCount * 12);
	m_polygonIndices.reserve(pointCount * 12);
	m_lineIndices.reserve(pointCount * 12);

	if (pointCount == 0) return;

	double radian = 10.0;
	glm::dvec2 dir[12] = {
		{ 1.0,  0.0}, { 0.866,  0.5},   { 0.5,    0.866},
		{ 0.0,  1.0}, {-0.5,    0.866}, {-0.866,  0.5},
		{-1.0,  0.0}, {-0.866, -0.5},   {-0.5,   -0.866},
		{ 0.0, -1.0}, { 0.5,   -0.866}, { 0.866, -0.5}
	};

	for (int32_t dataId = 0; dataId < pointCount; dataId++) {
		PointObject& pointObj = m_layer.pointObjects[dataId];
		uint32_t polygonVertStart = (uint32_t)m_polygonVertices.size();
		uint32_t polygonIndexStart = (uint32_t)m_polygonIndices.size();
		uint32_t lineIndexStart = (uint32_t)m_lineIndices.size();

		for (int32_t angle = 0; angle < 12; angle++) {
			glm::dvec2 point = pointObj.point + dir[angle] * m_layer.m_objSize;
			m_polygonVertices.push_back({ static_cast<float>(point.x), static_cast<float>(point.y), 10.0, 200, 200, 200, 255 });
		}
		for (int32_t indicesNum = 0; indicesNum < 10; indicesNum++) {
			m_polygonIndices.push_back({ polygonVertStart });
			m_polygonIndices.push_back({ polygonVertStart + indicesNum + 1 });
			m_polygonIndices.push_back({ polygonVertStart + indicesNum + 2 });
		}
		for (int32_t indicesNum = 0; indicesNum < 11; indicesNum++) {
			m_lineIndices.push_back({ polygonVertStart + indicesNum });
			m_lineIndices.push_back({ polygonVertStart + indicesNum + 1 });
		}
		m_lineIndices.push_back({ polygonVertStart + 11 });
		m_lineIndices.push_back({ polygonVertStart });

		uint32_t polygonVertCount = (uint32_t)m_polygonVertices.size() - polygonVertStart;
		uint32_t polygonIndexCount = (uint32_t)m_polygonIndices.size() - polygonIndexStart;
		uint32_t lineIndexCount = (uint32_t)m_lineIndices.size() - lineIndexStart;

		m_polygonDrawInfos[dataId] = { polygonIndexStart, polygonIndexCount, polygonVertStart, polygonVertCount };
		m_lineDrawInfos[dataId] = { lineIndexStart, lineIndexCount, 0, 0 };
	}

	m_polygonVertices.shrink_to_fit();
	m_polygonIndices.shrink_to_fit();
	m_lineIndices.shrink_to_fit();
}

// 선(너비를 부여해 직사각형으로) 빌드
void MeshManager::BuildPolyLineMesh()
{
	int32_t polyLineCount = static_cast<int32_t>(m_layer.polyLineObjects.size());
	m_polygonDrawInfos.resize(polyLineCount);
	m_lineDrawInfos.resize(polyLineCount);
	if (polyLineCount == 0) return;

	for (int32_t dataId = 0; dataId < m_layer.polyLineObjects.size(); dataId++) {
		PolyObject& polyLine = m_layer.polyLineObjects[dataId];
		uint32_t polygonVertStart = (uint32_t)m_polygonVertices.size();
		uint32_t polygonIndexStart = (uint32_t)m_polygonIndices.size();
		uint32_t lineIndexStart = (uint32_t)m_lineIndices.size();

		m_polygonVertices.reserve(m_polygonVertices.size() + (((polyLine.points.size() - 1) > 0) ? (polyLine.points.size() - 1) : 0) * 4);
		m_polygonIndices.reserve(m_polygonVertices.size() + (((polyLine.points.size() - 1) > 0) ? (polyLine.points.size() - 1) : 0) * 6);
		m_lineIndices.reserve(m_polygonVertices.size() + (((polyLine.points.size() - 1) > 0) ? (polyLine.points.size() - 1) : 0) * 4);

		for (int32_t partNum = 0; partNum < polyLine.parts.size(); partNum++) {
			int32_t startPoint = polyLine.parts[partNum];
			int32_t endPoint = (partNum + 1 < polyLine.parts.size()) ? polyLine.parts[partNum + 1] : static_cast<int32_t>(polyLine.points.size());
			for (int32_t pointNum = startPoint; pointNum < endPoint - 1; pointNum++) {
				glm::dvec2 pointOrigin1 = polyLine.points[pointNum];     //+(point.points[pointNum]     - point.mbrBox.GetCenter()) * 100.0;
				glm::dvec2 pointOrigin2 = polyLine.points[pointNum + 1]; //+(point.points[pointNum + 1] - point.mbrBox.GetCenter()) * 100.0;
				glm::dvec2 lineDir = pointOrigin2 - pointOrigin1;   // 이 방향 벡터에 수직인 방향으로 두 point에 m_layer.m_objSize 만큼 떨어진 거리에 점 생성
				glm::dvec2 widthDir = glm::normalize(glm::dvec2(-lineDir.y, lineDir.x));   // 수직 벡터(너비 방향 벡터)
				glm::dvec2 widthValue = widthDir * m_layer.m_objSize;

				glm::dvec2 point1 = pointOrigin1 + widthValue;
				glm::dvec2 point2 = pointOrigin1 - widthValue;
				glm::dvec2 point3 = pointOrigin2 + widthValue;
				glm::dvec2 point4 = pointOrigin2 - widthValue;

				uint32_t pointVertexNum = (int32_t)m_polygonVertices.size();

				m_polygonVertices.push_back({ (float)point1.x, (float)point1.y, 0.0, 200, 200, 50, 255 });
				m_polygonVertices.push_back({ (float)point2.x, (float)point2.y, 0.0, 200, 200, 50, 255 });
				m_polygonVertices.push_back({ (float)point3.x, (float)point3.y, 0.0, 200, 200, 50, 255 });
				m_polygonVertices.push_back({ (float)point4.x, (float)point4.y, 0.0, 200, 200, 50, 255 });

				m_polygonIndices.push_back({ pointVertexNum });
				m_polygonIndices.push_back({ pointVertexNum + 1 });
				m_polygonIndices.push_back({ pointVertexNum + 2 });
				m_polygonIndices.push_back({ pointVertexNum + 1 });
				m_polygonIndices.push_back({ pointVertexNum + 2 });
				m_polygonIndices.push_back({ pointVertexNum + 3 });

				m_lineIndices.push_back({ pointVertexNum });
				m_lineIndices.push_back({ pointVertexNum + 2 });
				m_lineIndices.push_back({ pointVertexNum + 1 });
				m_lineIndices.push_back({ pointVertexNum + 3 });
			}
		}

		uint32_t polygonVertCount = (uint32_t)m_polygonVertices.size() - polygonVertStart;
		uint32_t polygonIndexCount = (uint32_t)m_polygonIndices.size() - polygonIndexStart;
		uint32_t lineIndexCount = (uint32_t)m_lineIndices.size() - lineIndexStart;

		m_polygonDrawInfos[dataId] = { polygonIndexStart, polygonIndexCount, polygonVertStart, polygonVertCount };
		m_lineDrawInfos[dataId] = { lineIndexStart, lineIndexCount, 0, 0 };
	}

	m_polygonVertices.shrink_to_fit();
	m_polygonIndices.shrink_to_fit();
	m_lineIndices.shrink_to_fit();
}

// 면(지붕 + 벽) 메쉬 빌드. 폴리곤 객체만 대상
void MeshManager::BuildPolygonMesh()
{
	int32_t polygonCount = static_cast<int32_t>(m_layer.polygonObjects.size());
	m_polygonDrawInfos.resize(polygonCount);
	m_lineDrawInfos.resize(polygonCount);
	if (polygonCount == 0) return;

	// 삼각분할
	struct TriResult {
		std::vector<glm::dvec2> vertices;
		std::vector<uint32_t>   indices;
	};
	std::vector<TriResult> results(polygonCount);
	std::vector<int> indexArray(polygonCount);

	// 병렬 CDT
	std::iota(indexArray.begin(), indexArray.end(), 0);
	std::for_each(std::execution::par, indexArray.begin(), indexArray.end(), [&](int dataId) {
		results[dataId].indices = m_triangulate.TriangulatePolygonCDT(m_layer.polygonObjects[dataId], results[dataId].vertices);
	});

	// shp에서 받아온 정점을 렌더링하기 위한 형태로 저장하기
	for (int32_t dataId = 0; dataId < polygonCount; dataId++) {
		PolyObject& polygon = m_layer.polygonObjects[dataId];
		uint32_t polygonIndexStart = (uint32_t)m_polygonIndices.size();
		uint32_t polygonVertStart = (uint32_t)m_polygonVertices.size();
		uint32_t lineIndexStart = (uint32_t)m_lineIndices.size();

		if (results[dataId].vertices.size() < 3 || results[dataId].indices.empty()) {
			m_polygonDrawInfos[dataId] = { polygonIndexStart, 0, polygonVertStart, 0 };
			m_lineDrawInfos[dataId] = { lineIndexStart, 0, 0, 0 };
			continue;
		}

		// 지붕 정점
		m_polygonVertices.reserve(m_polygonVertices.size() + results[dataId].vertices.size());
		m_polygonVertices.reserve(m_polygonVertices.size() + polygon.parts.size());
		uint32_t roofVertexBase = (uint32_t)m_polygonVertices.size();
		for (const auto& point : results[dataId].vertices)
			m_polygonVertices.push_back({ (float)point.x, (float)point.y, (float)polygon.mbrBox.height, 190, 190, 220, 255 });

		for (uint32_t index : results[dataId].indices)
			m_polygonIndices.push_back(roofVertexBase + index);

		// 벽 정점 + 라인 인덱스 동시 빌드
		size_t partCount = polygon.parts.size();
		for (size_t partNum = 0; partNum < partCount; partNum++) {
			size_t startPoint = polygon.parts[partNum];
			size_t endPoint = (partNum + 1 < partCount) ? polygon.parts[partNum + 1] : polygon.points.size();
			if (endPoint <= startPoint + 1) continue;

			// SHP ring 닫힘점 제거
			size_t wallEnd = endPoint;
			if (wallEnd > startPoint + 1 && polygon.points[wallEnd - 1].x == polygon.points[startPoint].x && polygon.points[wallEnd - 1].y == polygon.points[startPoint].y) wallEnd--;
			if (wallEnd <= startPoint + 1) continue;

			size_t ringSize = wallEnd - startPoint;
			for (size_t ringNum = 0; ringNum < ringSize; ringNum++) {
				size_t index0 = startPoint + ringNum;
				size_t index1 = startPoint + (ringNum + 1) % ringSize;

				// 정점 저장
				const glm::vec2& point0 = static_cast<glm::vec2>(polygon.points[index0]);
				const glm::vec2& point1 = static_cast<glm::vec2>(polygon.points[index1]);
				float height = static_cast<float>(polygon.mbrBox.height);

				// 인덱스 저장
				uint32_t wallBottom0 = (uint32_t)m_polygonVertices.size();
				uint32_t wallBottom1 = wallBottom0 + 1;
				uint32_t wallTop0 = wallBottom0 + 2;
				uint32_t wallTop1 = wallBottom0 + 3;

				// 정점
				m_polygonVertices.push_back({ point0.x, point0.y, 0.0f,   100, 100, 120, 255 });
				m_polygonVertices.push_back({ point1.x, point1.y, 0.0f,   100, 100, 120, 255 });
				m_polygonVertices.push_back({ point0.x, point0.y, height, 160, 160, 180, 255 });
				m_polygonVertices.push_back({ point1.x, point1.y, height, 160, 160, 180, 255 });

				// 벽면: 삼각형 2개
				m_polygonIndices.push_back(wallBottom0);
				m_polygonIndices.push_back(wallTop0);
				m_polygonIndices.push_back(wallTop1);
				m_polygonIndices.push_back(wallBottom0);
				m_polygonIndices.push_back(wallTop1);
				m_polygonIndices.push_back(wallBottom1);

				// 라인: 벽 상단 모서리(지붕 높이)와 벽 하단 모서리
				m_lineIndices.push_back(wallTop0);    // 상단 수평선: wallTop0 → wallTop1
				m_lineIndices.push_back(wallTop1);
				m_lineIndices.push_back(wallBottom0); // 수직선: wallBottom0 → wallTop0
				m_lineIndices.push_back(wallTop0);
			}
		}

		uint32_t polygonIndexCount = (uint32_t)m_polygonIndices.size() - polygonIndexStart;
		uint32_t polygonVertCount = (uint32_t)m_polygonVertices.size() - polygonVertStart;
		uint32_t lineIndexCount = (uint32_t)m_lineIndices.size() - lineIndexStart;

		m_polygonDrawInfos[dataId] = { polygonIndexStart, polygonIndexCount, polygonVertStart, polygonVertCount };
		m_lineDrawInfos[dataId] = { lineIndexStart, lineIndexCount, 0, 0 };
	}
	//m_polygonVisibleIndices.reserve(m_polygonIndices.size());
	//m_lineVisibleIndices.reserve(m_lineIndices.size());

	m_polygonVertices.shrink_to_fit();
	m_polygonIndices.shrink_to_fit();
	m_lineIndices.shrink_to_fit();
}

// 트리 빌드 후 각 노드의 LOD 메쉬 생성
void MeshManager::BuildFakeMeshes()
{
	if (m_layer.m_shapeType != 5) return;

	m_fakeIndices.clear();
	BuildConvexHullNode(m_quadTree.m_nodes[0]); // 루트 노드부터 재귀적으로 가상 객체 생성 (자식 노드가 없으면 객체 중심점으로, 있으면 자식 노드의 LOD 점들로 볼록껍질 생성)
	//if (m_fakeIndices.empty()) return; // 데이터 없으면 업로드 스킵

	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fakeIBO);
	//glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m_fakeIndices.size(), m_fakeIndices.data(), GL_STATIC_DRAW);

	//m_fakeVisibleIndices.reserve(m_fakeIndices.size());
	m_fakeIndices.shrink_to_fit();
}

// hull 계산에 쓸 점: 좌표 + m_polygonVertices에서의 원래 인덱스
struct HullPoint {
	double   x, y;
	uint32_t originalIndex; // m_polygonVertices 인덱스
};

bool ComparePointXY(const HullPoint& a, const HullPoint& b) {
	return a.x < b.x || (a.x == b.x && a.y < b.y);
}

// 노드의 객체 중심점들을 이용한 가상 객체 생성, 폴리곤일때만
void MeshManager::BuildConvexHullNode(QuadTreeNode& node)
{
	std::vector<HullPoint> inputPoints;
	bool isLeaf = (node.m_childNodes[0] == -1 && node.m_childNodes[1] == -1 && node.m_childNodes[2] == -1 && node.m_childNodes[3] == -1); // 리프노드인지 여부

	if (isLeaf) { // 객체가 3개 이하이면 생략
		if (node.m_objectIds.size() < 3) {
			node.m_lodVertexOffset = -1;
			node.m_lodIndexOffset = -1;
			node.m_lodVertexCount = 0;
			node.m_lodIndexCount = 0;
			return;
		}
		inputPoints.reserve(node.m_objectIds.size());

		for (int32_t objectId : node.m_objectIds) { // 정점 수집
			if (objectId >= static_cast<int32_t>(m_polygonDrawInfos.size())) continue;

			const DrawInfo& info = m_polygonDrawInfos[objectId];
			if (info.vertexCount == 0) continue;

			// 지붕 정점만 수집
			for (uint32_t num = info.vertexOffset; num < info.vertexOffset + info.vertexCount; num++) {
				const Vertex& vertex = m_polygonVertices[num];

				if (vertex.z < 0.05f) continue;
				inputPoints.push_back({ vertex.x, vertex.y, num });
			}
		}
	}
	else {
		// 내부 노드: 자식 먼저 재귀
		for (int32_t childIdx = 0; childIdx < 4; childIdx++) {
			int32_t childNodeId = node.m_childNodes[childIdx];
			if (childNodeId == -1) continue;
			BuildConvexHullNode(m_quadTree.m_nodes[childNodeId]);
		}

		// 자식 hull이 참조하는 m_lodIndices 슬롯에서 원래 정점 인덱스 수집
		for (int32_t childIdx = 0; childIdx < 4; childIdx++) {
			int32_t childNodeId = node.m_childNodes[childIdx];
			if (childNodeId == -1) continue;
			const QuadTreeNode& child = m_quadTree.m_nodes[childNodeId];
			if (child.m_lodIndexCount == 0) continue;

			// child.m_lodIndexOffset ~ +m_lodVertexCount 범위가 m_lodIndices에서 hull 정점 인덱스를 저장한 위치
			for (uint32_t i = child.m_lodVertexOffset;
				i < child.m_lodVertexOffset + child.m_lodVertexCount; i++) {
				uint32_t polygonVertexIndex = m_fakeIndices[i]; // 원래 폴리곤 정점 인덱스
				const Vertex& v = m_polygonVertices[polygonVertexIndex];
				inputPoints.push_back({ v.x, v.y, polygonVertexIndex });
			}
		}

		if (inputPoints.size() < 3) {
			node.m_lodVertexOffset = -1; node.m_lodIndexOffset = -1;
			node.m_lodVertexCount = 0;  node.m_lodIndexCount = 0;
			return;
		}
	}

	// 정렬 (x, 그다음 y)
	std::sort(inputPoints.begin(), inputPoints.end(), ComparePointXY);

	// 로컬 벡터에서 monotone chain (전역 m_lodVertices를 직접 조작하지 않음)
	int32_t num = static_cast<int32_t>(inputPoints.size());
	std::vector<HullPoint> hull(2 * num);
	int32_t hullPointCount = 0; // hull에 쌓인 점 개수

	// cross product (HullPoint용)
	auto cross = [](const HullPoint& O, const HullPoint& A, const HullPoint& B) { return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);	};

	// 아래쪽 체인
	for (int32_t i = 0; i < num; i++) {
		while (hullPointCount >= 2 && cross(hull[hullPointCount - 2], hull[hullPointCount - 1], inputPoints[i]) <= 0) hullPointCount--;
		hull[hullPointCount++] = inputPoints[i];
	}
	// 위쪽 체인
	for (int32_t i = num - 2, t = hullPointCount + 1; i >= 0; i--) {
		while (hullPointCount >= t && cross(hull[hullPointCount - 2], hull[hullPointCount - 1], inputPoints[i]) <= 0) hullPointCount--;
		hull[hullPointCount++] = inputPoints[i];
	}
	// 마지막 점은 시작점과 중복이므로 제거
	hullPointCount--;

	if (hullPointCount < 3) { // 일직선 등 → hull 실패
		node.m_lodVertexOffset = -1; node.m_lodIndexOffset = -1;
		node.m_lodVertexCount = 0; node.m_lodIndexCount = 0;
		return;
	}


	// m_lodVertexOffset = hull 정점 인덱스들이 시작되는 m_lodIndices 위치
	node.m_lodVertexOffset = (uint32_t)m_fakeIndices.size();
	node.m_lodVertexCount = (uint32_t)hullPointCount;

	for (int32_t i = 0; i < hullPointCount; i++)
		m_fakeIndices.push_back(hull[i].originalIndex); // 폴리곤 정점 인덱스

	// fan 삼각분할: 정점 0을 중심으로 (0, i, i+1) 삼각형 생성
	node.m_lodIndexOffset = (uint32_t)m_fakeIndices.size();
	uint32_t base = node.m_lodVertexOffset;
	for (int32_t i = 1; i < hullPointCount - 1; i++) {
		m_fakeIndices.push_back(m_fakeIndices[base]); // 실제 폴리곤 정점 인덱스
		m_fakeIndices.push_back(m_fakeIndices[base + i]);
		m_fakeIndices.push_back(m_fakeIndices[base + i + 1]);
	}
	node.m_lodIndexCount = (uint32_t)m_fakeIndices.size() - node.m_lodIndexOffset;
}

// 라인/면의 모든 vertex 색상을 다시 칠하고 GPU에 부분 업데이트
// useLevelColor=true: 객체별 트리 레벨 색상, useLevelColor=false: 라인=회색, 면=연회색
// 면은 z값으로 위/아래 명도 자동 보정 (벽 그라디언트)
// glBufferSubData로 vertex 좌표는 유지, 색상만 GPU 갱신
void MeshManager::ApplyLevelColors(bool useLevelColor)
{
	// 면 색상
	int32_t polygonCount = static_cast<int32_t>(m_polygonDrawInfos.size());
	for (int32_t dataId = 0; dataId < polygonCount; dataId++) {
		const DrawInfo& info = m_polygonDrawInfos[dataId];
		if (info.indexCount == 0) continue;

		//unsigned char r, g, b;
		UCharColor color;
		if (useLevelColor) SetColorFromLevel(m_quadTree.m_objectLevels[dataId], color);
		else               color = m_layer.m_baseColor;

		uint32_t end = info.indexOffset + info.indexCount;
		for (uint32_t ii = info.indexOffset; ii < end; ii++) {
			uint32_t vertexIndex = m_polygonIndices[ii];
			if (vertexIndex >= m_polygonVertices.size()) continue;
			Vertex& vertex = m_polygonVertices[vertexIndex];

			// z = 0 → 벽 하단 (어둡게), z = 1 → 지붕/벽 상단 (밝게)
			int32_t shade = (vertex.z < 0.5f && m_layer.m_isBuilding) ? 3 : 2;
			vertex.color = color / shade * 2;
		}
	}

	// GPU 재업로드 (BufferSubData)
	if (!m_polygonVertices.empty()) {
		glBindBuffer(GL_ARRAY_BUFFER, m_polygonVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * m_polygonVertices.size(), m_polygonVertices.data());
	}
}