#include <pch.h>
#include "EditObject.h"
#include "MeshManager.h"

void EditObject::Init(UIState* uiState, int32_t* pickingLayerId, int32_t* pickingObjectId, int32_t* pickingNodeId, const std::unordered_map<int32_t, int32_t>* layerIdToIndex)
{
    m_editOverlay.Init();
    m_createOverlay.Init();
    m_uiState         = uiState;
    m_pickingLayerId  = pickingLayerId;
    m_pickingObjectId = pickingObjectId;
    m_pickingNodeId   = pickingNodeId;
    m_layerIdToIndex  = layerIdToIndex;
}

Layer* EditObject::ResolveLayer(std::vector<std::unique_ptr<Layer>>& layers, int32_t layerId) const
{
    if (layerId == -1 || !m_layerIdToIndex) return nullptr;
    auto it = m_layerIdToIndex->find(layerId);
    if (it == m_layerIdToIndex->end())      return nullptr;
    int32_t index = it->second;
    if (index < 0 || index >= static_cast<int32_t>(layers.size())) return nullptr;
    return layers[index].get();
}

// 선택한 객체를 편집 대상으로 설정, 복사본을 만들어서 이동/회전 시 원본 데이터에 바로 적용하지 않고, 복사본을 변환 후 렌더링
void EditObject::SetEditObject(std::vector<std::unique_ptr<Layer>>& layers)
{
    if (m_isEdittingObject || *m_pickingLayerId == -1 || *m_pickingObjectId == -1) return;
    Layer* layer = ResolveLayer(layers, *m_pickingLayerId);
    if (!layer) return;

    m_isEdittingObject = true;

    MeshManager* mesh      = layer->m_renderer->m_mesh.get();
    m_transformPolygonInfo = mesh->m_polygonDrawInfos[*m_pickingObjectId];
    m_transformLineInfo    = mesh->m_lineDrawInfos[*m_pickingObjectId];

    // 면(Polygon) 정점 복사 및 면 색상 설정 (연한 초록색)
    m_editOriginalVertices.assign(mesh->m_polygonVertices.begin() + m_transformPolygonInfo.vertexOffset,
        mesh->m_polygonVertices.begin() + m_transformPolygonInfo.vertexOffset + m_transformPolygonInfo.vertexCount);
    m_transformVertices = m_editOriginalVertices;

    for (Vertex& vertex : m_transformVertices) {
        vertex.color.red = 20; vertex.color.green = 230; vertex.color.blue = 50; vertex.color.alpha = 100;
    }

    // 면 인덱스 리매핑
    m_transformPolygonIndices.clear();
    for (uint32_t i = 0; i < m_transformPolygonInfo.indexCount; i++) {
        uint32_t originalIndex = mesh->m_polygonIndices[m_transformPolygonInfo.indexOffset + i];
        m_transformPolygonIndices.push_back(originalIndex - m_transformPolygonInfo.vertexOffset);
    }

    //  선 인덱스 리매핑
    m_transformLineIndices.clear();
    for (uint32_t i = 0; i < m_transformLineInfo.indexCount; i++) {
        uint32_t originalIndex = mesh->m_lineIndices[m_transformLineInfo.indexOffset + i];
        m_transformLineIndices.push_back((originalIndex - m_transformPolygonInfo.vertexOffset));
    }

    // 중심점 계산
    m_editCenter = glm::dvec3(0.0);
    for (const Vertex& vertex : m_editOriginalVertices)
        m_editCenter += glm::dvec3(vertex.x, vertex.y, vertex.z);
    m_editCenter /= static_cast<double>(m_editOriginalVertices.size());

    m_editTransform = Transform();

    // GPU 업로드
    m_editOverlay.Upload(m_transformVertices, m_transformPolygonIndices, m_transformLineIndices, GL_DYNAMIC_DRAW);
}

// 객체 편집 업데이트
void EditObject::UpdateEditObject()
{
    glm::dmat4 transformMatrix = m_editTransform.GetMatrix();

    glm::dmat4 finalMatrix = glm::dmat4(1.0);
    finalMatrix = glm::translate(finalMatrix, m_editCenter);
    finalMatrix = finalMatrix * transformMatrix;
    finalMatrix = glm::translate(finalMatrix, -m_editCenter);

    // 면 정점 변환 갱신 및 색상 재적용 (이동 시 원복 방지)
    for (size_t i = 0; i < m_editOriginalVertices.size(); i++) {
        const Vertex& origin = m_editOriginalVertices[i];
        glm::dvec4 transformedPos = finalMatrix * glm::dvec4(origin.x, origin.y, origin.z, 1.0);
        m_transformVertices[i].x = static_cast<float>(transformedPos.x);
        m_transformVertices[i].y = static_cast<float>(transformedPos.y);
        m_transformVertices[i].z = static_cast<float>(transformedPos.z);
    }

    // VBO 갱신
    m_editOverlay.UpdateVertices(m_transformVertices);
}

