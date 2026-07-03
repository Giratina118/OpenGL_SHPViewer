#include <pch.h>
#include <fstream>
#include "SHPLoader.h"
#include "ShpParser.h"
#include "ShxParser.h"
#include "DbfParser.h"
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
    ShpfileHeader shxHeader, shpHeader;
    std::vector<ShxRecord> shxRecords;
    const uint8_t* shpPtr = nullptr, * shxPtr = nullptr; // 파일 시작점

	bool hasShx = true, hasDbf = true;
    std::filesystem::path shpPath = filePath, shxPath = filePath, dbfPath = filePath;

    auto ext = shpPath.extension();
    if (ext != ".shp" && ext != ".shx" && ext != ".dbf") return;

    // 파일 경로, 존재 확인
    shpPath.replace_extension(".shp");
    shxPath.replace_extension(".shx");
    dbfPath.replace_extension(".dbf");

    if (!std::filesystem::exists(shpPath)) return;
    if (!std::filesystem::exists(shxPath)) hasShx = false;
    if (!std::filesystem::exists(dbfPath)) hasDbf = false;

    // 파일 열기
	std::vector<uint8_t> shpBuffer, shxBuffer, dbfBuffer;
               shpBuffer = OpenFile(shpPath);
    if(hasShx) shxBuffer = OpenFile(shxPath);
    if(hasDbf) dbfBuffer = OpenFile(dbfPath);

	// 헤더 파싱
                  shpPtr = shpBuffer.data(); shpHeader.ReadHeader(shpPtr);
    if (hasShx) { shxPtr = shxBuffer.data(); shxHeader.ReadHeader(shxPtr); }
    
    // 레이어 생성
    Layer& newLayer = layerManager.CreateLayer(shpPath.string(), shpHeader.shapeType, shpHeader.boundinBoxXY);
    newLayer.m_hasShx = hasShx;
	newLayer.m_hasDbf = hasDbf;

	// 레코드 파싱
    if (hasShx) shxParser.ShxParse(shxPtr,    shxRecords, shxHeader.fileLength); // shx 레코드 파싱
    if (hasDbf) dbfParser.DbfParse(dbfBuffer, newLayer.dbfTable);                // dbf 헤더 + 레코드 파싱, layer.dbfTable에 저장
                shpParser.ShpParse(shpBuffer, shxRecords, shpHeader, newLayer);  // shp 레코드 파싱,        layer에 저장

	newLayer.m_quadTree.get()->BuildQuadTree(); // 쿼드트리 빌드
}