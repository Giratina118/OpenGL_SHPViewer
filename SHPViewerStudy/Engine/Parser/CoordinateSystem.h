#pragma once
#include <string>
#include <fstream>
#include <unordered_map>
#include "glm/gtc/type_ptr.hpp"

class Layer;

// 타원체 정보
struct Ellipsoid
{
    std::string name;              // 타원체 이름
    double      semiMajorAxis;     // 장반경
    double      inverseFlattening; // 편평률의 역수 (1/편평률)
};

// 투영 파라미터
enum class Parameter
{
    FalseEasting,
    FalseNorthing,
    CentralMeridian,
    LatitudeOfOrigin,
    ScaleFactor
};

// 지리 좌표계 정보
struct Geographic
{
    std::string name;          // 지리좌표계 이름
    std::string datumName;     // 데이텀 이름
    Ellipsoid   ellipsoid;     // 타원체
    double      primeMeridian; // 중앙 경선
    double      unit;          // 각도 단위 (3.141592.../180)
};

// 투영 좌표계 정보
struct Projected
{
    std::string name;           // 투영 좌표계 이름
    std::string projectionName; // 투영법 이름
    std::unordered_map<Parameter, double> parameters; // 투영법 파라미터
    double unit; // 길이 단위 (미터)
};

// 좌표계 클래스
class CoordinateSystem
{
private:
    const char* m_ptr = nullptr;
    const char* m_end = nullptr;

public:
    bool isProjected; // 투영 좌표계인지

    Geographic gcs; // 지리 좌표계 정보
    Projected  pcs; // 투영 좌표계 정보

    void PrjParse(std::vector<uint8_t>& prjBuffer); // 파서
    std::string ReadValue(); // 하나씩 읽기
    void SetTargetCoordinate(int32_t epsg);
};

// 좌표계 변환
class CoordinateTransformer
{
public:
    void TransformCoordinate(CoordinateSystem& prjCoordinate, Layer& newLayer);
    glm::dvec2 TransformPoint(glm::dvec2& point, CoordinateSystem& source, CoordinateSystem& destination);
    void InverseProjection(glm::dvec2& point, CoordinateSystem& source); // 역투영
    void Projection(glm::dvec2& point, CoordinateSystem& destination);   // 정투영
    glm::dvec3 LLAtoECEF(glm::dvec2& llaPoint,  CoordinateSystem& source);
    glm::dvec2 ECEFtoLLA(glm::dvec3& ecefPoint, CoordinateSystem& destination);
    void EllipsoidTransform(glm::dvec3& ecef);
};