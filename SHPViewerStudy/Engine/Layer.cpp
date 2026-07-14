#include <pch.h>
#include <Layer.h>

// 레이어 생성
Layer& LayerManager::CreateLayer(std::string name, uint32_t shpType, BoundingBox& layerBox)
{
    layers.emplace_back(std::make_unique<Layer>());
    m_layerIdToIndex[m_nextLayerId] = static_cast<int32_t>(layers.size() - 1);

    Layer& newLayer        = *layers.back();
	newLayer.m_id          = m_nextLayerId++;
    newLayer.m_name        = name;
    newLayer.m_shapeType   = shpType;
    newLayer.m_quadTree    = std::make_unique<QuadTree>(newLayer, layerBox.GetMaxExtent());
    newLayer.m_boundingBox = layerBox;
    newLayer.m_isVisible   = true;
	if (layers.size() == 1) newLayer.m_isBuilding = true; // 첫 번째 레이어(건물 정보)일 시 표시, 높이값 적용을 위해

    boundingBox = boundingBox.CombineBox(layerBox);
    //visibleLayers.push_back(static_cast<int32_t>(layers.size()) - 1);
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

    /*
    int32_t lastLayerIndex   = static_cast<int32_t>(layers.size()) - 1;
    std::swap(layers[deleteLayerIndex], layers[lastLayerIndex]);
    layers.pop_back();

    m_layerIdToIndex[m_nextLayerId - 1] = deleteLayerIndex;
    m_layerIdToIndex.erase(layerId);
    */



    ReDraw();
}

// 렌더 초기화, 레이어별 Renderer 생성
bool LayerManager::InitRenderer(HWND hWnd)
{
    if (!InitEGL(hWnd)) return false;
    for (std::unique_ptr<Layer>& layer : layers) layer->m_renderer = std::make_unique<Renderer>(hWnd, *layer, *layer->m_quadTree);
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
        layer->m_renderer->Shutdown(hWnd);

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

void LayerManager::Render(CameraController& camera, UIState& uiState, int32_t screenWidth, int32_t screenHeight, int32_t panelWidthLeft, glm::dvec3 hitPoint)
{
    // render여부 체크 -> 변화 없으면 그냥 return (CPU/GPU idle, 화면은 이전 프레임 유지)
    bool needRebuild = camera.GetCameraChange() || m_needRedraw;
    if (!needRebuild) return;

    // 실제 EGL surface 크기 (윈도우와 다를 수 있음 - 리사이즈 중 한 프레임 지연)
    EGLint surficeWidth = 0, surficeHeight = 0;
    eglQuerySurface(m_display, m_surface, EGL_WIDTH, &surficeWidth);
    eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &surficeHeight);
    if (surficeWidth <= 0 || surficeHeight <= 0) { surficeWidth = screenWidth; surficeHeight = screenHeight; }

    // 전체 회색 클리어
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, screenWidth, screenHeight);
    glClearColor(0.94f, 0.94f, 0.94f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 3D 영역만 viewport + scissor (둘 다 동일 좌표 = surface 기준)
    glViewport(panelWidthLeft, 0, screenWidth - panelWidthLeft, screenHeight);
    glEnable(GL_SCISSOR_TEST);
    glScissor(panelWidthLeft, 0, screenWidth - panelWidthLeft, screenHeight);
    glClearColor(0.8f, 0.8f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 레이어 별 렌더링
    for (int32_t layerId = 0; layerId < layers.size(); layerId++) {
        if (layers[layerId] == nullptr || !layers[layerId]->m_isVisible) continue;

        Renderer* renderer = layers[layerId]->m_renderer.get();
        if (renderer != nullptr && !(!uiState.isShowBuilding && layers[layerId]->m_isBuilding)) {
            //glEnable(GL_POLYGON_OFFSET_FILL);
            //glPolygonOffset(0.5f, 4.0f);
            renderer->Render(camera, uiState, screenWidth, screenHeight, panelWidthLeft, hitPoint);
        }
        else {
            //glEnable(GL_POLYGON_OFFSET_FILL);
            //glPolygonOffset(1.0f, 4.0f);
        }
    }
    //glDisable(GL_POLYGON_OFFSET_FILL);

    glDisable(GL_SCISSOR_TEST);           // UI 패널 제외한 영역에만 그리기 설정 해제
    eglSwapBuffers(m_display, m_surface); // 화면에 그려진 결과 출력 (더블 버퍼링에서 백 버퍼와 프론트 버퍼 교체, 실제로는 GPU가 알아서 최적화해서 처리)

    // surface가 아직 윈도우 크기를 못 따라잡았으면 다음 프레임에 다시 그림
    if (surficeWidth != screenWidth || surficeHeight != screenHeight) ReDraw();
    else { m_needRedraw = false; camera.SetCameraChange(); }
}

void LayerManager::Resize(int32_t screenWidth, int32_t screenHeight, int32_t panelWidthLeft)
{
    glViewport(panelWidthLeft, 0, screenWidth - panelWidthLeft, screenHeight);
    ReDraw();
}

void LayerManager::Refresh()
{
    for (std::unique_ptr<Layer>& layer : layers) {
        layer->m_renderer->BuildMesh();
        layer->m_renderer->RebuildQuadTree();
    }
	ReDraw();
}

void LayerManager::ApplyObjectColorWithLevel(bool useLevelColor)
{
    for (int32_t layerId = 0; layerId < layers.size(); layerId++) {
        if (layers[layerId] == nullptr || !layers[layerId]->m_isVisible) continue;
		layers[layerId]->m_renderer->ApplyLevelColors(useLevelColor);
    }
}

/*
void LayerManager::CleanPoints()
{
    // TODO: 현재 폴리곤만 있음
    // 사용한 정점 정보 정리
    for (PolyObject& polygonObject : polygonObjects)
        polygonObject.ClearData();
}
*/