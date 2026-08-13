#pragma once
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include "VertexBuffer.h"
#include "UI/UIState.h"
#include "Layer.h"

class EditObject {
private:
	bool     m_isEdittingObject = false;   // 객체 편집 모드 여부
	UIState* m_uiState          = nullptr; // UI 버튼 토글 상태
	int32_t* m_pickingLayerId   = nullptr; // 피킹한 객체의 레이어 아이디
	int32_t* m_pickingObjectId  = nullptr; // 피킹한 객체의 아이디, 이전 피킹과 현재 피킹 객체가 같은지 아닌지 판별 시 사용하기 위해 저장
	int32_t* m_pickingNodeId    = nullptr; // 피킹한 객체의 노드 아이디
	const std::unordered_map<int32_t, int32_t>* m_layerIdToIndex = nullptr;

	OverlayMesh m_editOverlay;   // 편집 중인 오브젝트
	OverlayMesh m_createOverlay; // 생성 중인 오브젝트

	Transform   m_editTransform; // 편집 중인 객체의 변환 정보
	std::vector<Vertex>   m_transformVertices; // 면 vertex (BuildMapMesh에서 만든 라인 vertex와 별개)
	std::vector<Vertex>   m_transformLineVertices;
	std::vector<uint32_t> m_transformPolygonIndices;
	std::vector<uint32_t> m_transformLineIndices;
	DrawInfo m_transformPolygonInfo; // 편집하는 객체의 면   DrawInfo
	DrawInfo m_transformLineInfo;    // 편집하는 객체의 라인 DrawInfo
	glm::dvec3 m_editCenter;
	std::vector<Vertex> m_editOriginalVertices;
	std::vector<Vertex> m_editOriginalLineVertices;

	// 객체 생성 시 사용
	std::vector<Vertex>   m_createVertices;
	std::vector<Vertex>   m_createLineVertices;
	std::vector<uint32_t> m_createPolygonIndices;
	std::vector<uint32_t> m_createLineIndices;
	glm::dvec3 m_createCenter;


	Layer* ResolveLayer(std::vector<std::unique_ptr<Layer>>& layers, int32_t layerId) const;

public:
	void Init(UIState* uiState, int32_t* pickingLayerId, int32_t* pickingObjectId, int32_t* pickingNodeId, const std::unordered_map<int32_t, int32_t>* layerIdToIndex);
	void Shutdown() { m_editOverlay.Shutdown(); m_createOverlay.Shutdown(); }
	void DrawEditObject() { if (m_uiState->isEditObjectMode && m_isEdittingObject) m_editOverlay.Draw(); }
	void SetIsEditting(bool value) { m_isEdittingObject = value; }

	// 객체 편집
	void SetEditObject(std::vector<std::unique_ptr<Layer>>& layers);
	void UpdateEditObject();
	void MoveObject  (glm::dvec3& moveDelta);
	void RotateObject(glm::dvec3& moveDelta);
	void ScaleObject (glm::dvec3& scaleDelta);
	void SaveEditObject(std::vector<std::unique_ptr<Layer>>& layers);
	void CancelEditObject();

	void CreateObject(int32_t shape, std::vector<std::unique_ptr<Layer>>& layers, int32_t hitLayerId, glm::dvec3 createPos);
	void UpdateCreateObject(glm::dvec3 pickingPos);
	void DrawCreateObject() { if (m_uiState->isCreateObjectMode) m_createOverlay.Draw(); }
};