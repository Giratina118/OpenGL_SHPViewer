#include <pch.h>
#include <vector>
#include "CoordinateSystem.h"

// 좌표계 변환
glm::dvec2 CoordinateTransformer::Transform(glm::dvec2& point, CoordinateSystem& source, CoordinateSystem& destination)
{
    // 현재는 5186으로만 변환
    // 일단 GRS80으로 동일하다고 가정, 다른 경우는 이후 추가

    point.x -= source.pcs.parameters[Parameter::FalseEasting];
    point.y -= source.pcs.parameters[Parameter::FalseNorthing];

    point.x /= source.pcs.parameters[Parameter::ScaleFactor];
    point.y /= source.pcs.parameters[Parameter::ScaleFactor];

    // 발 위도 (y좌표값만을 고려했을 때의 가상 위도) 계산, 테일러 급수로 근사
    
    double arcLength; // 원점에서 위도까지의 호 길이

    double flattening = 1.0 / source.gcs.ellipsoid.inverseFlattening; // 편평률
    double eccentricitySquared = 2.0 * flattening - flattening * flattening; // 제1이심률 제곱, 타원체의 납작함을 나타내는 값


    point.x *= destination.pcs.parameters[Parameter::ScaleFactor];
    point.y *= destination.pcs.parameters[Parameter::ScaleFactor];

    point.x += destination.pcs.parameters[Parameter::FalseEasting];
    point.y += destination.pcs.parameters[Parameter::FalseNorthing];

	return glm::dvec2();
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
    unsigned char ch;
    std::string token = "";
    while (m_ptr < m_end) {
        char ch = *m_ptr++;

        if      (isalnum(ch) || ch == '_' || ch == '.') token += ch; // 숫자나 문자, _ . 일 때는 토큰에 이어붙임
        else if (!token.empty())                        break;
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