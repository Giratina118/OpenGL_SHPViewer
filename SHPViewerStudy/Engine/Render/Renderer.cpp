#include <pch.h>
#include <fstream>
#include <filesystem>
#include <execution>
#include <numeric>
#include <algorithm>

// Engine
#include "Renderer.h"
#include "Layer.h"
#include <glm/gtc/type_ptr.hpp>


// 전체 초기화 진입점, EGL/셰이더/버퍼/상태/쿼드트리를 준비
bool Renderer::Initialize(HWND hWnd)
{
	// 셰이더 설정
	if (!m_shader.CreateProgram("Resource/Shader/shader.vert", "Resource/Shader/shader.frag")) return false;
	m_viewProjectionLocation  = glGetUniformLocation(m_shader.GetProgram(), "u_viewProjection");
	m_colorMultiplierLocation = glGetUniformLocation(m_shader.GetProgram(), "u_colorMultiplier");
	
	// 깊이 테스트 활성화 (3D 건물, 면/라인 z-fighting 해결에 필요)
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_POLYGON_OFFSET_FILL); // 면 위에 라인을 그릴 때 z-fighting 방지, 면을 살짝 뒤로 밀어서 라인이 면 위에 보이게 함
	glPolygonOffset(1.0f, 1.0f);

	// 버퍼 등록
	GLuint iboNull = -1; // 인덱스버퍼 없는 것들
	InitBuffer(m_polygonVAO, m_polygonVBO, &m_polygonIBO); // 폴리곤 버퍼
	InitBuffer(m_mbrVAO,     m_mbrVBO,     nullptr);       // MBR박스 그리기 버퍼

	glGenBuffers(1, &m_polygonIBOVisible); // 가시 폴리곤 인덱스 버퍼
	glGenBuffers(1, &m_lineIBOVisible);	   // 가시 라인 인덱스 전용 버퍼
	glGenBuffers(1, &m_fakeIBO);           // 가상 객체 인덱스 버퍼
	glGenBuffers(1, &m_fakeIBOVisible);	   // 가시 가상 객체 인덱스 버퍼

	RebuildQuadTree(); // 쿼드트리 생성
	BuildMesh();

	return true;
}

// 버퍼 초기화, VAO + VBO (+ 옵션 IBO) 한 묶음으로 생성
void Renderer::InitBuffer(GLuint& vao, GLuint& vbo, GLuint* ibo)
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

// 업로드 & 그리기 (객체 mbr, 노드 mbr, 절두체 사각형 등)
void Renderer::UploadAndDraw(GLuint& vao, GLuint& vbo, std::vector<Vertex>& vertices, int drawType)
{
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
	glDrawArrays(drawType, 0, (GLsizei)vertices.size());
	glBindVertexArray(0);
}

// GPU 리소스 해제
void Renderer::Shutdown(HWND m_hWnd)
{
	if (m_shader.GetProgram() != 0) glDeleteProgram(m_shader.GetProgram());
	
	// 라인 메쉬 버퍼 해제
	if (m_lineIBOVisible)    glDeleteBuffers(1, &m_lineIBOVisible);
	if (m_polygonIBOVisible) glDeleteBuffers(1, &m_polygonIBOVisible);
	if (m_fakeIBOVisible)    glDeleteBuffers(1, &m_fakeIBOVisible);

	// 면 메쉬 버퍼 해제
	if (m_polygonIBO)        glDeleteBuffers(1, &m_polygonIBO);
	if (m_polygonVBO)        glDeleteBuffers(1, &m_polygonVBO);
	if (m_polygonVAO)        glDeleteVertexArrays(1, &m_polygonVAO);

	// MBR 버퍼 해제
	if (m_mbrVBO)            glDeleteBuffers(1, &m_mbrVBO);
	if (m_mbrVAO)            glDeleteVertexArrays(1, &m_mbrVAO);

	// 가상 객체 버퍼 해제
	if (m_fakeIBO)           glDeleteBuffers(1, &m_fakeIBO);
}

