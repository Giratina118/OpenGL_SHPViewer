#include <pch.h>
#include <Layer.h>
#include <random>
#include "UI/CPanelRight.h"
#include "UI/CLayerListCtrl.h"

// 시간 측정을 위한 매크로 준비
#define PROFILE_START(name) auto start_##name = std::chrono::high_resolution_clock::now();
#define PROFILE_END(name) \
    auto end_##name = std::chrono::high_resolution_clock::now(); \
    auto time_##name = std::chrono::duration_cast<std::chrono::microseconds>(end_##name - start_##name).count(); \
    std::string msg_##name = std::string(#name) + " 소요 시간: " + std::to_string(time_##name / 1000.0) + " ms\n"; \
    OutputDebugStringA(msg_##name.c_str());


// 레이어 생성
Layer& LayerManager::CreateLayer(std::string name, uint32_t shpType, BoundingBox& layerBox)
{
    layers.emplace_back(std::make_unique<Layer>());
    m_layerIdToIndex[m_nextLayerId] = static_cast<int32_t>(layers.size() - 1);

    Layer& newLayer        = *layers.back();
	newLayer.m_id          = m_nextLayerId++;
    newLayer.m_name        = name;
    newLayer.m_shapeType   = shpType;
    newLayer.m_quadTree    = std::make_unique<QuadTree>(newLayer);
    newLayer.m_boundingBox = layerBox;
    newLayer.m_isVisible   = true;
	if (layers.size() == 1) newLayer.m_isBuilding = true; // 첫 번째 레이어(건물 정보)일 시 표시, 높이값 적용을 위해

    static std::random_device rand;
    static std::mt19937 gen(rand());
    std::uniform_int_distribution<int> dis(64, 255);

	newLayer.m_baseColor.red   = static_cast<unsigned char>(dis(gen));
	newLayer.m_baseColor.green = static_cast<unsigned char>(dis(gen));
	newLayer.m_baseColor.blue  = static_cast<unsigned char>(dis(gen));
    newLayer.m_baseColor.alpha = 255;

    return newLayer;
}

// 레이어 삭제
void LayerManager::DeleteLayer(int32_t layerId)
{
    int32_t deleteLayerIndex = m_layerIdToIndex[layerId]; // 삭제할 레이어 인덱스 번호
    layers.erase(layers.begin() + deleteLayerIndex);
    m_layerIdToIndex.erase(layerId);

    for (int32_t i = deleteLayerIndex; i < layers.size(); ++i)
        m_layerIdToIndex[layers[i]->m_id] = i;

    ReDraw();
}

// 렌더 초기화, 레이어별 Renderer 생성
bool LayerManager::InitRenderer(HWND hWnd, UIState* uiState)
{
    m_uiState = uiState;
    if (!InitEGL(hWnd)) return false;
    for (std::unique_ptr<Layer>& layer : layers) layer->m_renderer = std::make_unique<Renderer>(hWnd, *layer, *layer->m_quadTree);

    // 셰이더 설정
    if (!m_shader.CreateProgram("Resource/Shader/shader.vert", "Resource/Shader/shader.frag")) return false;
    m_viewProjectionLocation  = glGetUniformLocation(m_shader.GetProgram(), "u_viewProjection");
    m_colorMultiplierLocation = glGetUniformLocation(m_shader.GetProgram(), "u_colorMultiplier");

    // 깊이 테스트 활성화 (3D 건물, 면/라인 z-fighting 해결에 필요)
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_POLYGON_OFFSET_FILL); // 면 위에 라인을 그릴 때 z-fighting 방지, 면을 살짝 뒤로 밀어서 라인이 면 위에 보이게 함
    glPolygonOffset(1.0f, 1.0f);


    glGenVertexArrays(1, &m_debugVAO);			// vao 생성(id를 1개 생성해서 변수에 담기)
    glGenBuffers(1, &m_debugVBO);				// vbo id 1개 생성
    glBindVertexArray(m_debugVAO);				// 조작할 vao 등록
    glBindBuffer(GL_ARRAY_BUFFER, m_debugVBO);	// 조작할 vbo 등록

    // location 0: position (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); // GPU에게 데이터 구조(Vertex) 전달
    glEnableVertexAttribArray(0);		// 방금 세팅한 슬롯(0번)을 활성화

    // location 1: color (vec4)
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0); // vao 등록 해제





    glGenVertexArrays(1, &m_transformVAO);
    glGenBuffers(1, &m_transformVBO);
    glGenBuffers(1, &m_transformIBO);              // 면 IBO
    glGenBuffers(1, &m_transformLineIBO);          // 선 IBO

    glBindVertexArray(m_transformVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_transformVBO);

    // location 0: position (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    // location 1: color (vec4)
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);



    glGenVertexArrays(1, &m_hoverVAO);
    glGenBuffers(1, &m_hoverVBO);
    glGenBuffers(1, &m_hoverIBO);
    glGenBuffers(1, &m_hoverLineIBO);

    glBindVertexArray(m_hoverVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_hoverVBO);

    // location 0: position (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    // location 1: color (vec4)
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);


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

    // 조건에 만는 설정 선택
    if (!eglChooseConfig(m_display, configAttributes, &eglConfig, 1, &configCount)) return false;

    // 그림 그릴 표면(Surface) 생성
    m_surface = eglCreateWindowSurface(m_display, eglConfig, hwnd, nullptr);
    if (m_surface == EGL_NO_SURFACE) return false;

    // 작업 환경(Context) 생성 - OpenGLES 3.x Context 생성
    const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    m_context = eglCreateContext(m_display, eglConfig, EGL_NO_CONTEXT, contextAttributes);
    if (m_context == EGL_NO_CONTEXT) return false;

    // 현재 스레드와 context 연결, 지금부터 이 GPU 작업환경을 사용하겠다 선언
    if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context)) return false;

    return true;
}

