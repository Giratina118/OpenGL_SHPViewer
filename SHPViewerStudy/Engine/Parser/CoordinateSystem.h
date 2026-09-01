#pragma once
#include <string>
#include <fstream>
#include <unordered_map>
#include "glm/gtc/type_ptr.hpp"

class Layer;
class CoordinateSystem;

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

// 좌표계 변환 시 사용하는 상수 저장
struct EllipsoidParams
{
    bool   isBessel = false; // 베젤 타원체인지
    double semiMajorAxis = 0.0; // 장반경
    double semiMinorAxis = 0.0; // 단반경
    double flattening    = 0.0; // 편평률

    double eccentricity2       = 0.0; // 제1이심률 제곱, 타원체의 납작함을 나타내는 값
    double eccentricity4       = 0.0; // 제1이심률 4제곱
    double eccentricity6       = 0.0; // 제1이심률 6제곱
    double secondEccentricity2 = 0.0; // 제2이심률 제곱

    double arcFactor0 = 0.0;
    double arcFactor2 = 0.0;
    double arcFactor4 = 0.0;
    double arcFactor6 = 0.0;

    double footPointE1 = 0.0;
    double footPointE2 = 0.0;
    double footPointE3 = 0.0;
    double footPointE4 = 0.0;

    void Set(CoordinateSystem& coordinate);
};

struct ProjectionParams
{
    bool isProjected = false;

    double angleUnit = 0.0;
    double falseEasting  = 0.0;
    double falseNorthing = 0.0;
    double scaleFactor   = 1.0;
    double latitudeOrigin  = 0.0;
    double longitudeOrigin = 0.0;

    double sinLat2 = 0.0;
    double sinLat4 = 0.0;
    double sinLat6 = 0.0;

    double arcLength = 0.0;

    void Set(CoordinateSystem& coordinate, EllipsoidParams& ellipsoid);
};

struct HelmertTransformParams
{
    double translationX = 0.0;
    double translationY = 0.0;
    double translationZ = 0.0;

    double rotationX = 0.0;
    double rotationY = 0.0;
    double rotationZ = 0.0;

    double scale = 1.0;

    double pivotX = 0.0;
    double pivotY = 0.0;
    double pivotZ = 0.0;

    void Set();
};

// 좌표게 변환 시 사용하는 상수 파라미터 저장
struct CoordinateTransformParameter
{
    EllipsoidParams  sourceEllipsoid;       // 출발 타원체
    EllipsoidParams  destinationEllipsoid;  // 도착 타원체
    ProjectionParams sourceProjection;      // 출발 좌표계
    ProjectionParams destinationProjection; // 도착 좌표계
    HelmertTransformParams helmert;         // 타원체 보정
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

    int GuessEpsg() const;
};

// 좌표계 변환
class CoordinateTransformer
{
public:
    CoordinateTransformParameter parameter;

    void TransformCoordinate(CoordinateSystem& prjCoordinate, Layer& newLayer);
    glm::dvec2 TransformPoint(glm::dvec2& point);
    void InverseProjection(glm::dvec2& point); // 역투영
    void Projection(glm::dvec2& point);   // 정투영
    glm::dvec3 LLAtoECEF(glm::dvec2& llaPoint);
    glm::dvec2 ECEFtoLLA(glm::dvec3& ecefPoint);
    void EllipsoidTransform(glm::dvec3& ecef);

    glm::dvec2 TransformPointInverse(glm::dvec2& point);
    void EllipsoidTransformInverse(glm::dvec3& ecef);
};