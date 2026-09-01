#pragma once

#include <iostream>
#include <cstdint>
#include <vector>
#include <string>
#include "FeatureObject.h"
#include "ShxParser.h"

using namespace std;

class Layer;
struct ShpfileHeader;

// .shp 레코드 헤더 (8 Bytes)
struct ShpRecordHeader {
    uint32_t recordNumber;   // 레코드 번호, Big Endian
    uint32_t contentLength; // 콘탠츠 길이, Big Endian (16-bit words)

    void Read(const uint8_t*& ptr);
};

// .shp 파싱
class ShpParser
{
public:
	int32_t m_contourInterver = 50; // 등고선 간격 (dbf에서 읽어옴)

    void ShpParse(vector<uint8_t>& buffer, std::vector<ShxRecord>& shxIndex, ShpfileHeader fileHeader, Layer& layer);

private:
    // .shp 레코드 콘텐츠 (8 Bytes)
    void ReadPoint (const uint8_t*& ptr, PointObject& object);
    void ReadPointZ(const uint8_t*& ptr, PointObject& object);
    void ReadPointM(const uint8_t*& ptr, PointObject& object);

    void ReadPoly (const uint8_t*& ptr, PolyObject& object);
    void ReadPolyZ(const uint8_t*& ptr, PolyObject& object);
    void ReadPolyM(const uint8_t*& ptr, PolyObject& object);

    void ReadMultiPoint (const uint8_t*& ptr, MultiPointObject& object);
    void ReadMultiPointZ(const uint8_t*& ptr, MultiPointObject& object);
    void ReadMultiPointM(const uint8_t*& ptr, MultiPointObject& object);

    void ReadMultiPatch(const uint8_t*& ptr, MultiPatchObject& object);
    
    template<typename TObject>
    void ReadTypeZ(const uint8_t*& ptr, TObject& object)
    {
        ptr += 16; // zRange값

        size_t pointCount = object.points.size();
        size_t byteSizeZ = pointCount * sizeof(double);

        object.zValues.emplace(pointCount);
        memcpy(object.zValues->data(), ptr, byteSizeZ);
        ptr += byteSizeZ;
    }

    template<typename TObject>
    void ReadTypeM(const uint8_t*& ptr, TObject& object)
    {
        ptr += 16; // mRange값

        size_t pointCount = object.points.size();
        size_t byteSizeM = pointCount * sizeof(double);

        object.mValues.emplace(pointCount);
        memcpy(object.mValues->data(), ptr, byteSizeM);
        ptr += byteSizeM;
    }
};