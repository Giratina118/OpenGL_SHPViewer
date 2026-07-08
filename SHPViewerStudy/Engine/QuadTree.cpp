#include <pch.h>
#include "QuadTree.h"
#include "CameraController.h"
#include "Layer.h"

QuadTreeNode::QuadTreeNode(int32_t level, BoundingBox& mbrBox)
{
	m_level = level;
	m_boundingBox = mbrBox;
}

// 쿼드트리 빌드
void QuadTree::BuildQuadTree()
{
	int32_t rootNodeId = 0;
	CreateNode(rootNodeId, m_layer.m_boundingBox); // 루트노드 생성

	// 데이터 삽입
	m_objectLevels.assign(std::max({ m_layer.pointObjects.size(), m_layer.polyLineObjects.size(), m_layer.polygonObjects.size() }), 0);

	int32_t objectsCount = static_cast<int32_t>(m_layer.pointObjects.size()); // 포인트 객체 삽입
	for (int objectNum = 0; objectNum < objectsCount; objectNum++) InsertData(rootNodeId, objectNum, m_layer.pointObjects[objectNum].mbrBox,    false);

	objectsCount = static_cast<int32_t>(m_layer.polyLineObjects.size());      // 폴리라인 객체 삽입
	for (int objectNum = 0; objectNum < objectsCount; objectNum++) InsertData(rootNodeId, objectNum, m_layer.polyLineObjects[objectNum].mbrBox, false);

	objectsCount = static_cast<int32_t>(m_layer.polygonObjects.size());       // 폴리곤 객체 삽입
	for (int objectNum = 0; objectNum < objectsCount; objectNum++) InsertData(rootNodeId, objectNum, m_layer.polygonObjects[objectNum].mbrBox,  m_layer.m_isBuilding);

	std::vector<std::vector<int>> levelInfo(m_maxLevel + 1, std::vector<int>(2, 0));
	SettingHeight(0); // 노드 높이 설정

	for (auto& node : m_nodes) node.m_objectIds.shrink_to_fit();
	m_nodes.shrink_to_fit();
}

