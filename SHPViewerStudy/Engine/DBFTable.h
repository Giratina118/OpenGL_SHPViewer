#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>

// dbf 레이어 저장
// .dbf 컬럼 타입
enum class DBFColumnType : uint8_t
{
    Unknown = 0,
    Int32  = 'N',
    Double = 'F',
    String = 'C',
    Bool   = 'L',
    Date   = 'D'
};

// .dbf 문자열 관리
struct FixedStringColumn
{
    int width    = 0; // 문자열 한 행의 고정 길이
    int rowCount = 0; // 현재 저장된 행의 개수
    std::vector<char> buffer; // 데이터 저장 공간

    void  Resize(int32_t rows); // 초기화 없이
    char* GetRow(int32_t row);  // 문자열 반환
    std::string_view GetView       (int32_t row) const; // 복사 없이 문자열 보기
    std::string_view GetTrimmedView(int32_t row) const;
};

struct DBFTable;

// .dbf 컬럼 정보
struct ColumnInfo
{
    std::string   name;     // 이름
    DBFColumnType type;     // 타입
    uint8_t  length    = 0; // 길이
    uint8_t  decimals  = 0; // 소수점 자리 수
    uint32_t typeIndex = 0; // 몇 번째 칼럼인지
    uint32_t offset    = 0; // 레코드 내 위치
};

// .dbf 데이터 저장
struct DBFTable
{
    uint32_t rowCount    = 0;
    uint32_t intCount    = 0;
    uint32_t doubleCount = 0;
    uint32_t stringCount = 0;
    uint32_t boolCount   = 0;
    int32_t  heightPos   = -1;
    int32_t  floorPos    = -1;

    std::vector<ColumnInfo> columns;
    std::vector<std::vector<int32_t>> intColumns;
    std::vector<std::vector<double>>  doubleColumns;
    std::vector<FixedStringColumn>    stringColumns;
    std::vector<std::vector<unsigned char>> logicalColumns;

    uint32_t AddIntIndex()    { return intCount++; }
    uint32_t AddDoubleIndex() { return doubleCount++; }
    uint32_t AddStringIndex() { return stringCount++; }
    uint32_t AddBoolIndex()   { return boolCount++; }

    void ReadColumnInfo(const uint8_t*& ptr, int32_t columnInfoNum);
    CString PrintAttribute(int32_t dataId);
};