// 메인 렌더 함수
void Renderer::Render(CameraController& camera, UIState& uiState, int32_t screenWidth, int32_t screenHeight, int32_t panelWidthLeft, glm::dvec3 hitPoint)
{
	// 쿼드트리에서 가시 객체 검색 (컬링, LOD)
	if (!uiState.isShowFrustumView) {
		m_renderObjectIds.clear();
		m_quadTree.m_visibleNodeIds.clear();
		m_quadTree.m_visibleNodeFakeObjIds.clear();

		double halfFovRad = glm::radians(camera.fov * 0.5);
		double lodFactor  = std::max(screenHeight, 1) / (2.0 * std::tan(halfFovRad));
		m_quadTree.SearchRenderingData(m_renderObjectIds, 0, camera, camera.transform.position, lodFactor);

		m_currentRenderCount     = static_cast<int32_t>(m_renderObjectIds.size());
		m_currentRenderFakeCount = static_cast<int32_t>(m_quadTree.m_visibleNodeFakeObjIds.size());
	}

	// 셰이더 설정
	m_shader.UseProgram();
	glUniformMatrix4fv(m_viewProjectionLocation, 1, GL_FALSE, glm::value_ptr(glm::mat4(camera.GetMatrix())));
	glUniform1f(m_colorMultiplierLocation, 1.0f); // 기본값

	// TODO: 면, 라인, fake 그리는 함수를 한데 묶어 처리
	// 면 가시 인덱스, 가시 인덱스를 모아서 GPU에 stream 업로드
	size_t  totalPolygonIndices = 0;
	int32_t polygonCount = static_cast<int32_t>(m_polygonDrawInfos.size());
	for (int32_t id : m_renderObjectIds) { // 컬링 통과한 객체 ID를 순회
		if (id < 0 || id >= polygonCount) continue; // ID 유효성 체크 (안전장치)
		totalPolygonIndices += m_polygonDrawInfos[id].indexCount; // 총 가시 인덱스 수 계산
	}
	if (totalPolygonIndices > 0) { // 가시 인덱스가 하나라도 있으면 진행
		m_polygonVisibleIndices.resize(totalPolygonIndices); // 가시 인덱스 임시 버퍼 크기 조정
		uint32_t* writePtr = m_polygonVisibleIndices.data(); // 가시 인덱스 버퍼 채우기
		
		// 컬링 통과한 객체 ID 순회하면서 가시 인덱스 버퍼 채우기
		for (int32_t id : m_renderObjectIds) {
			if (id < 0 || id >= polygonCount) continue; // ID 유효성 체크
			const DrawInfo& info = m_polygonDrawInfos[id]; // 객체별 인덱스 범위 정보
			if (info.indexCount == 0) continue; // 인덱스 없는 객체는 건너뛰기
			memcpy(writePtr, m_polygonIndices.data() + info.indexOffset, info.indexCount * sizeof(uint32_t)); // CPU 측 전체 인덱스에서 해당 객체의 인덱스 범위를 가시 인덱스 버퍼로 복사
			writePtr += info.indexCount; // 가시 인덱스 버퍼 쓰기 포인터 이동
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_polygonIBOVisible); // 가시 인덱스 버퍼 바인딩
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * totalPolygonIndices, nullptr, GL_STREAM_DRAW); // nullptr로 먼저 한 번 호출 = "이전 버퍼 내용 버려, 새로 할당해줘"
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * totalPolygonIndices, m_polygonVisibleIndices.data(), GL_STREAM_DRAW); // 가시 인덱스 데이터 GPU로 업로드 (stream draw: 매 프레임 바뀌는 데이터 -> 드라이버가 빠른 쓰기용 메모리에 배치)

		// 면 그리기
		glBindVertexArray(m_polygonVAO); // 면 VAO 바인딩
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_polygonIBOVisible); // 가시 인덱스 버퍼 바인딩
		glDrawElements(GL_TRIANGLES, (GLsizei)totalPolygonIndices, GL_UNSIGNED_INT, nullptr); // 인덱스 드로우콜, 가시 인덱스 수만큼 그리기, 실제 그리기 명령
		glBindVertexArray(0); // VAO 바인딩 해제
	}

	// 라인 가시 인덱스
	size_t  totalLineIndices = 0;
	int32_t lineCount = static_cast<int32_t>(m_lineDrawInfos.size());
	for (int32_t id : m_renderObjectIds) {
		if (id < 0 || id >= lineCount) continue;
		totalLineIndices += m_lineDrawInfos[id].indexCount;
	}
	if (totalLineIndices > 0) {
		m_lineVisibleIndices.resize(totalLineIndices);
		uint32_t* writePtr = m_lineVisibleIndices.data();

		for (int32_t id : m_renderObjectIds) {
			if (id < 0 || id >= lineCount) continue;
			const DrawInfo& info = m_lineDrawInfos[id];
			if (info.indexCount == 0) continue;
			memcpy(writePtr, m_lineIndices.data() + info.indexOffset, info.indexCount * sizeof(uint32_t));
			writePtr += info.indexCount;
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lineIBOVisible);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * totalLineIndices, nullptr, GL_STREAM_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * totalLineIndices, m_lineVisibleIndices.data(), GL_STREAM_DRAW);

		glUniform1f(m_colorMultiplierLocation, 0.6f); // 어둡게
		glBindVertexArray(m_polygonVAO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lineIBOVisible);
		glDrawElements(GL_LINES, (GLsizei)totalLineIndices, GL_UNSIGNED_INT, nullptr);
		glUniform1f(m_colorMultiplierLocation, 1.0f); // 복원
		glBindVertexArray(0);
	}

	// Fake Object (LOD 메쉬) 그리기
	int32_t quadTreenodeCount = static_cast<int32_t>(m_quadTree.m_nodes.size());
	if (uiState.isShowFakeObject) {
		size_t totalLodIndices = 0;
		for (int32_t nodeId : m_quadTree.m_visibleNodeFakeObjIds) {
			if (nodeId < 0 || nodeId >= quadTreenodeCount) continue;
			totalLodIndices += m_quadTree.m_nodes[nodeId].m_lodIndexCount;
		}
		if (m_quadTree.m_visibleNodeFakeObjIds.size() > 0) {
			m_fakeVisibleIndices.resize(totalLodIndices);
			uint32_t* writePtr = m_fakeVisibleIndices.data();
			for (int32_t nodeId : m_quadTree.m_visibleNodeFakeObjIds) {
				if (nodeId < 0 || nodeId >= quadTreenodeCount) continue;
				const QuadTreeNode& node = m_quadTree.m_nodes[nodeId];
				if (node.m_lodIndexCount == 0) continue;
				memcpy(writePtr, m_fakeIndices.data() + node.m_lodIndexOffset, node.m_lodIndexCount * sizeof(uint32_t));
				writePtr += node.m_lodIndexCount;
			}
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fakeIBOVisible);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * totalLodIndices, nullptr, GL_STREAM_DRAW);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * totalLodIndices, m_fakeVisibleIndices.data(), GL_STREAM_DRAW);
			
			glBindVertexArray(m_polygonVAO);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fakeIBOVisible);
			glDrawElements(GL_TRIANGLES, (GLsizei)totalLodIndices, GL_UNSIGNED_INT, nullptr);
			glBindVertexArray(0);
		}
	}

	if (uiState.isShowObjectMBR)   DrawObjectMBR();           // 객체 MBR 그리기
	if (uiState.isShowNodeMBR)     DrawQuadTreeNodeMBR();     // 노드 MBR 그리기
	if (uiState.isShowFrustumView) DrawCameraFrustum(camera); // 카메라 절두체 라인 그리기
	DrawDebugRect(hitPoint, 10.0f);
}

