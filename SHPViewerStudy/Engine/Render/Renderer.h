#pragma once

#include <string>
#include <GLES3/gl3.h>
#include <EGL/egl.h>

// Engine
#include "CameraController.h"
#include "UI/UIState.h"
#include "MeshManager.h"

class Layer;
class QuadTree;
class QuadTreeNode;

class Renderer
{
public:
	QuadTree&   m_quadTree; // 쿼드트리 클래스
	Layer&      m_layer;    // 레이어   클래스
	std::unique_ptr<MeshManager> m_mesh; // 메쉬관리 클래스

	std::vector<int32_t> m_renderObjectIds; // 렌더링 할 객체(컬링 통과 객체) ID 목록

	int32_t m_currentRenderCount     = 0; // UI에 표시할 렌더링 객체 수
	int32_t m_currentRenderFakeCount = 0; // UI에 표시할 가상 객체 수

public:
	Renderer(HWND hWnd, Layer& layer, QuadTree& quadtree) : m_layer(layer), m_quadTree(quadtree) { Initialize(hWnd); }
	~Renderer() {}

	bool Initialize(HWND hWnd); // 전체 초기화 진입점
	
	// 렌더링
	void Render(CameraController& camera, UIState& uiState, UISize& uiSize, bool isSelected); // 메인 렌더 함수
	void DrawObjectMBR();       // 객체 MBR 박스 그리기
	void DrawQuadTreeNodeMBR(); // 노드 MBR 박스 그리기
	void UploadAndDraw(GLuint& vao, GLuint& vbo, std::vector<Vertex>& vertices, int drawType); // 업로드 & 그리기 (객체 mbr, 노드 mbr)

	void PushBoundingBoxLine(const BoundingBox& boundingBox, std::vector<Vertex>& vertices, UCharColor color, bool hasHeight); // mbr 정점 버퍼 삽입
	void GetLevelColor(int32_t level, UCharColor& color); // 객체 레벨에 따른 색상 계산

	void HighlightObjectColor(int32_t objectId); // 피킹 객체 색상 강조
	void RestoreObjectColor(int32_t objectId, UIState& uiState, bool isSelectedLayer); // 강조했던 색상 복구
};