// 메모리 해제, EGL 종료
void LayerManager::Shutdown(HWND hWnd)
{
    for (std::unique_ptr<Layer>& layer : layers)
        layer->m_renderer->m_mesh->Shutdown(hWnd);

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
        ReleaseDC(hWnd, m_deviceContext);
        m_deviceContext = nullptr;
    }
}

// 메인 렌더 함수
void LayerManager::Render(CameraController& camera, UISize& uiSize, glm::dvec3 hitPoint)
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
    glScissor(uiSize.panelWidth, 0, uiSize.clientWidth - uiSize.panelWidth, uiSize.clientHeight);
    glClearColor(0.8f, 0.8f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 레이어 별 렌더링
    /*
    for (int32_t layerId = 0; layerId < layers.size(); layerId++) {
        if (layers[layerId] == nullptr || !layers[layerId]->m_isVisible) continue;
        bool isSelectedLayer = (m_hitLayerId == -1 || m_hitLayerId == layers[layerId]->m_id) ? true : false;

        Renderer* renderer = layers[layerId]->m_renderer.get();
        if (renderer != nullptr && !(!m_uiState->isShowBuilding && layers[layerId]->m_isBuilding)) {
            renderer->Render(camera, *m_uiState, uiSize, isSelectedLayer);
			glClear(GL_DEPTH_BUFFER_BIT); // 레이어 간의 우선순위 확실히, z-fighting 방지, 레이어별로 깊이 버퍼 초기화
        }
    }
    */
    // 만약 프로그램 초기화 직후라 m_layerOrder가 비어있다면, 기본 layers 배열의 순서대로 채워줌 (렌더링이 아예 안되는 현상 방지)
    if (m_layerOrder.empty() && !layers.empty()) 
        for (int32_t i = 0; i < static_cast<int32_t>(layers.size()); i++)
            if (layers[i] != nullptr) m_layerOrder.push_back(i); // 여기서 i는 고유ID가 아니라 인덱스라면 i를 넣음


    // 레이어 순회 렌더링
    for (int32_t i = m_layerOrder.size() - 1; i >= 0; i--) {
        // 안전장치: index가 범위를 벗어났거나 유효하지 않은 데이터면 무시 (크래시 및 꼬임 방지)
        int32_t layerIndex = m_layerOrder[i];
        if (layerIndex < 0 || layerIndex >= static_cast<int32_t>(layers.size()) || layers[layerIndex] == nullptr)
            continue;

        if (!layers[layerIndex]->m_isVisible)
            continue;

        bool isSelectedLayer = (m_hitLayerId == -1 || m_hitLayerId == layers[layerIndex]->m_id);
		bool hasPickingData  = (m_pickingLayerId == layerIndex && m_pickingDataId != -1);
        Renderer* renderer   = layers[layerIndex]->m_renderer.get();

        if (renderer != nullptr && !(!m_uiState->isShowBuilding && layers[layerIndex]->m_isBuilding)) {
            renderer->Render(camera, *m_uiState, uiSize, isSelectedLayer, hasPickingData, m_pickingDataId);
            glClear(GL_DEPTH_BUFFER_BIT);
        }
    }


    if (m_isHovering) {
        glBindVertexArray(m_hoverVAO);

        // 면 그리기
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_hoverIBO);
        glDrawElements(GL_TRIANGLES, (GLsizei)m_hoverPolygonIndices.size(), GL_UNSIGNED_INT, nullptr);

        // 외곽선 그리기
        if (!m_hoverLineIndices.empty()) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_hoverLineIBO);
            glLineWidth(3.0f); // Hover 시 선 두께 (원하는 대로 조절)
            glDrawElements(GL_LINES, (GLsizei)m_hoverLineIndices.size(), GL_UNSIGNED_INT, nullptr);
            glLineWidth(1.0f);
        }

        glBindVertexArray(0);
    }

	// 에디트 모드일 때, 변환 중인 객체의 면과 외곽선 그리기
    if (m_uiState->isEditObjectMode) {
        glBindVertexArray(m_transformVAO);

        // 1. 면(Polygon) 그리기
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_transformIBO);
        glDrawElements(GL_TRIANGLES, (GLsizei)m_transformPolygonIndices.size(), GL_UNSIGNED_INT, nullptr);

        // 2. 외곽선(Line) 그리기
        if (m_transformLineInfo.indexCount > 0) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_transformLineIBO); // 선 IBO로 교체
            glLineWidth(20.0f);
            glDrawElements(GL_LINES, (GLsizei)m_transformLineIndices.size(), GL_UNSIGNED_INT, nullptr); // GL_LINES 사용
            glLineWidth(1.0f);
        }

        glBindVertexArray(0);
    }

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
    for (int32_t layerId = 0; layerId < layers.size(); layerId++) {
        bool isSelectedLayer = (m_hitLayerId == -1 || m_hitLayerId == layers[layerId]->m_id) ? true : false;
        if (layers[layerId] == nullptr || !layers[layerId]->m_isVisible || !isSelectedLayer) continue;

        totalObjCount  += static_cast<int32_t>(layers[layerId]->pointObjects.size()) + static_cast<int32_t>(layers[layerId]->polyLineObjects.size()) + static_cast<int32_t>(layers[layerId]->polygonObjects.size());
        renderObjCount += layers[layerId]->m_renderer->m_currentRenderCount;
        fakeObjCount   += m_uiState->isShowFakeObject ? layers[layerId]->m_renderer->m_currentRenderFakeCount : 0;
    }
}