// 데이터 삽입
void QuadTree::InsertData(int32_t currentNodeId, int32_t dataId, BoundingBox& dataMbrBox, bool isBuilding)
{
	assert(currentNodeId >= 0);
	assert(currentNodeId < m_nodes.size());

	assert(dataId >= 0);
	assert(dataId < m_layer.polygonObjects.size());

	assert(dataMbrBox.minX <= dataMbrBox.maxX);
	assert(dataMbrBox.minY <= dataMbrBox.maxY);

	QuadTreeNode& curNode    = m_nodes[currentNodeId]; // 현재 노드
	BoundingBox&  curNodeBox = curNode.m_boundingBox;  // 현재 노드 mbr박스
	BoundingBox*  curObjBox  = nullptr; // 현재 데이터 mbr박스
	glm::dvec2    nodeCenter = curNodeBox.GetCenter(); // 노드 중점
	glm::dvec2    dataCenter = dataMbrBox.GetCenter(); // 데이터 중점

	switch (m_layer.m_shapeType) {
	case 1: curObjBox = &m_layer.pointObjects[dataId].mbrBox;    break;
	case 3: curObjBox = &m_layer.polyLineObjects[dataId].mbrBox; break;
	case 5: curObjBox = &m_layer.polygonObjects[dataId].mbrBox;  break;
	}


	// 최대 깊이일 경우 현재 노드에 데이터 삽입
	if (curNode.m_level == m_maxLevel) {
		curNode.m_objectIds.push_back(dataId);

		if (isBuilding) curObjBox->SetHeight(m_layer.dbfTable.doubleColumns[m_layer.dbfTable.heightPos][dataId], m_layer.dbfTable.doubleColumns[m_layer.dbfTable.floorPos][dataId], curNodeBox.GetMaxExtent());
		if (curObjBox->height > curNodeBox.height) curNodeBox.height = curObjBox->height;
		if (dataId >= 0 && dataId < static_cast<int32_t>(m_objectLevels.size())) m_objectLevels[dataId] = curNode.m_level; // 객체가 들어간 레벨 기록 (레벨 색상)
		return;
	}

	// 특정 크기 이상만 현재 노드에 데이터 삽입
	if (curNodeBox.GetMaxExtent() * m_limitSizeRate < dataMbrBox.GetMaxExtent() ||
		nodeCenter.x - dataMbrBox.minX > curNodeBox.GetLengthX() * m_looseBoxRate && dataMbrBox.maxX - nodeCenter.x > curNodeBox.GetLengthX() * m_looseBoxRate ||
		nodeCenter.y - dataMbrBox.minY > curNodeBox.GetLengthY() * m_looseBoxRate && dataMbrBox.maxY - nodeCenter.y > curNodeBox.GetLengthY() * m_looseBoxRate) {
		curNode.m_objectIds.push_back(dataId);

		if (isBuilding) curObjBox->SetHeight(m_layer.dbfTable.doubleColumns[m_layer.dbfTable.heightPos][dataId], m_layer.dbfTable.doubleColumns[m_layer.dbfTable.floorPos][dataId], curNodeBox.GetMaxExtent());
		if (curObjBox->height > curNodeBox.height) curNodeBox.height = curObjBox->height;
		if (dataId >= 0 && dataId < static_cast<int32_t>(m_objectLevels.size())) m_objectLevels[dataId] = curNode.m_level; // 객체가 들어간 레벨 기록 (레벨 색상용)
		return;
	}
	
	// 자식 노드에 넣어야 하는 경우
	int32_t childNodeType; // 어느 위치의 자식노드인지

	if (dataCenter.y > nodeCenter.y) {
		if (dataCenter.x < nodeCenter.x)  childNodeType = static_cast<int32_t>(NodeChild::NorthWest);
		else							  childNodeType = static_cast<int32_t>(NodeChild::NorthEast);
	}
	else {
		if (dataCenter.x < nodeCenter.x)  childNodeType = static_cast<int32_t>(NodeChild::SouthWest);
		else							  childNodeType = static_cast<int32_t>(NodeChild::SouthEast);
	}

	int32_t childNodeId = curNode.m_childNodes[childNodeType];  // 목표 자식 노드 id
	if (childNodeId == -1)										// 자식노드가 없다면 (-1: 초기값, 자식 없음을 의미)
		childNodeId = DivideNode(currentNodeId, childNodeType); // 분할, 생성된 자식노드 Id 반환

	InsertData(childNodeId, dataId, dataMbrBox, isBuilding); // 재귀 분할
}

// 분할
int32_t QuadTree::DivideNode(int32_t currentNodeId, int32_t childNodeType)
{
	int32_t childNodeId    = static_cast<int32_t>(m_nodes.size()); // 자식노드 id
	int32_t childNodeLevel = m_nodes[currentNodeId].m_level + 1;   // 자식노드 레벨
	BoundingBox childBox   = m_nodes[currentNodeId].m_boundingBox.ChildBox(childNodeType); // 자식노드 boundingBox

	m_nodes[currentNodeId].m_childNodes[childNodeType] = childNodeId; // 부모노드에 자식노드 id 저장
	CreateNode(childNodeLevel, childBox); // 자식노드 생성

	m_nodes[childNodeId].m_parentNodeId = currentNodeId; // 자식노드에 부모노드 id 저장

	return childNodeId;
}

// 노드 높이 설정
double QuadTree::SettingHeight(int32_t currentNodeId)
{
	QuadTreeNode& curNode = m_nodes[currentNodeId];
	double height = curNode.m_boundingBox.height;

	if (curNode.m_level == m_maxLevel) return height;

	for (int child = 0; child < 4; child++) {
		if (curNode.m_childNodes[child] == -1) continue;

		double childHeight = SettingHeight(curNode.m_childNodes[child]);
		if (childHeight > height) height = childHeight;
	}

	curNode.m_boundingBox.height = height;
	return height;
}

