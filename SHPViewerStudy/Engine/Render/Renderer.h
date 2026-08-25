#pragma once

#include <string>
#include <GLES3/gl3.h>
#include <EGL/egl.h>

// Engine
#include "CameraManager.h"
#include "UI/UIState.h"
#include "MeshManager.h"
#include "VertexBuffer.h"

class Layer;
class QuadTree;
class QuadTreeNode;

class Renderer
{
public:
	QuadTree& m_quadTree; // 쿼드트리 클래스
	Layer&    m_layer;    // 레이어   클래스
	std::unique_ptr<MeshManager> m_mesh;    // 메쉬관리 클래스
	std::vector<int32_t> m_renderObjectIds; // 렌더링 할 객체(컬링 통과 객체) ID 목록

	int32_t m_currentRenderCount     = 0;   // UI에 표시할 렌더링 객체 수
	int32_t m_currentRenderFakeCount = 0;   // UI에 표시할 가상 객체 수

public:
	Renderer(HWND hWnd, Layer& layer, QuadTree& quadtree) : m_layer(layer), m_quadTree(quadtree) { Initialize(hWnd); }
	~Renderer() {}

	bool Initialize(HWND hWnd); // 전체 초기화 진입점
	
	// 렌더링
	void Render(CameraManager& camera, UIState& uiState, UISize& uiSize, bool isSelected, bool hasPickingData, int32_t pickingDataId, GLint colorMultiplierLocation, DebugVertexBuffer& mbrBuffer); // 메인 렌더 함수
	void DrawObjectMBR(DebugVertexBuffer& mbrBuffer);       // 객체 MBR 박스 그리기
	void DrawQuadTreeNodeMBR(DebugVertexBuffer& mbrBuffer); // 노드 MBR 박스 그리기

	void PushBoundingBoxLine(const BoundingBox& boundingBox, std::vector<Vertex>& vertices, UCharColor color, bool hasHeight); // mbr 정점 버퍼 삽입

	template <typename TIdContainer, typename SkipFn, typename OffsetFn, typename CountFn>
	void DrawVisibleIndexed(const TIdContainer& ids, const std::vector<uint32_t>& sourceIndices, std::vector<uint32_t>& scratchBuffer, GLuint vao, GLuint ibo, GLenum mode, SkipFn&& skip, OffsetFn&& offsetOf, CountFn&& countOf);
};