#pragma once
#include "QuadTree.h"
#include "Render/Renderer.h"
#include <string>
#include <unordered_map>

class  CRightPanel;
class  MeshManager;
struct LayerItemData;


// 레이어 클래스, 하나의 레이어 클래스가 하나의 shp파일을 담당
class Layer
{ // m_shapeType에 따른 objects벡터에서 m_startIndex부터 m_length만큼이 해당 레이어의 objects(객체)
public:
	std::string m_name;        // 레이어 이름 (파일명과 동일)
	uint32_t    m_shapeType;   // 객체 타입   (1: Point, 3: PolyLine, 5: Polygon, 8: MultiPoint, 31: MultiPatch)
	double	    m_objSize =  5.0; // 객체 크기   (선 객체 -> 너비, 점 객체 -> 반지름)
	int32_t     m_id      = -1;   // 레이어 인덱스 (LayerManager에서 관리하는 layers 벡터의 인덱스)
	UCharColor  m_baseColor = { 255, 255, 255, 255 }; // 레이어 색상 (선/면 객체에 적용)

	std::unique_ptr<Renderer> m_renderer;
	std::unique_ptr<QuadTree> m_quadTree;
	BoundingBox               m_boundingBox; // 레이어 전체 MBR, 루트노드 생성 시 사용
	DBFTable                  m_dbfTable;    // dbf 데이터 테이블

	bool m_isVisible  = false; // 레이어 가시 상태
	bool m_isBuilding = false; // 건물인지 여부
	bool m_hasShx     = false; // .shx 파일 존재 여부
	bool m_hasDbf     = false; // .dbf 파일 존재 여부

	std::vector<PointObject> pointObjects;    // Point      객체 배열
	std::vector<PolyObject>  polyLineObjects; // PolyLine   객체 배열
	std::vector<PolyObject>  polygonObjects;  // Polygon    객체 배열

	void SetMBRBox(double minX, double minY, double maxX, double maxY) { m_boundingBox.minX = minX; m_boundingBox.minY = minY; m_boundingBox.maxX = maxX; m_boundingBox.maxY = maxY; }
};

inline Layer* ResolveLayerById(std::vector<std::unique_ptr<Layer>>& layers, const std::unordered_map<int32_t, int32_t>& layerIdToIndex, int32_t layerId) {
	if (layerId == -1) return nullptr;
	auto it = layerIdToIndex.find(layerId);
	if (it == layerIdToIndex.end()) return nullptr;
	int32_t index = it->second;
	if (index < 0 || index >= static_cast<int32_t>(layers.size())) return nullptr;
	return layers[index].get();
}