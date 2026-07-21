#include <pch.h>
#include <vector>
#include "CoordinateSystem.h"

// prj 파싱
void CoordinateSystem::PrjParse(std::vector<uint8_t>& prjBuffer)
{
    m_ptr = reinterpret_cast<const char*>(prjBuffer.data());
    m_end = m_ptr + prjBuffer.size();
    std::string token;

    while (m_ptr < m_end) {
        token = ReadValue();

        if      (token.empty())     continue;
        if      (token == "PROJCS") { pcs.name = ReadValue(); }
        else if (token == "GEOGCS") { gcs.name = ReadValue(); }
        else if (token == "DATUM")  { gcs.datumName = ReadValue(); }
        else if (token == "SPHEROID") {
            gcs.ellipsoid.name              = ReadValue();
            gcs.ellipsoid.semiMajorAxis     = std::stod(ReadValue());
            gcs.ellipsoid.inverseFlattening = std::stod(ReadValue());
        }
        else if (token == "PRIMEM") {
            ReadValue();
            gcs.primeMeridian = std::stod(ReadValue());
        }
        else if (token == "PROJECTION") {
            pcs.projectionName = ReadValue();
            isProjected        = true;
        }
        else if (token == "PARAMETER") {
            std::string name  = ReadValue();
            double      value = std::stod(ReadValue());

            if      (name == "False_Easting"      || name == "false_easting"     ) pcs.parameters[Parameter::FalseEasting]     = value;
            else if (name == "False_Northing"     || name == "false_northing"    ) pcs.parameters[Parameter::FalseNorthing]    = value;
            else if (name == "Central_Meridian"   || name == "central_meridian"  ) pcs.parameters[Parameter::CentralMeridian]  = value;
            else if (name == "Latitude_Of_Origin" || name == "latitude_of_origin") pcs.parameters[Parameter::LatitudeOfOrigin] = value;
            else if (name == "Scale_Factor"       || name == "scale_factor"      ) pcs.parameters[Parameter::ScaleFactor]      = value;
        }
        else if (token == "UNIT") {
            ReadValue();
            double vlaue = std::stod(ReadValue());

            if (isProjected) pcs.unit = vlaue;
            else             gcs.unit = vlaue;
        }
    }
}

std::string CoordinateSystem::ReadValue()
{
    std::string token = "";
    int32_t doubleQuotesCount = 0;

    while (m_ptr < m_end) {
        char ch = *m_ptr++;

        if      (ch == '"' && doubleQuotesCount == 0) { doubleQuotesCount++; }
        else if (ch == '"' && doubleQuotesCount == 1) { break; }
        else if (ch == ' ' && token.empty())          continue;
        else if (ch != ',' && ch != '[' && ch != ']') token += ch;
        else if (!token.empty()) break;
    }
    
    return token;
}

void CoordinateSystem::SetTargetCoordinate(int32_t epsg)
{
    // 지금은 epsg 5186으로만 설정
    isProjected = true;
    gcs.name      = "GCS_KGD2002";
    gcs.datumName = "D_Korea_Geodetic_Datum_2002";
    gcs.ellipsoid.name              = "GRS_1980";
    gcs.ellipsoid.semiMajorAxis     = 6378137.0;
    gcs.ellipsoid.inverseFlattening = 298.257222101;
    gcs.primeMeridian = 0.0;
    gcs.unit = 0.0174532925199433;

    pcs.name           = "KGD2002_Central_Belt_2010";
    pcs.projectionName = "Transverse_Mercator";
    pcs.parameters[Parameter::FalseEasting]     = 200000.0;
    pcs.parameters[Parameter::FalseNorthing]    = 600000.0;
    pcs.parameters[Parameter::CentralMeridian]  = 127.0;
    pcs.parameters[Parameter::LatitudeOfOrigin] = 38.0;
    pcs.parameters[Parameter::ScaleFactor]      = 1.0;
    pcs.unit = 1.0;
}


// 좌표계 변환
glm::dvec2 CoordinateTransformer::Transform(glm::dvec2& point, CoordinateSystem& source, CoordinateSystem& destination)
{
    // 현재는 5186으로만 변환
    // 일단 GRS80으로 동일하다고 가정, 다른 경우는 이후 추가


    // 투영 좌표계(미터) -> 경위도 좌표계로 역투영
    InverseProjection(point, source);


    //
    // 타원체 변환 Bessel -> GRS80
    //



    // 경위도 좌표계 -> 투영 좌표계(미터)로 정투영
    Projection(point, destination);

    return point;
}

