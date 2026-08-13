#include <pch.h>
#include "VertexBuffer.h"

// ---------------- DebugVertexBuffer ----------------
void DebugVertexBuffer::Init()
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // location 0: position (vec3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    // location 1: color (vec4)
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void DebugVertexBuffer::Shutdown()
{
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    m_count = 0;
}

void DebugVertexBuffer::Upload(const std::vector<Vertex>& vertices, GLenum usage)
{
    m_count = static_cast<GLsizei>(vertices.size());
    if (m_count == 0) return;

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), usage);
    glBindVertexArray(0);
}

void DebugVertexBuffer::Draw(GLenum drawMode) const
{
    if (m_count == 0) return;

    glBindVertexArray(m_vao);
    glDrawArrays(drawMode, 0, m_count);
    glBindVertexArray(0);
}

// ---------------- OverlayMesh ----------------
void OverlayMesh::Init()
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_polygonIBO);
    glGenBuffers(1, &m_lineIBO);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void OverlayMesh::Shutdown()
{
    if (m_lineIBO) { glDeleteBuffers(1, &m_lineIBO);    m_lineIBO = 0; }
    if (m_polygonIBO) { glDeleteBuffers(1, &m_polygonIBO); m_polygonIBO = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo);        m_vbo = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao);   m_vao = 0; }
    m_polygonIndexCount = m_lineIndexCount = 0;
}

void OverlayMesh::Upload(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& polygonIndices, const std::vector<uint32_t>& lineIndices, GLenum usage)
{
    m_polygonIndexCount = static_cast<GLsizei>(polygonIndices.size());
    m_lineIndexCount    = static_cast<GLsizei>(lineIndices.size());

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), usage);

    if (m_polygonIndexCount > 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_polygonIBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * polygonIndices.size(), polygonIndices.data(), usage);
    }
    if (m_lineIndexCount > 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lineIBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * lineIndices.size(), lineIndices.data(), usage);
    }
}

void OverlayMesh::UpdateVertices(const std::vector<Vertex>& vertices)
{
    // 인덱스 구조는 그대로, 정점 좌표/색만 부분 갱신 (편집 드래그 중처럼 매 프레임 호출되는 경로)
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * vertices.size(), vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OverlayMesh::Draw() const
{
    if (Empty()) return;

    glBindVertexArray(m_vao);

    // 1. 면 (depth test 켠 상태로)
    if (m_polygonIndexCount > 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_polygonIBO);
        glDrawElements(GL_TRIANGLES, m_polygonIndexCount, GL_UNSIGNED_INT, nullptr);
    }

    // 2. 외곽선 (면에 파묻히지 않도록 depth test 잠시 끔)
    if (m_lineIndexCount > 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lineIBO);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(1.0f);
        glDrawElements(GL_LINES, m_lineIndexCount, GL_UNSIGNED_INT, nullptr);
        glEnable(GL_DEPTH_TEST);
    }

    glBindVertexArray(0);
}