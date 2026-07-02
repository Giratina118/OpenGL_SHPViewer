#pragma once
#include <cstdint>
#include <string>
#include <vector>

using namespace std;

struct ShxRecord
{
    uint32_t offset;
    uint32_t contentLength;
};

class ShxParser
{
public:
    void ShxParse(const uint8_t* ptr, std::vector<ShxRecord>& records, uint32_t fileLength); // .shx ÆÄ½Ì
    void ReadRecord(const uint8_t*& ptr, ShxRecord& record);
};