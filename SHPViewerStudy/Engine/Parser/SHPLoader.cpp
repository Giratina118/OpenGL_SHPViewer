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

void SHPLoader::Parse(const std::filesystem::path& shpPath, const std::filesystem::path& shxPath, const std::filesystem::path& dbfPath, LayerManager& layerManager)
{
    // 파서 생성
    ShxParser shxParser;
	ShpParser shpParser;
    DbfParser dbfParser;
	ShpfileHeader shxHeader, shpHeader;
	std::vector<ShxRecord> shxRecords;

    // 파일 열기
    std::vector<uint8_t> shxBuffer = OpenFile(shxPath);
    std::vector<uint8_t> shpBuffer = OpenFile(shpPath);
    std::vector<uint8_t> dbfBuffer = OpenFile(dbfPath);

    // TODO: 현재는 shx와 dbf가 없으면 안 열리도록 되어있지만 추후에 두 가지가 없어도 열리도록 수정
    if (shxBuffer.empty() || shpBuffer.empty() || dbfBuffer.empty()) return;

    const uint8_t* shxPtr = shxBuffer.data(); // 시작점
    const uint8_t* shpPtr = shpBuffer.data(); // 시작점

	// 헤더 파싱
    shxHeader.ReadHeader(shxPtr);
    shpHeader.ReadHeader(shpPtr);
    
    TCHAR buf[256];
    _stprintf_s(buf, _T("\n\nParsingBox : min=(%.3f, %.3f) max=(%.3f, %.3f)\n"), shpHeader.boundinBoxXY.minX, shpHeader.boundinBoxXY.minY, shpHeader.boundinBoxXY.maxX, shpHeader.boundinBoxXY.maxY);
    OutputDebugString(buf);

    // TODO: 여러 레이어를 사용할 때를 위한 부분, 현재하나의 파일만 받고 있기 때문에 layer.layerDatas[0]만 사용 중
    Layer& newLayer = layerManager.CreateLayer(shpPath.string(), shpHeader.shapeType, shpHeader.boundinBoxXY);
    
	// 레코드 파싱
	shxParser.ShxParse(shxPtr,    shxRecords, shxHeader.fileLength);          // shx 레코드 파싱
	shpParser.ShpParse(shpBuffer, shxRecords, shpHeader.shapeType, newLayer); // shp 레코드 파싱,        layer에 저장
    dbfParser.DbfParse(dbfBuffer, newLayer.dbfTable);                         // dbf 헤더 + 레코드 파싱, layer.dbfTable에 저장


    _stprintf_s(buf, _T("polygon=%d line=%d point=%d\n"), (int)newLayer.polygonObjects.size(), (int)newLayer.polyLineObjects.size(), (int)newLayer.pointObjects.size());
    OutputDebugString(buf);

	newLayer.m_quadTree.get()->BuildQuadTree(); // 쿼드트리 빌드

    _stprintf_s(buf, _T("[CreateLayer] newLayer.m_quadTree.get()->m_nodes.size() = %d\n"), newLayer.m_quadTree.get()->m_nodes.size());
    OutputDebugString(buf);
}