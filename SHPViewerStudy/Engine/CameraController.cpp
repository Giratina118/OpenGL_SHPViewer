#include "pch.h"
#include <algorithm>
#include "CameraController.h"

// 카메라 초기화
void CameraController::Init(const BoundingBox& boundingBox, int screenWidth, int screenHeight)
{
    // 카메라 위치 설정 (넣은 파일의 객체가 보이도록 위치 이동)
    double centerX     = (boundingBox.minX + boundingBox.maxX) * 0.5;
    double centerY     = (boundingBox.minY + boundingBox.maxY) * 0.5;
    double maxExtent   = std::max(boundingBox.maxX - boundingBox.minX, boundingBox.maxY - boundingBox.minY);

    transform.position = glm::dvec3(centerX, centerY, maxExtent);
    transform.rotation = glm::angleAxis(glm::radians(0.0), glm::dvec3(1, 0, 0));
    m_thirdMovePos     = transform.position;

    aspect = (screenWidth <= 0 || screenHeight <= 0) ? 1.0 : static_cast<double>(screenWidth) / screenHeight;
    UpdateMatrix();
}

// 종횡비 갱신
void CameraController::UpdateAspect(int screenWidth, int screenHeight)
{
    if (screenWidth <= 0 || screenHeight <= 0) return;

    aspect = static_cast<double>(screenWidth) / screenHeight;
    UpdateMatrix();
}

// 로컬 이동
void CameraController::MoveLocal(double deltaX, double deltaY)
{
    double speed = abs(transform.position.z) * moveSpeedCorrection;
    if (speed < 1.0f) speed = 1.0f;

    transform.MoveLocal(glm::dvec3(-deltaX * speed, deltaY * speed, 0.0f));
    UpdateMatrix();
}

// 월드 이동
void CameraController::MoveWorld(double deltaX, double deltaY)
{
    double speed = abs(transform.position.z) * moveSpeedCorrection;
    if (speed < 1.0f) speed = 1.0f;

    transform.MoveWorld(glm::dvec3(-deltaX * speed, deltaY * speed, 0.0f));
    UpdateMatrix();
}

/*
// 특정 지점 기준 3차원 이동
void CameraController::MoveThird(glm::dvec3& currentPos, glm::dvec3& beforePos)
{
    glm::dvec3 deltaPos = currentPos - beforePos;
    transform.position += deltaPos;
    UpdateMatrix();
}
*/

// 로컬 회전
void CameraController::RotateLocal(double deltaX, double deltaY, double deltaZ)
{
    double yawDelta   = -deltaX * rotateAngleCorrection;
    double pitchDelta = -deltaY * rotateAngleCorrection;
    double rollDelta  = -deltaZ * rotateAngleCorrection;

    // yaw와 pitch를 한번에 적용
    transform.RotateLocal(yawDelta, pitchDelta, rollDelta);
    UpdateMatrix();
}

// 월드 회전
void CameraController::RotateWorld(double deltaX, double deltaY, double deltaZ)
{
    double yawDelta   = -deltaX * rotateAngleCorrection;
    double pitchDelta = -deltaY * rotateAngleCorrection;
    double rollDelta  = -deltaZ * rotateAngleCorrection;

    // yaw와 pitch를 한번에 적용
    transform.RotateWorld(yawDelta, pitchDelta, rollDelta);
    UpdateMatrix();
}

// 월드 회전
void CameraController::RotateFirst(double deltaX, double deltaY, double deltaZ)
{
    double yawDelta   = -deltaX * rotateAngleCorrection;
    double pitchDelta = -deltaY * rotateAngleCorrection;
    double rollDelta  = -deltaZ * rotateAngleCorrection;

    // yaw와 pitch를 한번에 적용
    transform.RotateFirst(yawDelta, pitchDelta, rollDelta);
    UpdateMatrix();
}

// 특정 지점 기준 3차원 회전
void CameraController::RotateThird(double deltaX, double deltaY, double deltaZ, glm::dvec3& point)
{
    glm::dvec3 currentPos = transform.position;
    glm::dvec3 movePos = point - currentPos;

    double yawDelta   = -deltaX * rotateAngleCorrection;
    double pitchDelta = -deltaY * rotateAngleCorrection;
    double rollDelta  = -deltaZ * rotateAngleCorrection;

    // yaw와 pitch를 한번에 적용
    transform.RotateThird(yawDelta, pitchDelta, point);
    UpdateMatrix();
}

// 줌 인 / 줌 아웃
void CameraController::ZoomLocal(double factor)
{
    double speed = abs(transform.position.z) * zoomSpeedCorrection;
    if (speed < 1.0f) speed = 1.0f;

    transform.MoveLocal(glm::dvec3(0.0f, 0.0f, factor * speed));
    UpdateMatrix();
}

