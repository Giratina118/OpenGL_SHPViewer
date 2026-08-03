// ProjTransformer.h
#pragma once
#include <glm/glm.hpp>
#include <string>

// proj.h 포함 (PROJ 라이브러리)
#include <proj.h>

class Layer;

class ProjTransformer
{
public:
    ProjTransformer() = default;
    ~ProjTransformer() { Shutdown(); }

    // 초기화: 원본 EPSG → 목표 EPSG (예: 5174 → 5186)
    bool Init(int srcEpsg, int dstEpsg);
    void Shutdown();

    // 점 하나 변환 (미터 → 미터)
    glm::dvec2 TransformPoint(glm::dvec2 point);

    // 레이어 전체 변환 + MBR 재계산
    void TransformLayer(Layer& layer);

    bool IsValid() const { return m_ctx != nullptr && m_transform != nullptr; }

private:
    PJ_CONTEXT* m_ctx = nullptr;
    PJ* m_transform = nullptr;
    int         m_srcEpsg = 0;
    int         m_dstEpsg = 0;
};