// 메쉬 빌드 진입점, 파일이 열리면 실행
void Renderer::BuildMesh()
{
	TCHAR buf[256];
	_stprintf_s(buf, _T("[Build Mesh] start\n"));
	OutputDebugString(buf);

	// 모든 멤버 클리어
	m_polygonVertices.clear(); // TODO: 클리어 부분 하나로 모아서 함수 만들기
	m_polygonIndices.clear();
	m_polygonDrawInfos.clear();
	m_polygonVisibleIndices.clear();
	m_lineIndices.clear();
	m_lineDrawInfos.clear();
	m_lineVisibleIndices.clear();

	switch (m_layer.m_shapeType) {
	case 1: break;
	case 3: BuildPolyLineMesh(); break; // 선 정점/인덱스 빌드, 선을 직사각형의 폴리곤으로 만들어 너비를 설정
	case 5: BuildPolygonMesh();  break; // 면 정점/인덱스 빌드
	}

	// 기본 색상 적용 (레벨 색상 OFF 상태)
	ApplyLevelColors(false);

	// GPU 업로드  VBO 하나에 모든 정점
	glBindVertexArray(m_polygonVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_polygonVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * m_polygonVertices.size(), m_polygonVertices.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_polygonIBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m_polygonIndices.size(), m_polygonIndices.data(), GL_STATIC_DRAW);
	glBindVertexArray(0);

	// 라인 IBO 업로드
	if (!m_lineIndices.empty()) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lineIBOVisible);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m_lineIndices.size(), m_lineIndices.data(), GL_STATIC_DRAW);
	}

	// GPU 업로드 완료, CPU 사본 해제 (더 이상 안 쓰임)
	m_polygonVertices.shrink_to_fit();
	m_polygonIndices.shrink_to_fit();
	m_lineIndices.shrink_to_fit();

	//TCHAR buf[256];
	_stprintf_s(buf, _T("[Build Mesh] finish\n"));
	OutputDebugString(buf);
}

