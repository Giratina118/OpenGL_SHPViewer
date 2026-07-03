#include <pch.h>
#include "DbfParser.h"
#include <iostream>
#include <fstream>
#include <charconv>

// .dbf 헤더
void DbfHeader::Read(const uint8_t*& ptr) {
    memcpy(&version, ptr, 1);
    ptr += 1;

    memcpy(lastUpdate, ptr, 3);
    ptr += 3;

    memcpy(&recordCount, ptr, 4);
    ptr += 4;

    memcpy(&headerSize, ptr, 2);
    ptr += 2;

    memcpy(&recordSize, ptr, 2);
    ptr += 2;

    ptr += 20;
}

// .dbf 문자열 관리
void FixedStringColumn::Resize(int rows) {             // 초기화 없이
    buffer.resize(rows * width);
    rowCount = rows;
}
char* FixedStringColumn::GetRow(int row) {             // 문자열 반환
    return &buffer[row * width];
}
string_view FixedStringColumn::GetView(int row) const {// 복사 없이 문자열 보기
    return string_view(&buffer[row * width], width);
}
string_view FixedStringColumn::GetTrimmedView(int row) const {
    const char* start = &buffer[row * width];
    const char* end   = start + width;
    while (end > start && end[-1] == ' ') end--;
    return string_view(start, end - start);
}

// .dbf 파싱
void DbfParser::DbfParse(vector<uint8_t>& buffer, DBFTable& dbfTable) {
    const uint8_t* ptr = buffer.data(); // 시작점

    OutputDebugStringA(("dbfTable = " + std::to_string((uintptr_t)&dbfTable) + "\n").c_str());

    // 고정 헤더 읽기
    DbfHeader fileHeader;
    fileHeader.Read(ptr);

    if (fileHeader.headerSize % 32 != 1) return;
    uint16_t columnCount = (fileHeader.headerSize - 33) / 32; // 필드 개수
    dbfTable.columns.resize(columnCount);
    uint32_t rowCount = dbfTable.rowCount = fileHeader.recordCount;

    // 필드 기술자 읽기
    uint16_t currentOffset = 1; // 삭제 플래그(1바이트)
    for (uint16_t i = 0; i < columnCount; i++) {
        ColumnInfo& colInfo = dbfTable.columns[i];
        dbfTable.ReadColumnInfo(ptr, i);
        colInfo.offset = currentOffset;
        currentOffset += colInfo.length;

        if (dbfTable.columns[i].name == "HEIGHT")   dbfTable.heightPos = dbfTable.columns[i].typeIndex;
        if (dbfTable.columns[i].name == "GRND_FLR") dbfTable.floorPos  = dbfTable.columns[i].typeIndex;
    }

    dbfTable.intColumns.resize    (dbfTable.intCount,    vector<int32_t>(rowCount));
    dbfTable.doubleColumns.resize (dbfTable.doubleCount, vector<double>(rowCount));
    dbfTable.logicalColumns.resize(dbfTable.boolCount,   vector<unsigned char>(rowCount));
    dbfTable.stringColumns.resize (dbfTable.stringCount);

    for (const ColumnInfo& colInfo : dbfTable.columns) {
        if (colInfo.type == DBFColumnType::String) {
            FixedStringColumn& strCol = dbfTable.stringColumns[colInfo.typeIndex];
            strCol.width = colInfo.length;
            strCol.Resize(rowCount);

            /*
            TCHAR buf[256];
            _stprintf_s(buf, _T("\n[DBF 문자열] 문자열 길이 = %d,  %d"), static_cast<int32_t>(sizeof(strCol)), static_cast<int32_t>(strCol.width));
            OutputDebugString(buf);
            */
        }
    }

    /*
    // 한 레코드 당 569 바이트의 문자열,  총 472,691개의 레코드
    TCHAR buf[256]; 
    _stprintf_s(buf, _T("\n\n[DBF] 컬럼 개수 = %d,  %d,  %d,  레코드 개수 = %d"), static_cast<int32_t>(dbfTable.columns.size()), columnCount, static_cast<int32_t>(dbfTable.columns.capacity()), rowCount);
    OutputDebugString(buf);
    _stprintf_s(buf, _T("\n[DBF size]     정수 = %d,  실수 = %d,  논리 = %d,  문자 = %d"), static_cast<int32_t>(dbfTable.intColumns.size()), static_cast<int32_t>(dbfTable.doubleColumns.size()), static_cast<int32_t>(dbfTable.logicalColumns.size()), static_cast<int32_t>(dbfTable.stringColumns.size()));
    OutputDebugString(buf);
    _stprintf_s(buf, _T("\n[DBF capacity] 정수 = %d,  실수 = %d,  논리 = %d,  문자 = %d\n\n"), static_cast<int32_t>(dbfTable.intColumns.capacity()), static_cast<int32_t>(dbfTable.doubleColumns.capacity()), static_cast<int32_t>(dbfTable.logicalColumns.capacity()), static_cast<int32_t>(dbfTable.stringColumns.capacity()));
    OutputDebugString(buf);
    */


    if (*ptr != 0x0D) return;
    ptr++;

    // 레코드 읽기
    for (uint32_t row = 0; row < rowCount; row++) {
        const uint8_t* recordPtr = ptr;

        if (*recordPtr == 0x2A) { // 삭제된 데이터
            ptr += fileHeader.recordSize;
            continue;
        }

        for (int col = 0; col < columnCount; col++) {
            ColumnInfo& currentColumn = dbfTable.columns[col];
            const uint8_t* src = recordPtr + currentColumn.offset;
            const char* begin  = reinterpret_cast<const char*>(src);
            const char* end    = begin + currentColumn.length;

            switch (currentColumn.type) {
            case DBFColumnType::Int32:
            {
                // 공백 skip
                while (begin < end && *begin == ' ') begin++;

                int32_t value = 0;
                if (begin < end) {
                    auto result = from_chars(begin, end, value);
                    if (result.ec != errc()) value = 0;
                }
                dbfTable.intColumns[currentColumn.typeIndex][row] = value;
                break;
            }
            case DBFColumnType::Double:
            {
                while (begin < end && *begin == ' ') begin++;

                // from_chars대신 std::strtod 사용하기도 함, from_chars이 빠르지만 double형 지원이 길지 않음
                double value = 0.0;
                if (begin < end) {
                    auto result = from_chars(begin, end, value);
                    if (result.ec != errc()) value = 0.0;
                }
                dbfTable.doubleColumns[currentColumn.typeIndex][row] = value;
                break;
            }
            case DBFColumnType::String:
                memcpy(dbfTable.stringColumns[currentColumn.typeIndex].GetRow(row), src, currentColumn.length);
                break;
            case DBFColumnType::Bool:
                dbfTable.logicalColumns[currentColumn.typeIndex][row] = *src;
                break;
            default:
                break;
            }
        }
        ptr += fileHeader.recordSize;
    }
}