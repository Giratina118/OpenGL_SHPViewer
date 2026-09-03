#include <pch.h>
#include "VWorldMapManager.h"
#include "VWorldDownloader.h"
#include "VWorldTexture.h"
#include "CameraManager.h"
#include "LayerManager.h"
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

bool VWorldMapManager::Initialize(const std::wstring& apiKey)
{
    m_apiKey = apiKey;

    if (!InitBuffer()) return false;
    if (!InitShader()) return false;

    CoordinateSystem targetCoordinate;
    targetCoordinate.SetTargetCoordinate(5186);

    m_coordinateTransformer.parameter.sourceEllipsoid.Set(targetCoordinate);
    m_coordinateTransformer.parameter.sourceProjection.Set(targetCoordinate, m_coordinateTransformer.parameter.sourceEllipsoid);

    m_coordinateTransformer.parameter.destinationEllipsoid.Set(targetCoordinate);
    m_coordinateTransformer.parameter.destinationProjection.Set(targetCoordinate, m_coordinateTransformer.parameter.destinationEllipsoid);

    m_stopDownloadThread = false;

    // 동시 다운로드 스레드 개수 설정 (보통 4~8개)
    uint32_t threadCount = 6;

    for (uint32_t i = 0; i < threadCount; ++i) {
        m_downloadThreads.emplace_back(&VWorldMapManager::DownloadThread, this);
    }

    return true;
}
void VWorldMapManager::Update(CameraManager& camera, LayerManager& layerManager)
{
    bool isDownloaded = ProcessDownloadResults();

    if (isDownloaded)
        layerManager.ReDraw();

    glm::dmat4 currentViewProjection = camera.GetViewProjectionMatrix();

    bool cameraChanged =
        !m_hasPreviousViewProjection ||
        currentViewProjection != m_previousViewProjection;

    if (!cameraChanged)
        return;

    m_previousViewProjection = currentViewProjection;
    m_hasPreviousViewProjection = true;

    BuildVisibleTiles(camera);

    layerManager.ReDraw();
}

bool VWorldMapManager::InitBuffer()
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ibo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(MapVertex) * 4, nullptr, GL_DYNAMIC_DRAW);

    uint32_t indices[6] = { 0, 1, 2, 0, 2, 3 };

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MapVertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(MapVertex), (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    return true;
}

bool VWorldMapManager::InitShader()
{
    if (!m_shader.CreateProgram("Resource/Shader/map.vert", "Resource/Shader/map.frag"))
        return false;

    m_viewProjectionLocation = glGetUniformLocation(m_shader.GetProgram(), "u_viewProjection");
    m_textureLocation = glGetUniformLocation(m_shader.GetProgram(), "u_mapTexture");

    return m_viewProjectionLocation != -1 && m_textureLocation != -1;
}
void VWorldMapManager::BuildVisibleTiles(CameraManager& camera)
{
    double cameraHeight = std::abs(camera.transform.position.z);

    int32_t zoom = 19;

    while (cameraHeight > 256.0)
    {
        cameraHeight *= 0.5;
        zoom--;
    }

    zoom = glm::clamp(zoom, 6, 19);

    glm::dvec2 groundPoints[9];

    int32_t pointCount = 0;

    for (int32_t y = 0; y < 3; y++)
    {
        double ndcY = -1.0 + y * 1.0;

        for (int32_t x = 0; x < 3; x++)
        {
            double ndcX = -1.0 + x * 1.0;

            if (GetGroundPoint(camera, ndcX, ndcY, groundPoints[pointCount]))
                pointCount++;
        }
    }

    if (pointCount < 2)
        return;

    double longitudeMin = std::numeric_limits<double>::max();
    double latitudeMin = std::numeric_limits<double>::max();
    double longitudeMax = std::numeric_limits<double>::lowest();
    double latitudeMax = std::numeric_limits<double>::lowest();

    for (int32_t i = 0; i < pointCount; i++)
    {
        double longitude;
        double latitude;

        WorldToLonLat(groundPoints[i], longitude, latitude);

        longitudeMin = std::min(longitudeMin, longitude);
        longitudeMax = std::max(longitudeMax, longitude);
        latitudeMin = std::min(latitudeMin, latitude);
        latitudeMax = std::max(latitudeMax, latitude);
    }

    TileKey minTile = LonLatToTile(longitudeMin, latitudeMax, zoom);
    TileKey maxTile = LonLatToTile(longitudeMax, latitudeMin, zoom);

    minTile.x--;
    minTile.y--;
    maxTile.x++;
    maxTile.y++;

    m_visibleTiles.clear();

    int32_t maxTileIndex = (1 << zoom) - 1;

    for (int32_t tileY = minTile.y; tileY <= maxTile.y; tileY++)
    {
        if (tileY < 0 || tileY > maxTileIndex)
            continue;

        for (int32_t tileX = minTile.x; tileX <= maxTile.x; tileX++)
        {
            int32_t wrappedTileX = tileX;

            if (wrappedTileX < 0)
                wrappedTileX += maxTileIndex + 1;

            if (wrappedTileX > maxTileIndex)
                wrappedTileX -= maxTileIndex + 1;

            TileKey key;
            key.zoom = zoom;
            key.x = wrappedTileX;
            key.y = tileY;

            m_visibleTiles.push_back(key);

            if (m_tiles.find(key) == m_tiles.end())
                BuildTile(key.zoom, key.x, key.y);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_downloadMutex);

        m_requiredTiles.clear();

        for (const TileKey& key : m_visibleTiles)
            m_requiredTiles.insert(key);
    }

    for (const TileKey& key : m_visibleTiles)
    {
        if (m_tiles.find(key) == m_tiles.end())
            BuildTile(key.zoom, key.x, key.y);
    }


    OutputDebugStringA(("[VWORLD] Zoom = " + std::to_string(zoom) + ", Visible Tile Count = " + std::to_string(m_visibleTiles.size()) + "\n").c_str());
}

