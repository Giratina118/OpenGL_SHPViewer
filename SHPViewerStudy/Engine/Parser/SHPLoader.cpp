#include <pch.h>
#include <fstream>
#include "SHPLoader.h"
#include "ShpParser.h"
#include "ShxParser.h"
#include "DbfParser.h"
#include "CoordinateSystem.h"
#include "Layer.h"

void ShpfileHeader::ReadHeader(const uint8_t*& ptr)
{
    memcpy(&fileCode, ptr, 4);
    fileCode = SwapEndian(fileCode);
    ptr += 24;

    memcpy(&fileLength, ptr, 4);
    fileLength = SwapEndian(fileLength) * 2;
    ptr += 4;

    memcpy(&version, ptr, 4);
    ptr += 4;

    memcpy(&shapeType, ptr, 4);
    ptr += 4;

    memcpy(&boundinBoxXY, ptr, 32);
    ptr += 32;

    memcpy(&boundinBoxZM, ptr, 32);
    ptr += 32;
}

// 파일 열기
vector<uint8_t> SHPLoader::OpenFile(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};      // 파일 없으면 종료

    std::streamsize size = file.tellg(); // 전체 크기, 현재 포인터의 위치(끝) 읽음
    if (size < 100) return {};           // 크기가 이상하면 종료

    file.seekg(0, std::ios::beg);        // 포인터를 다시 맨 앞으로 이동
    std::vector<uint8_t> buffer(size);   // 크기만큼 버퍼를 한 번에 할당
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return {};

    return buffer;
}

