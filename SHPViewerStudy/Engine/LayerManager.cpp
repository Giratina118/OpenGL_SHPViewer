#include <pch.h>
#include <random>

#include "LayerManager.h"
#include "UI/CPanelRight.h"
#include "UI/CLayerListCtrl.h"

// 레이어 생성
Layer& LayerManager::CreateLayer(std::string name, uint32_t shpType, BoundingBox& layerBox)
{
    m_layers.emplace_back(std::make_unique<Layer>());
    m_layerIdToIndex[m_nextLayerId] = static_cast<int32_t>(m_layers.size() - 1);

    Layer& newLayer = *m_layers.back();
    newLayer.m_id = m_nextLayerId++;
    newLayer.m_name = name;
    newLayer.m_shapeType = shpType;
    newLayer.m_quadTree = std::make_unique<QuadTree>(newLayer);
    newLayer.m_boundingBox = layerBox;
    newLayer.m_isVisible = true;
    if (m_layers.size() == 1) newLayer.m_isBuilding = true; // 첫 번째 레이어(건물 정보)일 시 표시, 높이값 적용을 위해

    static std::random_device rand;
    static std::mt19937 gen(rand());
    std::uniform_int_distribution<int> dis(64, 255);

    newLayer.m_baseColor.red = static_cast<unsigned char>(dis(gen));
    newLayer.m_baseColor.green = static_cast<unsigned char>(dis(gen));
    newLayer.m_baseColor.blue = static_cast<unsigned char>(dis(gen));
    newLayer.m_baseColor.alpha = 255;

    return newLayer;
}

// 레이어 삭제
void LayerManager::DeleteLayer(int32_t layerId)
{
    auto mapIt = m_layerIdToIndex.find(layerId);
    if (mapIt == m_layerIdToIndex.end()) return;

    int32_t deleteLayerIndex = mapIt->second;

    m_layers.erase(m_layers.begin() + deleteLayerIndex);
    m_layerIdToIndex.erase(mapIt);

    for (int32_t i = deleteLayerIndex; i < static_cast<int32_t>(m_layers.size()); i++)
        m_layerIdToIndex[m_layers[i]->m_id] = i;

    for (auto orderIt = m_layerOrder.begin(); orderIt != m_layerOrder.end(); ) {
        if (*orderIt == deleteLayerIndex) 
            orderIt = m_layerOrder.erase(orderIt);
        else {
            if (*orderIt > deleteLayerIndex) (*orderIt)--;
            ++orderIt;
        }
    }

    if (m_hitLayerId     == layerId) m_hitLayerId = -1;
    if (m_pickingLayerId == layerId) m_pickingLayerId = m_pickingObjectId = m_pickingNodeId = -1;

    ReDraw();
}

// 렌더 초기화, 레이어별 Renderer 생성
bool LayerManager::InitRenderer(HWND hWnd, UIState* uiState)
{
    m_hWnd = hWnd;
    m_deviceContext = GetDC(hWnd);
    m_uiState = uiState;
    if (!InitEGL(hWnd)) return false;
    for (std::unique_ptr<Layer>& layer : m_layers) layer->m_renderer = std::make_unique<Renderer>(hWnd, *layer, *layer->m_quadTree);

    // 셰이더 설정
    if (!m_shader.CreateProgram("Resource/Shader/shader.vert", "Resource/Shader/shader.frag")) return false;
    m_viewProjectionLocation  = glGetUniformLocation(m_shader.GetProgram(), "u_viewProjection");
    m_colorMultiplierLocation = glGetUniformLocation(m_shader.GetProgram(), "u_colorMultiplier");


    if (!m_shader.CreateProgram("Resource/Shader/shader.vert", "Resource/Shader/shader.frag")) return false;


    // 깊이 테스트 활성화 (3D 건물, 면/라인 z-fighting 해결에 필요)
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_POLYGON_OFFSET_FILL); // 면 위에 라인을 그릴 때 z-fighting 방지, 면을 살짝 뒤로 밀어서 라인이 면 위에 보이게 함
    glPolygonOffset(1.0f, 1.0f);

    m_debugRectBuffer.Init();
    m_mbrBuffer.Init();
    m_frustumBuffer.Init();
    m_hoverOverlay.Init();
    m_editObject.Init(uiState, &m_pickingLayerId, &m_pickingObjectId, &m_pickingNodeId, &m_layerIdToIndex);

    return true;
}

