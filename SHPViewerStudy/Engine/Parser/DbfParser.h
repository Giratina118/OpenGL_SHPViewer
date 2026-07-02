#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include "FeatureObject.h"

using namespace std;

// .dbf 헤더
struct DbfHeader
{
    uint8_t version;       // 버전
    uint8_t lastUpdate[3]; // 최종 갱신일
    uint32_t recordCount;  // 레코드 개수
    uint16_t headerSize;   // 헤더 크기
    uint16_t recordSize;   // 레코드 크기

    void Read(const uint8_t*& ptr);
};

// .dbf 파싱
class DbfParser
{
public:
    void DbfParse(vector<uint8_t>& buffer, DBFTable& dbfTable);
};