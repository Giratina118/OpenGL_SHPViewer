#include <pch.h>
#include <execution>
#include "MeshManager.h"
#include "FeatureObject.h"
#include "QuadTree.h"
#include "Layer.h"
#include "TimeDebug.h"

#include "Map/VWorldDownloader.h"

bool MeshManager::InitRenderMesh()
{
	UploadBuffer(m_polygonVAO, m_polygonVBO, nullptr);
	glGenBuffers(1, &m_visibleIBO);

	std::vector<uint8_t> imageData;

	const std::wstring apiKey = L"154D5395-08E5-4375-A82F-10CCC9F63D52";
	const std::wstring path = L"/req/wmts/1.0.0/" + apiKey + L"/Base/10/396/873.png";

	if (VWorldDownloader::Download(L"api.vworld.kr", path, imageData))
		OutputDebugStringA(("[VWORLD] Tile Download Success : " + std::to_string(imageData.size()) + " bytes\n").c_str());
	else
		OutputDebugStringA("[VWORLD] Tile Download Failed\n");

	BuildMesh();

	return true;
}

/*
bool MeshManager::InitRenderMesh()
{
	UploadBuffer(m_polygonVAO, m_polygonVBO, nullptr);
	glGenBuffers(1, &m_visibleIBO);

	BuildMesh();

	std::chrono::steady_clock::time_point fakeMeshStart = std::chrono::steady_clock::now();
	BuildFakeMeshes(); // fake object 활성화
	PrintElapsedTime(L"BuildFakeMeshes", fakeMeshStart);


	return true;
}
*/

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
	auto meshBuildStart = std::chrono::steady_clock::now();

	// 모든 멤버 클리어
	m_polygonVertices.clear();
	m_polygonIndices.clear();
	m_polygonDrawInfos.clear();
	m_lineIndices.clear();
	m_lineDrawInfos.clear();

	switch (m_layer.m_shapeType) {
	case 1: BuildPointMesh();    break;
	case 3: {
		auto lineMeshBuildStart = std::chrono::steady_clock::now();
		BuildPolyLineMesh();
		PrintElapsedTime(L"Build Line Mesh", lineMeshBuildStart);
		break; // 선 정점/인덱스 빌드, 선을 직사각형의 폴리곤으로 만들어 너비를 설정
	}
	case 5: {
		  auto polygonMeshBuildStart = std::chrono::steady_clock::now();
		  BuildPolygonMesh();
		  PrintElapsedTime(L"Build Polygon Mesh", polygonMeshBuildStart);
		  break; // 면 정점/인덱스 빌드
	}
	}

	// 기본 색상 적용 (레벨 색상 OFF 상태)
	ApplyLevelColors(false);

	// GPU 업로드  VBO 하나에 모든 정점
	
	auto meshUploadStart = std::chrono::steady_clock::now();
	glBindVertexArray(m_polygonVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_polygonVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * m_polygonVertices.size(), m_polygonVertices.data(), GL_DYNAMIC_DRAW);
	glBindVertexArray(0);
	PrintElapsedTime(L"Mesh GPU 업로드", meshUploadStart);

	// GPU 업로드 완료, CPU 사본 해제 (더 이상 안 쓰임)
	//m_polygonVertices.shrink_to_fit();
	//m_polygonIndices.shrink_to_fit();
	//m_lineIndices.shrink_to_fit();

	PrintElapsedTime(L"BuildMesh 전체", meshBuildStart);
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

		uint32_t polygonVertCount  = (uint32_t)m_polygonVertices.size() - polygonVertStart;
		uint32_t polygonIndexCount = (uint32_t)m_polygonIndices.size()  - polygonIndexStart;
		uint32_t lineIndexCount    = (uint32_t)m_lineIndices.size()     - lineIndexStart;

		m_polygonDrawInfos[dataId] = { polygonIndexStart, polygonIndexCount, polygonVertStart, polygonVertCount };
		m_lineDrawInfos[dataId]    = { lineIndexStart, lineIndexCount, 0, 0 };
	}
}