// 피킹
void LayerManager::Picking(glm::dvec3& rayStart, glm::dvec3& rayDir, CRightPanel& rightPanel)
{
    int32_t beforePickingDataId  = m_pickingDataId;  // 이전 피킹 데이터
    int32_t beforePickingLayerId = m_pickingLayerId; // 이전 피킹 데이터가 있는 레이어
    m_pickingDataId = m_pickingLayerId = -1;

    // 레이와 객체의 충돌 검사, 쿼드트리를 이용한 피킹
    double collisionDistance = std::numeric_limits<double>::max(); // 거리 최대치 설정
    int32_t hitObj = -1;


    for (int32_t i = 0; i < m_layerOrder.size(); i++) {
        int32_t layerId = m_layerOrder[i];

        if (layers[layerId] == nullptr || !layers[layerId]->m_isVisible) continue;
        
        // 현재 가장 가까운 거리보다 더 가까운 거리에서 객체와 접했을 경우 -1 이외의 수 저장
        hitObj = layers[layerId]->m_quadTree->SearchPickingData(rayStart, rayDir, 0, collisionDistance, layers[layerId]->m_renderer->m_mesh->GetPolygonDrawInfo(), layers[layerId]->m_renderer->m_mesh->GetPolygonIndices(), layers[layerId]->m_renderer->m_mesh->GetPolygonVertices());
        if (hitObj != -1) { 
            m_pickingDataId  = hitObj;
            m_pickingLayerId = layerId;
			break; // 레이어 우선순위에 따라 가장 가까운 객체를 찾았으므로 루프 종료
        }
    }

    bool isSelectedLayer = (m_hitLayerId == -1 || m_hitLayerId == beforePickingLayerId);

    if ((beforePickingLayerId != m_pickingLayerId || beforePickingDataId != m_pickingDataId) && m_pickingLayerId != -1 && m_pickingDataId != -1)
        m_isEdittingObject = false;

    // 객체가 없는 빈 공간 선택 또는 이전과 같은 객체 선택 시 색 복구
    if (m_pickingDataId == -1 && beforePickingDataId != -1/* || m_pickingDataId != -1 && beforePickingDataId == m_pickingDataId && beforePickingLayerId == m_pickingLayerId*/) {
        //layers[beforePickingLayerId]->m_renderer->RestoreObjectColor(beforePickingDataId, *m_uiState, isSelectedLayer);
        rightPanel.Show(false);
        m_pickingLayerId = m_pickingDataId = -1;

        if (m_uiState->isEditObjectMode) {
            m_pickingLayerId = beforePickingLayerId;
			m_pickingDataId  = beforePickingDataId;
        }

        return; 
    }

    // 피킹된 객체 정보 얻기
    if (m_pickingLayerId == -1 || layers[m_pickingLayerId]->polygonObjects.size() < 1 && layers[m_pickingLayerId]->polyLineObjects.size() < 1 && layers[m_pickingLayerId]->pointObjects.size() < 1) return;

    // 새로운 객체 선택
    if (m_pickingDataId != -1) {
        //if (beforePickingLayerId != -1) layers[beforePickingLayerId]->m_renderer->RestoreObjectColor(beforePickingDataId, *m_uiState, isSelectedLayer);
        //layers[m_pickingLayerId]->m_renderer->HighlightObjectColor(m_pickingDataId);
        rightPanel.SetPickingInfo(layers[m_pickingLayerId]->m_dbfTable.PrintAttribute(m_pickingDataId)); // 선택 객체 dbf 정보 출력
    }
}