// 줌 인 / 줌 아웃
void CameraController::ZoomWorld(double factor)
{
    double speed = abs(transform.position.z) * zoomSpeedCorrection;
    if (speed < 1.0f) speed = 1.0f;

    transform.MoveWorld(glm::dvec3(0.0f, 0.0f, factor * speed));
    UpdateMatrix();
}

// 특정 지점 기준 3차원 줌인/줌아웃
void CameraController::ZoomThird(double factor, glm::dvec3& point)
{
    glm::dvec3 dir = point - transform.position;
    double speed = abs(transform.position.z) * zoomSpeedCorrection;
    if (speed < 1.0f) speed = 1.0f;

    transform.MoveWorld(glm::normalize(dir) * factor * speed);
    UpdateMatrix();
}

// 행렬 갱신
void CameraController::UpdateMatrix()
{
    double safeAspect = (aspect <= 0.0f) ? 1.0f : aspect;

	if (transform.position.z < 1.1) transform.position.z = 1.1; // 카메라가 지면 아래로 내려가지 않도록 제한

    // 뷰행렬
    glm::dvec3 eye    = transform.position;
    glm::dvec3 target = transform.position + transform.GetForward();
    glm::dvec3 up     = transform.GetUp();
    viewMatrix = glm::lookAt(eye, target, up);

	// 투영행렬 (원근 투영)
    double nearPlane  = 1.0;
    double farPlane   = 1000000.0;
    projectionMatrix  = glm::perspective(glm::radians(fov), safeAspect, nearPlane, farPlane);
    
    viewProjectionMatrix = projectionMatrix * viewMatrix;

    ExtractFrustumPlanes(); // 절두체 평면 업데이트

	m_viewBoxReset    = true; // 시야 박스 재계산 필요
    m_isCameraChanged = true; // 카메라 변경 표시
}

// 뷰-프로젝션 행렬에서 6개 평면 추출 (Gribb & Hartmann 방법)
void CameraController::ExtractFrustumPlanes()
{
    const glm::dmat4& vp = viewProjectionMatrix;

    // Left Plane
    m_frustumPlanes[0].normal.x = vp[0][3] + vp[0][0];
    m_frustumPlanes[0].normal.y = vp[1][3] + vp[1][0];
    m_frustumPlanes[0].normal.z = vp[2][3] + vp[2][0];
    m_frustumPlanes[0].distance = vp[3][3] + vp[3][0];

    // Right Plane
    m_frustumPlanes[1].normal.x = vp[0][3] - vp[0][0];
    m_frustumPlanes[1].normal.y = vp[1][3] - vp[1][0];
    m_frustumPlanes[1].normal.z = vp[2][3] - vp[2][0];
    m_frustumPlanes[1].distance = vp[3][3] - vp[3][0];

    // Bottom Plane
    m_frustumPlanes[2].normal.x = vp[0][3] + vp[0][1];
    m_frustumPlanes[2].normal.y = vp[1][3] + vp[1][1];
    m_frustumPlanes[2].normal.z = vp[2][3] + vp[2][1];
    m_frustumPlanes[2].distance = vp[3][3] + vp[3][1];

    // Top Plane
    m_frustumPlanes[3].normal.x = vp[0][3] - vp[0][1];
    m_frustumPlanes[3].normal.y = vp[1][3] - vp[1][1];
    m_frustumPlanes[3].normal.z = vp[2][3] - vp[2][1];
    m_frustumPlanes[3].distance = vp[3][3] - vp[3][1];

    // Near Plane
    m_frustumPlanes[4].normal.x = vp[0][3] + vp[0][2];
    m_frustumPlanes[4].normal.y = vp[1][3] + vp[1][2];
    m_frustumPlanes[4].normal.z = vp[2][3] + vp[2][2];
    m_frustumPlanes[4].distance = vp[3][3] + vp[3][2];

    // Far Plane
    m_frustumPlanes[5].normal.x = vp[0][3] - vp[0][2];
    m_frustumPlanes[5].normal.y = vp[1][3] - vp[1][2];
    m_frustumPlanes[5].normal.z = vp[2][3] - vp[2][2];
    m_frustumPlanes[5].distance = vp[3][3] - vp[3][2];

    // 모든 평면 정규화
    for (int i = 0; i < 6; ++i) {
        m_frustumPlanes[i].Normalize();
    }
}