// 역투영
void CoordinateTransformer::InverseProjection(glm::dvec2& point, CoordinateSystem& source)
{
    // 가산치 (음수 보정) 제거
    point.x -= source.pcs.parameters[Parameter::FalseEasting];
    point.y -= source.pcs.parameters[Parameter::FalseNorthing];

    // 축척 제거
    double scaleFactor = (source.pcs.parameters[Parameter::ScaleFactor] > 0.0) ? source.pcs.parameters[Parameter::ScaleFactor] : 1.0;
    point.x /= scaleFactor;
    point.y /= scaleFactor;


    // 투영 좌표계(미터) -> 경위도 좌표계로 역연산
    double flattening          = 1.0 / source.gcs.ellipsoid.inverseFlattening; // 편평률
    double eccentricitySquared = 2.0 * flattening - flattening * flattening; // 제1이심률 제곱, 타원체의 납작함을 나타내는 값
    double semiMajorAxis       = source.gcs.ellipsoid.semiMajorAxis; // 장반경
    double latitudeOrigin      = source.pcs.parameters[Parameter::LatitudeOfOrigin] * source.gcs.unit; // 원점 위도
    double longitudeOrigin     = source.pcs.parameters[Parameter::CentralMeridian]  * source.gcs.unit; // 원점 경도

    double eccentricityTemp = (1 - eccentricitySquared / 4.0 - 3.0 * glm::pow(eccentricitySquared, 2.0) / 64.0 - 5.0 * glm::pow(eccentricitySquared, 3.0) / 256.0); // 재활용을 위한 이심률 중간식

    // 원점에서 위도까지의 호 길이, 테일러 급수를 이용한 타원체 자오선 호의 길이
    // 적도에서 좌표계의 투영 원점까지 지표면을 따라 걸었을 때의 실제 곡면 거리(m) 구하기
    double arcLength = semiMajorAxis * (eccentricityTemp * latitudeOrigin
        - (3.0 * eccentricitySquared / 8.0 + 3.0 * glm::pow(eccentricitySquared, 2.0) / 32.0 + 45.0 * glm::pow(eccentricitySquared, 3.0) / 1024.0) * glm::sin(2.0 * latitudeOrigin)
        + (15.0 * glm::pow(eccentricitySquared, 2.0) / 256.0 + 45.0 * glm::pow(eccentricitySquared, 3.0) / 1024.0) * glm::sin(4.0 * latitudeOrigin)
        - (35.0 * glm::pow(eccentricitySquared, 3.0) / 3072.0) * glm::sin(6.0 * latitudeOrigin));

    // 구면 위도, 적도부터 내 위치까지의 총 거리를 지구의 평균 곡률 반경으로 나눔
    // 지구가 완벽한 구형이라고 가정했을 때의 임시 각도(라디안)를 얻음
    double rectifyingLatitude = (arcLength + point.y) / (semiMajorAxis * eccentricityTemp);


    double eccentricityTemp2 = glm::sqrt(1.0 - eccentricitySquared); // 재활용을 위한 이심률 중간식 2
    double footpointEccentricity_1 = (1.0 - eccentricityTemp2) / (1.0 + eccentricityTemp2); // 보정 상수, 발 위도용 이심률

    // 발 위도 최종 계산, 테일러 급수를 이용한 발 위도 보정 공식,구면 위도에 지구가 납작해서 생기는 오차를 sin함수의 파동 형태로 깎아냄
    // 현재 좌표가 세로 방향으로만 올라갔을 때 위치하게 될 가상의 위도(기준점)를 확정함
    double footpointLatitude = rectifyingLatitude
        + (3.0    * footpointEccentricity_1                / 2.0  - 27.0 * glm::pow(footpointEccentricity_1, 3.0) / 32.0) * glm::sin(2.0 * rectifyingLatitude)
        + (21.0   * glm::pow(footpointEccentricity_1, 2.0) / 16.0 - 55.0 * glm::pow(footpointEccentricity_1, 4.0) / 32.0) * glm::sin(4.0 * rectifyingLatitude)
        + (151.0  * glm::pow(footpointEccentricity_1, 3.0) / 96.0)  * glm::sin(6.0 * rectifyingLatitude)
        + (1097.0 * glm::pow(footpointEccentricity_1, 4.0) / 512.0) * glm::sin(8.0 * rectifyingLatitude);

    // 제2이심률의 제곱
    double secondEccentricitySquared = eccentricitySquared / (1.0 - eccentricitySquared);


    double eccentricityTemp3 = glm::sqrt(1.0 - eccentricitySquared * glm::pow(glm::sin(footpointLatitude), 2.0)); // 재활용을 위한 이심률 중간식 3

    // 발 위도에서의 지구 곡률 파라미터 계산, 발 위도 위치에서 지구가 가로/세로로 각각 얼마나 휘어있는지 반지름을 구하고 보정용 변수를 만듦
    double transverseRadius = semiMajorAxis / eccentricityTemp3;; // 가로 곡률 반경
    double meridionalRadius = semiMajorAxis * (1.0 - eccentricitySquared) / glm::pow(eccentricityTemp3, 3.0); // 세로 곡률 반경

    // 각도 임시 변수, 테일러 급수에 들어갈 삼각함수 임시 값들
    double t1 = glm::pow(glm::tan(footpointLatitude), 2.0);
    double c1 = secondEccentricitySquared * glm::pow(glm::cos(footpointLatitude), 2.0);

    // 가로 이동 각도 비율, 순수 가로 이동 거리를 횡곡률로 나누어 라디안 형태의 각도 비율로 만든다
    double d = point.x / transverseRadius;

    // 최종 위도, 경도 도출, 준비한 파라미터들을 테일러 급수에 대입하여 최종 각도를 구함
    double latitudeRadian = footpointLatitude - (transverseRadius * glm::tan(footpointLatitude) / meridionalRadius) * (glm::pow(d, 2.0) / 2.0
        - (5.0  + 3.0  * t1 + 10.0  * c1 - 4.0  * glm::pow(c1, 2.0) - 9.0   * secondEccentricitySquared) * glm::pow(d, 4.0) / 24.0
        + (61.0 + 90.0 * t1 + 298.0 * c1 + 45.0 * glm::pow(t1, 2.0) - 252.0 * secondEccentricitySquared - 3.0 * glm::pow(c1, 2.0)) * glm::pow(d, 6.0) / 720.0);

    double longitudeRadian = longitudeOrigin + (1.0 / glm::cos(footpointLatitude)) * (d - (1.0 + 2.0 * t1 + c1) * glm::pow(d, 3.0) / 6.0
        + (5.0 - 2.0 * c1 + 28.0 * t1 - 3.0 * glm::pow(c1, 2.0) + 8.0 * secondEccentricitySquared + 24.0 * glm::pow(t1, 2.0)) * glm::pow(d, 5.0) / 120.0);

    point.x = longitudeRadian / source.gcs.unit; // 경도 (x축에 매핑)
    point.y = latitudeRadian  / source.gcs.unit; // 위도 (y축에 매핑)
}