// 객체 색상 지정
void LayerManager::ApplyObjectColorWithLevel()
{
    for (int32_t layerId = 0; layerId < layers.size(); layerId++) {
        bool isSelectedLayer = (m_hitLayerId == -1 || m_hitLayerId == layers[layerId]->m_id) ? true : false;
        if (layers[layerId] == nullptr || !layers[layerId]->m_isVisible) continue;
		layers[layerId]->m_renderer->m_mesh->ApplyLevelColors(m_uiState->isShowLevelColor && isSelectedLayer);
    }
}

// 선택한 객체를 편집 대상으로 설정, 복사본을 만들어서 이동/회전 시 원본 데이터에 바로 적용하지 않고, 복사본을 변환 후 렌더링
void LayerManager::SetEditObject()
{
    if (m_isEdittingObject || m_pickingLayerId == -1 || m_pickingDataId == -1) return;
    m_isEdittingObject = true;

    MeshManager* mesh = layers[m_pickingLayerId]->m_renderer->m_mesh.get();
    m_transformPolygonInfo = mesh->m_polygonDrawInfos[m_pickingDataId];
    m_transformLineInfo = mesh->m_lineDrawInfos[m_pickingDataId];

    // 1. 원본 정점 복사 (하나의 정점 배열만 사용)
    m_editOriginalVertices.assign(mesh->m_polygonVertices.begin() + m_transformPolygonInfo.vertexOffset,
        mesh->m_polygonVertices.begin() + m_transformPolygonInfo.vertexOffset + m_transformPolygonInfo.vertexCount);
    m_transformVertices = m_editOriginalVertices;

    for (Vertex& vertex : m_transformVertices) {
        vertex.color.red   = 20;
        vertex.color.green = 230;
        vertex.color.blue  = 50;
        vertex.color.alpha = 100;
	}

    // 2-1. 면 인덱스 리매핑
    m_transformPolygonIndices.clear();
    for (uint32_t i = 0; i < m_transformPolygonInfo.indexCount; i++) {
        uint32_t originalIndex = mesh->m_polygonIndices[m_transformPolygonInfo.indexOffset + i];
        m_transformPolygonIndices.push_back(originalIndex - m_transformPolygonInfo.vertexOffset);
    }

    // 2-2. 선 인덱스 리매핑 (선 오프셋 기준)
    m_transformLineIndices.clear();
    for (uint32_t i = 0; i < m_transformLineInfo.indexCount; i++) {
        uint32_t originalIndex = mesh->m_lineIndices[m_transformLineInfo.indexOffset + i];
        m_transformLineIndices.push_back(originalIndex - m_transformPolygonInfo.vertexOffset);
    }

    // 3. 중심점(Centroid) 계산
    m_editCenter = glm::dvec3(0.0);
    for (const Vertex& vertex : m_editOriginalVertices)
        m_editCenter += glm::dvec3(vertex.x, vertex.y, vertex.z);
    m_editCenter /= static_cast<double>(m_editOriginalVertices.size());

    // 4. Transform 초기화
    m_editTransform = Transform();

    // 5. GPU 업로드
    glBindVertexArray(m_transformVAO);

    // VBO 업로드 (정점 하나만 관리)
    glBindBuffer(GL_ARRAY_BUFFER, m_transformVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * m_transformVertices.size(), m_transformVertices.data(), GL_DYNAMIC_DRAW);

    // 면 IBO 업로드
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_transformIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m_transformPolygonIndices.size(), m_transformPolygonIndices.data(), GL_DYNAMIC_DRAW);

    // 선 IBO 업로드
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_transformLineIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m_transformLineIndices.size(), m_transformLineIndices.data(), GL_DYNAMIC_DRAW);

    glBindVertexArray(0);
}

