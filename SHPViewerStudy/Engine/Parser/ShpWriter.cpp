#include <pch.h>
#include "ShpWriter.h"
#include "DbfWriter.h"

bool ShpWriter::WriteLayer(Layer& layer)
{
    if (layer.m_filePath.empty()) return false;

    std::filesystem::path shpPath = layer.m_filePath;
    std::filesystem::path shxPath = shpPath; shxPath.replace_extension(".shx");
    std::filesystem::path dbfPath = shpPath; dbfPath.replace_extension(".dbf");

    std::ofstream shp(shpPath, std::ios::binary | std::ios::trunc);
    std::ofstream shx(shxPath, std::ios::binary | std::ios::trunc);
    if (!shp.is_open() || !shx.is_open()) return false;

    std::vector<int32_t> keepIds;
    int32_t totalBytes = 0;
    BoundingBox layerBox;

    auto accumulate = [&](auto& objects, auto contentBytesFn) {
        for (int32_t i = 0; i < static_cast<int32_t>(objects.size()); i++) {
            if (objects[i].isDeleted) continue;
            keepIds.push_back(i);
            totalBytes += 8 + contentBytesFn(objects[i]);
            layerBox = layerBox.CombineBox(objects[i].mbrBox);
        }
    };

    if      (layer.m_shapeType == 1) accumulate(layer.pointObjects,    [this](const PointObject&)  { return PointContentBytes(); });
    else if (layer.m_shapeType == 3) accumulate(layer.polyLineObjects, [this](const PolyObject& o) { return PolyContentBytes(o); });
    else if (layer.m_shapeType == 5) accumulate(layer.polygonObjects,  [this](const PolyObject& o) { return PolyContentBytes(o); });
    else return false;

    WriteMainHeader(shp, (100 + totalBytes) / 2, layer.m_shapeType, layerBox);
    WriteMainHeader(shx, (100 + static_cast<int32_t>(keepIds.size()) * 8) / 2, layer.m_shapeType, layerBox);

    int32_t currentOffsetBytes = 100;
    int32_t recordNumber = 1;

    for (int32_t id : keepIds) {
        int32_t contentBytes = (layer.m_shapeType == 1) ? PointContentBytes()
            : PolyContentBytes(layer.m_shapeType == 3 ? layer.polyLineObjects[id] : layer.polygonObjects[id]);

        WriteBE32(shp, recordNumber);
        WriteBE32(shp, contentBytes / 2);

        if      (layer.m_shapeType == 1) WritePointRecordContent(shp, layer.pointObjects[id]);
        else if (layer.m_shapeType == 3) WritePolyRecordContent (shp, layer.polyLineObjects[id], layer.m_shapeType);
        else if (layer.m_shapeType == 5) WritePolyRecordContent (shp, layer.polygonObjects[id],  layer.m_shapeType);

        WriteBE32(shx, currentOffsetBytes / 2);
        WriteBE32(shx, contentBytes / 2);

        currentOffsetBytes += 8 + contentBytes;
        recordNumber++;
    }

    shp.close();
    shx.close();


    // dbf 저장 전에, HEIGHT 컬럼이 있다면 각 객체의 실제 mbrBox.height 값으로 동기화
    DBFTable& dbfTable = layer.m_dbfTable;
    if (dbfTable.heightPos != -1) {
        for (int32_t id : keepIds) {
            if (id < 0) continue;

            double currentHeight = 0.0;
            if      (layer.m_shapeType == 1 && id < static_cast<int32_t>(layer.pointObjects.size()))    currentHeight = layer.pointObjects[id].mbrBox.height;
            else if (layer.m_shapeType == 3 && id < static_cast<int32_t>(layer.polyLineObjects.size())) currentHeight = layer.polyLineObjects[id].mbrBox.height;
            else if (layer.m_shapeType == 5 && id < static_cast<int32_t>(layer.polygonObjects.size()))  currentHeight = layer.polygonObjects[id].mbrBox.height;

            if (id < static_cast<int32_t>(dbfTable.doubleColumns[dbfTable.heightPos].size()))
                dbfTable.doubleColumns[dbfTable.heightPos][id] = currentHeight;
        }
    }

    DbfWriter dbfWriter;
    return dbfWriter.WriteDbf(dbfPath, layer.m_dbfTable, keepIds);
}

void ShpWriter::WritePolyRecordContent(std::ofstream& shp, const PolyObject& obj, uint32_t shapeType)
{
    WriteLE32(shp, static_cast<int32_t>(shapeType));
    WriteLEDouble(shp, obj.mbrBox.minX); WriteLEDouble(shp, obj.mbrBox.minY);
    WriteLEDouble(shp, obj.mbrBox.maxX); WriteLEDouble(shp, obj.mbrBox.maxY);
    WriteLE32(shp, static_cast<int32_t>(obj.parts.size()));
    WriteLE32(shp, static_cast<int32_t>(obj.points.size()));
    for (int32_t part : obj.parts) WriteLE32(shp, part);
    for (const glm::dvec2& point : obj.points) { WriteLEDouble(shp, point.x); WriteLEDouble(shp, point.y); }
}

void ShpWriter::WritePointRecordContent(std::ofstream& shp, const PointObject& obj)
{
    WriteLE32(shp, static_cast<int32_t>(obj.shapeType));
    WriteLEDouble(shp, obj.point.x); WriteLEDouble(shp, obj.point.y);
}

void ShpWriter::WriteMainHeader(std::ofstream& out, int32_t fileLengthWords, uint32_t shapeType, const BoundingBox& box)
{
    WriteBE32(out, 9994);
    int32_t zero = 0;
    for (int i = 0; i < 5; i++) out.write(reinterpret_cast<char*>(&zero), 4);
    WriteBE32(out, fileLengthWords);
    WriteLE32(out, 1000);
    WriteLE32(out, static_cast<int32_t>(shapeType));
    WriteLEDouble(out, box.minX); WriteLEDouble(out, box.minY);
    WriteLEDouble(out, box.maxX); WriteLEDouble(out, box.maxY);
    double zeroD = 0.0;
    for (int i = 0; i < 4; i++) out.write(reinterpret_cast<char*>(&zeroD), 8);
}