// 렌더링할 객체 탐색 (+ LOD)
void QuadTree::SearchRenderingData(std::vector<int32_t>& renderObjectIds, int32_t currentNodeId, const CameraController& camera, const glm::dvec3& cameraPos, double worldToScreenScale)
{
	if (m_nodes.empty()) return;
	if (currentNodeId < 0 || currentNodeId >= static_cast<int32_t>(m_nodes.size())) return;

	QuadTreeNode& node = m_nodes[currentNodeId];
	node.m_isVisibleNode = false;
	node.m_isVisibleFake = false;

	//m_visitedNodeCount++; // 노드를 방문 수
	//m_frustumTestCount++; // 노드 절두체 판정 수

	FrustumState nodeFrustumState = camera.GetFrustumState(node.m_boundingBox.GetLooseBox(m_looseBoxRate));
	// 조금이라도 겹치는가, 전혀 안 겹치면 return (컬링)
	if (nodeFrustumState == FrustumState::OUTSIDE)
		return;

	// 거리 기반 LOD
	// 카메라와 노드 중심점 사이의 실제 3D 거리 계산
	glm::dvec2 center = node.m_boundingBox.GetCenter();
	double distanceX = cameraPos.x - center.x;
	double distanceY = cameraPos.y - center.y;
	double distanceZ = cameraPos.z; // 노드는 지면(Z=0)에 있다고 가정
	double distance  = std::max(1.0, std::sqrt(distanceX * distanceX + distanceY * distanceY + distanceZ * distanceZ));

	// 노드 바운딩 박스가 작을 수록 더 가까이 가야 볼 수 있음
	// 8레벨: 180m -> 180m * viewRangeRate 일때 보이도록
	if (node.m_boundingBox.GetMaxExtent() * camera.m_viewRangeRate < distance) {
		//m_visibleNodeFakeObjIds.push_back(currentNodeId); // 이 노드는 화면에 보이는 노드 목록에 추가 (fake 오브젝트로 대체)

		int32_t parentId = node.m_parentNodeId;
		if (parentId != -1 && !m_nodes[parentId].m_isVisibleFake) {
			m_nodes[parentId].m_isVisibleFake = true;
			m_visibleNodeFakeObjIds.push_back(parentId);
		}
		return;
	}

	// 충분히 큰 경우
	if (!node.m_objectIds.empty()) {
		int32_t parentId = currentNodeId;
		while (!m_nodes[parentId].m_isVisibleNode && parentId != -1) {
			m_nodes[parentId].m_isVisibleNode = true;
			m_visibleNodeIds.push_back(parentId); // 이 노드는 화면에 보이는 노드 목록에 추가
			parentId = m_nodes[parentId].m_parentNodeId;
		}
	}


	// 완전히 포함되는가
	if (nodeFrustumState == FrustumState::INSIDE) {
		InputRenderingDataAll(renderObjectIds, currentNodeId, camera, cameraPos, worldToScreenScale);
		return;
	}

	// 현재 노드 데이터 검사 (카메라 안에 들어오는지)
	int32_t pointObjCount   = static_cast<int32_t>(m_layer.pointObjects.size());
	int32_t lineObjCount    = static_cast<int32_t>(m_layer.polyLineObjects.size());
	int32_t polygonObjCount = static_cast<int32_t>(m_layer.polygonObjects.size());

	switch (m_layer.m_shapeType) {
	case 1:
		for (int32_t dataId : node.m_objectIds) {
			if (dataId < 0 || dataId >= pointObjCount) continue;

			//m_objectTestCount++;
			//m_frustumTestCount++;

			if (camera.GetFrustumState(m_layer.pointObjects[dataId].mbrBox) == FrustumState::OUTSIDE) continue;
			renderObjectIds.push_back(dataId);
		}
		break;
	case 3:
		for (int32_t dataId : node.m_objectIds) {
			if (dataId < 0 || dataId >= lineObjCount) continue;
			
			//m_objectTestCount++;
			//m_frustumTestCount++;
			
			if (camera.GetFrustumState(m_layer.polyLineObjects[dataId].mbrBox) == FrustumState::OUTSIDE) continue;
			renderObjectIds.push_back(dataId);
		}
		break;
	case 5:
		for (int32_t dataId : node.m_objectIds) {
			if (dataId < 0 || dataId >= polygonObjCount) continue;
			
			//m_objectTestCount++;
			//m_frustumTestCount++;

			if (camera.GetFrustumState(m_layer.polygonObjects[dataId].mbrBox) == FrustumState::OUTSIDE) continue;
			renderObjectIds.push_back(dataId);
		}
		break;
	}

	// 자식 노드 재귀 탐색
	for (int32_t childNodeId : node.m_childNodes) {
		if (childNodeId != -1) {
			SearchRenderingData(renderObjectIds, childNodeId, camera, cameraPos, worldToScreenScale);
		}
	}
}

