#include <pch.h>
#include "ShxParser.h"
#include "SwapEndian.h"
#include <fstream>

// .shx 레코드 파싱
void ShxParser::ShxParse(const uint8_t* ptr, std::vector<ShxRecord>& records, uint32_t fileLength)
{
    uint32_t recordCount = (fileLength - 100) / 8;
    records.resize(recordCount);

    for (int i = 0; i < recordCount; i++) ReadRecord(ptr, records[i]);
}

// .shx 레코드
void ShxParser::ReadRecord(const uint8_t*& ptr, ShxRecord& record) 
{
    memcpy(&record.offset, ptr, 4);
    record.offset = SwapEndian(record.offset) * 2;
    ptr += 4;

    memcpy(&record.contentLength, ptr, 4);
    record.contentLength = SwapEndian(record.contentLength) * 2;
    ptr += 4;
}