FrustumState CameraController::GetFrustumState(const BoundingBox& box) const
{
    glm::dvec3 minP(box.minX, box.minY, 0.0f);
    glm::dvec3 maxP(box.maxX, box.maxY, 0.0f);
    bool allInside = true;

    for (int i = 0; i < 6; ++i) {
        // 평면 방향으로 가장 튀어나온 점 (p-vertex)
        glm::dvec3 p = minP;
        if (m_frustumPlanes[i].normal.x >= 0) p.x = maxP.x;
        if (m_frustumPlanes[i].normal.y >= 0) p.y = maxP.y;
        if (m_frustumPlanes[i].normal.z >= 0) p.z = maxP.z;

        // 평면 반대 방향으로 가장 들어간 점 (n-vertex)
        glm::dvec3 n = maxP;
        if (m_frustumPlanes[i].normal.x >= 0) n.x = minP.x;
        if (m_frustumPlanes[i].normal.y >= 0) n.y = minP.y;
        if (m_frustumPlanes[i].normal.z >= 0) n.z = minP.z;

        // p-vertex가 밖이면 완전히 밖 (OUTSIDE)
        if (glm::dot(m_frustumPlanes[i].normal, p) + m_frustumPlanes[i].distance < 0) {
            return FrustumState::OUTSIDE;
        }

        // n-vertex가 밖이면 완전히 포함된 것은 아님 (걸쳐있음)
        if (glm::dot(m_frustumPlanes[i].normal, n) + m_frustumPlanes[i].distance < 0) {
            allInside = false;
        }
    }

    return allInside ? FrustumState::INSIDE : FrustumState::INTERSECT;
}

// 카메라 시야 박스 반환 (절두체)
BoundingBox CameraController::GetCameraViewBox()
{
    if (!m_viewBoxReset) return m_viewBox; // 카메라가 움직인 경우에만 재계산

    glm::dmat4 inverseViewProjectionMatrix = glm::inverse(viewProjectionMatrix);

    // 화면 네 모서리에서 광선을 쏘아 z=0 지면과 교차한 4개 점의 AABB를 시야 박스로 사용
    glm::vec2 ndcCorners[4] = { {-1.0f, -1.0f}, { 1.0f, -1.0f}, { 1.0f,  1.0f}, {-1.0f,  1.0f} };

    m_viewBox.minX = std::numeric_limits<double>::max();
    m_viewBox.minY = std::numeric_limits<double>::max();
    m_viewBox.maxX = std::numeric_limits<double>::lowest();
    m_viewBox.maxY = std::numeric_limits<double>::lowest();

    for (int i = 0; i < 4; ++i) {
        // NDC near(z=-1) ~ far(z=+1) 두 점을 unproject해 광선 생성
        glm::dvec4 nearPoint = inverseViewProjectionMatrix * glm::dvec4(ndcCorners[i], -1.0f, 1.0f);
        glm::dvec4 farPoint  = inverseViewProjectionMatrix * glm::dvec4(ndcCorners[i],  1.0f, 1.0f);
        if (nearPoint.w != 0.0f) nearPoint /= nearPoint.w;
        if (farPoint.w  != 0.0f) farPoint  /= farPoint.w;

        glm::dvec3 rayOrigin = glm::dvec3(nearPoint);
        glm::dvec3 rayDir    = glm::dvec3(farPoint) - glm::dvec3(nearPoint);

        double hitX, hitY;
        // z=0 평면과 광선의 교차: rayOrigin.z + t * rayDir.z = 0  →  t = -rayOrigin.z / rayDir.z
        if (std::abs(rayDir.z) > 1e-6) {
            double t = -rayOrigin.z / rayDir.z;
            if (t >= 0.0 && t <= 1.0) { // near~far 사이에서 z=0과 만남 → 교차점 사용
                hitX = rayOrigin.x + t * rayDir.x;
                hitY = rayOrigin.y + t * rayDir.y;
            }
            else { // 광선이 z=0에 닿지 않거나 카메라 뒤편 (예: 카메라가 기울어져 수평선 너머를 봄) -> far plane 위치 사용
                hitX = farPoint.x;
                hitY = farPoint.y;
            }
        }
        else { // 광선이 z 평면과 거의 평행한 경우 (수평으로 봄)
            hitX = farPoint.x;
            hitY = farPoint.y;
        }

        m_viewBox.minX = std::min(m_viewBox.minX, hitX);
        m_viewBox.maxX = std::max(m_viewBox.maxX, hitX);
        m_viewBox.minY = std::min(m_viewBox.minY, hitY);
        m_viewBox.maxY = std::max(m_viewBox.maxY, hitY);
    }

    m_viewBoxReset = false;
    return m_viewBox;
}