// 선(너비를 부여해 직사각형으로) 빌드
void MeshManager::BuildPolyLineMesh()
{
	int32_t polyLineCount = static_cast<int32_t>(m_layer.polyLineObjects.size());
	m_polygonDrawInfos.resize(polyLineCount);
	m_lineDrawInfos.resize(polyLineCount);
	if (polyLineCount == 0) return;

	// 사이즈 할당
	size_t totalSegmentCount = 0;
	for (const PolyObject& polyLine : m_layer.polyLineObjects) {
		for (size_t partNum = 0; partNum < polyLine.parts.size(); partNum++) {
			size_t startPoint = polyLine.parts[partNum];
			size_t endPoint = (partNum + 1 < polyLine.parts.size()) ? polyLine.parts[partNum + 1] : polyLine.points.size();

			if (endPoint > startPoint + 1) totalSegmentCount += endPoint - startPoint - 1;
		}
	}
	m_polygonVertices.reserve(totalSegmentCount * 2);
	m_polygonIndices.reserve(totalSegmentCount * 6);
	m_lineIndices.reserve(totalSegmentCount * 4);


	int64_t rdpTotalMicroseconds = 0;

	std::vector<glm::dvec2> subPointsBuffer;
	subPointsBuffer.reserve(100000);

	for (int32_t dataId = 0; dataId < m_layer.polyLineObjects.size(); dataId++) {
		PolyObject& polyLine = m_layer.polyLineObjects[dataId];
		uint32_t polygonVertStart = (uint32_t)m_polygonVertices.size();
		uint32_t polygonIndexStart = (uint32_t)m_polygonIndices.size();
		uint32_t lineIndexStart = (uint32_t)m_lineIndices.size();

		for (size_t partNum = 0; partNum < polyLine.parts.size(); partNum++) {
			int32_t startPoint = polyLine.parts[partNum];
			int32_t endPoint = (partNum + 1 < polyLine.parts.size()) ? polyLine.parts[partNum + 1] : static_cast<int32_t>(polyLine.points.size());
			int32_t pointCount = endPoint - startPoint;

			if (pointCount < 2) continue; // 점이 2개 미만이면 선 생성 불가

			uint32_t partBaseVertIdx = (uint32_t)m_polygonVertices.size();



			auto rdpStart = std::chrono::steady_clock::now();
			// 1. 만들어둔 버퍼에 값만 덮어씌움 (메모리 재할당 발생 안 함)
			subPointsBuffer.assign(polyLine.points.begin() + startPoint, polyLine.points.begin() + endPoint);
			// 2. 버퍼를 RDP에 전달
			std::vector<glm::dvec2> points = RamerDouglasPeucker(subPointsBuffer);

			rdpTotalMicroseconds += static_cast<int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - rdpStart).count());



			int32_t pointsCount = static_cast<int32_t>(points.size());
			if (pointsCount < 2) continue; // 간략화 후 점이 2개 미만이면 스킵

			// 선분들의 방향(정규화) 벡터를 미리 캐싱해둠 (루프 내 중복 sqrt 연산 제거)
			std::vector<glm::dvec2> segmentDirs(pointsCount - 1);
			for (int32_t j = 0; j < pointsCount - 1; ++j)
				segmentDirs[j] = glm::normalize(points[j + 1] - points[j]);
			

			// 3. 줄어든 점 개수(pointsCount)와 간략화된 배열(points)을 기준으로 루프 순회
			for (int32_t i = 0; i < pointsCount; i++) {
				glm::dvec2 currPt = points[i];

				glm::dvec2 normal(0.0);

				if (i == 0) {
					glm::dvec2 dir = segmentDirs[0];
					normal = glm::dvec2(-dir.y, dir.x);
				}
				else if (i == pointsCount - 1) {
					glm::dvec2 dir = segmentDirs[i - 1];
					normal = glm::dvec2(-dir.y, dir.x);
				}
				else {
					glm::dvec2 dirPrev = segmentDirs[i - 1];
					glm::dvec2 dirNext = segmentDirs[i];

					glm::dvec2 normPrev(-dirPrev.y, dirPrev.x);
					glm::dvec2 normNext(-dirNext.y, dirNext.x);

					normal = glm::normalize(normPrev + normNext);
				}

				glm::dvec2 widthValue = normal * m_layer.m_objSize;
				glm::dvec2 ptLeft = currPt + widthValue;
				glm::dvec2 ptRight = currPt - widthValue;

				// 해당 점에서의 좌/우 버텍스 추가
				m_polygonVertices.push_back({ (float)ptLeft.x,  (float)ptLeft.y,  (float)polyLine.mbrBox.height, 200, 200, 50, 255 });
				m_polygonVertices.push_back({ (float)ptRight.x, (float)ptRight.y, (float)polyLine.mbrBox.height, 200, 200, 50, 255 });

				// 사각형 및 라인 인덱스 구성
				if (i > 0) {
					uint32_t prevLeft = partBaseVertIdx + (i - 1) * 2;
					uint32_t prevRight = partBaseVertIdx + (i - 1) * 2 + 1;
					uint32_t currLeft = partBaseVertIdx + i * 2;
					uint32_t currRight = partBaseVertIdx + i * 2 + 1;

					// 사각형 면 (삼각형 2개)
					m_polygonIndices.push_back(prevLeft);
					m_polygonIndices.push_back(prevRight);
					m_polygonIndices.push_back(currLeft);
					m_polygonIndices.push_back(prevRight);
					m_polygonIndices.push_back(currRight);
					m_polygonIndices.push_back(currLeft);

					// 외각 라인 (좌측 선, 우측 선)
					m_lineIndices.push_back(prevLeft);
					m_lineIndices.push_back(currLeft);
					m_lineIndices.push_back(prevRight);
					m_lineIndices.push_back(currRight);
				}
			}

		}

		uint32_t polygonVertCount = (uint32_t)m_polygonVertices.size() - polygonVertStart;
		uint32_t polygonIndexCount = (uint32_t)m_polygonIndices.size() - polygonIndexStart;
		uint32_t lineIndexCount = (uint32_t)m_lineIndices.size() - lineIndexStart;

		m_polygonDrawInfos[dataId] = { polygonIndexStart, polygonIndexCount, polygonVertStart, polygonVertCount };
		m_lineDrawInfos[dataId] = { lineIndexStart, lineIndexCount, 0, 0 };
	}

	TCHAR debugText[256];
	_stprintf_s(debugText, _T("[TIME] RDP 전체: %.3f ms\n"), rdpTotalMicroseconds / 1000.0);
	OutputDebugString(debugText);
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


	auto cdtStart = std::chrono::high_resolution_clock::now();

	// 병렬 CDT
	std::iota(indexArray.begin(), indexArray.end(), 0);
	std::for_each(std::execution::par, indexArray.begin(), indexArray.end(), [&](int dataId) {
		results[dataId].indices = m_triangulate.TriangulatePolygonCDT(m_layer.polygonObjects[dataId], results[dataId].vertices);
	});

	auto cdtEnd = std::chrono::high_resolution_clock::now();
	OutputDebugStringA(("[TIME] CDT 삼각분할(병렬) : " + std::to_string(std::chrono::duration<double, std::milli>(cdtEnd - cdtStart).count()) + " ms\n").c_str());


	// 루프 돌기 전 밖에서 한 번에 계산
	size_t totalVerts = 0;
	size_t totalPolygonIndices = 0;
	size_t totalLineIndices = 0;

	for (int32_t i = 0; i < polygonCount; i++) {
		if (m_layer.polygonObjects[i].isDeleted) continue;

		const auto& poly = m_layer.polygonObjects[i];

		// 1. 지붕 정점 및 인덱스 수
		totalVerts += results[i].vertices.size();
		totalPolygonIndices += results[i].indices.size();

		// 2. 벽면 정점 수: 점 1개당 선분 1개라고 가정 시, 선분당 정점 4개 추가
		totalVerts += poly.points.size() * 4;

		// 3. 벽면 면 인덱스 수: 선분 1개당 삼각형 2개 = 인덱스 6개 추가
		totalPolygonIndices += poly.points.size() * 6;

		// 4. 벽면 라인 인덱스 수: 선분 1개당 선 2개 = 인덱스 4개 추가
		totalLineIndices += poly.points.size() * 4;
	}
	m_polygonVertices.reserve(totalVerts);
	m_polygonIndices.reserve(totalPolygonIndices);
	m_lineIndices.reserve(totalLineIndices);

	// shp에서 받아온 정점을 렌더링하기 위한 형태로 저장하기
	for (int32_t dataId = 0; dataId < polygonCount; dataId++) {
		PolyObject& polygon = m_layer.polygonObjects[dataId];
		uint32_t polygonIndexStart = (uint32_t)m_polygonIndices.size();
		uint32_t polygonVertStart  = (uint32_t)m_polygonVertices.size();
		uint32_t lineIndexStart    = (uint32_t)m_lineIndices.size();

		if (polygon.isDeleted || results[dataId].vertices.size() < 3 || results[dataId].indices.empty()) {
			m_polygonDrawInfos[dataId] = { polygonIndexStart, 0, polygonVertStart, 0 };
			m_lineDrawInfos[dataId] = { lineIndexStart, 0, 0, 0 };
			continue;
		}

		// 지붕 정점
		//m_polygonVertices.reserve(m_polygonVertices.size() + results[dataId].vertices.size());
		//m_polygonVertices.reserve(m_polygonVertices.size() + polygon.parts.size());
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

		uint32_t polygonIndexCount = static_cast<uint32_t>(m_polygonIndices.size())  - polygonIndexStart;
		uint32_t polygonVertCount  = static_cast<uint32_t>(m_polygonVertices.size()) - polygonVertStart;
		uint32_t lineIndexCount    = static_cast<uint32_t>(m_lineIndices.size())     - lineIndexStart;

		m_polygonDrawInfos[dataId] = { polygonIndexStart, polygonIndexCount, polygonVertStart, polygonVertCount };
		m_lineDrawInfos[dataId] = { lineIndexStart, lineIndexCount, 0, 0 };
	}

	auto assembleEnd = std::chrono::high_resolution_clock::now();
	OutputDebugStringA(("[TIME] 정점/인덱스 조립(순차) : " + std::to_string(std::chrono::duration<double, std::milli>(assembleEnd - cdtEnd).count()) + " ms\n").c_str());

}