void VWorldMapManager::BuildTile(int32_t zoom, int32_t tileX, int32_t tileY)
{
    TileKey key;
    key.zoom = zoom;
    key.x = tileX;
    key.y = tileY;

    if (m_tiles.find(key) != m_tiles.end())
        return;

    {
        std::lock_guard<std::mutex> lock(m_downloadMutex);

        if (m_loadingTiles.find(key) != m_loadingTiles.end())
            return;

        m_loadingTiles.insert(key);
        m_downloadRequests.push({ key });
    }

    m_downloadCondition.notify_one();
}

bool VWorldMapManager::ProcessDownloadResults()
{
	bool isDownloaded = false;
    while (true)
    {
        VWorldDownloadResult result;

        {
            std::lock_guard<std::mutex> lock(m_downloadMutex);

            if (m_downloadResults.empty())
                break;

            result = std::move(m_downloadResults.front());
            m_downloadResults.pop();

            m_loadingTiles.erase(result.key);
        }

        if (!result.success)
            continue;

        GLuint texture = VWorldTexture::Create(result.imageData);

        if (texture == 0)
            continue;

        double longitudeMin;
        double latitudeMin;
        double longitudeMax;
        double latitudeMax;

        TileBounds(result.key.zoom, result.key.x, result.key.y, longitudeMin, latitudeMin, longitudeMax, latitudeMax);

        VWorldTile tile;

        tile.bottomLeft = LonLatToWorld(longitudeMin, latitudeMin);
        tile.bottomRight = LonLatToWorld(longitudeMax, latitudeMin);
        tile.topRight = LonLatToWorld(longitudeMax, latitudeMax);
        tile.topLeft = LonLatToWorld(longitudeMin, latitudeMax);

        tile.texture = texture;

        m_tiles[result.key] = std::move(tile);
		isDownloaded = true;
    }
	return isDownloaded;
}

void VWorldMapManager::Render(CameraManager& camera)
{
    if (m_visibleTiles.empty())
        return;

    glm::mat4 viewProjection = glm::mat4(camera.GetViewProjectionMatrix());

    m_shader.UseProgram();
    glUniformMatrix4fv(m_viewProjectionLocation, 1, GL_FALSE, glm::value_ptr(viewProjection));

    glBindVertexArray(m_vao);

    for (const TileKey& key : m_visibleTiles) {
        auto tileIt = m_tiles.find(key);
        if (tileIt == m_tiles.end())
            continue;

        VWorldTile& tile = tileIt->second;

        MapVertex vertices[4] =
        {
            { static_cast<float>(tile.bottomLeft.x),  static_cast<float>(tile.bottomLeft.y),  0.0f, 0.0f, 1.0f },
            { static_cast<float>(tile.bottomRight.x), static_cast<float>(tile.bottomRight.y), 0.0f, 1.0f, 1.0f },
            { static_cast<float>(tile.topRight.x),    static_cast<float>(tile.topRight.y),    0.0f, 1.0f, 0.0f },
            { static_cast<float>(tile.topLeft.x),     static_cast<float>(tile.topLeft.y),     0.0f, 0.0f, 0.0f }
        };

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tile.texture);
        glUniform1i(m_textureLocation, 0);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    }

    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void VWorldMapManager::Shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_downloadMutex);
        m_stopDownloadThread = true;
    }

    // 모든 대기 중인 스레드 깨우기
    m_downloadCondition.notify_all();

    // 모든 스레드 종료 대기
    for (auto& thread : m_downloadThreads) {
        if (thread.joinable())
            thread.join();
    }
    m_downloadThreads.clear();

    for (auto& [key, tile] : m_tiles) {
        if (tile.texture)
            glDeleteTextures(1, &tile.texture);
    }

    m_tiles.clear();
    m_visibleTiles.clear();

    if (m_ibo) { glDeleteBuffers(1, &m_ibo); m_ibo = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }

    if (m_shader.GetProgram()) {
        glDeleteProgram(m_shader.GetProgram());
    }
}

