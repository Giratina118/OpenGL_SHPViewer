#include <pch.h>
#include "FeatureObject.h"

void BoundingBox::SetHeight(int32_t heightData, int32_t floorData, double nodeLength)
{
    int32_t trashHeightDataLimit = 20;

    if (heightData != -1)
        height = heightData;
    if (floorData != -1 && (height == 0 || height > floorData * trashHeightDataLimit))
        height = floorData * 4; // 1층 높아질 때마다 4미터 올라간다고 가정
    if (height == 0)
        height = std::min((GetLengthX() + GetLengthY()) * 0.5, nodeLength * 0.1);
}

// 자식 박스 반환
BoundingBox BoundingBox::ChildBox(int32_t child)
{
    BoundingBox childBox;

    switch (child) {
    case 0: childBox = { minX, GetCenter().y, GetCenter().x, maxY }; break; // 북서
    case 1: childBox = { GetCenter().x, GetCenter().y, maxX, maxY }; break; // 북동
    case 2: childBox = { minX, minY, GetCenter().x, GetCenter().y }; break; // 남서
    case 3: childBox = { GetCenter().x, minY, maxX, GetCenter().y }; break; // 남동
    }

    return childBox;
}

// 박스 결합
BoundingBox BoundingBox::CombineBox(BoundingBox& otherBox)
{
    return BoundingBox{std::min(minX, otherBox.minX), std::min(minY, otherBox.minY), std::max(maxX, otherBox.maxX), std::max(maxY, otherBox.maxY) };
}

// 박스 접촉 체크
bool BoundingBox::IsOnCollisionBox(BoundingBox& otherBox) const
{
    return (minX <= otherBox.maxX && maxX >= otherBox.minX && minY <= otherBox.maxY && maxY >= otherBox.minY);
}

// 점 접촉 체크
bool BoundingBox::IsOnCollisionPoint(glm::dvec3& otherPoint) const
{
    return (minX <= otherPoint.x && maxX >= otherPoint.x && minY <= otherPoint.y && maxY >= otherPoint.y);
}

// 선 접촉 체크, dir은 단위벡터
bool BoundingBox::IsOnCollisionRay(glm::dvec3& start, glm::dvec3& dir) const
{
    // 출발점이 이미 박스 안에 있으면 접촉 판정
    if (minX <= start.x && maxX >= start.x && minY <= start.y && maxY >= start.y && 0 <= start.z && height >= start.z)
        return true;

    // mbr 박스 중 x, y, z값이 더 가까운 3개 지점에서만 검사하면 됨
    double distance = 0.0;
    glm::dvec3 targetPoint;

    if (std::abs(dir.x) > 0.01) {
        if (std::abs(start.x - minX) < std::abs(start.x - maxX)) distance = (minX - start.x) / dir.x;
        else                                                     distance = (maxX - start.x) / dir.x;
        targetPoint = start + dir * distance;
        if (minY < targetPoint.y && maxY > targetPoint.y && 0 < targetPoint.z && height > targetPoint.z)  return true;
    }
    if (std::abs(dir.y) > 0.01) {
        if (std::abs(start.y - minY) < std::abs(start.y - maxY)) distance = (minY - start.y) / dir.y;
        else                                                     distance = (maxY - start.y) / dir.y;
        targetPoint = start + dir * distance;
        if (minX < targetPoint.x && maxX > targetPoint.x && 0 < targetPoint.z && height > targetPoint.z)  return true;
    }
    if (std::abs(dir.z) > 0.01) {
        if (std::abs(start.z - 0) < std::abs(start.z - height))  distance = (0      - start.z) / dir.z;
        else                                                     distance = (height - start.z) / dir.z;
        targetPoint = start + dir * distance;
        if (minX < targetPoint.x && maxX > targetPoint.x && minY < targetPoint.y && maxY > targetPoint.y) return true;
    }

    return false;
}

// 박스 포함 체크
bool BoundingBox::IsInclude(BoundingBox& otherBox) const
{
    return (minX <= otherBox.minX && maxX >= otherBox.maxX && minY <= otherBox.minY && maxY >= otherBox.maxY);
}