// EGL 컨텍스트 생성
bool LayerManager::InitEGL(HWND hwnd)
{
    // EGL 연결
    m_deviceContext = GetDC(hwnd);
    m_display = eglGetDisplay(m_deviceContext); // GPU 드라이버 연결 핸들, gpu 시스템
    if (m_display == EGL_NO_DISPLAY) return false;

    // EGL 초기화
    EGLint majorVersion, minorVersion;
    if (!eglInitialize(m_display, &majorVersion, &minorVersion)) return false;

    // GPU 렌더링 옵션 선택
    EGLConfig eglConfig; // gpu 렌더링 설정
    EGLint configCount;
    const EGLint configAttributes[] = {
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,	 // 윈도우 화면에 출력
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, // OpenGL ES 3.x 사용
        EGL_RED_SIZE, 8,  EGL_GREEN_SIZE, 8,  EGL_BLUE_SIZE, 8,  EGL_ALPHA_SIZE, 8, // RGBA 각각 8비트, 32비트 컬러 사용
        EGL_DEPTH_SIZE, 24,	// 깊이 버퍼
        EGL_NONE
    };
    
    if (!eglChooseConfig(m_display, configAttributes, &eglConfig, 1, &configCount)) return false; // 조건에 만는 설정 선택

    m_surface = eglCreateWindowSurface(m_display, eglConfig, hwnd, nullptr);                      // 그림 그릴 표면(Surface) 생성
    if (m_surface == EGL_NO_SURFACE) return false;

    const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };               // 작업 환경(Context) 생성 - OpenGLES 3.x Context 생성
    m_context = eglCreateContext(m_display, eglConfig, EGL_NO_CONTEXT, contextAttributes);
    if (m_context == EGL_NO_CONTEXT) return false;

    if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context)) return false;                // 현재 스레드와 context 연결, 지금부터 이 GPU 작업환경을 사용하겠다 선언

    return true;
}

// 메모리 해제, EGL 종료
void LayerManager::Shutdown()
{
    m_layers.clear();
    m_mapManager.Shutdown();
    m_debugRectBuffer.Shutdown();
    m_mbrBuffer.Shutdown();
    m_frustumBuffer.Shutdown();
    m_hoverOverlay.Shutdown();
    m_editObject.Shutdown();

    if (m_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_context != EGL_NO_CONTEXT) eglDestroyContext(m_display, m_context);
        if (m_surface != EGL_NO_SURFACE) eglDestroySurface(m_display, m_surface);
        eglTerminate(m_display);
    }

    m_display = EGL_NO_DISPLAY;
    m_surface = EGL_NO_SURFACE;
    m_context = EGL_NO_CONTEXT;

    if (m_deviceContext) {
        ReleaseDC(m_hWnd, m_deviceContext);
        m_deviceContext = nullptr;
    }
}

