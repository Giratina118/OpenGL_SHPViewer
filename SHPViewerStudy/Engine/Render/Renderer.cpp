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
	// 깊이 테스트 활성화 (3D 건물, 면/라인 z-fighting 해결에 필요)
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_POLYGON_OFFSET_FILL); // 면 위에 라인을 그릴 때 z-fighting 방지, 면을 살짝 뒤로 밀어서 라인이 면 위에 보이게 함
	glPolygonOffset(1.0f, 1.0f);

	m_mesh = std::make_unique<MeshManager>(m_layer, m_quadTree); // 메쉬 매니저 생성
	return m_mesh->InitRenderMesh(); // 메쉬 매니저 초기화
}


// 메인 렌더 함수
void Renderer::Render(CameraController& camera, UIState& uiState, UISize& uiSize, bool isSelected)
{
	// 쿼드트리에서 가시 객체 검색 (컬링, LOD)
	if (!uiState.isShowFrustumView) {
		m_renderObjectIds.clear();
		m_quadTree.m_visibleNodeIds.clear();
		m_quadTree.m_visibleNodeFakeObjIds.clear();

		double halfFovRad = glm::radians(camera.fov * 0.5);
		double lodFactor = std::max(uiSize.clientHeight, 1) / (2.0 * std::tan(halfFovRad));
		m_quadTree.SearchRenderingData(m_renderObjectIds, 0, camera, camera.transform.position, lodFactor);

		m_currentRenderCount = static_cast<int32_t>(m_renderObjectIds.size());
		m_currentRenderFakeCount = static_cast<int32_t>(m_quadTree.m_visibleNodeFakeObjIds.size());
	}

	
	// TODO: 면, 라인, fake 그리는 함수를 한데 묶어 처리
	// 면 가시 인덱스, 가시 인덱스를 모아서 GPU에 stream 업로드
	size_t  totalPolygonIndices = 0;
	int32_t polygonCount = static_cast<int32_t>(m_mesh->m_polygonDrawInfos.size());
	for (int32_t id : m_renderObjectIds) { // 컬링 통과한 객체 ID를 순회
		if (id < 0 || id >= polygonCount) continue; // ID 유효성 체크 (안전장치)
		totalPolygonIndices += m_mesh->m_polygonDrawInfos[id].indexCount; // 총 가시 인덱스 수 계산
	}
	if (totalPolygonIndices > 0) { // 가시 인덱스가 하나라도 있으면 진행
		m_mesh->m_polygonVisibleIndices.resize(totalPolygonIndices); // 가시 인덱스 임시 버퍼 크기 조정
		uint32_t* writePtr = m_mesh->m_polygonVisibleIndices.data(); // 가시 인덱스 버퍼 채우기

		// 컬링 통과한 객체 ID 순회하면서 가시 인덱스 버퍼 채우기
		for (int32_t id : m_renderObjectIds) {
			if (id < 0 || id >= polygonCount) continue; // ID 유효성 체크
			const DrawInfo& info = m_mesh->m_polygonDrawInfos[id]; // 객체별 인덱스 범위 정보
			if (info.indexCount == 0) continue; // 인덱스 없는 객체는 건너뛰기
			memcpy(writePtr, m_mesh->m_polygonIndices.data() + info.indexOffset, info.indexCount * sizeof(uint32_t)); // CPU 측 전체 인덱스에서 해당 객체의 인덱스 범위를 가시 인덱스 버퍼로 복사
			writePtr += info.indexCount; // 가시 인덱스 버퍼 쓰기 포인터 이동
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mesh->m_polygonIBOVisible); // 가시 인덱스 버퍼 바인딩
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * totalPolygonIndices, nullptr, GL_STREAM_DRAW); // nullptr로 먼저 한 번 호출 = "이전 버퍼 내용 버려, 새로 할당해줘"
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * totalPolygonIndices, m_mesh->m_polygonVisibleIndices.data(), GL_STREAM_DRAW); // 가시 인덱스 데이터 GPU로 업로드 (stream draw: 매 프레임 바뀌는 데이터 -> 드라이버가 빠른 쓰기용 메모리에 배치)

		// 면 그리기
		glBindVertexArray(m_mesh->m_polygonVAO); // 면 VAO 바인딩
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mesh->m_polygonIBOVisible); // 가시 인덱스 버퍼 바인딩
		glDrawElements(GL_TRIANGLES, (GLsizei)totalPolygonIndices, GL_UNSIGNED_INT, nullptr); // 인덱스 드로우콜, 가시 인덱스 수만큼 그리기, 실제 그리기 명령
		glBindVertexArray(0); // VAO 바인딩 해제
	}

	// 라인 가시 인덱스
	size_t  totalLineIndices = 0;
	int32_t lineCount = static_cast<int32_t>(m_mesh->m_lineDrawInfos.size());
	for (int32_t id : m_renderObjectIds) {
		if (id < 0 || id >= lineCount) continue;
		totalLineIndices += m_mesh->m_lineDrawInfos[id].indexCount;
	}
	if (totalLineIndices > 0) {
		m_mesh->m_lineVisibleIndices.resize(totalLineIndices);
		uint32_t* writePtr = m_mesh->m_lineVisibleIndices.data();

		for (int32_t id : m_renderObjectIds) {
			if (id < 0 || id >= lineCount) continue;
			const DrawInfo& info = m_mesh->m_lineDrawInfos[id];
			if (info.indexCount == 0) continue;
			memcpy(writePtr, m_mesh->m_lineIndices.data() + info.indexOffset, info.indexCount * sizeof(uint32_t));
			writePtr += info.indexCount;
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mesh->m_lineIBOVisible);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * totalLineIndices, nullptr, GL_STREAM_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * totalLineIndices, m_mesh->m_lineVisibleIndices.data(), GL_STREAM_DRAW);

		glUniform1f(m_mesh->m_colorMultiplierLocation, 0.6f); // 어둡게
		glBindVertexArray(m_mesh->m_polygonVAO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mesh->m_lineIBOVisible);
		glDrawElements(GL_LINES, (GLsizei)totalLineIndices, GL_UNSIGNED_INT, nullptr);
		glUniform1f(m_mesh->m_colorMultiplierLocation, 1.0f); // 복원
		glBindVertexArray(0);
	}

	/*
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
	*/


	if (isSelected) {
		if (uiState.isShowObjectMBR)   DrawObjectMBR();           // 객체 MBR 그리기
		if (uiState.isShowNodeMBR)     DrawQuadTreeNodeMBR();     // 노드 MBR 그리기
	}
}

