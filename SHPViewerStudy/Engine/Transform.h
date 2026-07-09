#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

// 객체 위치, 방향, 크기 관리 클래스
class Transform
{
public:
    glm::dvec3 position = glm::dvec3(0.0);                // 위치
    glm::dquat rotation = glm::dquat(1.0, 0.0, 0.0, 0.0); // 방향
    glm::dvec3 scale    = glm::dvec3(1.0);                // 크기

public:
    glm::dvec3 GetForward() const { return rotation * glm::dvec3(0.0, 0.0, -1.0); }; // 앞
    glm::dvec3 GetRight()   const { return rotation * glm::dvec3(1.0, 0.0,  0.0); };  // 오른쪽
    glm::dvec3 GetUp()      const { return rotation * glm::dvec3(0.0, 1.0,  0.0); };  // 위
    glm::dmat4 GetMatrix()  const; // 행렬 반환

    // 현재 카메라 이동에 로컬 이동, 로컬 회전 사용 중
    void MoveLocal(const glm::dvec3& delta); // 로컬 이동
    void MoveWorld(const glm::dvec3& delta); // 월드 이동
    void MoveThird(const glm::dvec3& delta);

    void RotateLocal(double yawDeg, double pitchDeg, double rollDeg); // 로컬 회전
    void RotateWorld(double yawDeg, double pitchDeg, double rollDeg); // 월드 회전
    void RotateFirst(double yawDeg, double pitchDeg, double rollDeg); // 로컬 회전
    void RotateThird(double yawDeg, double pitchDeg, const glm::dvec3& point); // 3인칭 회전
};