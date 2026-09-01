#include <pch.h>
#include "VWorldMapManager.h"
#include "VWorldDownloader.h"
#include "VWorldTexture.h"
#include "CameraManager.h"
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

bool VWorldMapManager::Initialize(const std::wstring& apiKey)
{
    m_apiKey = apiKey;

    if (!InitBuffer()) return false;
    if (!InitShader()) return false;

    CoordinateSystem targetCoordinate;
    targetCoordinate.SetTargetCoordinate(5186);

    m_coordinateTransformer.parameter.destinationEllipsoid.Set(targetCoordinate);
    m_coordinateTransformer.parameter.destinationProjection.Set(targetCoordinate, m_coordinateTransformer.parameter.destinationEllipsoid);

    return true;
}

void VWorldMapManager::Update(CameraManager& camera)
{
    BuildVisibleTiles(camera);
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
    const int32_t zoom = 10;

    glm::dvec2 groundPoints[4];

    if (!GetGroundPoint(camera, -1.0, -1.0, groundPoints[0])) return;
    if (!GetGroundPoint(camera, 1.0, -1.0, groundPoints[1])) return;
    if (!GetGroundPoint(camera, 1.0, 1.0, groundPoints[2])) return;
    if (!GetGroundPoint(camera, -1.0, 1.0, groundPoints[3])) return;

    double longitudeMin = std::numeric_limits<double>::max();
    double latitudeMin = std::numeric_limits<double>::max();
    double longitudeMax = std::numeric_limits<double>::lowest();
    double latitudeMax = std::numeric_limits<double>::lowest();

    for (const glm::dvec2& point : groundPoints) {
        double longitude;
        double latitude;

        WorldToLonLat(point, longitude, latitude);

        longitudeMin = std::min(longitudeMin, longitude);
        longitudeMax = std::max(longitudeMax, longitude);
        latitudeMin = std::min(latitudeMin, latitude);
        latitudeMax = std::max(latitudeMax, latitude);
    }

    TileKey minTile = LonLatToTile(longitudeMin, latitudeMax, zoom);
    TileKey maxTile = LonLatToTile(longitudeMax, latitudeMin, zoom);

    m_visibleTiles.clear();

    for (int32_t tileY = minTile.y; tileY <= maxTile.y; tileY++) {
        for (int32_t tileX = minTile.x; tileX <= maxTile.x; tileX++) {
            TileKey key;
            key.zoom = zoom;
            key.x = tileX;
            key.y = tileY;

            m_visibleTiles.push_back(key);

            if (m_tiles.find(key) == m_tiles.end())
                BuildTile(zoom, tileX, tileY);
        }
    }
}

void VWorldMapManager::BuildTile(int32_t zoom, int32_t tileX, int32_t tileY)
{
    std::vector<uint8_t> imageData;

    const std::wstring path =
        L"/req/wmts/1.0.0/" +
        m_apiKey +
        L"/Base/" +
        std::to_wstring(zoom) +
        L"/" +
        std::to_wstring(tileY) +
        L"/" +
        std::to_wstring(tileX) +
        L".png";

    if (!VWorldDownloader::Download(L"api.vworld.kr", path, imageData))
    {
        OutputDebugStringA("[VWORLD] Tile Download Failed\n");
        return;
    }

    GLuint texture = VWorldTexture::Create(imageData);

    if (texture == 0)
        return;

    double longitudeMin;
    double latitudeMin;
    double longitudeMax;
    double latitudeMax;

    TileBounds(zoom, tileX, tileY, longitudeMin, latitudeMin, longitudeMax, latitudeMax);

    VWorldTile tile;

    tile.texture = texture;
    tile.bottomLeft = LonLatToWorld(longitudeMin, latitudeMin);
    tile.bottomRight = LonLatToWorld(longitudeMax, latitudeMin);
    tile.topRight = LonLatToWorld(longitudeMax, latitudeMax);
    tile.topLeft = LonLatToWorld(longitudeMin, latitudeMax);

    TileKey key;
    key.zoom = zoom;
    key.x = tileX;
    key.y = tileY;

    m_tiles.emplace(key, tile);
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
    glBindTexture(GL_TEXTURE_2D, 0);
}

void VWorldMapManager::Shutdown()
{
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