// 메인 렌더 함수
void LayerManager::Render(CameraManager& camera, UISize& uiSize, glm::dvec3 hitPoint)
{
    // render여부 체크 -> 변화 없으면 그냥 return (CPU/GPU idle, 화면은 이전 프레임 유지)
    if (!m_needRedraw) return;

    m_shader.UseProgram();
    glUniformMatrix4fv(m_viewProjectionLocation, 1, GL_FALSE, glm::value_ptr(glm::mat4(camera.GetViewProjectionMatrix())));
    glUniform1f(m_colorMultiplierLocation, 1.0f); // 기본값

    // 실제 EGL surface 크기 (윈도우와 다를 수 있음 - 리사이즈 중 한 프레임 지연)
    EGLint surficeWidth = 0, surficeHeight = 0;
    eglQuerySurface(m_display, m_surface, EGL_WIDTH,  &surficeWidth);
    eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &surficeHeight);
    if (surficeWidth <= 0 || surficeHeight <= 0) { surficeWidth = uiSize.clientWidth; surficeHeight = uiSize.clientHeight; }

    // 전체 회색 클리어
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, uiSize.clientWidth, uiSize.clientHeight);
    glClearColor(0.94f, 0.94f, 0.94f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 3D 영역만 viewport + scissor (둘 다 동일 좌표 = surface 기준)
    glViewport(uiSize.panelWidth, 0, uiSize.clientWidth - uiSize.panelWidth, uiSize.clientHeight);
    glEnable(GL_SCISSOR_TEST);
    glScissor (uiSize.panelWidth, 0, uiSize.clientWidth - uiSize.panelWidth, uiSize.clientHeight);
    glClearColor(0.8f, 0.8f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    m_mapManager.Update(camera);
    m_mapManager.Render(camera);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);


    if (m_layerOrder.empty() && !m_layers.empty()) {
        for (int32_t i = 0; i < static_cast<int32_t>(m_layers.size()); i++)
            if (m_layers[i] != nullptr)
                m_layerOrder.push_back(i);
    }

    // 레이어 순회 렌더링
    for (int32_t i = static_cast<int32_t>(m_layerOrder.size()) - 1; i >= 0; i--) {
        int32_t layerIndex = m_layerOrder[i];
        if (layerIndex < 0 || layerIndex >= static_cast<int32_t>(m_layers.size()) || m_layers[layerIndex] == nullptr || !m_layers[layerIndex]->m_isVisible)
            continue;

        int32_t layerId = m_layers[layerIndex]->m_id; // 실제 아이디

        bool isSelectedLayer = (m_hitLayerId == -1 || m_hitLayerId == layerId);
        bool hasPickingData = (m_pickingLayerId == layerIndex && m_pickingObjectId != -1);
        Renderer* renderer = m_layers[layerIndex]->m_renderer.get();

        if (renderer != nullptr && !(!m_uiState->isShowBuilding && m_layers[layerIndex]->m_isBuilding)) {
            renderer->Render(camera, *m_uiState, uiSize, isSelectedLayer, hasPickingData, m_pickingObjectId, m_colorMultiplierLocation, m_mbrBuffer);
            glClear(GL_DEPTH_BUFFER_BIT);
        }
    }

    if (m_uiState->isEditObjectMode || !m_uiState->isPickingMode) m_isHovering = false; // 에디트 모드 진입 시 호버링 잔상 강제 차단
    if (m_isHovering) m_hoverOverlay.Draw(m_colorMultiplierLocation);

    m_editObject.DrawEditObject(m_colorMultiplierLocation);   // 에디트 모드일 때, 변환 중인 객체의 면과 외곽선 그리기
    m_editObject.DrawCreateObject(m_colorMultiplierLocation); // 객체 생성 모드일때 그리기

    if (m_uiState->isShowFrustumView) DrawCameraFrustum(camera); // 카메라 절두체 라인 그리기
    DrawDebugRect(hitPoint, 10.0f);

    glDisable(GL_SCISSOR_TEST);           // UI 패널 제외한 영역에만 그리기 설정 해제
    eglSwapBuffers(m_display, m_surface); // 화면에 그려진 결과 출력 (더블 버퍼링에서 백 버퍼와 프론트 버퍼 교체, 실제로는 GPU가 알아서 최적화해서 처리)

    // surface가 아직 윈도우 크기를 못 따라잡았으면 다음 프레임에 다시 그림
    if (surficeWidth != uiSize.clientWidth || surficeHeight != uiSize.clientHeight) ReDraw();
    else { m_needRedraw = false; }
}

// 창 크기 변경
void LayerManager::Resize(int32_t screenWidth, int32_t screenHeight, int32_t panelWidthLeft)
{
    glViewport(panelWidthLeft, 0, screenWidth - panelWidthLeft, screenHeight);
    ReDraw();
}

