#include <pch.h>
#include "ShpParser.h"
#include "SHPLoader.h"
#include "SwapEndian.h"
#include "Layer.h"
#include <fstream>

// .shp 레코드 헤더
void ShpRecordHeader::Read(const uint8_t*& ptr)
{
    memcpy(&recordNumber, ptr, 4);
    recordNumber = SwapEndian(recordNumber);
    ptr += 4;

    memcpy(&contentLength, ptr, 4);
    contentLength = SwapEndian(contentLength) * 2;
    ptr += 4;
}

// .shp 파싱
void ShpParser::ShpParse(vector<uint8_t>& buffer, std::vector<ShxRecord>& shxIndex, ShpfileHeader fileHeader, Layer& layer)
{
    // 레코드 순차적으로 읽기
    const uint8_t* ptr = buffer.data(); // 시작점
    const uint8_t* end = buffer.data() + buffer.size();
    size_t recordCount = shxIndex.size();


    auto parseLoop = [&]<typename TObject>(std::vector<TObject>&objects, auto readFunc)
    {
        int32_t objectsOffset = static_cast<int32_t>(objects.size());
        objects.resize(objectsOffset + recordCount);
        for (size_t recordNum = 0; recordNum < recordCount; recordNum++) {
            ptr = buffer.data() + shxIndex[recordNum].offset;
            ShpRecordHeader recordHeader;
            recordHeader.Read(ptr);
            readFunc(ptr, objects[objectsOffset + recordNum]);
        }


        if (recordCount < 1) {
            while (ptr < end) {
                ShpRecordHeader recordHeader;
                recordHeader.Read(ptr);
                readFunc(ptr, objects[objectsOffset + recordCount]);
			}
        }
    };

    switch (fileHeader.shapeType) {
    case 1:  parseLoop(layer.pointObjects,      [&](const uint8_t*& ptr, PointObject& obj)      { ReadPoint (ptr, obj); });      break;
    case 11: parseLoop(layer.pointObjects,      [&](const uint8_t*& ptr, PointObject& obj)      { ReadPointZ(ptr, obj); });      break;
    case 21: parseLoop(layer.pointObjects,      [&](const uint8_t*& ptr, PointObject& obj)      { ReadPointM(ptr, obj); });      break;

    case 3:  parseLoop(layer.polyLineObjects,   [&](const uint8_t*& ptr, PolyObject& obj)       { ReadPoly (ptr, obj); });       break;
    case 13: parseLoop(layer.polyLineObjects,   [&](const uint8_t*& ptr, PolyObject& obj)       { ReadPolyZ(ptr, obj); });       break;
    case 23: parseLoop(layer.polyLineObjects,   [&](const uint8_t*& ptr, PolyObject& obj)       { ReadPolyM(ptr, obj); });       break;

    case 5:  parseLoop(layer.polygonObjects,    [&](const uint8_t*& ptr, PolyObject& obj)       { ReadPoly (ptr, obj); });       break;
    case 15: parseLoop(layer.polygonObjects,    [&](const uint8_t*& ptr, PolyObject& obj)       { ReadPolyZ(ptr, obj); });       break;
    case 25: parseLoop(layer.polygonObjects,    [&](const uint8_t*& ptr, PolyObject& obj)       { ReadPolyM(ptr, obj); });       break;

    case 8:  parseLoop(layer.multiPointObjects, [&](const uint8_t*& ptr, MultiPointObject& obj) { ReadMultiPoint (ptr, obj); }); break;
    case 18: parseLoop(layer.multiPointObjects, [&](const uint8_t*& ptr, MultiPointObject& obj) { ReadMultiPointZ(ptr, obj); }); break;
    case 28: parseLoop(layer.multiPointObjects, [&](const uint8_t*& ptr, MultiPointObject& obj) { ReadMultiPointM(ptr, obj); }); break;
    
    case 31: parseLoop(layer.multiPatchObjects, [&](const uint8_t*& ptr, MultiPatchObject& obj) { ReadMultiPatch(ptr, obj); });  break;
    }
}

