#pragma once
#include <cstdint>
#include <string>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <queue>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include "Render/ShaderLoadAndUse.h"
#include "Parser/CoordinateSystem.h"

class CameraManager;
class LayerManager;

struct MapVertex
{
    float x;
    float y;
    float z;
    float u;
    float v;
};

struct TileKey
{
    int32_t zoom;
    int32_t x;
    int32_t y;

    bool operator==(const TileKey& other) const
    {
        return zoom == other.zoom && x == other.x && y == other.y;
    }
};

struct TileKeyHash
{
    size_t operator()(const TileKey& key) const
    {
        size_t hashZoom = std::hash<int32_t>{}(key.zoom);
        size_t hashX = std::hash<int32_t>{}(key.x);
        size_t hashY = std::hash<int32_t>{}(key.y);

        return hashZoom ^ (hashX << 1) ^ (hashY << 2);
    }
};

struct VWorldTile
{
    GLuint texture = 0;

    glm::dvec2 bottomLeft;
    glm::dvec2 bottomRight;
    glm::dvec2 topRight;
    glm::dvec2 topLeft;
};

struct VWorldDownloadRequest
{
    TileKey key;
};

struct VWorldDownloadResult
{
    TileKey key;
    std::vector<uint8_t> imageData;
    bool success = false;
};

class VWorldMapManager
{
public:
    bool Initialize(const std::wstring& apiKey);
    void Update(CameraManager& camera, LayerManager& layerManager);
    void Render(CameraManager& camera);
    void Shutdown();

private:
    bool InitBuffer();
    bool InitShader();
    void BuildVisibleTiles(CameraManager& camera);
    void BuildTile(int32_t zoom, int32_t tileX, int32_t tileY);
    bool ProcessDownloadResults();

    void WorldToLonLat(const glm::dvec2& worldPoint, double& longitude, double& latitude);
    glm::dvec2 LonLatToWorld(double longitude, double latitude);

    void TileBounds(int32_t zoom, int32_t tileX, int32_t tileY, double& longitudeMin, double& latitudeMin, double& longitudeMax, double& latitudeMax);
    TileKey LonLatToTile(double longitude, double latitude, int32_t zoom);

    bool GetGroundPoint(CameraManager& camera, double ndcX, double ndcY, glm::dvec2& worldPoint);
    void DownloadThread();

private:
    std::wstring m_apiKey;

    Shader m_shader;
    GLint m_viewProjectionLocation = -1;
    GLint m_textureLocation = -1;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ibo = 0;

    std::unordered_map<TileKey, VWorldTile, TileKeyHash> m_tiles;
    std::vector<TileKey> m_visibleTiles;

    CoordinateTransformer m_coordinateTransformer;


    std::vector<std::thread> m_downloadThreads;
    std::mutex m_downloadMutex;
    std::condition_variable m_downloadCondition;
    std::queue<VWorldDownloadRequest> m_downloadRequests;
    std::queue<VWorldDownloadResult> m_downloadResults;
    std::unordered_set<TileKey, TileKeyHash> m_loadingTiles;
    bool m_stopDownloadThread = false;

    glm::dmat4 m_previousViewProjection = glm::dmat4(0.0);
    bool m_hasPreviousViewProjection = false;
    std::unordered_set<TileKey, TileKeyHash> m_requiredTiles;
};