// 객체 개수 세기
void LayerManager::CountObject(int32_t& totalObjCount, int32_t& renderObjCount, int32_t& fakeObjCount)
{
    for (int32_t layerIndex = 0; layerIndex < m_layers.size(); layerIndex++) {
        if (m_layers[layerIndex] == nullptr || !m_layers[layerIndex]->m_isVisible) continue; // null 체크
        bool isSelectedLayer = (m_hitLayerId == -1 || m_hitLayerId == m_layers[layerIndex]->m_id);
        if (!isSelectedLayer) continue;

        totalObjCount  += static_cast<int32_t>(m_layers[layerIndex]->pointObjects.size()) + static_cast<int32_t>(m_layers[layerIndex]->polyLineObjects.size()) + static_cast<int32_t>(m_layers[layerIndex]->polygonObjects.size());
        renderObjCount += m_layers[layerIndex]->m_renderer->m_currentRenderCount;
        fakeObjCount   += m_uiState->isShowFakeObject ? m_layers[layerIndex]->m_renderer->m_currentRenderFakeCount : 0;
    }
}

// 피킹
glm::dvec3 LayerManager::Picking(glm::dvec3& rayStart, glm::dvec3& rayDir, CRightPanel& rightPanel)
{
    int32_t beforePickingObjectId = m_pickingObjectId; // 이전 피킹 객체
    int32_t beforePickingLayerId  = m_pickingLayerId;  // 이전 피킹 객체가 있는 레이어
    m_pickingObjectId = m_pickingLayerId = -1;

    // 레이와 객체의 충돌 검사, 쿼드트리를 이용한 피킹
    double collisionDistance = std::numeric_limits<double>::max(); // 거리 최대치 설정
    int32_t hitObj = -1;

    for (int32_t i = 0; i < m_layerOrder.size(); i++) {
        int32_t layerIndex = m_layerOrder[i];
        int32_t nodeId     = -1;

        if (layerIndex < 0 || layerIndex >= static_cast<int32_t>(m_layers.size()) || m_layers[layerIndex] == nullptr || !m_layers[layerIndex]->m_isVisible) continue;

        // 현재 가장 가까운 거리보다 더 가까운 거리에서 객체와 접했을 경우 -1 이외의 수 저장
        MeshManager& mesh = *m_layers[layerIndex]->m_renderer->m_mesh;
        hitObj = m_layers[layerIndex]->m_quadTree->SearchPickingData(rayStart, rayDir, 0, collisionDistance, mesh.GetPolygonDrawInfo(), mesh.GetPolygonIndices(), mesh.GetPolygonVertices(), nodeId);
        if (hitObj != -1) {
            m_pickingObjectId = hitObj;
            m_pickingLayerId  = m_layers[layerIndex]->m_id;
            m_pickingNodeId   = nodeId;
            break; // 레이어 우선순위에 따라 가장 가까운 객체를 찾았으므로 루프 종료
        }
    }

    if ((beforePickingLayerId != m_pickingLayerId || beforePickingObjectId != m_pickingObjectId) && m_pickingLayerId != -1 && m_pickingObjectId != -1)
        m_editObject.SetIsEditting(false);

    if (m_pickingObjectId == -1 && beforePickingObjectId != -1) {
        rightPanel.Show(false);
        m_pickingLayerId = m_pickingObjectId = -1;

        if (m_uiState->isEditObjectMode) {
            m_pickingLayerId  = beforePickingLayerId;
            m_pickingObjectId = beforePickingObjectId;
        }
        return glm::dvec3();
    }

    Layer* pickedLayer = GetLayerById(m_pickingLayerId);
    if (!pickedLayer || (pickedLayer->polygonObjects.empty() && pickedLayer->polyLineObjects.empty() && pickedLayer->pointObjects.empty()))
        return glm::dvec3();

    if (m_pickingObjectId != -1 && !m_uiState->isCreateObjectMode && beforePickingObjectId != m_pickingObjectId)
        rightPanel.SetPickingInfo(pickedLayer->m_dbfTable.PrintAttribute(m_pickingObjectId));

    return rayStart + rayDir * collisionDistance;
}