// 정투영
void CoordinateTransformer::Projection(glm::dvec2& point, CoordinateSystem& destination)
{
    double longitudeRadian = point.x * destination.gcs.unit;
    double latitudeRadian  = point.y * destination.gcs.unit;

    double semiMajorAxis = destination.gcs.ellipsoid.semiMajorAxis; // 장반경
    double flattening = 1.0 / destination.gcs.ellipsoid.inverseFlattening; // 편평률
    double latitudeOrigin = destination.pcs.parameters[Parameter::LatitudeOfOrigin] * destination.gcs.unit; // 원점 위도
    double longitudeOrigin = destination.pcs.parameters[Parameter::CentralMeridian] * destination.gcs.unit; // 원점 경도
    double eccentricitySquared = 2.0 * flattening - flattening * flattening; // 제1이심률 제곱, 타원체의 납작함을 나타내는 값
    double secondEccentricitySquared = eccentricitySquared / (1.0 - eccentricitySquared); // 제2이심률의 제곱

    double eccentricityTemp = (1 - eccentricitySquared / 4.0 - 3.0 * glm::pow(eccentricitySquared, 2.0) / 64.0 - 5.0 * glm::pow(eccentricitySquared, 3.0) / 256.0); // 재활용을 위한 이심률 중간식


    // 원점에서 위도까지의 호 길이, 테일러 급수를 이용한 타원체 자오선 호의 길이
    // 적도에서 좌표계의 투영 원점까지 지표면을 따라 걸었을 때의 실제 곡면 거리(m) 구하기

    // 이거 하는 중
    double arcLength = semiMajorAxis 
        * ((1  - eccentricitySquared / 4.0 - 3.0 * glm::pow(eccentricitySquared, 2.0) / 64.0 - 5.0  * glm::pow(eccentricitySquared, 3.0) / 256.0)  * latitudeOrigin
        - (3.0 * eccentricitySquared / 8.0 + 3.0 * glm::pow(eccentricitySquared, 2.0) / 32.0 + 45.0 * glm::pow(eccentricitySquared, 3.0) / 1024.0) * glm::sin(2.0 * latitudeOrigin)
        + (15.0 * glm::pow(eccentricitySquared, 2.0) / 256.0 + 45.0 * glm::pow(eccentricitySquared, 3.0) / 1024.0) * glm::sin(4.0 * latitudeOrigin)
        - (35.0 * glm::pow(eccentricitySquared, 3.0) / 3072.0) * glm::sin(6.0 * latitudeOrigin));





    double longitudeDelta = longitudeRadian - longitudeOrigin;
    double transverseRadius = semiMajorAxis / glm::sqrt(1.0 - eccentricitySquared * glm::pow(glm::sin(latitudeRadian), 2.0)); // 횡곡률 반경
    double t = glm::pow(glm::tan(latitudeRadian), 2.0);
    double c = secondEccentricitySquared * glm::pow(glm::cos(latitudeRadian), 2.0);
    double a = longitudeDelta * glm::cos(latitudeRadian);

    // 최종 x, y 계산
    // ...

    //point.x *= destination.pcs.parameters[Parameter::ScaleFactor];
    //point.y *= destination.pcs.parameters[Parameter::ScaleFactor];

    //point.x += destination.pcs.parameters[Parameter::FalseEasting];
    //point.y += destination.pcs.parameters[Parameter::FalseNorthing];
}