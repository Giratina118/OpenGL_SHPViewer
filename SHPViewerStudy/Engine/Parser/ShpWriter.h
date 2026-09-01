#pragma once
#include "SwapEndian.h"
#include "Parser/CoordinateSystem.h"
#include "Layer.h"
#include <fstream>

class Layer;

class ShpWriter
{
public:
    // layer.m_filePath 기준으로 .shp/.shx/.dbf를 다시 씀. isDeleted 객체는 제외.
    bool WriteLayer(Layer& layer);
    void WritePolyRecordContent(std::ofstream& shp, const PolyObject& obj, uint32_t shapeType);
    void WritePointRecordContent(std::ofstream& shp, const PointObject& obj);
    void WriteMainHeader(std::ofstream& out, int32_t fileLengthWords, uint32_t shapeType, const BoundingBox& box);

    void WriteBE32(std::ofstream& out, int32_t value)    { int32_t swapped = SwapEndian(value); out.write(reinterpret_cast<char*>(&swapped), 4); }
    void WriteLE32(std::ofstream& out, int32_t value)    { out.write(reinterpret_cast<char*>(&value), 4); }
    void WriteLEDouble(std::ofstream& out, double value) { out.write(reinterpret_cast<char*>(&value), 8); }
    int32_t PolyContentBytes(const PolyObject& obj) { return 4 + 32 + 4 + 4 + static_cast<int32_t>(obj.parts.size()) * 4 + static_cast<int32_t>(obj.points.size()) * 16;}
    int32_t PointContentBytes() { return 4 + 16; }

    bool BuildInverseTransformedCopy(Layer& layer, std::vector<PolyObject>& polygonBackup, std::vector<PolyObject>& lineBackup, std::vector<PointObject>& pointBackup);
};