// 객체 색상 지정
void LayerManager::ApplyObjectColorWithLevel()
{
    for (int32_t layerIndex = 0; layerIndex < static_cast<int32_t>(m_layers.size()); layerIndex++) {
        if (m_layers[layerIndex] == nullptr || !m_layers[layerIndex]->m_isVisible) continue;
        bool isSelectedLayer = (m_hitLayerId == -1 || m_hitLayerId == m_layers[layerIndex]->m_id);
        m_layers[layerIndex]->m_renderer->m_mesh->ApplyLevelColors(m_uiState->isShowLevelColor && isSelectedLayer);
    }
}

// 카메라 절두체 시각화 (지면과 교차하는 4개 변), NDC 모서리 4개를 unproject해서 z=0 평면과의 교차점을 구해 라인으로 표시
void LayerManager::DrawCameraFrustum(CameraManager& camera)
{
    if (!m_drawedFrustum) {
        m_frustumLineVertices.clear();
        m_frustumLineVertices.reserve(16);

        glm::dmat4 inverseViewProjectionMatrix = glm::inverse(camera.GetViewProjectionMatrix());
        glm::vec2 ndcCorners[4] = { {-1.0f, -1.0f}, { 1.0f, -1.0f}, { 1.0f,  1.0f}, {-1.0f,  1.0f} };
        glm::dvec3 hitPoints[4];

        for (int32_t dataId = 0; dataId < 4; ++dataId) {
            glm::vec4 nearPoint = inverseViewProjectionMatrix * glm::vec4(ndcCorners[dataId], -1.0f, 1.0f);
            glm::vec4 farPoint = inverseViewProjectionMatrix * glm::vec4(ndcCorners[dataId], 1.0f, 1.0f);
            if (nearPoint.w != 0.0f) nearPoint /= nearPoint.w;
            if (farPoint.w != 0.0f) farPoint /= farPoint.w;

            glm::dvec3 rayOrigin = glm::dvec3(nearPoint);
            glm::dvec3 rayDir = glm::dvec3(farPoint) - glm::dvec3(nearPoint);

            if (std::abs(rayDir.z) > 1e-6) {
                double t = -rayOrigin.z / rayDir.z;
                if (t >= 0.0 && t <= 1.0) {
                    hitPoints[dataId].x = static_cast<float>(rayOrigin.x + t * rayDir.x);
                    hitPoints[dataId].y = static_cast<float>(rayOrigin.y + t * rayDir.y);
                }
                else hitPoints[dataId] = glm::dvec3(farPoint);
            }
            else hitPoints[dataId] = glm::dvec3(farPoint);
        }

        // 지면과 교차하는 실제 절두체 라인 조립
        glm::dvec3 cameraPosition = camera.transform.position;
        for (int32_t dataId = 0; dataId < 4; ++dataId) {
            int32_t next = (dataId + 1) % 4;
            m_frustumLineVertices.push_back({ static_cast<float>(hitPoints[dataId].x), static_cast<float>(hitPoints[dataId].y), 0.0f, 0, 0, 0, 255 });
            m_frustumLineVertices.push_back({ static_cast<float>(hitPoints[next].x),   static_cast<float>(hitPoints[next].y),   0.0f, 0, 0, 0, 255 });

            m_frustumLineVertices.push_back({ static_cast<float>(hitPoints[dataId].x), static_cast<float>(hitPoints[dataId].y), 0.0f, 25, 25, 100, 255 });
            m_frustumLineVertices.push_back({ static_cast<float>(cameraPosition.x),    static_cast<float>(cameraPosition.y),    static_cast<float>(cameraPosition.z), 25, 25, 100, 255 });
        }
        m_drawedFrustum = true;
        m_frustumBuffer.Upload(m_frustumLineVertices, GL_STATIC_DRAW); // 스냅샷 시점에만 업로드
    }

    // GPU 업로드 및 렌더링
    m_frustumBuffer.Draw(GL_LINES); // 매 프레임엔 그리기만
}