// 완전 포함되는 노드는 데이터 모두 넣기
void QuadTree::InputRenderingDataAll(std::vector<int32_t>& renderObjectIds, int32_t currentNodeId, const CameraController& camera, const glm::dvec3& cameraPos, double worldToScreenScale)
{
	if (currentNodeId < 0 || currentNodeId >= static_cast<int32_t>(m_nodes.size())) return;

	QuadTreeNode& node = m_nodes[currentNodeId];
	node.m_isVisibleNode = false;
	node.m_isVisibleFake = false;

	//m_visitedNodeCount++; // 노드를 방문 수

	// 카메라와 노드 중심점 사이의 실제 3D 거리 계산
	glm::dvec2 center = node.m_boundingBox.GetCenter();
	double distanceX = cameraPos.x - center.x;
	double distanceY = cameraPos.y - center.y;
	double distanceZ = cameraPos.z; // 노드는 지면(Z=0)에 있다고 가정
	double distance = std::max(1.0, std::sqrt(distanceX * distanceX + distanceY * distanceY + distanceZ * distanceZ));

	// 노드 바운딩 박스가 작을 수록 더 가까이 가야 볼 수 있음
	// 8레벨: 180m -> 180m * viewRangeRate 일때 보이도록
	if (node.m_boundingBox.GetMaxExtent() * camera.m_viewRangeRate < distance)
	{
		//m_visibleNodeFakeObjIds.push_back(currentNodeId); // 이 노드는 화면에 보이는 노드 목록에 추가 (fake 오브젝트로 대체)
		
		int32_t parentId = node.m_parentNodeId;
		if (parentId != -1 && !m_nodes[parentId].m_isVisibleFake) {
			m_nodes[parentId].m_isVisibleFake = true;
			m_visibleNodeFakeObjIds.push_back(parentId);
		}
		
		return;
	}

	if (!node.m_objectIds.empty()) {
		int32_t parentId = currentNodeId;
		while (parentId != -1 && !m_nodes[parentId].m_isVisibleNode) {
			m_nodes[parentId].m_isVisibleNode = true;
			m_visibleNodeIds.push_back(parentId); // 이 노드는 화면에 보이는 노드 목록에 추가
			parentId = m_nodes[parentId].m_parentNodeId;
		}
	}

	//m_objectnotCullCount += node.m_objectIds.size(); // 완전 포함된 노드의 객체는 모두 컬링 안 됨

	// 현재 노드 데이터
	int32_t pointObjCount   = static_cast<int32_t>(m_layer.pointObjects.size());
	int32_t lineObjCount    = static_cast<int32_t>(m_layer.polyLineObjects.size());
	int32_t polygonObjCount = static_cast<int32_t>(m_layer.polygonObjects.size());
	switch (m_layer.m_shapeType) {
	case 1:
		for (int32_t dataId : node.m_objectIds) {
			if (dataId >= 0 && dataId < pointObjCount)
				renderObjectIds.push_back(dataId);
		}
		break;
	case 3:
		for (int32_t dataId : node.m_objectIds) {
			if (dataId >= 0 && dataId < lineObjCount)
				renderObjectIds.push_back(dataId);
		}
		break;
	case 5:
		for (int32_t dataId : node.m_objectIds) {
			if (dataId >= 0 && dataId < polygonObjCount)
				renderObjectIds.push_back(dataId);
		}
		break;
	}

	// 자식 노드 재귀
	for (int32_t childNodeId : node.m_childNodes) {
		if (childNodeId != -1) {
			InputRenderingDataAll(renderObjectIds, childNodeId, camera, cameraPos, worldToScreenScale);
		}
	}
}

