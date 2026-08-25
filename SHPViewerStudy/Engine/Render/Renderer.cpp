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
void Renderer::Render(CameraManager& camera, UIState& uiState, UISize& uiSize, bool isSelected, bool hasPickingData, int32_t pickingDataId, GLint colorMultiplierLocation, DebugVertexBuffer& mbrBuffer)
{
	// 쿼드트리에서 가시 객체 검색 (컬링, LOD)
	if (!uiState.isShowFrustumView) {
		m_renderObjectIds.clear();
		m_quadTree.m_visibleNodeIds.clear();
		m_quadTree.m_visibleNodeFakeObjIds.clear();

		double halfFovRad = glm::radians(camera.fov * 0.5);
		double lodFactor  = std::max(uiSize.clientHeight, 1) / (2.0 * std::tan(halfFovRad));
		m_quadTree.SearchRenderingData(m_renderObjectIds, 0, camera, camera.transform.position, lodFactor);

		m_currentRenderCount     = static_cast<int32_t>(m_renderObjectIds.size());
		m_currentRenderFakeCount = static_cast<int32_t>(m_quadTree.m_visibleNodeFakeObjIds.size());
	}

	// 면 가시 인덱스, 가시 인덱스를 모아서 GPU에 stream 업로드
	int32_t polygonCount = static_cast<int32_t>(m_mesh->m_polygonDrawInfos.size());
	int32_t lineCount    = static_cast<int32_t>(m_mesh->m_lineDrawInfos.size());

	auto skipPicked = [&](int32_t id) { return id < 0 || (uiState.isEditObjectMode && hasPickingData && id == pickingDataId); };

	// 면
	DrawVisibleIndexed(m_renderObjectIds, m_mesh->m_polygonIndices, m_mesh->m_visibleIndices, m_mesh->m_polygonVAO, m_mesh->m_visibleIBO, GL_TRIANGLES,
		[&](int32_t id) { return skipPicked(id) || id >= polygonCount; },
		[&](int32_t id) { return m_mesh->m_polygonDrawInfos[id].indexOffset; },
		[&](int32_t id) { return m_mesh->m_polygonDrawInfos[id].indexCount; });

	// 라인 (어둡게 표시)
	glUniform1f(colorMultiplierLocation, 0.6f);
	DrawVisibleIndexed(m_renderObjectIds, m_mesh->m_lineIndices, m_mesh->m_visibleIndices, m_mesh->m_polygonVAO, m_mesh->m_visibleIBO, GL_LINES,
		[&](int32_t id) { return skipPicked(id) || id >= lineCount; },
		[&](int32_t id) { return m_mesh->m_lineDrawInfos[id].indexOffset; },
		[&](int32_t id) { return m_mesh->m_lineDrawInfos[id].indexCount; });
	glUniform1f(colorMultiplierLocation, 1.0f);

	// 가상 객체 (Fake/LOD)
	if (uiState.isShowFakeObject) {
		int32_t quadTreeNodeCount = static_cast<int32_t>(m_quadTree.m_nodes.size());
		DrawVisibleIndexed(m_quadTree.m_visibleNodeFakeObjIds, m_mesh->m_fakeIndices, m_mesh->m_visibleIndices, m_mesh->m_polygonVAO, m_mesh->m_visibleIBO, GL_TRIANGLES,
			[&](int32_t nodeId) { return nodeId < 0 || nodeId >= quadTreeNodeCount; },
			[&](int32_t nodeId) { return m_quadTree.m_nodes[nodeId].m_lodIndexOffset; },
			[&](int32_t nodeId) { return m_quadTree.m_nodes[nodeId].m_lodIndexCount; });
	}
	
	if (isSelected) {
		if (uiState.isShowObjectMBR) DrawObjectMBR(mbrBuffer);       // 객체 MBR 그리기
		if (uiState.isShowNodeMBR)   DrawQuadTreeNodeMBR(mbrBuffer); // 노드 MBR 그리기
	}
}

// 객체 MBR 박스 그리기 (보이는 객체만)
void Renderer::DrawObjectMBR(DebugVertexBuffer& mbrBuffer)
{
	m_mesh->m_mbrBoxVertices.clear();
	m_mesh->m_mbrBoxVertices.reserve(m_renderObjectIds.size() * 24);

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
		SetColorFromLevel(level, color);
		PushBoundingBoxLine(*mbrBox, m_mesh->m_mbrBoxVertices, color, true);
	}

	if (m_mesh->m_mbrBoxVertices.empty()) return;

	mbrBuffer.Upload(m_mesh->m_mbrBoxVertices, GL_DYNAMIC_DRAW);
	mbrBuffer.Draw(GL_LINES);

	m_mesh->m_mbrBoxVertices.clear();
}

// 노드 MBR 박스 그리기 (컬링에서 통과한 노드만)
void Renderer::DrawQuadTreeNodeMBR(DebugVertexBuffer& mbrBuffer)
{
	m_mesh->m_mbrBoxVertices.clear();
	m_mesh->m_mbrBoxVertices.reserve(m_quadTree.m_visibleNodeIds.size() * 8);

	for (int32_t nodeId : m_quadTree.m_visibleNodeIds) {
		if (nodeId < 0) continue;
		const QuadTreeNode& node = m_quadTree.m_nodes[nodeId];
		UCharColor color;
		SetColorFromLevel(node.m_level, color);
		PushBoundingBoxLine(node.m_boundingBox/*.GetLooseBox(m_quadTree.m_looseBoxRate)*/, m_mesh->m_mbrBoxVertices, color, true);
	}

	if (m_mesh->m_mbrBoxVertices.empty()) return;

	mbrBuffer.Upload(m_mesh->m_mbrBoxVertices, GL_DYNAMIC_DRAW);
	mbrBuffer.Draw(GL_LINES);

	m_mesh->m_mbrBoxVertices.clear();
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

template<typename TIdContainer, typename SkipFn, typename OffsetFn, typename CountFn>
void Renderer::DrawVisibleIndexed(const TIdContainer& ids, const std::vector<uint32_t>& sourceIndices, std::vector<uint32_t>& scratchBuffer, GLuint vao, GLuint ibo, GLenum mode, SkipFn&& skip, OffsetFn&& offsetOf, CountFn&& countOf)
{
	size_t total = 0;
	for (auto id : ids) {
		if (skip(id)) continue;
		total += countOf(id);
	}
	if (total == 0) return;

	scratchBuffer.resize(total);
	uint32_t* writePtr = scratchBuffer.data();
	for (auto id : ids) {
		if (skip(id)) continue;
		uint32_t count = countOf(id);
		if (count == 0) continue;
		memcpy(writePtr, sourceIndices.data() + offsetOf(id), count * sizeof(uint32_t));
		writePtr += count;
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * total, nullptr, GL_STREAM_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * total, scratchBuffer.data(), GL_STREAM_DRAW);

	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glDrawElements(mode, (GLsizei)total, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}