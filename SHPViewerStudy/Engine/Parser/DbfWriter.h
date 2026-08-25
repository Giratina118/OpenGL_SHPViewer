#pragma once
#include <filesystem>
#include <vector>
struct DBFTable;

class DbfWriter
{
public:
    // keepIds: 남길 행(원본 dbf 행 번호) 목록, 순서 그대로 유지하며 씀
    bool WriteDbf(const std::filesystem::path& path, DBFTable& dbfTable, const std::vector<int32_t>& keepIds);
};