// 객체 편집(이동)
void EditObject::MoveObject(glm::dvec3& moveDelta)
{
    if (m_transformPolygonInfo.vertexCount == 0) return;
    m_editTransform.MoveWorld(moveDelta);
    UpdateEditObject(); // 버퍼 갱신
}

// 객체 편집(회전)
void EditObject::RotateObject(glm::dvec3& rotateDelta)
{
    if (m_transformPolygonInfo.vertexCount == 0) return;

    double yaw   = rotateDelta.x * 0.5;
    double pitch = rotateDelta.y * 0.5;
    double roll  = rotateDelta.z * 0.5;

    m_editTransform.RotateWorld(yaw, pitch, roll); // (yaw, pitch, roll)
    UpdateEditObject(); // 버퍼 갱신
}

// 객체 편집(스케일)
void EditObject::ScaleObject(glm::dvec3& scaleDelta)
{
    if (m_transformPolygonInfo.vertexCount == 0) return;
    m_editTransform.ScaleWorld(scaleDelta * 0.001);
    UpdateEditObject(); // 버퍼 갱신
}

// 객체 편집 완료, 원본 데이터에 적용
void EditObject::SaveEditObject(std::vector<std::unique_ptr<Layer>>& layers)
{
    m_isEdittingObject = false;
    //m_uiState->isEditObjectMode = false;

    if (*m_pickingLayerId != -1 && *m_pickingObjectId != -1 && *m_pickingNodeId != -1) {
        Layer* layer = ResolveLayer(layers, *m_pickingLayerId);
        if (!layer || layer->m_shapeType != 5) {
            *m_pickingLayerId  = -1;
            *m_pickingObjectId = -1;
            *m_pickingNodeId   = -1;
            return;
        }

        MeshManager* mesh          = layer->m_renderer->m_mesh.get();
        glm::dmat4 transformMatrix = m_editTransform.GetMatrix();
        glm::dmat4 finalMatrix     = glm::dmat4(1.0);

        finalMatrix = glm::translate(finalMatrix, m_editCenter);
        finalMatrix = finalMatrix * transformMatrix;
        finalMatrix = glm::translate(finalMatrix, -m_editCenter);

        PolyObject& object = layer->polygonObjects[*m_pickingObjectId];
        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double maxY = std::numeric_limits<double>::lowest();

        for (glm::dvec2& point : object.points) {
            glm::dvec4 transformedPos = finalMatrix * glm::dvec4(point.x, point.y, 0.0, 1.0);
            point.x = transformedPos.x;
            point.y = transformedPos.y;
            minX = std::min(minX, point.x); maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y); maxY = std::max(maxY, point.y);
        }
        object.SetMBRBox(minX, minY, maxX, maxY);

        layer->m_isDirty = true;
        layer->m_quadTree->UpdateTransformObject(*m_pickingNodeId, *m_pickingObjectId);

        // CDT 재삼각분할이 필요하므로 GPU 버퍼는 부분갱신(glBufferSubData) 대신 전체 재빌드
        layer->m_renderer->m_mesh->BuildMesh();
        layer->m_renderer->m_mesh->BuildFakeMeshes();
    }

    *m_pickingLayerId  = -1;
    *m_pickingObjectId = -1;
    *m_pickingNodeId   = -1;
}

// 객체 편집 취소, 원본 데이터에 적용하지 않고 편집 모드 종료
void EditObject::CancelEditObject()
{
    TCHAR buf[256];
    _stprintf_s(buf, _T("캔슬\n"));
    OutputDebugString(buf);

    m_isEdittingObject = false;
    //m_uiState->isEditObjectMode = false;

    *m_pickingLayerId  = -1;
    *m_pickingObjectId = -1;
    *m_pickingNodeId   = -1;
}