// 선(너비를 부여해 직사각형으로) 빌드
void Renderer::BuildPolyLineMesh()
{
	m_polygonVertices.clear(); // TODO: 클리어 부분 하나로 모아서 함수 만들기
	m_polygonIndices.clear();
	m_polygonDrawInfos.clear();
	m_lineIndices.clear();
	m_lineDrawInfos.clear();

	int32_t polyLineCount = static_cast<int32_t>(m_layer.polyLineObjects.size());
	m_polygonDrawInfos.resize(polyLineCount);
	m_lineDrawInfos.resize(polyLineCount);
	if (polyLineCount == 0) return;

	for (int32_t dataId = 0; dataId < m_layer.polyLineObjects.size(); dataId++) {
		PolyObject& polyLine       = m_layer.polyLineObjects[dataId];
		uint32_t polygonVertStart  = (uint32_t)m_polygonVertices.size();
		uint32_t polygonIndexStart = (uint32_t)m_polygonIndices.size();
		uint32_t lineIndexStart    = (uint32_t)m_lineIndices.size();

		m_polygonVertices.reserve(m_polygonVertices.size() + (((polyLine.points.size() - 1) > 0) ? (polyLine.points.size() - 1) : 0) * 4);
		m_polygonIndices.reserve (m_polygonVertices.size() + (((polyLine.points.size() - 1) > 0) ? (polyLine.points.size() - 1) : 0) * 6);
		m_lineIndices.reserve    (m_polygonVertices.size() + (((polyLine.points.size() - 1) > 0) ? (polyLine.points.size() - 1) : 0) * 4);

		for (int32_t partNum = 0; partNum < polyLine.parts.size(); partNum++) {
			int32_t startPoint = polyLine.parts[partNum];
			int32_t endPoint = (partNum + 1 < polyLine.parts.size()) ? polyLine.parts[partNum + 1] : static_cast<int32_t>(polyLine.points.size());
			for (int32_t pointNum = startPoint; pointNum < endPoint - 1; pointNum++) {
				glm::dvec2 pointOrigin1 = polyLine.points[pointNum];     //+(polyLine.points[pointNum] - polyLine.mbrBox.GetCenter()) * 100.0;
				glm::dvec2 pointOrigin2 = polyLine.points[pointNum + 1]; //+(polyLine.points[pointNum + 1] - polyLine.mbrBox.GetCenter()) * 100.0;
				glm::dvec2 lineDir      = pointOrigin2 - pointOrigin1; // 이 방향 벡터에 수직인 방향으로 두 point에 m_layer.m_objSize 만큼 떨어진 거리에 점 생성
				glm::dvec2 widthDir     = glm::normalize(glm::dvec2(-lineDir.y, lineDir.x));   // 수직 벡터(너비 방향 벡터)
				glm::dvec2 widthValue   = widthDir * static_cast<double>(m_layer.m_objSize);
				
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

		uint32_t polygonVertCount  = (uint32_t)m_polygonVertices.size() - polygonVertStart;
		uint32_t polygonIndexCount = (uint32_t)m_polygonIndices.size()  - polygonIndexStart;
		uint32_t lineIndexCount    = (uint32_t)m_lineIndices.size()     - lineIndexStart;

		m_polygonDrawInfos[dataId] = { polygonIndexStart, polygonIndexCount, polygonVertStart, polygonVertCount };
		m_lineDrawInfos[dataId]    = { lineIndexStart, lineIndexCount, 0, 0 };
	}

	m_polygonVertices.shrink_to_fit();
	m_polygonIndices.shrink_to_fit();
	m_lineIndices.shrink_to_fit();
}

// 면(지붕 + 벽) 메쉬 빌드. 폴리곤 객체만 대상
void Renderer::BuildPolygonMesh()
{
	m_polygonVertices.clear(); // TODO: 클리어 부분 하나로 모아서 함수 만들기
	m_polygonIndices.clear();
	m_polygonDrawInfos.clear();
	m_lineIndices.clear();
	m_lineDrawInfos.clear();

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
	if (m_isCDTTriangluate) {
		std::iota(indexArray.begin(), indexArray.end(), 0);
		std::for_each(std::execution::par, indexArray.begin(), indexArray.end(), [&](int dataId) {
			results[dataId].indices = m_triangulate.TriangulatePolygonCDT(m_layer.polygonObjects[dataId], results[dataId].vertices);
		});
	}
	else { // Ear Clipping
		// TODO: 이어 클리핑
		for (int dataId = 0; dataId < polygonCount; dataId++) {
			results[dataId].indices = m_triangulate.TriangulateEarClipping(m_layer.polygonObjects[dataId], results[dataId].vertices);
		}
	}

	// shp에서 받아온 정점을 렌더링하기 위한 형태로 저장하기
	for (int32_t dataId = 0; dataId < polygonCount; dataId++) {
		PolyObject& polygon        = m_layer.polygonObjects[dataId];
		uint32_t polygonIndexStart = (uint32_t)m_polygonIndices.size();
		uint32_t polygonVertStart  = (uint32_t)m_polygonVertices.size();
		uint32_t lineIndexStart    = (uint32_t)m_lineIndices.size();

		if (results[dataId].vertices.size() < 3 || results[dataId].indices.empty()) {
			m_polygonDrawInfos[dataId] = { polygonIndexStart, 0, polygonVertStart, 0 };
			m_lineDrawInfos[dataId]    = { lineIndexStart, 0, 0, 0 };
			continue;
		}

		// 지붕 정점
		m_polygonVertices.reserve(m_polygonVertices.size() + results[dataId].vertices.size());
		m_polygonVertices.reserve(m_polygonVertices.size() + polygon.parts.size());
		uint32_t roofVertexBase = (uint32_t)m_polygonVertices.size();
		for (const auto& point : results[dataId].vertices)
			m_polygonVertices.push_back({(float)point.x, (float)point.y, (float)polygon.mbrBox.height, 190, 190, 220, 255 });

		for (uint32_t index : results[dataId].indices)
			m_polygonIndices.push_back(roofVertexBase + index);

		// 벽 정점 + 라인 인덱스 동시 빌드
		size_t partCount = polygon.parts.size();
		for (size_t partNum = 0; partNum < partCount; partNum++) {
			size_t startPoint = polygon.parts[partNum];
			size_t endPoint   = (partNum + 1 < partCount) ? polygon.parts[partNum + 1] : polygon.points.size();
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
				uint32_t wallTop0    = wallBottom0 + 2;
				uint32_t wallTop1    = wallBottom0 + 3;

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

		uint32_t polygonIndexCount = (uint32_t)m_polygonIndices.size()  - polygonIndexStart;
		uint32_t polygonVertCount  = (uint32_t)m_polygonVertices.size() - polygonVertStart;
		uint32_t lineIndexCount    = (uint32_t)m_lineIndices.size()     - lineIndexStart;

		m_polygonDrawInfos[dataId] = { polygonIndexStart, polygonIndexCount, polygonVertStart, polygonVertCount };
		m_lineDrawInfos[dataId]    = { lineIndexStart, lineIndexCount, 0, 0 };
	}
	//m_polygonVisibleIndices.reserve(m_polygonIndices.size());
	//m_lineVisibleIndices.reserve(m_lineIndices.size());

	m_polygonVertices.shrink_to_fit();
	m_polygonIndices.shrink_to_fit();
	m_lineIndices.shrink_to_fit();
}


// 쿼드트리 재빌드 (파일 새로 로딩 시 호출)
void Renderer::RebuildQuadTree()
{
	m_quadTree.BuildQuadTree(); // 트리 빌드
	ApplyLevelColors(false);    // 트리에 따라 색상 적용

	//auto fakeBuildStart = std::chrono::high_resolution_clock::now(); // 디버그용, 시간 측정

	BuildFakeMeshes(); // 트리 빌드 후 각 노드의 가상 객체 메쉬 생성

	// 디버그용, 시간 측정
	/*
	auto   fakeBuildEnd    = std::chrono::high_resolution_clock::now();
	double fakeBuildMicros = std::chrono::duration<double, std::micro>(fakeBuildEnd - fakeBuildStart).count();
	TCHAR buf[256];
	_stprintf_s(buf, _T("시간 간격: %.1f\n"), fakeBuildMicros);
	OutputDebugString(buf);
	*/

	//ReDraw(); // 다시 그리기
}


// 트리 빌드 후 각 노드의 LOD 메쉬 생성
void Renderer::BuildFakeMeshes()
{
	if (m_layer.m_shapeType != 5) return;

	m_fakeIndices.clear();
	BuildConvexHullNode(m_quadTree.m_nodes[0]); // 루트 노드부터 재귀적으로 가상 객체 생성 (자식 노드가 없으면 객체 중심점으로, 있으면 자식 노드의 LOD 점들로 볼록껍질 생성)
	if (m_fakeIndices.empty()) return; // 데이터 없으면 업로드 스킵
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fakeIBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m_fakeIndices.size(), m_fakeIndices.data(), GL_STATIC_DRAW);

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
void Renderer::BuildConvexHullNode(QuadTreeNode& node)
{
	std::vector<HullPoint> inputPoints;
	bool isLeaf = (node.m_childNodes[0] == -1 && node.m_childNodes[1] == -1 && node.m_childNodes[2] == -1 && node.m_childNodes[3] == -1); // 리프노드인지 여부

	if (isLeaf) { // 객체가 3개 이하이면 생략
		if (node.m_objectIds.size() < 3) {
			node.m_lodVertexOffset = -1;
			node.m_lodIndexOffset  = -1;
			node.m_lodVertexCount  = 0;
			node.m_lodIndexCount   = 0;
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
		for (int childIdx = 0; childIdx < 4; childIdx++) {
			int32_t childNodeId = node.m_childNodes[childIdx];
			if (childNodeId == -1) continue;
			BuildConvexHullNode(m_quadTree.m_nodes[childNodeId]);
		}

		// 자식 hull이 참조하는 m_lodIndices 슬롯에서 원래 정점 인덱스 수집
		for (int childIdx = 0; childIdx < 4; childIdx++) {
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
	size_t num = inputPoints.size();
	std::vector<HullPoint> hull(2 * num);
	int hullPointCount = 0; // hull에 쌓인 점 개수

	// cross product (HullPoint용)
	auto cross = [](const HullPoint& O, const HullPoint& A, const HullPoint& B) { return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);	};

	// 아래쪽 체인
	for (size_t i = 0; i < num; i++) {
		while (hullPointCount >= 2 && cross(hull[hullPointCount - 2], hull[hullPointCount - 1], inputPoints[i]) <= 0) hullPointCount--;
		hull[hullPointCount++] = inputPoints[i];
	}
	// 위쪽 체인
	for (int i = (int)num - 2, t = hullPointCount + 1; i >= 0; i--) {
		while (hullPointCount >= t && cross(hull[hullPointCount - 2], hull[hullPointCount - 1], inputPoints[i]) <= 0) hullPointCount--;
		hull[hullPointCount++] = inputPoints[i];
	}
	// 마지막 점은 시작점과 중복이므로 제거
	hullPointCount--;

	if (hullPointCount < 3) { // 일직선 등 → hull 실패
		node.m_lodVertexOffset = -1; node.m_lodIndexOffset = -1;
		node.m_lodVertexCount  = 0;  node.m_lodIndexCount  = 0;
		return;
	}

	fakeObjCount++;

	// m_lodVertexOffset = hull 정점 인덱스들이 시작되는 m_lodIndices 위치
	node.m_lodVertexOffset = (uint32_t)m_fakeIndices.size();
	node.m_lodVertexCount  = (uint32_t)hullPointCount;

	for (int i = 0; i < hullPointCount; i++)
		m_fakeIndices.push_back(hull[i].originalIndex); // 폴리곤 정점 인덱스

	// fan 삼각분할: 정점 0을 중심으로 (0, i, i+1) 삼각형 생성
	node.m_lodIndexOffset = (uint32_t)m_fakeIndices.size();
	uint32_t base = node.m_lodVertexOffset;
	for (int i = 1; i < hullPointCount - 1; i++) {
		m_fakeIndices.push_back(m_fakeIndices[base]);         // 실제 폴리곤 정점 인덱스
		m_fakeIndices.push_back(m_fakeIndices[base + i]);
		m_fakeIndices.push_back(m_fakeIndices[base + i + 1]);
	}
	node.m_lodIndexCount = (uint32_t)m_fakeIndices.size() - node.m_lodIndexOffset;
}

// 객체 MBR 박스 그리기 (보이는 객체만)
void Renderer::DrawObjectMBR()
{
	m_objMbrBoxVertices.clear();
	m_objMbrBoxVertices.reserve(m_renderObjectIds.size() * 24);

	int32_t polygonCount = static_cast<int32_t>(m_layer.polygonObjects.size());
	for (int32_t objectId : m_renderObjectIds) {
		if (objectId < 0) continue;

		// 폴리곤이면 polygonObjects, 폴리라인이면 polyLineObjects 참조
		const BoundingBox* mbrBox = nullptr;
		if (objectId < polygonCount && objectId < static_cast<int32_t>(m_layer.polygonObjects.size())) {
			mbrBox = &m_layer.polygonObjects[objectId].mbrBox;
		}
		else {
			int32_t lineIdx = objectId - polygonCount;
			if (lineIdx >= 0 && lineIdx < static_cast<int32_t>(m_layer.polyLineObjects.size()))
				mbrBox = &m_layer.polyLineObjects[lineIdx].mbrBox;
		}
		if (!mbrBox) continue;

		int32_t level = (objectId < static_cast<int32_t>(m_quadTree.m_objectLevels.size())) ? m_quadTree.m_objectLevels[objectId] : 0;
		unsigned char r, g, b;
		GetLevelColor(level, r, g, b);
		PushBoundingBoxLine(*mbrBox, m_objMbrBoxVertices, r, g, b, true);
	}

	if (m_objMbrBoxVertices.empty()) return;

	UploadAndDraw(m_mbrVAO, m_mbrVBO, m_objMbrBoxVertices, GL_LINES);

	m_objMbrBoxVertices.clear();
	m_objMbrBoxVertices.shrink_to_fit();
}

// 노드 MBR 박스 그리기 (컬링에서 통과한 노드만)
void Renderer::DrawQuadTreeNodeMBR()
{
	m_nodeMbrBoxVertices.clear();
	m_nodeMbrBoxVertices.reserve(m_quadTree.m_visibleNodeIds.size() * 8);

	for (int32_t nodeId : m_quadTree.m_visibleNodeIds) {
		if (nodeId < 0) continue;
		const QuadTreeNode& node = m_quadTree.m_nodes[nodeId];
		unsigned char r, g, b;
		GetLevelColor(node.m_level, r, g, b);
		PushBoundingBoxLine(node.m_boundingBox, m_nodeMbrBoxVertices, r, g, b, true);
	}

	if (m_nodeMbrBoxVertices.empty()) return;

	UploadAndDraw(m_mbrVAO, m_mbrVBO, m_nodeMbrBoxVertices, GL_LINES);

	m_nodeMbrBoxVertices.clear();
	m_nodeMbrBoxVertices.shrink_to_fit();
}

// 카메라 절두체 시각화 (지면과 교차하는 4개 변), NDC 모서리 4개를 unproject해서 z=0 평면과의 교차점을 구해 라인으로 표시
void Renderer::DrawCameraFrustum(CameraController& camera)
{
	if (!m_layer.m_isBuilding) return;

	if (!m_drawedFrustum)
	{
		m_frustumLineVertices.clear();
		m_frustumLineVertices.reserve(16); // 지면 투영 4개 라인(8개 정점) + AABB 4개 라인(8개 정점)

		// TODO: screenToWorld 함수 사용
		// 카메라 시야의 4개 NDC 모서리가 지면(z=0)과 만나는 실제 좌표 계산 (사다리꼴/사각형)
		glm::dmat4 inverseViewProjectionMatrix = glm::inverse(camera.GetMatrix());
		glm::vec2 ndcCorners[4] = { {-1.0f, -1.0f}, { 1.0f, -1.0f}, { 1.0f,  1.0f}, {-1.0f,  1.0f} };
		glm::dvec3 hitPoints[4];

		for (int dataId = 0; dataId < 4; ++dataId) {
			glm::vec4 nearPoint = inverseViewProjectionMatrix * glm::vec4(ndcCorners[dataId], -1.0f, 1.0f);
			glm::vec4 farPoint  = inverseViewProjectionMatrix * glm::vec4(ndcCorners[dataId],  1.0f, 1.0f);
			if (nearPoint.w != 0.0f) nearPoint /= nearPoint.w;
			if (farPoint.w  != 0.0f) farPoint  /= farPoint.w;

			glm::dvec3 rayOrigin = glm::dvec3(nearPoint);
			glm::dvec3 rayDir = glm::dvec3(farPoint) - glm::dvec3(nearPoint);

			if (std::abs(rayDir.z) > 1e-6) {
				double t = -rayOrigin.z / rayDir.z;
				if (t >= 0.0 && t <= 1.0) {
					hitPoints[dataId].x = static_cast<float>(rayOrigin.x + t * rayDir.x);
					hitPoints[dataId].y = static_cast<float>(rayOrigin.y + t * rayDir.y);
				}
				else {
					hitPoints[dataId] = glm::dvec3(farPoint);
				}
			}
			else {
				hitPoints[dataId] = glm::dvec3(farPoint);
			}
		}

		// 지면과 교차하는 실제 절두체 라인 조립
		float r1 = 1.0f, g1 = 0.0f, b1 = 1.0f;
		for (int dataId = 0; dataId < 4; ++dataId) {
			int next = (dataId + 1) % 4;
			m_frustumLineVertices.push_back({ static_cast<float>(hitPoints[dataId].x), static_cast<float>(hitPoints[dataId].y), 0.0f, 0, 0, 0, 255});
			m_frustumLineVertices.push_back({ static_cast<float>(hitPoints[next].x),   static_cast<float>(hitPoints[next].y),   0.0f, 0, 0, 0, 255 });

			m_frustumLineVertices.push_back({ static_cast<float>(hitPoints[dataId].x), static_cast<float>(hitPoints[dataId].y), 0.0f, 25, 25, 100, 255 });
			m_frustumLineVertices.push_back({ static_cast<float>(camera.transform.position.x), static_cast<float>(camera.transform.position.y), static_cast<float>(camera.transform.position.z), 25, 25, 100, 255 });
		}

		m_drawedFrustum = true;
	}

	if (m_frustumLineVertices.empty()) return;

	// GPU 업로드 및 렌더링
	UploadAndDraw(m_mbrVAO, m_mbrVBO, m_frustumLineVertices, GL_LINES);
}

// MBR 박스 출력에 사용
void Renderer::PushBoundingBoxLine(const BoundingBox& boundingBox, std::vector<Vertex>& vertices, unsigned char r, unsigned char g, unsigned char b, bool hasHeight)
{
	unsigned char alpha = 255;
	Vertex v0{ (float)boundingBox.minX, (float)boundingBox.minY, 0.0f, r, g, b, alpha };
	Vertex v1{ (float)boundingBox.maxX, (float)boundingBox.minY, 0.0f, r, g, b, alpha };
	Vertex v2{ (float)boundingBox.maxX, (float)boundingBox.maxY, 0.0f, r, g, b, alpha };
	Vertex v3{ (float)boundingBox.minX, (float)boundingBox.maxY, 0.0f, r, g, b, alpha };

	vertices.push_back(v0); vertices.push_back(v1);
	vertices.push_back(v1); vertices.push_back(v2);
	vertices.push_back(v2); vertices.push_back(v3);
	vertices.push_back(v3); vertices.push_back(v0);

	if (hasHeight) {
		Vertex v5{ (float)boundingBox.minX, (float)boundingBox.minY, (float)boundingBox.height, r, g, b, alpha };
		Vertex v6{ (float)boundingBox.maxX, (float)boundingBox.minY, (float)boundingBox.height, r, g, b, alpha };
		Vertex v7{ (float)boundingBox.maxX, (float)boundingBox.maxY, (float)boundingBox.height, r, g, b, alpha };
		Vertex v8{ (float)boundingBox.minX, (float)boundingBox.maxY, (float)boundingBox.height, r, g, b, alpha };

		vertices.push_back(v0); vertices.push_back(v5);
		vertices.push_back(v1); vertices.push_back(v6);
		vertices.push_back(v2); vertices.push_back(v7);
		vertices.push_back(v3); vertices.push_back(v8);

		vertices.push_back(v5); vertices.push_back(v6);
		vertices.push_back(v6); vertices.push_back(v7);
		vertices.push_back(v7); vertices.push_back(v8);
		vertices.push_back(v8); vertices.push_back(v5);
	}
}

// 라인/면의 모든 vertex 색상을 다시 칠하고 GPU에 부분 업데이트
// useLevelColor=true: 객체별 트리 레벨 색상, useLevelColor=false: 라인=회색, 면=연회색
// 면은 z값으로 위/아래 명도 자동 보정 (벽 그라디언트)
// glBufferSubData로 vertex 좌표는 유지, 색상만 GPU 갱신
void Renderer::ApplyLevelColors(bool useLevelColor)
{
	// 면 색상
	int32_t polygonCount = static_cast<int32_t>(m_polygonDrawInfos.size());
	for (int32_t dataId = 0; dataId < polygonCount; dataId++) {
		const DrawInfo& info = m_polygonDrawInfos[dataId];
		if (info.indexCount == 0) continue;

		unsigned char r, g, b;
		if (useLevelColor) GetLevelColor(m_quadTree.m_objectLevels[dataId], r, g, b);
		else               { r = 190; g = 190; b = 220; }

		uint32_t end = info.indexOffset + info.indexCount;
		for (uint32_t ii = info.indexOffset; ii < end; ii++) {
			uint32_t vertexIndex = m_polygonIndices[ii];
			if (vertexIndex >= m_polygonVertices.size()) continue;
			Vertex& vertex = m_polygonVertices[vertexIndex];

			// z = 0 → 벽 하단 (어둡게), z = 1 → 지붕/벽 상단 (밝게)
			// GPU가 두 점 사이를 자동 보간 → 벽에 자연스러운 그라디언트 생김
			int32_t shade = (vertex.z < 0.5f) ? 2 : 1;
			vertex.r = r / shade;
			vertex.g = g / shade;
			vertex.b = b / shade;
			vertex.a = 255;
		}
	}

	// GPU 재업로드 (BufferSubData)
	if (!m_polygonVertices.empty()) {
		glBindBuffer(GL_ARRAY_BUFFER, m_polygonVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * m_polygonVertices.size(), m_polygonVertices.data());
	}
}

// 트리 레벨에 따른 색상 설정
void Renderer::GetLevelColor(int32_t level, unsigned char& r, unsigned char& g, unsigned char& b)
{
	int32_t levelToColor = level % (sizeof(palette) / sizeof(palette[0]));
	r = palette[levelToColor].red; 
	g = palette[levelToColor].green;
	b = palette[levelToColor].blue;
}

void Renderer::SetSelectedObject(int32_t objectId, UIState& uiState)
{
	// 이전 선택 객체 색상 복원
	if (m_selectedObjectId >= 0)
		RestoreObjectColor(m_selectedObjectId, uiState);

	m_selectedObjectId = objectId;

	if (objectId < 0) return;

	// 새 선택 객체 색상 강조
	HighlightObjectColor(objectId);
	//ReDraw();
}

// 피킹 객체 색상 강조
void Renderer::HighlightObjectColor(int32_t objectId)
{
	const DrawInfo& info = m_polygonDrawInfos[objectId];
	if (info.vertexCount == 0) return;

	// CPU 버퍼에서 해당 vertex 범위만 색상 변경
	for (uint32_t i = info.vertexOffset; i < info.vertexOffset + info.vertexCount; i++) {
		Vertex& v = m_polygonVertices[i];
		v.r = 20; 
		v.g = 230; 
		v.b = 50;
	}

	// GPU의 해당 위치만 덮어쓰기
	glBindBuffer(GL_ARRAY_BUFFER, m_polygonVBO);
	glBufferSubData(GL_ARRAY_BUFFER, info.vertexOffset * sizeof(Vertex), info.vertexCount * sizeof(Vertex), m_polygonVertices.data() + info.vertexOffset);
}

// 강조했던 색상 복구
void Renderer::RestoreObjectColor(int32_t objectId, UIState& uiState)
{
	const DrawInfo& info = m_polygonDrawInfos[objectId];
	if (info.vertexCount == 0) return;

	// CPU 버퍼에서 원래 색상으로 복원
	// ApplyLevelColors와 동일한 로직
	bool useLevelColor = uiState.isShowLevelColor; // 현재 토글 상태
	unsigned char r, g, b;
	if (useLevelColor && objectId < static_cast<int32_t>(m_quadTree.m_objectLevels.size()))
		GetLevelColor(m_quadTree.m_objectLevels[objectId], r, g, b);
	else
		r = 190; g = 190; b = 220;

	for (uint32_t i = info.vertexOffset; i < info.vertexOffset + info.vertexCount; i++) {
		Vertex& v = m_polygonVertices[i];
		int32_t shade = (v.z < 0.5f) ? 2 : 1;
		v.r = r / shade; 
		v.g = g / shade; 
		v.b = b / shade;
	}

	glBindBuffer(GL_ARRAY_BUFFER, m_polygonVBO);
	glBufferSubData(GL_ARRAY_BUFFER, info.vertexOffset * sizeof(Vertex), info.vertexCount * sizeof(Vertex), m_polygonVertices.data() + info.vertexOffset);
}

void Renderer::DrawDebugRect(const glm::dvec3& center, float size)
{
	if (!m_layer.m_isBuilding) return;

	float halfSize = size * 0.5f;

	std::vector<Vertex> vertices(4);

	vertices[0] = { static_cast<float>(center.x - halfSize), static_cast<float>(center.y - halfSize), static_cast<float>(center.z) + 1.0f, 255,0,0,255 };
	vertices[1] = { static_cast<float>(center.x + halfSize), static_cast<float>(center.y - halfSize), static_cast<float>(center.z) + 1.0f, 255,0,0,255 };
	vertices[2] = { static_cast<float>(center.x + halfSize), static_cast<float>(center.y + halfSize), static_cast<float>(center.z) + 1.0f, 255,0,0,255 };
	vertices[3] = { static_cast<float>(center.x - halfSize), static_cast<float>(center.y + halfSize), static_cast<float>(center.z) + 1.0f, 255,0,0,255 };

	UploadAndDraw(m_mbrVAO, m_mbrVBO, vertices, GL_LINE_LOOP);
}