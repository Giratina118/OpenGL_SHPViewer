// ProjTransformer.cpp
#include "pch.h"
#include "ProjTransformer.h"
#include "Layer.h"

bool ProjTransformer::Init(int srcEpsg, int dstEpsg)
{
    m_srcEpsg = srcEpsg;
    m_dstEpsg = dstEpsg;

    // PROJ 컨텍스트 생성
    m_ctx = proj_context_create();
    if (!m_ctx) {
        OutputDebugString(_T("[PROJ] 컨텍스트 생성 실패\n"));
        return false;
    }

    // PROJ 데이터 경로 설정 (QGIS 내 PROJ 데이터)
    const char* projDataPath = "C:\\Program Files\\QGIS 4.0.1\\share\\proj";
    proj_context_set_search_paths(m_ctx, 1, &projDataPath);

    // PROJ 네트워크 활성화 
    //proj_context_set_enable_network(m_ctx, 1);
    proj_context_set_enable_network(m_ctx, 0);

    // 변환 파이프라인 생성
    CStringA srcStr, dstStr;
    srcStr.Format("EPSG:%d", srcEpsg);
    dstStr.Format("EPSG:%d", dstEpsg);

    m_transform = proj_create_crs_to_crs(m_ctx, srcStr.GetString(), dstStr.GetString(), nullptr); // area of interest: nullptr = 전 지구 

    if (!m_transform) {
        TCHAR buf[256];
        _stprintf_s(buf, _T("[PROJ] 변환 생성 실패: %S\n"), proj_context_errno_string(m_ctx, proj_context_errno(m_ctx)));
        OutputDebugString(buf);
        return false;
    }

    // m_transform 생성 직후, normalize 전에 추가
    PJ* pipeline    = proj_create_crs_to_crs(m_ctx, "EPSG:5178", "EPSG:5186", nullptr);
    PJ* normalized  = proj_normalize_for_visualization(m_ctx,  pipeline);
    const char* str = proj_as_proj_string(m_ctx, normalized, PJ_PROJ_5, nullptr);

    if (str) OutputDebugStringA(str);

    // 축 순서를 항상 (경도, 위도) 또는 (x, y)로 고정
    PJ* normalizedTransform = proj_normalize_for_visualization(m_ctx, m_transform);
    if (normalizedTransform) {
        proj_destroy(m_transform);
        m_transform = normalizedTransform;
    }

    TCHAR buf[256];
    _stprintf_s(buf, _T("[PROJ] 초기화 성공: EPSG:%d → EPSG:%d\n"), srcEpsg, dstEpsg);
    OutputDebugString(buf);
    return true;
}

void ProjTransformer::Shutdown()
{
    if (m_transform) { proj_destroy(m_transform);         m_transform = nullptr; }
    if (m_ctx) { proj_context_destroy(m_ctx);       m_ctx = nullptr; }
}

glm::dvec2 ProjTransformer::TransformPoint(glm::dvec2 point)
{
    if (!IsValid()) return point;

    // PROJ 좌표 구조체 (x=경도or동서, y=위도or남북)
    PJ_COORD coord = proj_coord(point.x, point.y, 0.0, 0.0);
    PJ_COORD result = proj_trans(m_transform, PJ_FWD, coord);

    if (result.xy.x == HUGE_VAL || result.xy.y == HUGE_VAL) {
        TCHAR buf[256];
        _stprintf_s(buf, _T("[PROJ] 변환 실패: (%.3f, %.3f)\n"), point.x, point.y);
        OutputDebugString(buf);
        return point; // 실패 시 원본 반환
    }

    return { result.xy.x, result.xy.y };
}

void ProjTransformer::TransformLayer(Layer& layer)
{
    if (!IsValid()) return;

    double layerMinX = std::numeric_limits<double>::max();
    double layerMinY = std::numeric_limits<double>::max();
    double layerMaxX = std::numeric_limits<double>::lowest();
    double layerMaxY = std::numeric_limits<double>::lowest();

    auto transformPolyObjects = [&](auto& objects) {
        for (auto& obj : objects) {
            double objMinX = std::numeric_limits<double>::max();
            double objMinY = std::numeric_limits<double>::max();
            double objMaxX = std::numeric_limits<double>::lowest();
            double objMaxY = std::numeric_limits<double>::lowest();

            for (auto& point : obj.points) {
                point = TransformPoint(point);
                if (point.x < objMinX) objMinX = point.x;
                if (point.x > objMaxX) objMaxX = point.x;
                if (point.y < objMinY) objMinY = point.y;
                if (point.y > objMaxY) objMaxY = point.y;
            }
            obj.SetMBRBox(objMinX, objMinY, objMaxX, objMaxY);

            if (objMinX < layerMinX) layerMinX = objMinX;
            if (objMaxX > layerMaxX) layerMaxX = objMaxX;
            if (objMinY < layerMinY) layerMinY = objMinY;
            if (objMaxY > layerMaxY) layerMaxY = objMaxY;
        }
    };

    transformPolyObjects(layer.polyLineObjects);
    transformPolyObjects(layer.polygonObjects);
    layer.SetMBRBox(layerMinX, layerMinY, layerMaxX, layerMaxY);

    // 변환 결과 출력
    TCHAR buf[256];
    _stprintf_s(buf, _T("[PROJ] 변환 완료: MBR (%.6f,%.6f)~(%.6f,%.6f)\n"), layerMinX, layerMinY, layerMaxX, layerMaxY);
    OutputDebugString(buf);
}