void EditObject::CreateObject(int32_t shape, std::vector<std::unique_ptr<Layer>>& layers, int32_t hitLayerId, glm::dvec3 createPos)
{
    m_createVertices.clear();
    m_createPolygonIndices.clear();
    m_createLineIndices.clear();

    // 생성할 객체를 추가할 레이어 탐색
    Layer* targetLayer = ResolveLayer(layers, hitLayerId);
    if (!targetLayer || targetLayer->m_shapeType != 5) {
        targetLayer = nullptr;
        for (auto& layer : layers)
            if (layer != nullptr && layer->m_shapeType == 5) { targetLayer = layer.get(); break; }
        
        if (!targetLayer) return;
    }

    UCharColor color = targetLayer->m_baseColor;
    m_createLayerId  = targetLayer->m_id;
    m_createCenter   = createPos;

    switch (shape)
    {
    case 1: // 원
        for (int32_t vertexIndex = 0; vertexIndex < 20; vertexIndex++) {
            Vertex vertex = { m_createCenter.x + glm::sin(glm::radians(vertexIndex * 18.0)) * 100.0, m_createCenter.y + glm::cos(glm::radians(vertexIndex * 18.0)) * 100.0, m_createCenter.z, color};
            m_createVertices.push_back(vertex);
        }
        break;
    case 2: // 사각형
        for (int32_t vertexIndex = 0; vertexIndex < 4; vertexIndex++) {
            Vertex vertex = { m_createCenter.x + glm::sin(glm::radians(45.0 + vertexIndex * 90.0)) * 100.0, m_createCenter.y + glm::cos(glm::radians(45.0 + vertexIndex * 90.0)) * 100.0, m_createCenter.z, color };
            m_createVertices.push_back(vertex);
        }
        break;
    case 3: // 삼각형
        for (int32_t vertexIndex = 0; vertexIndex < 3; vertexIndex++) {
            Vertex vertex = { m_createCenter.x + glm::sin(glm::radians(vertexIndex * 120.0)) * 100.0, m_createCenter.y + glm::cos(glm::radians(vertexIndex * 120.0)) * 100.0, m_createCenter.z, color };
            m_createVertices.push_back(vertex);
        }
        break;
    }

    for (int32_t vertexIndex = 1; vertexIndex < m_createVertices.size() - 1; vertexIndex++) {
        m_createPolygonIndices.push_back(0);
        m_createPolygonIndices.push_back(vertexIndex);
        m_createPolygonIndices.push_back(vertexIndex + 1);
    }

    for (int32_t vertexIndex = 0; vertexIndex < m_createVertices.size(); vertexIndex++) {
        m_createLineIndices.push_back(vertexIndex);
        m_createLineIndices.push_back((vertexIndex + 1) % m_createVertices.size());
    }

    // GPU 업로드
    m_createOverlay.Upload(m_createVertices, m_createPolygonIndices, m_createLineIndices, GL_DYNAMIC_DRAW);
}

void EditObject::UpdateCreateObject(glm::dvec3 pickingPos)
{
    glm::dvec3 delta = pickingPos - m_createCenter;
    m_createCenter   = pickingPos;

    for (int32_t vertexIndex = 0; vertexIndex < m_createVertices.size(); vertexIndex++) {
        m_createVertices[vertexIndex].x += delta.x;
        m_createVertices[vertexIndex].y += delta.y;
        m_createVertices[vertexIndex].z += delta.z;
    }

    m_createOverlay.UpdateVertices(m_createVertices);
}