// 객체 MBR 박스 그리기 (보이는 객체만)
void Renderer::DrawObjectMBR()
{
	m_mesh->m_objMbrBoxVertices.clear();
	m_mesh->m_objMbrBoxVertices.reserve(m_renderObjectIds.size() * 24);

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
		UCharColor color;
		GetLevelColor(level, color);
		PushBoundingBoxLine(*mbrBox, m_mesh->m_objMbrBoxVertices, color, true);
	}

	if (m_mesh->m_objMbrBoxVertices.empty()) return;

	UploadAndDraw(m_mesh->m_mbrVAO, m_mesh->m_mbrVBO, m_mesh->m_objMbrBoxVertices, GL_LINES);

	m_mesh->m_objMbrBoxVertices.clear();
	m_mesh->m_objMbrBoxVertices.shrink_to_fit();
}

// 노드 MBR 박스 그리기 (컬링에서 통과한 노드만)
void Renderer::DrawQuadTreeNodeMBR()
{
	m_mesh->m_nodeMbrBoxVertices.clear();
	m_mesh->m_nodeMbrBoxVertices.reserve(m_quadTree.m_visibleNodeIds.size() * 8);

	for (int32_t nodeId : m_quadTree.m_visibleNodeIds) {
		if (nodeId < 0) continue;
		const QuadTreeNode& node = m_quadTree.m_nodes[nodeId];
		UCharColor color;
		GetLevelColor(node.m_level, color);
		PushBoundingBoxLine(node.m_boundingBox/*.GetLooseBox(m_quadTree.m_looseBoxRate)*/, m_mesh->m_nodeMbrBoxVertices, color, true);
	}

	if (m_mesh->m_nodeMbrBoxVertices.empty()) return;

	UploadAndDraw(m_mesh->m_mbrVAO, m_mesh->m_mbrVBO, m_mesh->m_nodeMbrBoxVertices, GL_LINES);

	m_mesh->m_nodeMbrBoxVertices.clear();
	m_mesh->m_nodeMbrBoxVertices.shrink_to_fit();
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

// MBR 박스 출력에 사용
void Renderer::PushBoundingBoxLine(const BoundingBox& boundingBox, std::vector<Vertex>& vertices, UCharColor color, bool hasHeight)
{
	color.alpha = 255;
	Vertex v0{ (float)boundingBox.minX, (float)boundingBox.minY, 0.0f, color };
	Vertex v1{ (float)boundingBox.maxX, (float)boundingBox.minY, 0.0f, color };
	Vertex v2{ (float)boundingBox.maxX, (float)boundingBox.maxY, 0.0f, color };
	Vertex v3{ (float)boundingBox.minX, (float)boundingBox.maxY, 0.0f, color };

	vertices.push_back(v0); vertices.push_back(v1);
	vertices.push_back(v1); vertices.push_back(v2);
	vertices.push_back(v2); vertices.push_back(v3);
	vertices.push_back(v3); vertices.push_back(v0);

	if (hasHeight) {
		Vertex v5{ (float)boundingBox.minX, (float)boundingBox.minY, (float)boundingBox.height, color };
		Vertex v6{ (float)boundingBox.maxX, (float)boundingBox.minY, (float)boundingBox.height, color };
		Vertex v7{ (float)boundingBox.maxX, (float)boundingBox.maxY, (float)boundingBox.height, color };
		Vertex v8{ (float)boundingBox.minX, (float)boundingBox.maxY, (float)boundingBox.height, color };

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

// 트리 레벨에 따른 색상 설정
void Renderer::GetLevelColor(int32_t level, UCharColor& color)
{
	int32_t levelToColor = level % (sizeof(palette) / sizeof(palette[0]));
	color = palette[levelToColor];
}

// 피킹 객체 색상 강조
void Renderer::HighlightObjectColor(int32_t objectId)
{
	const DrawInfo& info = m_mesh->m_polygonDrawInfos[objectId];
	if (info.vertexCount == 0) return;

	// CPU 버퍼에서 해당 vertex 범위만 색상 변경
	for (uint32_t i = info.vertexOffset; i < info.vertexOffset + info.vertexCount; i++)
		m_mesh->m_polygonVertices[i].color = { 20, 230, 50, 255 };

	// GPU의 해당 위치만 덮어쓰기
	glBindBuffer(GL_ARRAY_BUFFER, m_mesh->m_polygonVBO);
	glBufferSubData(GL_ARRAY_BUFFER, info.vertexOffset * sizeof(Vertex), info.vertexCount * sizeof(Vertex), m_mesh->m_polygonVertices.data() + info.vertexOffset);
}

// 강조했던 색상 복구
void Renderer::RestoreObjectColor(int32_t objectId, UIState& uiState, bool isSelectedLayer)
{
	const DrawInfo& info = m_mesh->m_polygonDrawInfos[objectId];
	if (info.vertexCount == 0) return;

	// 원래 색상으로 복원, ApplyLevelColors와 동일한 로직
	UCharColor color;
	if (uiState.isShowLevelColor && isSelectedLayer) GetLevelColor(m_quadTree.m_objectLevels[objectId], color);
	else color = m_layer.m_baseColor;

	for (uint32_t i = info.vertexOffset; i < info.vertexOffset + info.vertexCount; i++) {
		Vertex& vertex = m_mesh->m_polygonVertices[i];
		int32_t shade = (vertex.z < 0.5f && m_layer.m_isBuilding) ? 3 : 2;
		vertex.color = color / shade * 2;
	}

	glBindBuffer(GL_ARRAY_BUFFER, m_mesh->m_polygonVBO);
	glBufferSubData(GL_ARRAY_BUFFER, info.vertexOffset * sizeof(Vertex), info.vertexCount * sizeof(Vertex), m_mesh->m_polygonVertices.data() + info.vertexOffset);
}