// .shp 레코드 콘텐츠
void ShpParser::ReadPoint(const uint8_t*& ptr, PointObject& object)
{
    memcpy(&object.shapeType, ptr, 4);
    ptr += 4;

    if (object.shapeType == 0) return;

    memcpy(&object.point, ptr, sizeof(glm::dvec2));
    ptr += sizeof(glm::dvec2);

    object.SetMBRBox();
}
void ShpParser::ReadPointZ(const uint8_t*& ptr, PointObject& object)
{
    ReadPoint(ptr, object);

    memcpy(&object.z, ptr, sizeof(double));
    ptr += sizeof(double);

    memcpy(&object.m, ptr, sizeof(double));
    ptr += sizeof(double);
}
void ShpParser::ReadPointM(const uint8_t*& ptr, PointObject& object)
{
    ReadPoint(ptr, object);

    memcpy(&object.m, ptr, sizeof(double));
    ptr += sizeof(double);
}

void ShpParser::ReadPoly(const uint8_t*& ptr, PolyObject& object)
{
    memcpy(&object.shapeType, ptr, 4);
    ptr += 4;

    if (object.shapeType == 0) return;

    memcpy(&object.mbrBox, ptr, 32);
    ptr += 32;

    uint32_t partsCount;
    memcpy(&partsCount, ptr, 4);
    ptr += 4;

    uint32_t pointsCount;
    memcpy(&pointsCount, ptr, 4);
    ptr += 4;

    object.parts.resize(partsCount);
    memcpy(object.parts.data(), ptr, partsCount * sizeof(int32_t));
    ptr += partsCount * sizeof(int32_t);

    object.points.resize(pointsCount);
    memcpy(object.points.data(), ptr, pointsCount * sizeof(glm::dvec2));
    ptr += pointsCount * sizeof(glm::dvec2);
}
void ShpParser::ReadPolyZ(const uint8_t*& ptr, PolyObject& object)
{
    ReadPoly(ptr, object);
    ReadTypeZ(ptr, object);
    ReadTypeM(ptr, object);
}
void ShpParser::ReadPolyM(const uint8_t*& ptr, PolyObject& object)
{
    ReadPoly(ptr, object);
    ReadTypeM(ptr, object);
}

void ShpParser::ReadMultiPoint(const uint8_t*& ptr, MultiPointObject& object)
{
    memcpy(&object.shapeType, ptr, 4);
    ptr += 4;

    if (object.shapeType == 0) return;

    memcpy(&object.mbrBox, ptr, 32);
    ptr += 32;

    uint32_t pointsCount;
    memcpy(&pointsCount, ptr, 4);
    ptr += 4;

    object.points.resize(pointsCount);
    memcpy(object.points.data(), ptr, pointsCount * sizeof(glm::dvec2));
    ptr += pointsCount * sizeof(glm::dvec2);
}
void ShpParser::ReadMultiPointZ(const uint8_t*& ptr, MultiPointObject& object)
{
    ReadMultiPoint(ptr, object);
    ReadTypeZ(ptr, object);
    ReadTypeM(ptr, object);
}
void ShpParser::ReadMultiPointM(const uint8_t*& ptr, MultiPointObject& object)
{
    ReadMultiPoint(ptr, object);
    ReadTypeM(ptr, object);
}

void ShpParser::ReadMultiPatch(const uint8_t*& ptr, MultiPatchObject& object)
{
    memcpy(&object.shapeType, ptr, 4);
    ptr += 4;

    if (object.shapeType == 0) return;

    memcpy(&object.mbrBox, ptr, 32);
    ptr += 32;

    uint32_t partsCount;
    memcpy(&partsCount, ptr, 4);
    ptr += 4;

    uint32_t pointsCount;
    memcpy(&pointsCount, ptr, 4);
    ptr += 4;

    object.parts.resize(partsCount);
    memcpy(object.parts.data(), ptr, partsCount * 4);
    ptr += partsCount * 4;

    object.partTypes.resize(partsCount);
    memcpy(object.partTypes.data(), ptr, partsCount * 4);
    ptr += partsCount * 4;

    object.points.resize(pointsCount);
    memcpy(object.points.data(), ptr, pointsCount * sizeof(glm::dvec2));
    ptr += pointsCount * sizeof(glm::dvec2);

    ReadTypeZ(ptr, object);
    ReadTypeM(ptr, object);
}