// 객체 편집 업데이트
void LayerManager::UpdateEditObject()
{
    glm::dmat4 transformMatrix = m_editTransform.GetMatrix();

    glm::dmat4 finalMatrix = glm::dmat4(1.0);
    finalMatrix = glm::translate(finalMatrix, m_editCenter);
    finalMatrix = finalMatrix * transformMatrix;
    finalMatrix = glm::translate(finalMatrix, -m_editCenter);

    // 1. 면(Polygon) 정점 변환 갱신
    for (size_t i = 0; i < m_editOriginalVertices.size(); i++) {
        const Vertex& origin = m_editOriginalVertices[i];
        glm::dvec4 pos(origin.x, origin.y, origin.z, 1.0);
        glm::dvec4 transformedPos = finalMatrix * pos;

        m_transformVertices[i].x = static_cast<float>(transformedPos.x);
        m_transformVertices[i].y = static_cast<float>(transformedPos.y);
        m_transformVertices[i].z = static_cast<float>(transformedPos.z);
    }

    // 2. 라인(Line) 정점도 동일하게 변환 갱신
    for (size_t i = 0; i < m_editOriginalLineVertices.size(); i++) {
        const Vertex& origin = m_editOriginalLineVertices[i];
        glm::dvec4 pos(origin.x, origin.y, origin.z, 1.0);
        glm::dvec4 transformedPos = finalMatrix * pos;

        m_transformLineVertices[i].x = static_cast<float>(transformedPos.x);
        m_transformLineVertices[i].y = static_cast<float>(transformedPos.y);
        m_transformLineVertices[i].z = static_cast<float>(transformedPos.z);
        m_transformLineVertices[i].color *= 0.6f;
    }

    // 3. 면 VBO 갱신
    glBindBuffer(GL_ARRAY_BUFFER, m_transformVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * m_transformVertices.size(), m_transformVertices.data());

    // 4. 라인 VBO 갱신
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * m_transformLineVertices.size(), m_transformLineVertices.data());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// 객체 편집(이동)
void LayerManager::MoveObject(glm::dvec3& moveDelta)
{
    if (m_transformPolygonInfo.vertexCount == 0) return;
    m_editTransform.MoveWorld(moveDelta); 
    UpdateEditObject(); // 버퍼 갱신
}

