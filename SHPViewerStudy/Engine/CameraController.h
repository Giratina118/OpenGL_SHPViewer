#pragma once

#include "FeatureObject.h"
#include "Transform.h"
#include <algorithm>

// 평면(Plane)을 표현하는 구조체 (Ax + By + Cz + D = 0)
struct FrustumPlane {
	glm::dvec3 normal;
	double distance;

	// 평면 정규화
	void Normalize() {
		double mag = glm::length(normal);
		normal   /= mag;
		distance /= mag;
	}
};

// 절두체 포함 상태 enum
enum class FrustumState {
	OUTSIDE,   // 시야 밖에 있음 (컬링)
	INTERSECT, // 시야 경계에 걸쳐 있음 (정밀 검사 필요)
	INSIDE     // 시야에 완전히 들어와 있음 (전체 렌더링)
};

class CameraController
{
private:
	float moveSpeedCorrection   = 0.002f; // 이동 속도 보정
	float zoomSpeedCorrection   = 0.1f;   // 줌 속도 보정
	float rotateAngleCorrection = 0.1f;   // 회전 각도 보정

	glm::dmat4 viewMatrix           = glm::dmat4(1.0f); // 뷰 행렬
	glm::dmat4 projectionMatrix     = glm::dmat4(1.0f); // 투영 행렬
	glm::dmat4 viewProjectionMatrix = glm::dmat4(1.0f); // 뷰-투영 행렬

	BoundingBox m_viewBox;      // 시야 범위 박스
	bool m_viewBoxReset = true; // 시야 범위 박스 초기화 여부
	bool m_isCameraChanged = true; // 카메라 변경 표시
	FrustumPlane m_frustumPlanes[6]; // 절두체 평면 (좌, 우, 하, 상, 근, 원)


public:
    Transform transform;
	double fov = 60.0;   // 시야각(FOV)
    double aspect = 1.0; // 화면 종횡비
	glm::dvec3 m_thirdMovePos;
	int32_t m_viewRangeRate = 40; // LOD 기준 거리 배율 (180m 길이의 노드가 있을 때 이 배율이 10이라면 카메라와 노드 중심의 거리가 1800m보다 가깝다면 보이도록 한다.)

    void Init(const BoundingBox& boundingBox, int screenWidth, int screenHeight); // 카메라 초기화
    void UpdateAspect(int screenWidth, int screenHeight); // 종횡비 설정

	void MoveLocal(double deltaX, double deltaY); // 로컬 이동
	void MoveWorld(double deltaX, double deltaY); // 월드 이동
	//void MoveThird(glm::dvec3& currentPos, glm::dvec3& beforePos); // 3인칭 이동
    void RotateLocal(double deltaX, double deltaY, double deltaZ); // 로컬 회전
    void RotateWorld(double deltaX, double deltaY, double deltaZ); // 월드 회전
    void RotateFirst(double deltaX, double deltaY, double deltaZ); // 월드 회전
	void RotateThird(double deltaX, double deltaY, double deltaZ, glm::dvec3& point); // 3인칭 회전
	void ZoomLocal(double factor); // 로컬 줌인/줌아웃
	void ZoomWorld(double factor); // 월드 줌인/줌아웃
	void ZoomThird(double factor, glm::dvec3& point); // 3인칭 줌인/줌아웃
	void UpdateMatrix(); // 행렬 갱신 (위치/회전/줌 변경 시 호출)
	void SetViewRange(int32_t viewRangeRate) { m_viewRangeRate = viewRangeRate; } // 시야 범위 조절 (LOD 기준값 조절)

	void ExtractFrustumPlanes();
	FrustumState GetFrustumState(const BoundingBox& box) const;

    BoundingBox GetCameraViewBox(); // 카메라 시야 박스 반환
	const glm::dmat4 GetMatrix() const { return viewProjectionMatrix; } // 뷰-투영 행렬 반환

	void SetCameraChange() { m_isCameraChanged = false; }      // 카메라 변경 표시 초기화 (렌더러가 처리 후 호출)
	bool GetCameraChange() const { return m_isCameraChanged; } // 카메라 변경 여부 반환
};