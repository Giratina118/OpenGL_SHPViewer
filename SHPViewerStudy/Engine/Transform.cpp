#include <pch.h>
#include "Transform.h"

// 행렬 출력
glm::dmat4 Transform::GetMatrix() const
{
    glm::dmat4 translateMatrix = glm::translate(glm::dmat4(1.0), position);
    glm::dmat4 rotationMatrix  = glm::toMat4(rotation);
    glm::dmat4 scaleMatrix     = glm::scale(glm::dmat4(1.0), scale);

    return translateMatrix * rotationMatrix * scaleMatrix;
}

// 로컬 이동
void Transform::MoveLocal(const glm::dvec3& delta)
{
    position += GetRight()   * delta.x;
    position += GetUp()      * delta.y;
    position += GetForward() * delta.z;
}

// 월드 이동
void Transform::MoveWorld(const glm::dvec3& delta)
{
    position += delta;
}

void Transform::MoveThird(const glm::dvec3& delta)
{

}

// 로컬 회전
void Transform::RotateLocal(double yawDeg, double pitchDeg, double rollDeg)
{
    if (yawDeg == 0.0f && pitchDeg == 0.0f && rollDeg == 0.0f) return;

    glm::dquat yawRotation   = glm::angleAxis(glm::radians(yawDeg),   GetUp());
    glm::dquat pitchRotation = glm::angleAxis(glm::radians(pitchDeg), GetRight());
    glm::dquat rollRotation  = glm::angleAxis(glm::radians(rollDeg),  GetForward());

    glm::dquat deltaRot = yawRotation * pitchRotation * rollRotation;
    rotation = glm::normalize(deltaRot * rotation);
}

// 월드 회전
void Transform::RotateWorld(double yawDeg, double pitchDeg, double rollDeg)
{
    glm::dquat yawRotation   = glm::angleAxis(glm::radians(yawDeg),   glm::dvec3(0, 1, 0));
    glm::dquat pitchRotation = glm::angleAxis(glm::radians(pitchDeg), glm::dvec3(1, 0, 0));
    glm::dquat rollRotation  = glm::angleAxis(glm::radians(rollDeg),  glm::dvec3(0, 0, -1));

    glm::dquat deltaRot = yawRotation * pitchRotation * rollRotation;
    rotation = glm::normalize(deltaRot * rotation);
}

void Transform::RotateFirst(double yawDeg, double pitchDeg, double rollDeg)
{
    if (yawDeg == 0.0f && pitchDeg == 0.0f && rollDeg == 0.0f) return;

    glm::dquat yawRotation   = glm::angleAxis(glm::radians(yawDeg),   glm::dvec3(0.0, 0.0, 1.0));
    glm::dquat pitchRotation = glm::angleAxis(glm::radians(pitchDeg), GetRight());
    glm::dquat rollRotation  = glm::angleAxis(glm::radians(rollDeg),  GetForward());

    glm::dquat deltaRot = yawRotation * pitchRotation * rollRotation;
    rotation = glm::normalize(deltaRot * rotation);
}

void Transform::RotateThird(double yawDeg, double pitchDeg, const glm::dvec3& point)
{
    glm::dvec3 currentPos = position;
    glm::dvec3 movePos = currentPos - point;

    
    TCHAR buf[256];
    _stprintf_s(buf, _T("[targetPos] x: %f,  y: %f,  z: %f  /  [currentPos] x: %f,  y: %f,  z: %f  /  [movePos] x: %f,  y: %f,  z: %f\n"),
        point.x, point.y, point.z, currentPos.x, currentPos.y, currentPos.z, movePos.x, movePos.y, movePos.z);
    OutputDebugString(buf);
    
    // 1. 회전 변화량(Quaternion) 생성
    glm::dquat yawRotation   = glm::angleAxis(glm::radians(yawDeg), glm::dvec3(0.0, 0.0, 1.0));
    glm::dquat pitchRotation = glm::angleAxis(glm::radians(pitchDeg), GetRight());
    glm::dquat deltaRot      = yawRotation * pitchRotation;

    // 2. 기준점(point)에서 현재 위치(position)로 향하는 방향 벡터 계산
    glm::dvec3 dir = position - point;

    // 3. 방향 벡터를 회전
    dir = deltaRot * dir;

    // 4. 회전된 벡터를 기준점에 더해 새로운 위치 적용
    position = point + dir;

    // 5. 물체 자체의 방향(회전값)도 함께 갱신하여 공전 중에도 기준점을 계속 바라보도록 처리
    rotation = glm::normalize(deltaRot * rotation);
}