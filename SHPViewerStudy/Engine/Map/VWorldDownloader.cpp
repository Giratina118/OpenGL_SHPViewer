#include <pch.h>
#include "VWorldDownloader.h"
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

bool VWorldDownloader::Download(const std::wstring& host, const std::wstring& path, std::vector<uint8_t>& data)
{
    data.clear();

    HINTERNET session = WinHttpOpen(L"SHPViewer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (session == nullptr) return false;

    HINTERNET connection = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);

    if (connection == nullptr) {
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(connection, L"GET", path.c_str(),  nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

    if (request == nullptr) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    bool success = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr);

    if (success) {
        DWORD availableSize = 0;

        while (WinHttpQueryDataAvailable(request, &availableSize) && availableSize > 0) {
            std::vector<uint8_t> buffer(availableSize);
            DWORD readSize = 0;

            if (!WinHttpReadData(request, buffer.data(), availableSize, &readSize)) {
                success = false;
                break;
            }

            data.insert(data.end(), buffer.begin(), buffer.begin() + readSize);
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    return success && !data.empty();
}