// 트리 빌드 후 각 노드의 LOD 메쉬 생성
void MeshManager::BuildFakeMeshes()
{
	if (m_layer.m_shapeType != 5) return;

	m_fakeIndices.clear();
	BuildConvexHullNode(m_quadTree.m_nodes[0]); // 루트 노드부터 재귀적으로 가상 객체 생성 (자식 노드가 없으면 객체 중심점으로, 있으면 자식 노드의 LOD 점들로 볼록껍질 생성)
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

std::vector<glm::dvec2> MeshManager::RamerDouglasPeucker(std::vector<glm::dvec2>& points)
{
	double nearSkip = 2.0 * 2.0; // 두 점 간의 거리가 이보다 짧다면 해당 점을 건너뜀
	double epsilon  = 5.0 * 5.0; // 두 점을 이은 선으로부터 이 범위 안에 들어온 점들은 생략

	std::vector<glm::dvec2> nearSkipPoints, epsilonSkipPoints;
	std::vector<bool> isSkipped;
	nearSkipPoints.reserve(points.size());
	epsilonSkipPoints.reserve(points.size());

	for (glm::dvec2& point : points) {
		if (!nearSkipPoints.empty() && (nearSkipPoints.back().x - point.x) * (nearSkipPoints.back().x - point.x) + (nearSkipPoints.back().y - point.y) * (nearSkipPoints.back().y - point.y) < nearSkip) continue;
		nearSkipPoints.push_back(point);
	}

	// rdp 적용
	isSkipped.resize(nearSkipPoints.size(), false);
	RDPEpsilon(nearSkipPoints, isSkipped, 0, nearSkipPoints.size() - 1, epsilon);

	for (int32_t num = 0; num < isSkipped.size(); num++) {
		if (isSkipped[num]) continue;
		epsilonSkipPoints.push_back(nearSkipPoints[num]);
	}
	
	return epsilonSkipPoints;
}

void MeshManager::RDPEpsilon(std::vector<glm::dvec2>& points, std::vector<bool>& isSkipped, int32_t start, int32_t end, double epsilonSq)
{
	if (end <= start + 1) return;

	glm::dvec2 line = points[end] - points[start];
	double lineLenSq = line.x * line.x + line.y * line.y;

	int32_t farPointNum = -1;

	if (line.x == 0.0 && line.y == 0.0) {
		double  farDistanceSq = epsilonSq;
		for (int32_t num = start + 1; num < end; num++) {
			glm::dvec2 startToPoint = points[num] - points[start];
			double distSq = startToPoint.x * startToPoint.x + startToPoint.y * startToPoint.y;
			
			if (distSq > farDistanceSq) {
				farDistanceSq = distSq;
				farPointNum = num;
			}
		}
	}
	else {
		double  farDistance = epsilonSq * lineLenSq;
		for (int32_t num = start + 1; num < end; num++) {
			glm::dvec2 startToPoint = points[num] - points[start];

			double cross = startToPoint.x * line.y - startToPoint.y * line.x;
			double distance = cross * cross;
			if (distance > farDistance) {
				farDistance = distance;
				farPointNum = num;
			}
		}
	}

	if (farPointNum != -1) {
		RDPEpsilon(points, isSkipped, start, farPointNum, epsilonSq);
		RDPEpsilon(points, isSkipped, farPointNum, end, epsilonSq);
	}
	else {
		for (int32_t num = start + 1; num < end; num++)
			isSkipped[num] = true;
	}
}