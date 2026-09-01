#include <pch.h>
#include <vector>
#include <execution>
#include "CoordinateSystem.h"
#include "Layer.h"


std::string ToLower(std::string str)
{
    for (char& ch : str)
        if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';

    return str;
}

// prj 파싱
void CoordinateSystem::PrjParse(std::vector<uint8_t>& prjBuffer)
{
    m_ptr = reinterpret_cast<const char*>(prjBuffer.data());
    m_end = m_ptr + prjBuffer.size();
    std::string token;

    while (m_ptr < m_end) {
        token = ReadValue();

        if      (token.empty())     continue;
        if      (token == "PROJCS") { pcs.name = ToLower(ReadValue()); }
        else if (token == "GEOGCS") { gcs.name = ToLower(ReadValue()); }
        else if (token == "DATUM")  { gcs.datumName = ToLower(ReadValue()); }
        else if (token == "SPHEROID") {
            gcs.ellipsoid.name              = ToLower(ReadValue());
            gcs.ellipsoid.semiMajorAxis     = std::stod(ReadValue());
            gcs.ellipsoid.inverseFlattening = std::stod(ReadValue());
        }
        else if (token == "PRIMEM") {
            ReadValue();
            gcs.primeMeridian = std::stod(ReadValue());
        }
        else if (token == "PROJECTION") {
            pcs.projectionName = ToLower(ReadValue());
            isProjected        = true;
        }
        else if (token == "PARAMETER") {
            std::string name  = ToLower(ReadValue());
            double      value = std::stod(ReadValue());

            if      (name == "false_easting"     ) pcs.parameters[Parameter::FalseEasting]     = value;
            else if (name == "false_northing"    ) pcs.parameters[Parameter::FalseNorthing]    = value;
            else if (name == "central_meridian"  ) pcs.parameters[Parameter::CentralMeridian]  = value;
            else if (name == "latitude_of_origin") pcs.parameters[Parameter::LatitudeOfOrigin] = value;
            else if (name == "scale_factor"      ) pcs.parameters[Parameter::ScaleFactor]      = value;
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
        unsigned char ch = *m_ptr++;

        if      (ch == '"' && doubleQuotesCount == 0) { doubleQuotesCount++; }
        else if (ch == '"' && doubleQuotesCount == 1) { break; }
        else if (ch == ' ' && token.empty())          continue;
        else if (ch != ',' && ch != '[' && ch != ']')  token += ch;
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
void CoordinateTransformer::TransformCoordinate(CoordinateSystem& prjCoordinate, Layer& newLayer)
{
    CoordinateSystem destCoordinate; // 목표 좌표계(5186)

    destCoordinate.SetTargetCoordinate(5186);
    parameter.sourceEllipsoid.Set(prjCoordinate);
    parameter.sourceProjection.Set(prjCoordinate, parameter.sourceEllipsoid);
    parameter.destinationEllipsoid.Set(destCoordinate);
    parameter.destinationProjection.Set(destCoordinate, parameter.destinationEllipsoid);
    parameter.helmert.Set();

    if (prjCoordinate.pcs.name != destCoordinate.pcs.name || prjCoordinate.gcs.name != destCoordinate.gcs.name) {
        double layerMinX, layerMinY, layerMaxX, layerMaxY;
        double objMinX, objMinY, objMaxX, objMaxY;
        layerMinX = layerMinY = objMinX = objMinY = std::numeric_limits<double>::max();
        layerMaxX = layerMaxY = objMaxX = objMaxY = std::numeric_limits<double>::lowest();

        // Point 객체 변환 및 MBR 재계산
        if (newLayer.pointObjects.size() > 0) {
            for (auto& obj : newLayer.pointObjects) {
                obj.point = TransformPoint(obj.point);
                obj.SetMBRBox(); // 변환된 좌표로 갱신

                // 레이어 mbr 박스 계산
                if (obj.point.x < layerMinX) layerMinX = obj.point.x;
                if (obj.point.x > layerMaxX) layerMaxX = obj.point.x;
                if (obj.point.y < layerMinY) layerMinY = obj.point.y;
                if (obj.point.y > layerMaxY) layerMaxY = obj.point.y;
            }
        }

        // PolyLine / Polygon 객체 변환 (람다 활용)
        auto transformPolyObjects = [&](auto& objects) { // std::execution::par로 병렬화
            std::for_each(std::execution::par, objects.begin(), objects.end(), [&](auto& obj) {
                double localMinX = std::numeric_limits<double>::max(),    localMinY = std::numeric_limits<double>::max();
                double localMaxX = std::numeric_limits<double>::lowest(), localMaxY = std::numeric_limits<double>::lowest();

                for (auto& point : obj.points) {
                    point = TransformPoint(point);
                    localMinX = std::min(localMinX, point.x);
                    localMaxX = std::max(localMaxX, point.x);
                    localMinY = std::min(localMinY, point.y);
                    localMaxY = std::max(localMaxY, point.y);
                }
                obj.SetMBRBox(localMinX, localMinY, localMaxX, localMaxY);
            });

            // 병렬 루프 종료 후 순차로 레이어 MBR 집계 (레이스 컨디션 방지)
            for (auto& obj : objects) {
                if (obj.mbrBox.minX < layerMinX) layerMinX = obj.mbrBox.minX;
                if (obj.mbrBox.maxX > layerMaxX) layerMaxX = obj.mbrBox.maxX;
                if (obj.mbrBox.minY < layerMinY) layerMinY = obj.mbrBox.minY;
                if (obj.mbrBox.maxY > layerMaxY) layerMaxY = obj.mbrBox.maxY;
            }
        };

        if (newLayer.polyLineObjects.size() > 0) transformPolyObjects(newLayer.polyLineObjects);
        if (newLayer.polygonObjects.size()  > 0) transformPolyObjects(newLayer.polygonObjects);
        newLayer.SetMBRBox(layerMinX, layerMinY, layerMaxX, layerMaxY); // 레이어 mbr 갱신

        // 변환 결과 출력
        TCHAR buf[256];
        _stprintf_s(buf, _T("[PROJ] 변환 완료: MBR (%.6f,%.6f)~(%.6f,%.6f)\n"), layerMinX, layerMinY, layerMaxX, layerMaxY);
        OutputDebugString(buf);
    }
}


// 좌표계 변환 - 정점 좌표 변환
glm::dvec2 CoordinateTransformer::TransformPoint(glm::dvec2& point)
{
    // 현재는 5186으로만 변환
    // 투영 좌표계(미터) -> 경위도 좌표계로 역투영
    if (parameter.sourceProjection.isProjected) InverseProjection(point); // point의 x y가 각각 경도와 위도 값으로 변환
    
    // 타원체 변환 Bessel -> GRS80 (현재는 하나만 적용)
    if (parameter.sourceEllipsoid.isBessel) { // bessel이 타원체 이름에 포함되어 있을 경우 변환
        glm::dvec3 ecef = LLAtoECEF(point);
        EllipsoidTransform(ecef);
        point = ECEFtoLLA(ecef);
    }

    // 경위도 좌표계 -> 투영 좌표계(미터)로 정투영
    Projection(point); // point의 경도와 위도가 각각 x y 값으로 변환

    return point;
}


// 역투영
void CoordinateTransformer::InverseProjection(glm::dvec2& point)
{
    auto& proj      = parameter.sourceProjection;
    auto& ellipsoid = parameter.sourceEllipsoid;

    // 가산치 (음수 보정) 제거
    point.x -= proj.falseEasting;
    point.y -= proj.falseNorthing;

    // 축척 제거
    point.x /= proj.scaleFactor;
    point.y /= proj.scaleFactor;

    double baseLatitude = (proj.arcLength + point.y) / (ellipsoid.semiMajorAxis * ellipsoid.arcFactor0);
    double footpointLatitude = baseLatitude
        + (3.0    * ellipsoid.footPointE1 / 2.0  - 27.0 * ellipsoid.footPointE3 / 32.0) * glm::sin(2.0 * baseLatitude)
        + (21.0   * ellipsoid.footPointE2 / 16.0 - 55.0 * ellipsoid.footPointE4 / 32.0) * glm::sin(4.0 * baseLatitude)
        + (151.0  * ellipsoid.footPointE3 / 96.0)  * glm::sin(6.0 * baseLatitude)
        + (1097.0 * ellipsoid.footPointE4 / 512.0) * glm::sin(8.0 * baseLatitude);

    // 제2이심률의 제곱
    double sinLat = glm::sin(footpointLatitude);
    double cosLat = glm::cos(footpointLatitude);
    double tanLat = sinLat / cosLat;
    double eTemp = glm::sqrt(1.0 - ellipsoid.eccentricity2 * sinLat * sinLat); // 재활용을 위한 이심률 중간식

    // 발 위도에서의 지구 곡률 파라미터 계산, 발 위도 위치에서 지구가 가로/세로로 각각 얼마나 휘어있는지 반지름을 구하고 보정용 변수를 만듦
    double transverseRadius = ellipsoid.semiMajorAxis / eTemp; // 가로 곡률 반경
    double meridionalRadius = ellipsoid.semiMajorAxis * (1.0 - ellipsoid.eccentricity2) / (eTemp * eTemp * eTemp); // 세로 곡률 반경

    // 각도 임시 변수, 테일러 급수에 들어갈 삼각함수 임시 값들
    double tanLat2 = tanLat * tanLat;
    double se2CosLat2 = ellipsoid.secondEccentricity2 * cosLat * cosLat;
    double d = point.x / transverseRadius; // 가로 이동 각도 비율, 순수 가로 이동 거리를 횡곡률로 나누어 라디안 형태의 각도 비율로 만든다

    // 테일러 급수 차수 증가
    double d2 = d  * d,  d3 = d2 * d,  d4 = d3 * d,  d5 = d4 * d;
    double d6 = d5 * d,  d7 = d6 * d,  d8 = d7 * d;
    double se2CosLat4 = se2CosLat2 * se2CosLat2;
    double tanLat4 = tanLat2 * tanLat2;
    double tanLat6 = tanLat4 * tanLat2;

    double latitudeRadian = footpointLatitude - (transverseRadius * tanLat / meridionalRadius) * (d2 / 2.0
        - (5.0  + 3.0  * tanLat2 + 10.0  * se2CosLat2 - 4.0  * se2CosLat4 - 9.0 * ellipsoid.secondEccentricity2) * d4 / 24.0
        + (61.0 + 90.0 * tanLat2 + 298.0 * se2CosLat2 + 45.0 * tanLat4 - 252.0 * ellipsoid.secondEccentricity2 - 3.0 * se2CosLat4) * d6 / 720.0
        - (1385.0 + 3633.0 * tanLat2 + 4095.0 * tanLat4 + 1575.0 * tanLat6) * d8 / 40320.0); // <-- 8차항 추가

    double longitudeRadian = proj.longitudeOrigin + (1.0 / cosLat) * (d
        - (1.0 + 2.0 * tanLat2 + se2CosLat2) * d3 / 6.0
        + (5.0 - 2.0 * se2CosLat2 + 28.0 * tanLat2 - 3.0 * se2CosLat4 + 8.0 * ellipsoid.secondEccentricity2 + 24.0 * tanLat4) * d5 / 120.0
        - (61.0 + 662.0 * tanLat2 + 1320.0 * tanLat4 + 720.0 * tanLat6) * d7 / 5040.0); // <-- 7차항 추가
    
    point.x = longitudeRadian / proj.angleUnit; // 경도 (x축에 매핑)
    point.y = latitudeRadian  / proj.angleUnit; // 위도 (y축에 매핑)
}

// 정투영
void CoordinateTransformer::Projection(glm::dvec2& point)
{
    auto& proj      = parameter.destinationProjection;
    auto& ellipsoid = parameter.destinationEllipsoid;

    double longitudeRadian = point.x * proj.angleUnit; // 현재 경도
    double latitudeRadian  = point.y * proj.angleUnit; // 현재 위도

    // 자오선 호 길이 차이
    double arcLengthDelta = ellipsoid.semiMajorAxis * (ellipsoid.arcFactor0 * (latitudeRadian - proj.latitudeOrigin)
        - ellipsoid.arcFactor2 * (glm::sin(2.0 * latitudeRadian) - proj.sinLat2)
        + ellipsoid.arcFactor4 * (glm::sin(4.0 * latitudeRadian) - proj.sinLat4)
        - ellipsoid.arcFactor6 * (glm::sin(6.0 * latitudeRadian) - proj.sinLat6));

    double sinLat = glm::sin(latitudeRadian);
    double cosLat = glm::cos(latitudeRadian);
    double tanLat = sinLat / cosLat;
    double tanLat2 = tanLat * tanLat; // 위도 탄젠트 항

    double transverseRadius = ellipsoid.semiMajorAxis / glm::sqrt(1.0 - ellipsoid.eccentricity2 * sinLat * sinLat); // 횡곡률 반경
    double se2CosLat2 = ellipsoid.secondEccentricity2 * cosLat * cosLat; // 이심률 코사인 항
    double lonFactor = (longitudeRadian - proj.longitudeOrigin) * cosLat; // 경도차 보정 항

    // 테일러 급수 차수 증가
    double lonFactor2 = lonFactor  * lonFactor;
    double lonFactor3 = lonFactor2 * lonFactor;
    double lonFactor4 = lonFactor3 * lonFactor;
    double lonFactor5 = lonFactor4 * lonFactor;
    double lonFactor6 = lonFactor5 * lonFactor;
    double lonFactor7 = lonFactor6 * lonFactor;
    double lonFactor8 = lonFactor7 * lonFactor;

    point.x = transverseRadius * (lonFactor + (1.0 - tanLat2 + se2CosLat2) * lonFactor3 / 6.0
        + (5.0 - 18.0 * tanLat2 + tanLat2 * tanLat2 + 14.0 * se2CosLat2 - 58.0 * tanLat2 * se2CosLat2) * lonFactor5 / 120.0
        + (61.0 - 479.0 * tanLat2 + 179.0 * tanLat2 * tanLat2 - tanLat2 * tanLat2 * tanLat2)     * lonFactor7 / 5040.0); // <-- 7차항 추가

    point.y = arcLengthDelta + transverseRadius * glm::tan(latitudeRadian)
        * (lonFactor2 / 2.0 + (5.0 - tanLat2 + 9.0 * se2CosLat2 + 4.0 * se2CosLat2 * se2CosLat2) * lonFactor4 / 24.0
        + (61.0 - 58.0 * tanLat2 + tanLat2 * tanLat2 + 270.0 * se2CosLat2 - 330.0 * tanLat2 * se2CosLat2) * lonFactor6 / 720.0
        + (1385.0 - 3111.0 * tanLat2 + 543.0 * tanLat2 * tanLat2 - tanLat2 * tanLat2 * tanLat2)     * lonFactor8 / 40320.0); // <-- 8차항 추가
    
    point.x *= proj.scaleFactor;
    point.y *= proj.scaleFactor;

    point.x += proj.falseEasting;
    point.y += proj.falseNorthing;
}

glm::dvec3 CoordinateTransformer::LLAtoECEF(glm::dvec2& llaPoint)
{
    double unit      = parameter.sourceProjection.angleUnit;
    auto&  ellipsoid = parameter.sourceEllipsoid;

    double longitudeRadian = llaPoint.x * unit; // 현재 경도
    double latitudeRadian  = llaPoint.y * unit; // 현재 위도
    double sinLatRad = glm::sin(latitudeRadian);
    double cosLatRad = glm::cos(latitudeRadian);
    double sinLonRad = glm::sin(longitudeRadian);
    double cosLonRad = glm::cos(longitudeRadian);

    double height = 0.0;
    double transverseRadius = ellipsoid.semiMajorAxis / glm::sqrt(1.0 - ellipsoid.eccentricity2 * sinLatRad * sinLatRad);

    glm::dvec3 ecefPoint;
    ecefPoint.x = (transverseRadius + height) * cosLatRad * cosLonRad;
    ecefPoint.y = (transverseRadius + height) * cosLatRad * sinLonRad;
    ecefPoint.z = (transverseRadius * (1 - ellipsoid.eccentricity2) + height) * sinLatRad;

    return ecefPoint;
}

glm::dvec2 CoordinateTransformer::ECEFtoLLA(glm::dvec3& ecefPoint)
{
    double unit      = parameter.destinationProjection.angleUnit;
    auto&  ellipsoid = parameter.destinationEllipsoid;

    double longitude = glm::atan(ecefPoint.y, ecefPoint.x); // 경도
    double horizonLength = glm::sqrt(ecefPoint.x * ecefPoint.x + ecefPoint.y * ecefPoint.y);
    double theta     = glm::atan(ecefPoint.z * ellipsoid.semiMajorAxis, horizonLength * ellipsoid.semiMinorAxis);
    double sinTheta  = glm::sin(theta);
    double cosTheta  = glm::cos(theta);
    double latitude  = glm::atan(ecefPoint.z + ellipsoid.secondEccentricity2 * ellipsoid.semiMinorAxis * sinTheta * sinTheta * sinTheta, 
        horizonLength - ellipsoid.eccentricity2 * ellipsoid.semiMajorAxis * cosTheta * cosTheta * cosTheta); // 현재 위도
    double sinLat    = glm::sin(latitude);

    double transverseRadius = ellipsoid.semiMajorAxis / glm::sqrt(1.0 - ellipsoid.eccentricity2 * sinLat * sinLat); // 횡곡률 반경
    double height = horizonLength / glm::cos(latitude) - transverseRadius;

    return glm::dvec2(longitude / unit, latitude / unit);
}

// 타원체 변환 (bessel -> grs80) 현재는 이 경우 한 가지만 실행
void CoordinateTransformer::EllipsoidTransform(glm::dvec3& ecefPoint)
{
    // Bessel -> GRS80 변환 계수
    // x축이동, y축이동, z축이동, x축회전, y축회전, z축회전, 보정계수, x축회전중심, y축회전중심, z축회전중심
    // -145.907, 505.034, 685.756, -1.162, 2.347, 1.592, 6.342, -3159521.31, 4068151.32, 3748113.85
    // 회전 수치는 도가 아니라 초 단위, 도 단위로 변환 후 라디안으로 바꿔 사용해야 함
    // 보정 계수의 단위는 ppm(백만분율), 수식에 적용할 때는 1 + (6.342 * 10^{-6}) 형태로 적용해야 함

    auto& helmert = parameter.helmert;

    // 회전 중심점을 원점으로 이동 (Origin Shift)
    double x0 = ecefPoint.x - helmert.pivotX;
    double y0 = ecefPoint.y - helmert.pivotY;
    double z0 = ecefPoint.z - helmert.pivotZ;

    // 미세 회전 및 스케일 적용 (Small Angle Approximation 행렬)
    // 회전각이 극히 작으므로 sin(r) = r, cos(r) = 1 로 근사화된 3x3 행렬을 사용합니다.
    double xRot = helmert.scale * ( x0 + helmert.rotationX * y0 - helmert.rotationY * z0);
    double yRot = helmert.scale * (-helmert.rotationZ * x0 + y0 + helmert.rotationX * z0);
    double zRot = helmert.scale * (helmert.rotationY * x0 - helmert.rotationX * y0 + z0);

    // 회전 중심점 원복 및 원점 이동량(Translation) 최종 적용
    ecefPoint.x = xRot + helmert.pivotX + helmert.translationX;
    ecefPoint.y = yRot + helmert.pivotY + helmert.translationY;
    ecefPoint.z = zRot + helmert.pivotZ + helmert.translationZ;
}

int CoordinateSystem::GuessEpsg() const
{
    auto& param = pcs.parameters;
    std::string ellipsoidLower = ToLower(gcs.ellipsoid.name);
    std::string pcsLower = ToLower(pcs.name);

    // 1. 투영 없음 -> 지리 좌표계
    if (!isProjected) {
        if (ellipsoidLower.find("wgs") != std::string::npos) return 4326;
        return 0;
    }

    double longitudeOrigin = param.count(Parameter::CentralMeridian) ? param.at(Parameter::CentralMeridian) : 0.0;
    double falseEasting    = param.count(Parameter::FalseEasting)    ? param.at(Parameter::FalseEasting)    : 0.0;
    double falseNorthing   = param.count(Parameter::FalseNorthing)   ? param.at(Parameter::FalseNorthing)   : 0.0;
    double scaleFactor     = param.count(Parameter::ScaleFactor)     ? param.at(Parameter::ScaleFactor)     : 1.0;

    // 2. 통합 좌표계 (5178 / 5179) - Bessel 체크보다 먼저
    if (std::abs(longitudeOrigin - 127.5)     < 0.001 &&
        std::abs(falseEasting    - 1000000.0) < 1.0   &&
        std::abs(falseNorthing   - 2000000.0) < 1.0   &&
        std::abs(scaleFactor     - 0.9996)    < 0.00001)
    {
        if (ellipsoidLower.find("bessel") != std::string::npos) 
            return 5178;
        return 5179;
    }

    // 3. Bessel + 중부원점 -> 2097
    if (ellipsoidLower.find("bessel") != std::string::npos)
        return 2097;

    // 4. GRS80 + 벨트 계열 (5185 ~ 5188)
    if (std::abs(falseEasting  - 200000.0) < 1.0 &&
        std::abs(falseNorthing - 600000.0) < 1.0 &&
        std::abs(scaleFactor   - 1.0)      < 0.00001)
    {
        if (std::abs(longitudeOrigin - 125.0) < 0.001) return 5185;
        if (std::abs(longitudeOrigin - 127.0) < 0.001) return 5186;
        if (std::abs(longitudeOrigin - 129.0) < 0.001) return 5187;
        if (std::abs(longitudeOrigin - 131.0) < 0.001) return 5188;
    }

    return 0;
}

// 타원체 역변환 (GRS80 -> Bessel), EllipsoidTransform의 역방향
void CoordinateTransformer::EllipsoidTransformInverse(glm::dvec3& ecefPoint)
{
    double dx = -145.907;
    double dy = 505.034;
    double dz = 685.756;

    double arcSecToRadian = glm::pi<double>() / 648000.0;
    double rx = -1.162 * arcSecToRadian;
    double ry = 2.347 * arcSecToRadian;
    double rz = 1.592 * arcSecToRadian;

    double s = 6.342 * 0.000001;

    double px = -3159521.31;
    double py = 4068151.32;
    double pz = 3748113.85;

    // 정방향(이동+회전+스케일)의 역순으로 되돌림
    double xTemp = ecefPoint.x - dx - px;
    double yTemp = ecefPoint.y - dy - py;
    double zTemp = ecefPoint.z - dz - pz;

    double invScale = 1.0 / (1.0 + s);

    // 정방향 회전 행렬의 전치(근사 역행렬)를 적용
    double x0 = invScale * (xTemp + rz * yTemp - ry * zTemp);
    double y0 = invScale * (-rz * xTemp + yTemp + rx * zTemp);
    double z0 = invScale * (ry * xTemp - rx * yTemp + zTemp);

    ecefPoint.x = x0 + px;
    ecefPoint.y = y0 + py;
    ecefPoint.z = z0 + pz;
}

// 좌표계 역변환 - source(현재, 5186) 좌표를 destination(원본 좌표계)으로 되돌림. TransformPoint의 반대 방향.
glm::dvec2 CoordinateTransformer::TransformPointInverse(glm::dvec2& point)
{
    if (parameter.sourceProjection.isProjected) InverseProjection(point); // 투영좌표(미터) -> 경위도

    // destination이 Bessel이면 GRS80 -> Bessel 역변환 필요
    if (parameter.destinationEllipsoid.isBessel) {
        glm::dvec3 ecef = LLAtoECEF(point);
        EllipsoidTransformInverse(ecef);
        point = ECEFtoLLA(ecef);
    }

    Projection(point); // 경위도 -> destination(원본) 투영좌표

    return point;
}


void EllipsoidParams::Set(CoordinateSystem& coordinate)
{
    flattening    = 1.0 / coordinate.gcs.ellipsoid.inverseFlattening; // 편평률
    semiMajorAxis = coordinate.gcs.ellipsoid.semiMajorAxis; // 장반경
    semiMinorAxis = semiMajorAxis * (1.0 - flattening);           // 단반경

    eccentricity2 = 2.0 * flattening - flattening * flattening;   // 제1이심률 제곱, 타원체의 납작함을 나타내는 값
    eccentricity4 = eccentricity2 * eccentricity2;
    eccentricity6 = eccentricity4 * eccentricity2;
    secondEccentricity2 = eccentricity2 / (1.0 - eccentricity2);

    arcFactor0 = 1.0 - eccentricity2 / 4.0 - 3.0 * eccentricity4 / 64.0 - 5.0 * eccentricity6 / 256.0;
    arcFactor2 = 3.0 * eccentricity2 / 8.0 + 3.0 * eccentricity4 / 32.0 + 45.0 * eccentricity6 / 1024.0;
    arcFactor4 = 15.0 * eccentricity4 / 256.0 + 45.0 * eccentricity6 / 1024.0;
    arcFactor6 = 35.0 * eccentricity6 / 3072.0;

    double eTemp = glm::sqrt(1.0 - eccentricity2); // 계산을 위한 이심률 중간식
    footPointE1 = (1.0 - eTemp) / (1.0 + eTemp); // 보정 상수, 발 위도용 이심률
    footPointE2 = footPointE1 * footPointE1;
    footPointE3 = footPointE2 * footPointE1;
    footPointE4 = footPointE3 * footPointE1;
    isBessel = coordinate.gcs.ellipsoid.name.find("bessel") != std::string::npos;
}

void ProjectionParams::Set(CoordinateSystem& coordinate, EllipsoidParams& ellipsoid)
{
    auto& parameters = coordinate.pcs.parameters;
    falseEasting    = parameters[Parameter::FalseEasting];
    falseNorthing   = parameters[Parameter::FalseNorthing];
    scaleFactor     = parameters[Parameter::ScaleFactor] > 0.0 ? parameters[Parameter::ScaleFactor] : 1.0;
    latitudeOrigin  = parameters[Parameter::LatitudeOfOrigin] * coordinate.gcs.unit;
    longitudeOrigin = parameters[Parameter::CentralMeridian]  * coordinate.gcs.unit;

    sinLat2 = glm::sin(2.0 * latitudeOrigin);
    sinLat4 = glm::sin(4.0 * latitudeOrigin);
    sinLat6 = glm::sin(6.0 * latitudeOrigin);

    arcLength = ellipsoid.semiMajorAxis * (ellipsoid.arcFactor0 * latitudeOrigin
        - ellipsoid.arcFactor2 * sinLat2 + ellipsoid.arcFactor4 * sinLat4 - ellipsoid.arcFactor6 * sinLat6);

    angleUnit   = coordinate.gcs.unit;
    isProjected = coordinate.isProjected;
}

void HelmertTransformParams::Set()
{
    // 이동 계수
    translationX = -145.907;
    translationY = 505.034;
    translationZ = 685.756;

    // 회전 계수
    double arcSecToRadian = glm::pi<double>() / 648000.0;
    rotationX = -1.162 * arcSecToRadian;
    rotationY = 2.347 * arcSecToRadian;
    rotationZ = 1.592 * arcSecToRadian;

    // 보정 계수
    scale = 1.0 + 6.342 * 0.000001;

    // 회전 중심
    pivotX = -3159521.31;
    pivotY = 4068151.32;
    pivotZ = 3748113.85;
}