// 피킹 데이터 탐색 및 반환
int32_t QuadTree::SearchPickingData(glm::dvec3& rayStart, glm::dvec3& rayDir, int32_t currentNodeId, double& minDistance, std::vector<DrawInfo>& polygonDrawInfos, std::vector<uint32_t>& polygonIndices, std::vector<Vertex>& polygonVertices, glm::dvec3& hitPoint)
{
	// 루트노드 높이 접촉점 -> z=0 라인
	// 자신 노드 데이터와 비교
	// 자식노드 x, y, 최대 최소 4부분에서 선이 접하는지 검사, 그 지점에서 높이 또한 자식노드 높이보다 낮아야 함
	// 해당 자식 노드데이터와 비교 -> 면 접촉
	// -> 재귀탐색
	// 접촉한다면 접촉 지점 부터 카메라의 거리가 가장 짧을 경우에만 객체 아이디 저장

	int32_t selectDataId = -1;
	
	QuadTreeNode& node = m_nodes[currentNodeId];

	if (!node.m_isVisibleNode) return selectDataId; // 노드가 안 보이면 넘기기 -> 화면에 보이는 객체만 클릭할 수 있도록
	if (!node.m_boundingBox.IsOnCollisionRay(rayStart, rayDir)) return selectDataId; // TODO: 카메라와 노드 접촉점의 거리가 minDistance 보ek 작으면 그냥 노드 넘기도록
	
	// 노드 자신의 안에 있는 데이터들이 충돌하는지
	if (node.m_objectIds.size() > 0) {
		for (int32_t dataId : node.m_objectIds) {
			PolyObject& polygon = m_layer.polygonObjects[dataId];
			if (!polygon.mbrBox.IsOnCollisionRay(rayStart, rayDir)) continue; // 접하지 않으면 넘기기

			//double collisionRation = polygon.OnCollisionRay(rayOrigin, rayDir, m_nodes[0].m_boundingBox.height); // 폴리곤 mbr 검사

			for (uint32_t indicesId = polygonDrawInfos[dataId].indexOffset; indicesId < polygonDrawInfos[dataId].indexOffset + polygonDrawInfos[dataId].indexCount; indicesId += 3) {
				uint32_t index0 = polygonIndices[indicesId + 0];
				uint32_t index1 = polygonIndices[indicesId + 1];
				uint32_t index2 = polygonIndices[indicesId + 2];
				glm::dvec3 v0 = glm::dvec3(polygonVertices[index0].x, polygonVertices[index0].y, polygonVertices[index0].z);
				glm::dvec3 v1 = glm::dvec3(polygonVertices[index1].x, polygonVertices[index1].y, polygonVertices[index1].z);
				glm::dvec3 v2 = glm::dvec3(polygonVertices[index2].x, polygonVertices[index2].y, polygonVertices[index2].z);

				
				double triCollisionDistance = RayTriangle(rayStart, rayDir, v0, v1, v2);
				if (triCollisionDistance >= 0.0 && triCollisionDistance < minDistance) {
					minDistance = triCollisionDistance;
					selectDataId = dataId;
					hitPoint = rayStart + rayDir * triCollisionDistance;
				}
			}

			/*
			if (collisionRation >= 0.0 && collisionRation <= 1.0 && collisionRation < minDistanceRation) { // 충돌 거리가 가장 가깝다면 저장
				minDistanceRation = collisionRation;
				selectDataId = dataId;
			}
			*/
		}
	}
	
	// 자식 노드에서 재귀 탐색
	for (int32_t childNodeId : node.m_childNodes) {
		if (childNodeId == -1) continue; // 자식노드 없으면 넘기기
		QuadTreeNode& childNode = m_nodes[childNodeId];
		if (!childNode.m_boundingBox.IsOnCollisionRay(rayStart, rayDir)) continue; // 자식노드와 접하지 않으면 넘기기

		int32_t childPickingId = SearchPickingData(rayStart, rayDir, childNodeId, minDistance, polygonDrawInfos, polygonIndices, polygonVertices, hitPoint);
		if (childPickingId != -1) selectDataId = childPickingId;
	}
	

	/*
	// 데이터와 충돌하는지 검사
	if (!m_nodes[currentNodeId].m_objectIds.empty()) {
		for (int32_t dataId : m_nodes[currentNodeId].m_objectIds) {
			// 충돌한다면 dataId 반환

			// TODO: 현재는 폴리곤만 고려
			if (layer.polygonObjects[dataId].mbrBox.IsOnCollisionPoint(hitPoint)) {
				glm::dvec2 objBoxCenter = layer.polygonObjects[dataId].mbrBox.GetCenter();
				double newDistance = (objBoxCenter.x - hitPoint.x) * (objBoxCenter.x - hitPoint.x) + (objBoxCenter.y - hitPoint.y) * (objBoxCenter.y - hitPoint.y);

				if (minDistanceRation > newDistance) {
					minDistanceRation = newDistance;
					selectDataId = dataId;
				}
			}
		}
	}

	// 자식 노드로 재귀 탐색
	for (int32_t childNodeId : m_nodes[currentNodeId].m_childNodes) {
		if (childNodeId != -1) {
			int32_t dataId = SearchPickingData(layer, rayOrigin, hitPoint, rayDir, childNodeId, minDistanceRation);
			if (dataId != -1) selectDataId = dataId; // 충돌하는 데이터가 있다면 반환
		}
	}
	*/
	
	return selectDataId; // 충돌하는 데이터가 없다면 -1 반환
}

