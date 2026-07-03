#pragma once

#include "SwapEndian.h"
#include "FeatureObject.h"
#include <string>
#include <filesystem>

class LayerManager;

// .shp / .shx 메인 헤더 (100 Bytes)
struct ShpfileHeader
{
    uint32_t fileCode;   // 파일 코드, Big Endian (9994)
    uint32_t unused[5];  // Unused
    uint32_t fileLength; // 파일 길이, Big Endian (16-bit words)
    uint32_t version;    // 버전,      Little Endian (1000)
    uint32_t shapeType;  // 도형 타입, Little Endian (Point=1, PolyLine=3, Polygon=5, MultiPoint=8)
    BoundingBox boundinBoxXY; // xMin, yMin, xMax, yMax
    BoundingBox boundinBoxZM; // zMin, zMax, mMin, mMax

    void ReadHeader(const uint8_t*& ptr); // .shp .shx 메인 헤더 읽기
};

class SHPLoader
{
public:
    std::vector<uint8_t> OpenFile(const std::filesystem::path& filePath); // 파일 열기
	void Parse(std::filesystem::path filePath, LayerManager& layerManager);
};