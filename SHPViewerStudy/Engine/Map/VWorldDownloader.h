#pragma once
#include <cstdint>
#include <string>
#include <vector>

class VWorldDownloader
{
public:
    static bool Download(const std::wstring& host, const std::wstring& path, std::vector<uint8_t>& data);
};