double QuadTree::OnCollisionRayTriangle(glm::dvec3& rayStart, glm::dvec3& rayDir, glm::dvec3& trianglePoint1, glm::dvec3& trianglePoint2, glm::dvec3& trianglePoint3)
{
	glm::dvec3 edge1 = trianglePoint2 - trianglePoint1;
	glm::dvec3 edge2 = trianglePoint3 - trianglePoint1;
	glm::dvec3 normal = glm::normalize(glm::cross(edge1, edge2)); // 평면의 법선벡터
	double planeNumber = -glm::dot(normal, trianglePoint1); // 평면 방정식의 상수항

	double rayToPlaneDot = glm::dot(normal, rayDir); // 광선이 평면을 얼마나 향하고 있는가
	if (std::abs(rayToPlaneDot) < 1e-10) return -1.0; // 레이와 평면이 평행하면 충돌X

	double distance = -(glm::dot(normal, rayStart) + planeNumber) / rayToPlaneDot;
	if (distance < 0.0) return -1.0;
	glm::dvec3 hitPoint = rayStart + rayDir * distance;


}

// 뮐러 - 트럼보어 교차 알고리즘 (Muller-Trumbore intersection algorithm)
double QuadTree::RayTriangle(const glm::dvec3& rayOrigin, const glm::dvec3& rayDir, 
	const glm::dvec3& trianglePoint1, const glm::dvec3& trianglePoint2, const glm::dvec3& trianglePoint3)
{
	glm::dvec3 edge1  = trianglePoint2 - trianglePoint1;
	glm::dvec3 edge2  = trianglePoint3 - trianglePoint1;
	glm::dvec3 normal = glm::cross(rayDir, edge2);

	double det = glm::dot(edge1, normal);		   // 스칼라 삼중곱을 이용한 행렬식
	if (fabs(det) < 1e-8)       return -1.0;	   // 0에 가까우면 평행, 교차하지 않음

	double invDet = 1.0 / det;
	glm::dvec3 tVec = rayOrigin - trianglePoint1;  // O - A = T
	double u = glm::dot(tVec, normal) * invDet;    // u 계산 및 검사 
	if (u < 0.0 || u > 1.0)     return -1.0;	   // U는 0.0 ~ 1.0 사이여야 함

	glm::dvec3 qVec = glm::cross(tVec, edge1);     // V 계산 및 검사
	double v = glm::dot(rayDir, qVec) * invDet;
	if (v < 0.0 || u + v > 1.0)	return -1.0;       // V는 0.0 ~ 1.0 사이이고, U+V <= 1.0 이어야 함

	return glm::dot(edge2, qVec) * invDet;         // 거리 계산
}