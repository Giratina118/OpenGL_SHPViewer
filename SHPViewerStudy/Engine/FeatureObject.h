#pragma once
#include <cstdint>
#include <vector>
#include <optional>
#include <string>
#include "ColorData.h"
#include "DBFTable.h"
#include "Transform.h"

double CrossCheck(glm::dvec2 p1, glm::dvec2 p2, glm::dvec2 p3, glm::dvec2 p4); // 선분 교차 검사

// 정점 - 색상 포함
struct Vertex
{
	float x, y, z;
	UCharColor color;
};

// 각 폴리곤별 IBO 위치 정보를 담을 구조체
struct DrawInfo
{
	uint32_t indexOffset;  // 인덱스 시작 위치
	uint32_t indexCount;   // 그려야 할 인덱스 개수
	uint32_t vertexOffset; // 인덱스 시작 위치
	uint32_t vertexCount;  // 그려야 할 인덱스 개수
};

struct BoundingBox
{
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
	double height = 0.0;

	glm::dvec2 GetCenter() const { return glm::dvec2{ (maxX + minX) * 0.5, (maxY + minY) * 0.5 }; } // 중점 좌표 계산
	glm::dvec2 GetTop()    const { return glm::dvec2{ (maxX + minX) * 0.5, maxY }; }                // 중점 좌표 계산
	glm::dvec2 GetBottom() const { return glm::dvec2{ (maxX + minX) * 0.5, minY }; }                // 중점 좌표 계산
	glm::dvec2 GetRight()  const { return glm::dvec2{ maxX,                (maxY + minY) * 0.5 }; } // 중점 좌표 계산
	glm::dvec2 GetLeft()   const { return glm::dvec2{ minX,                (maxY + minY) * 0.5 }; } // 중점 좌표 계산
	double GetLengthX()    const { return maxX - minX; } // X축 길이 계산
	double GetLengthY()    const { return maxY - minY; } // Y축 길이 계산
	double GetMaxExtent()  const { return std::max(GetLengthX(), GetLengthY()); } // LOD용 최대 변 길이
	void SetHeight(double heightData, double floorData, double nodeLength);
    

	BoundingBox GetLooseBox(double looseBoxRate) const; // 느슨한 박스 크기 계산
    BoundingBox ChildBox   (int32_t child);             // 자식 박스 반환
    BoundingBox CombineBox (BoundingBox& otherBox);     // 박스 결합
    bool IsOnCollisionBox  (BoundingBox& otherBox)  const; // 박스 접촉 체크
	bool IsOnCollisionPoint(glm::dvec3& otherPoint) const; // 점 접촉 체크
	bool IsOnCollisionRay  (glm::dvec3& startPoint, glm::dvec3& dir) const; // 선 접촉 체크
    bool IsInclude         (BoundingBox& otherBox)  const; // 박스 포함 체크 (other이 자신 안에 완전히 포함되는지)
};


class ObjectBase
{
public:
	uint32_t    shapeType; // 객체 타입 (1: Point, 3: PolyLine, 5: Polygon, 8: MultiPoint, 31: MultiPatch)
    BoundingBox mbrBox;    // 객체 MBR
	Transform   transform; // 객체 위치, 회전, 스케일 관리

	void Move  (glm::dvec3& delta);
	void Rotate(glm::dvec3& delta);
	void Scale (glm::dvec3& delta);
	void UpdateMatrix();
};

class PointObject : public ObjectBase
{
public:
	glm::dvec2 point;  // x, y 좌표

	double z = 0.0; // z 좌표
	double m = 0.0; // m 값

	void SetMBRBox();
};

class MultiPointObject : public ObjectBase
{
public:
	std::vector<glm::dvec2> points; // x, y 좌표
    
	std::optional<std::vector<double>> zValues; // z 좌표 (선택적)
	std::optional<std::vector<double>> mValues; // m 값   (선택적)
};

class PolyObject : public ObjectBase
{
public:
	std::vector<glm::dvec2> points; // x, y 좌표
	std::vector<int32_t>    parts;  // parts 배열: 각 part의 시작점 인덱스 (0-based)

	//double z = 0.0; //  공통 z 좌표 (선택적)
	std::optional<std::vector<double>> zValues; // z 좌표 (선택적)
	std::optional<std::vector<double>> mValues; // m 값   (선택적)

	double OnCollisionRay(glm::dvec3& start, glm::dvec3& dir, double& rootNodeHeight);
	void ClearData() { points.clear(); points.shrink_to_fit(); parts.clear(); parts.shrink_to_fit(); }
	void SetMBRBox(double minX, double minY, double maxX, double maxY);
};

class MultiPatchObject : public PolyObject
{
public:
	std::vector<int32_t> partTypes; // partTypes 배열: 각 part의 타입
};