void VWorldMapManager::WorldToLonLat(const glm::dvec2& worldPoint, double& longitude, double& latitude)
{
    glm::dvec2 point = worldPoint;

    m_coordinateTransformer.InverseProjection(point);

    longitude = point.x;
    latitude = point.y;
}
glm::dvec2 VWorldMapManager::LonLatToWorld(double longitude, double latitude)
{
    glm::dvec2 point = { longitude, latitude };

    m_coordinateTransformer.Projection(point);

    return point;
}

void VWorldMapManager::TileBounds(int32_t zoom, int32_t tileX, int32_t tileY, double& longitudeMin, double& latitudeMin, double& longitudeMax, double& latitudeMax)
{
    double tileCount = std::ldexp(1.0, zoom);

    longitudeMin = static_cast<double>(tileX) / tileCount * 360.0 - 180.0;
    longitudeMax = static_cast<double>(tileX + 1) / tileCount * 360.0 - 180.0;

    auto tileYToLatitude = [&](double y)
    {
        double mercatorY = glm::pi<double>() * (1.0 - 2.0 * y / tileCount);
        return glm::degrees(std::atan(std::sinh(mercatorY)));
    };

    latitudeMax = tileYToLatitude(tileY);
    latitudeMin = tileYToLatitude(tileY + 1);
}
TileKey VWorldMapManager::LonLatToTile(double longitude, double latitude, int32_t zoom)
{
    double tileCount = std::ldexp(1.0, zoom);
    double latitudeRad = glm::radians(latitude);

    TileKey key;
    key.zoom = zoom;
    key.x = static_cast<int32_t>(std::floor((longitude + 180.0) / 360.0 * tileCount));
    key.y = static_cast<int32_t>(std::floor((1.0 - std::asinh(std::tan(latitudeRad)) / glm::pi<double>()) * 0.5 * tileCount));

    key.x = std::max(0, std::min(key.x, static_cast<int32_t>(tileCount) - 1));
    key.y = std::max(0, std::min(key.y, static_cast<int32_t>(tileCount) - 1));

    return key;
}

bool VWorldMapManager::GetGroundPoint(CameraManager& camera, double ndcX, double ndcY, glm::dvec2& worldPoint)
{
    glm::dmat4 inverseViewProjection = glm::inverse(camera.GetViewProjectionMatrix());

    glm::dvec4 nearPoint = inverseViewProjection * glm::dvec4(ndcX, ndcY, -1.0, 1.0);
    glm::dvec4 farPoint = inverseViewProjection * glm::dvec4(ndcX, ndcY, 1.0, 1.0);

    if (nearPoint.w == 0.0 || farPoint.w == 0.0)
        return false;

    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    glm::dvec3 rayOrigin(nearPoint);
    glm::dvec3 rayDirection = glm::dvec3(farPoint) - rayOrigin;

    if (std::abs(rayDirection.z) < 1e-9)
        return false;

    double distance = -rayOrigin.z / rayDirection.z;

    if (distance < 0.0)
        return false;

    glm::dvec3 hitPoint = rayOrigin + rayDirection * distance;

    worldPoint.x = hitPoint.x;
    worldPoint.y = hitPoint.y;

    return true;
}
void VWorldMapManager::DownloadThread()
{
    while (true)
    {
        VWorldDownloadRequest request;

        {
            std::unique_lock<std::mutex> lock(m_downloadMutex);

            m_downloadCondition.wait(lock, [this]()
            {
                return m_stopDownloadThread || !m_downloadRequests.empty();
            });

            if (m_stopDownloadThread)
                break;

            request = std::move(m_downloadRequests.front());
            m_downloadRequests.pop();

            if (m_requiredTiles.find(request.key) == m_requiredTiles.end())
            {
                m_loadingTiles.erase(request.key);
                continue;
            }
        }

        VWorldDownloadResult result;
        result.key = request.key;

        std::wstring path = L"/req/wmts/1.0.0/" + m_apiKey + L"/Base/" +
            std::to_wstring(request.key.zoom) + L"/" +
            std::to_wstring(request.key.y) + L"/" +
            std::to_wstring(request.key.x) + L".png";

        result.success = VWorldDownloader::Download( L"api.vworld.kr", path, result.imageData);

        {
            std::lock_guard<std::mutex> lock(m_downloadMutex);

            if (m_requiredTiles.find(result.key) == m_requiredTiles.end())
            {
                m_loadingTiles.erase(result.key);
                continue;
            }

            m_downloadResults.push(std::move(result));
        }
    }
}