// TODO: 현재는 shx와 dbf가 없으면 안 열리도록 되어있지만 추후에 두 가지가 없어도 열리도록 수정
void SHPLoader::Parse(std::filesystem::path filePath, LayerManager& layerManager)
{
    // 파서 생성
    ShxParser shxParser;
    ShpParser shpParser;
    DbfParser dbfParser;
    CoordinateSystem prjCoordinate;
    ShpfileHeader shxHeader, shpHeader;
    std::vector<ShxRecord> shxRecords;
    const uint8_t* shpPtr = nullptr, * shxPtr = nullptr; // 파일 시작점

	bool hasShx = true, hasDbf = true, hasPrj = true;
    std::filesystem::path shpPath = filePath, shxPath = filePath, dbfPath = filePath, prjPath = filePath;
	
    auto ext = shpPath.extension();
    if (ext != ".shp" && ext != ".shx" && ext != ".dbf") return;

    // 파일 경로, 존재 확인
    shpPath.replace_extension(".shp");
    shxPath.replace_extension(".shx");
    dbfPath.replace_extension(".dbf");
    prjPath.replace_extension(".prj");

    if (!std::filesystem::exists(shpPath)) return;
    if (!std::filesystem::exists(shxPath)) hasShx = false;
    if (!std::filesystem::exists(dbfPath)) hasDbf = false;
    if (!std::filesystem::exists(prjPath)) hasPrj = false;

    // 파일 열기
	std::vector<uint8_t> shpBuffer, shxBuffer, dbfBuffer, prjBuffer;
               shpBuffer = OpenFile(shpPath);
    if(hasShx) shxBuffer = OpenFile(shxPath);
    if(hasDbf) dbfBuffer = OpenFile(dbfPath);
    if(hasPrj) prjBuffer = OpenFile(prjPath);

	// 헤더 파싱
                  shpPtr = shpBuffer.data(); shpHeader.ReadHeader(shpPtr);
    if (hasShx) { shxPtr = shxBuffer.data(); shxHeader.ReadHeader(shxPtr); }
    
    // 레이어 생성
    Layer& newLayer = layerManager.CreateLayer(shpPath.stem().string(), shpHeader.shapeType, shpHeader.boundinBoxXY);
    newLayer.m_hasShx = hasShx;
	newLayer.m_hasDbf = hasDbf;

	// 레코드 파싱
    if (hasShx) shxParser.ShxParse(shxPtr,    shxRecords, shxHeader.fileLength); // shx 레코드 파싱
    if (hasDbf) dbfParser.DbfParse(dbfBuffer, newLayer.m_dbfTable);              // dbf 헤더 + 레코드 파싱, layer.dbfTable에 저장
                shpParser.ShpParse(shpBuffer, shxRecords, shpHeader, newLayer);  // shp 레코드 파싱,        layer에 저장

    if (newLayer.m_dbfTable.floorPos != -1 || newLayer.m_dbfTable.heightPos != -1) newLayer.m_isBuilding = true; // 높이/층수 정보가 있으면 건물 레이어로 간주

    
    // 좌표계 파싱 & 변환
    if (hasPrj) { 
        prjCoordinate.PrjParse(prjBuffer); 

        TCHAR debugBuf[512];
        double a    = prjCoordinate.gcs.ellipsoid.semiMajorAxis;
        double invF = prjCoordinate.gcs.ellipsoid.inverseFlattening;
        double cm   = prjCoordinate.pcs.parameters[Parameter::CentralMeridian];
        double lo   = prjCoordinate.pcs.parameters[Parameter::LatitudeOfOrigin];
        double fe   = prjCoordinate.pcs.parameters[Parameter::FalseEasting];
        double fn   = prjCoordinate.pcs.parameters[Parameter::FalseNorthing];

        _stprintf_s(debugBuf, _T("\n[PRJ Parse Data] 장반경(a): %f, 편평률 역수(1/f): %f, 중심자오선: %f, 기준 위도: %f, FE: %f, FN: %f\n\n"),
            a, invF, cm, lo, fe, fn);
        OutputDebugString(debugBuf);

        
        // TODO: layerManager나 시스템 전역 설정에서 뷰어의 '목표 좌표계(Destination)'를 가져옵니다.
        CoordinateSystem destCoordinate;
        CoordinateTransformer transformer;

        destCoordinate.SetTargetCoordinate(5186);

        // 원본과 목표 좌표계가 다를 경우에만 변환 수행, TODO: epsg 번호 저장하여 비교하는 것으로 변경
        if (prjCoordinate.pcs.name != destCoordinate.pcs.name || prjCoordinate.gcs.name != destCoordinate.gcs.name) {
            double layerMinX = std::numeric_limits<double>::max();
            double layerMinY = std::numeric_limits<double>::max();
            double layerMaxX = std::numeric_limits<double>::lowest();
            double layerMaxY = std::numeric_limits<double>::lowest();

            double objMinX = std::numeric_limits<double>::max();
            double objMinY = std::numeric_limits<double>::max();
            double objMaxX = std::numeric_limits<double>::lowest();
            double objMaxY = std::numeric_limits<double>::lowest();

            // 1. Point 객체 변환 및 MBR 재계산
            for (auto& obj : newLayer.pointObjects) {
                obj.point = transformer.Transform(obj.point, prjCoordinate, destCoordinate);
                obj.SetMBRBox(); // 변환된 좌표로 갱신
            }

            // 2. PolyLine / Polygon 객체 변환 (람다 활용)
            auto transformPolyObjects = [&](auto& objects) {
                for (auto& obj : objects) {
                    objMinX = std::numeric_limits<double>::max();
                    objMinY = std::numeric_limits<double>::max();
                    objMaxX = std::numeric_limits<double>::lowest();
                    objMaxY = std::numeric_limits<double>::lowest();

                    for (auto& point : obj.points) {
                        point = transformer.Transform(point, prjCoordinate, destCoordinate);

                        // 객체 mbr 박스 계산
                        if (point.x < objMinX) objMinX = point.x;
                        if (point.x > objMaxX) objMaxX = point.x;
                        if (point.y < objMinY) objMinY = point.y;
                        if (point.y > objMaxY) objMaxY = point.y;
                    }

                    glm::dvec2 mbrMin = { objMinX, objMinY };
                    glm::dvec2 mbrMax = { objMaxX, objMaxY };
                    obj.SetMBRBox(mbrMin, mbrMax);
                    

                    // 레이어 mbr 박스 계산
                    if (objMinX < layerMinX) layerMinX = objMinX;
                    if (objMaxX > layerMaxX) layerMaxX = objMaxX;
                    if (objMinY < layerMinY) layerMinY = objMinY;
                    if (objMaxY > layerMaxY) layerMaxY = objMaxY;


                    //TCHAR buf[256];
                    //_stprintf_s(buf, _T("[OBJ MBR Box] minX: %f, minY: %f, maxX: %f, maxY: %f\n"), obj.mbrBox.minX, obj.mbrBox.minY, obj.mbrBox.maxX, obj.mbrBox.maxY);
                    //OutputDebugString(buf);
                    

                    // 주의: 도형의 정점들이 바뀌었으므로 obj의 mbrBox를 새로 계산하는 함수가 필요합니다.
                    // obj.UpdateMBRBox(); 
                }
            };

            transformPolyObjects(newLayer.polyLineObjects);
            transformPolyObjects(newLayer.polygonObjects);
            // multiPatchObjects 도 동일하게 처리

            // 3. 레이어 전체의 BoundingBox(m_boundingBox)도 새로 계산된 객체들의 MBR을 합쳐서 갱신해야 합니다.
            glm::dvec2 mbrMin = { layerMinX, layerMinY };
            glm::dvec2 mbrMax = { layerMaxX, layerMaxY };
            newLayer.SetMBRBox(mbrMin, mbrMax);


            TCHAR debugBuf[256];
            double a = prjCoordinate.gcs.ellipsoid.semiMajorAxis;
            double invF = prjCoordinate.gcs.ellipsoid.inverseFlattening;
            double cm = prjCoordinate.pcs.parameters[Parameter::CentralMeridian];
            double lo = prjCoordinate.pcs.parameters[Parameter::LatitudeOfOrigin];
            double fe = prjCoordinate.pcs.parameters[Parameter::FalseEasting];
            double fn = prjCoordinate.pcs.parameters[Parameter::FalseNorthing];

            _stprintf_s(debugBuf, _T("\n[Layer MBR Box] minX: %f,  minY: %f,  maxX: %f,  maxY: %f\n\n"),
                newLayer.m_boundingBox.minX, newLayer.m_boundingBox.minY, newLayer.m_boundingBox.maxX, newLayer.m_boundingBox.maxY);
            OutputDebugString(debugBuf);
        }

        
    }

	newLayer.m_quadTree->BuildQuadTree(); // 쿼드트리 빌드
}