void EditObject::SaveCreateObject(std::vector<std::unique_ptr<Layer>>& layers)
{
    if (m_createVertices.size() < 3) return;

    Layer* targetLayer = ResolveLayer(layers, m_createLayerId); // CreateObject에서 저장해둔 레이어 ID 사용
    if (!targetLayer) return;

    // Vertex -> 실제 폴리곤 좌표(glm::dvec2) 변환
    PolyObject newObject;
    newObject.shapeType = 5;
    newObject.parts = { 0 };
    newObject.points.reserve(m_createVertices.size());
    for (const Vertex& vertex : m_createVertices)
        newObject.points.push_back({ vertex.x, vertex.y });

    // MBR 계산
    double minX = std::numeric_limits<double>::max(),    minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest(), maxY = std::numeric_limits<double>::lowest();
    for (const glm::dvec2& point : newObject.points) {
        minX = std::min(minX, point.x); maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y); maxY = std::max(maxY, point.y);
    }
    newObject.SetMBRBox(minX, minY, maxX, maxY);
    newObject.mbrBox.height = m_createCenter.z > 0.0 ? m_createCenter.z : 3.0; // 1층 높이값(3m)을 기준으로 절정

    int32_t newObjectId = static_cast<int32_t>(targetLayer->polygonObjects.size());
    targetLayer->polygonObjects.push_back(std::move(newObject));

    // 쿼드트리 갱신
    BoundingBox& objectBox = newObject.mbrBox;
    BoundingBox& layerBox  = targetLayer->m_boundingBox;

    if (!layerBox.IsInclude(objectBox)) { // 객체가 기존 쿼드트리 밖에 생성되었다면 쿼드트리 리빌드
        TCHAR buf[256]; _stprintf_s(buf, _T("루트노드 밖에 생성됨\n")); OutputDebugString(buf);

        layerBox = layerBox.CombineBox(objectBox);
        targetLayer->m_quadTree->BuildQuadTree();
    }
    else { // 객체가 기존 쿼드트리 안에 생성되었다면 객체 삽입
        TCHAR buf[256]; _stprintf_s(buf, _T("루트노드 안에 생성됨\n")); OutputDebugString(buf);

        if (static_cast<int32_t>(targetLayer->m_quadTree->m_objectLevels.size()) <= newObjectId)
            targetLayer->m_quadTree->m_objectLevels.resize(newObjectId + 1, 0);
        targetLayer->m_quadTree->InsertData(0, newObjectId, targetLayer->polygonObjects[newObjectId].mbrBox, false);
    }

    // dbf 행 추가
    DBFTable& dbfTable = targetLayer->m_dbfTable;
    for (auto& col : dbfTable.intColumns)     col.push_back(0);
    for (auto& col : dbfTable.doubleColumns)  col.push_back(0.0);
    for (auto& col : dbfTable.logicalColumns) col.push_back(0);
    for (auto& strCol : dbfTable.stringColumns) {
        int32_t oldRowCount = strCol.rowCount;
        strCol.Resize(oldRowCount + 1);
        std::string label = "New Object " + std::to_string(newObjectId);
        char* row = strCol.GetRow(oldRowCount);
        std::memset(row, ' ', strCol.width);
        std::memcpy(row, label.data(), std::min<size_t>(label.size(), strCol.width));
    }
    dbfTable.rowCount++;

    targetLayer->m_isDirty = true;

    // 메쉬 리빌드
    targetLayer->m_renderer->m_mesh->BuildMesh();
    targetLayer->m_renderer->m_mesh->BuildFakeMeshes();

    // 미리보기 정리
    m_createVertices.clear();
    m_createPolygonIndices.clear();
    m_createLineIndices.clear();
    m_createOverlay.Upload(m_createVertices, m_createPolygonIndices, m_createLineIndices, GL_DYNAMIC_DRAW);
    m_createLayerId = -1;
}

void EditObject::DeleteObject(std::vector<std::unique_ptr<Layer>>& layers)
{
    if (*m_pickingLayerId == -1 || *m_pickingObjectId == -1 || *m_pickingNodeId == -1) return;

    TCHAR buf[256]; _stprintf_s(buf, _T("객체 삭제\n")); OutputDebugString(buf);
    Layer* targetLayer = ResolveLayer(layers, *m_pickingLayerId);
    if (!targetLayer || targetLayer->m_shapeType != 5) return;

    int32_t deleteId = *m_pickingObjectId;
    if (deleteId < 0 || deleteId >= static_cast<int32_t>(targetLayer->polygonObjects.size())) return;

    // 쿼드트리 노드에서 객체 ID 제거
    auto& objectIds = targetLayer->m_quadTree->m_nodes[*m_pickingNodeId].m_objectIds;
    std::erase(objectIds, *m_pickingObjectId);

    // 실제 객체 데이터 논리적 삭제
    targetLayer->polygonObjects[*m_pickingObjectId].isDeleted = true;
    targetLayer->m_isDirty = true;

    if (m_isEdittingObject) {
        m_isEdittingObject = false;
        m_editOverlay.Upload({}, {}, {}, GL_DYNAMIC_DRAW);
    }

    // 피킹 상태 초기화
    *m_pickingLayerId  = -1;
    *m_pickingObjectId = -1;
    *m_pickingNodeId   = -1;
    m_isEdittingObject = false;

    // GPU 렌더링 버퍼 업데이트 (화면에서 사라지게 갱신)
    // targetLayer->UpdateBuffers();
}