// 객체 편집(회전)
void LayerManager::RotateObject(glm::dvec3& rotateDelta)
{
    if (m_transformPolygonInfo.vertexCount == 0) return;

    double yaw   = rotateDelta.x * 0.5;
    double pitch = rotateDelta.y * 0.5;
    double roll  = rotateDelta.z * 0.5;

    m_editTransform.RotateWorld(yaw, pitch, roll); // (yaw, pitch, roll)
    UpdateEditObject(); // 버퍼 갱신
}

// 객체 편집(스케일)
void LayerManager::ScaleObject(glm::dvec3& scaleDelta)
{
    if (m_transformPolygonInfo.vertexCount == 0) return;
	m_editTransform.ScaleWorld(scaleDelta * 0.001);
    UpdateEditObject(); // 버퍼 갱신
}

void LayerManager::SaveEditObject()
{
    m_isEdittingObject = false;
    m_uiState->isEditObjectMode = false;



}

void LayerManager::CancelEditObject()
{
    m_isEdittingObject = false;
    m_uiState->isEditObjectMode = false;

}

// 카메라 절두체 시각화 (지면과 교차하는 4개 변), NDC 모서리 4개를 unproject해서 z=0 평면과의 교차점을 구해 라인으로 표시
void LayerManager::DrawCameraFrustum(CameraController& camera)
{
    if (!m_drawedFrustum) {
        m_frustumLineVertices.clear();
        m_frustumLineVertices.reserve(16);

        glm::dmat4 inverseViewProjectionMatrix = glm::inverse(camera.GetViewProjectionMatrix());
        glm::vec2 ndcCorners[4] = { {-1.0f, -1.0f}, { 1.0f, -1.0f}, { 1.0f,  1.0f}, {-1.0f,  1.0f} };
        glm::dvec3 hitPoints[4];

        for (int32_t dataId = 0; dataId < 4; ++dataId) {
            glm::vec4 nearPoint = inverseViewProjectionMatrix * glm::vec4(ndcCorners[dataId], -1.0f, 1.0f);
            glm::vec4 farPoint  = inverseViewProjectionMatrix * glm::vec4(ndcCorners[dataId], 1.0f, 1.0f);
            if (nearPoint.w != 0.0f) nearPoint /= nearPoint.w;
            if (farPoint.w  != 0.0f) farPoint  /= farPoint.w;

            glm::dvec3 rayOrigin = glm::dvec3(nearPoint);
            glm::dvec3 rayDir    = glm::dvec3(farPoint) - glm::dvec3(nearPoint);

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
    }

    // GPU 업로드 및 렌더링
    DrawDebugPrimitives(m_frustumLineVertices, GL_LINES);
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

    DrawDebugPrimitives(vertices, GL_LINE_LOOP);
}

// 업로드 & 그리기 (객체 mbr, 노드 mbr, 절두체 사각형 등)
void LayerManager::DrawDebugPrimitives(const std::vector<Vertex>& vertices, GLenum drawMode)
{
    if (vertices.empty()) return;

    glBindVertexArray(m_debugVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(drawMode, 0, static_cast<GLsizei>(vertices.size())); // 그리기
    glBindVertexArray(0);
}

void LayerManager::ReOrderLayer(std::vector<LayerItemData>& items)
{
    m_layerOrder.clear();
    for (int32_t order = 0; order < static_cast<int32_t>(items.size()); order++)
        m_layerOrder.push_back(items[order].layerId);
}

// LayerManager.cpp
void LayerManager::SetHoverObject()
{
    // 피킹된 객체가 없으면 Hover 상태 해제
    if (m_pickingLayerId == -1 || m_pickingDataId == -1) {
        m_isHovering = false;
        return;
    }

    // 편집 중인 객체에 마우스가 올라간 경우 Hover를 무시하고 싶다면 아래 주석 해제
    // if (m_uiState->isEditObjectMode && m_isEdittingObject) return;

    m_isHovering = true;

    MeshManager* mesh = layers[m_pickingLayerId]->m_renderer->m_mesh.get();
    DrawInfo polyInfo = mesh->m_polygonDrawInfos[m_pickingDataId];
    DrawInfo lineInfo = mesh->m_lineDrawInfos[m_pickingDataId];

    // 1. 원본 정점 복사 및 색상 변경 (초록색)
    m_hoverVertices.assign(mesh->m_polygonVertices.begin() + polyInfo.vertexOffset,
        mesh->m_polygonVertices.begin() + polyInfo.vertexOffset + polyInfo.vertexCount);

    for (Vertex& vertex : m_hoverVertices) {
        vertex.color.red   = 20;
        vertex.color.green = 230;
        vertex.color.blue  = 50;
        vertex.color.alpha = 100;

        // Z축 값을 아주 살짝 높여주면 원본 메시와 겹칠 때 발생하는 Z-Fighting 현상을 방지할 수 있습니다.
        vertex.z += 0.1f;
    }

    // 2. 인덱스 리매핑
    m_hoverPolygonIndices.clear();
    for (uint32_t i = 0; i < polyInfo.indexCount; i++) {
        uint32_t originalIndex = mesh->m_polygonIndices[polyInfo.indexOffset + i];
        m_hoverPolygonIndices.push_back(originalIndex - polyInfo.vertexOffset);
    }

    m_hoverLineIndices.clear();
    for (uint32_t i = 0; i < lineInfo.indexCount; i++) {
        uint32_t originalIndex = mesh->m_lineIndices[lineInfo.indexOffset + i];
        m_hoverLineIndices.push_back(originalIndex - polyInfo.vertexOffset);
    }

    // 3. GPU 업로드
    glBindVertexArray(m_hoverVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_hoverVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * m_hoverVertices.size(), m_hoverVertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_hoverIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m_hoverPolygonIndices.size(), m_hoverPolygonIndices.data(), GL_DYNAMIC_DRAW);

    if (m_hoverLineIndices.size() > 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_hoverLineIBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m_hoverLineIndices.size(), m_hoverLineIndices.data(), GL_DYNAMIC_DRAW);
    }

    glBindVertexArray(0);
}