// 클릭한 지점 사각형으로 표시
void LayerManager::DrawDebugRect(const glm::dvec3& center, float size)
{
    float halfSize = size * 0.5f;
    std::vector<Vertex> vertices(4);

    vertices[0] = { static_cast<float>(center.x - halfSize), static_cast<float>(center.y - halfSize), static_cast<float>(center.z) + 1.0f, 255,0,0,255 };
    vertices[1] = { static_cast<float>(center.x + halfSize), static_cast<float>(center.y - halfSize), static_cast<float>(center.z) + 1.0f, 255,0,0,255 };
    vertices[2] = { static_cast<float>(center.x + halfSize), static_cast<float>(center.y + halfSize), static_cast<float>(center.z) + 1.0f, 255,0,0,255 };
    vertices[3] = { static_cast<float>(center.x - halfSize), static_cast<float>(center.y + halfSize), static_cast<float>(center.z) + 1.0f, 255,0,0,255 };

    m_debugRectBuffer.Upload(vertices, GL_DYNAMIC_DRAW); // 매 프레임 위치 바뀌므로 DYNAMIC 유지
    m_debugRectBuffer.Draw(GL_LINE_LOOP);
    //DrawDebugPrimitives(vertices, GL_LINE_LOOP);
}

void LayerManager::ReOrderLayer(std::vector<LayerItemData>& items)
{
    m_layerOrder.clear();

    for (int32_t order = 0; order < static_cast<int32_t>(items.size()); order++) {
        std::unordered_map<int32_t, int32_t>::const_iterator mapIt = m_layerIdToIndex.find(items[order].layerId);
        if (mapIt != m_layerIdToIndex.end())
            m_layerOrder.push_back(mapIt->second);
    }
}


void LayerManager::SetHoverObject()
{
    if (m_uiState->isCreateObjectMode) return;

    Layer* layer = GetLayerById(m_pickingLayerId); // 인덱스 대신 아이디 조회
    if (!layer || m_pickingObjectId == -1) {
        m_isHovering = false;
        return;
    }

    m_isHovering = true;

    MeshManager* mesh = layer->m_renderer->m_mesh.get();
    DrawInfo polyInfo = mesh->m_polygonDrawInfos[m_pickingObjectId];
    DrawInfo lineInfo = mesh->m_lineDrawInfos[m_pickingObjectId];

    m_hoverVertices.assign(mesh->m_polygonVertices.begin() + polyInfo.vertexOffset,
        mesh->m_polygonVertices.begin() + polyInfo.vertexOffset + polyInfo.vertexCount);

    for (Vertex& vertex : m_hoverVertices) {
        vertex.color.red = 100; vertex.color.green = 240; vertex.color.blue = 120; vertex.color.alpha = 100;
        vertex.z += 0.1f;
    }

    m_hoverPolygonIndices.clear();
    for (uint32_t i = 0; i < polyInfo.indexCount; i++)
        m_hoverPolygonIndices.push_back(mesh->m_polygonIndices[polyInfo.indexOffset + i] - polyInfo.vertexOffset);

    m_hoverLineIndices.clear();
    for (uint32_t i = 0; i < lineInfo.indexCount; i++)
        m_hoverLineIndices.push_back(mesh->m_lineIndices[lineInfo.indexOffset + i] - polyInfo.vertexOffset);

    m_hoverOverlay.Upload(m_hoverVertices, m_hoverPolygonIndices, m_hoverLineIndices, GL_DYNAMIC_DRAW);
}

// 레이어 저장
bool LayerManager::SaveLayer(int32_t layerId)
{
    Layer* layer = GetLayerById(layerId);
    if (!layer) return false;
    if (!m_shpWriter.WriteLayer(*layer)) return false;
    layer->m_isDirty = false;
    return true;
}

// 수정된(저장해야 하는) 레이어 아이디 반환
std::vector<int32_t> LayerManager::GetDirtyLayerIds(bool onlySelectedOrVisible) const
{
    std::vector<int32_t> result;
    for (const auto& layer : m_layers) {
        if (!layer || !layer->m_isDirty) continue;
        if (onlySelectedOrVisible) {
            bool matches = (m_hitLayerId != -1) ? (layer->m_id == m_hitLayerId) : layer->m_isVisible;
            if (!matches) continue;
        }
        result.push_back(layer->m_id);
    }
    return result;
}