// 선과의 접촉 체크, 접촉한다면 (카메라로부터) 접촉 거리 반환 (피킹 시 사용)
double PolyObject::OnCollisionRay(glm::dvec3& start, glm::dvec3& dir, double& rootNodeHeight)
{
    double result = 2.0;
    // rayEndPoint: 만일 카메라가 위를 바라보고 있다면(z=0과 접하지 않는다면) 노드의 높이와 닿는 지점을 선택
    glm::dvec3 rayEndPoint = (start.z > 0.0 && dir.z > 0.0 || start.z < 0.0) ? start + (rootNodeHeight - start.z) / dir.z * dir : start + (0.0 - start.z) / dir.z * dir;

    // 벽과의 충돌 체크
    for (int32_t part = 0; part < parts.size(); part++) {
        int32_t endPoint = (part == parts.size() - 1) ? points.size() : parts[part + 1];
        for (int32_t point = parts[part]; point < endPoint; point++) {
            glm::dvec2 point1 = points[point];
            glm::dvec2 point2 = (point == endPoint) ? points[parts[part]] : points[point + 1];
            
            double ration = CrossCheck(start, rayEndPoint, point1, point2);
            glm::dvec3 raycastPoint = start + (rayEndPoint - start) * ration;
            if (ration > 0.0 && ration < result && raycastPoint.z > 0 && raycastPoint.z < mbrBox.height) // 충돌하면 + 가장 가까운 충돌이면 저장
                result = ration;
        }
    }

    // 지붕과의 충돌 체크 (위에서 아래를 봤을 때만 체크)
    glm::dvec3 hitPoint = start + (mbrBox.height - start.z) / dir.z * dir;
    rayEndPoint         = start + (0.0           - start.z) / dir.z * dir;
    double ration       = std::abs(mbrBox.height - start.z) / std::abs(rayEndPoint.z - start.z);
    if (start.z > hitPoint.z && dir.z < 0 && ration < result && mbrBox.IsOnCollisionPoint(hitPoint))
    {
        // 접촉점이 도형 안에 있는지 체크
        glm::dvec2 hitPoint2D = (glm::dvec2)hitPoint;
        glm::dvec2 RightOfHit = { mbrBox.maxX, hitPoint2D.y }; // hitPoint2D에서 xMax로 뻗은 곳
        bool  isInsidePolygon = false;

        for (int32_t part = 0; part < parts.size(); part++) {
            int32_t partEndPoint = (part == parts.size() - 1) ? points.size() : parts[part + 1];
            bool    isInsidePart = false;
            double  crossSum     = 0.0;

            for (int32_t point = parts[part]; point < partEndPoint; point++) {
                glm::dvec2 point1 = points[point];
                glm::dvec2 point2 = (point == partEndPoint) ? points[parts[part]] : points[point + 1];

                if (0.0 < CrossCheck(RightOfHit, hitPoint2D, point1, point2)) isInsidePart = !isInsidePart;  // 교차 횟수(홀짝)로 도형 내부, 위부 구분
                crossSum += (points[point].x * points[point + 1].y - points[point].y * points[point + 1].x); // 홀인지 아닌지 구분
            }

            if (isInsidePart) {
                if (crossSum <= 0) { isInsidePolygon = true; }         // 외곽선 안에 있을 때
                else               { isInsidePolygon = false; break; } // 홀 안에 있을 때
            }
        }

        if (isInsidePolygon) result = ration;
    }

    return result;
}

// 두 선분의 접촉 체크, 접촉한다면 첫 번째 선분에서 어느 지점에 접촉하는지 반환, 접촉하지 않는다면 -1.0 반환
double CrossCheck(glm::dvec2 point1, glm::dvec2 point2, glm::dvec2 point3, glm::dvec2 point4)
{
    // 선분 A: rayStartPoint → rayEndPoint,  선분 B: point3 → point4
    auto cross = [](glm::dvec2 vec1, glm::dvec2 vec2) { return vec1.x * vec2.y - vec1.y * vec2.x; };

    glm::dvec2 dirA = point2 - point1;
    glm::dvec2 dirB = point4 - point3;
    glm::dvec2 aToB = point3 - point1;

    double crossOfDir = cross(dirA, dirB);
    if (std::abs(crossOfDir) < 1e-10) return -1.0; // 평행

    double ratioOnA = cross(aToB, dirB) / crossOfDir; // 선분 A 위의 교차점 비율 (0~1이면 선분 A 내부)
    double ratioOnB = cross(aToB, dirA) / crossOfDir; // 선분 B 위의 교차점 비율 (0~1이면 선분 B 내부)

    return (ratioOnA > 0.0 && ratioOnA < 1.0 && ratioOnB > 0.0 && ratioOnB < 1.0) ? ratioOnA : -1.0;
}