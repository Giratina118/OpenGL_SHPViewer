#pragma once
#include <GLES3/gl3.h>
#include <vector>
#include "FeatureObject.h"

// 인덱스 없이 정점만 그리는 디버그용 버퍼 (MBR박스, 피킹 사각형, 절두체 라인 등)
// glDrawArrays 사용. VAO 레이아웃(position vec3 + color ubyte4)은 프로젝트 전역과 동일하게 고정.
class DebugVertexBuffer
{
public:
    DebugVertexBuffer() = default;
    ~DebugVertexBuffer() { Shutdown(); }
    void Init();     // 초기화
    void Shutdown(); // 메모리 해제
    void Upload(const std::vector<Vertex>& vertices, GLenum usage = GL_DYNAMIC_DRAW); // 정점 데이터 GPU 업로드. usage는 자주 안 바뀌면 GL_STATIC_DRAW, 매 프레임 바뀌면 GL_DYNAMIC_DRAW
    void Draw(GLenum drawMode) const; // 업로드된 데이터를 그리기
    bool Empty() const { return m_count == 0; }

private:
    GLuint  m_vao = 0;
    GLuint  m_vbo = 0;
    GLsizei m_count = 0;
};


// 면 + 외곽선을 함께 그리는 오버레이 메쉬 (편집 중인 객체, 호버된 객체 등)
// 면: glDrawElements(GL_TRIANGLES), 외곽선: glDrawElements(GL_LINES, depth test 끄고)
class OverlayMesh
{
public:
    OverlayMesh() = default;
    ~OverlayMesh() { Shutdown(); }
    void Init();     // 초기화
    void Shutdown(); // 메모리 해제

    // 정점 + 면 인덱스 + 선 인덱스를 한 번에 업로드 (구조가 바뀔 때, 예: 새 객체를 편집/호버 대상으로 잡을 때)
    void Upload(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& polygonIndices, const std::vector<uint32_t>& lineIndices, GLenum usage = GL_DYNAMIC_DRAW);
    void UpdateVertices(const std::vector<Vertex>& vertices); // 인덱스 구조는 그대로 두고 정점 좌표/색상만 갱신
    void Draw()  const; // 면 -> (depth test 끄고) 외곽선 순서로 그리기
    bool Empty() const { return m_polygonIndexCount == 0 && m_lineIndexCount == 0; }

private:
    GLuint  m_vao = 0;
    GLuint  m_vbo = 0;
    GLuint  m_polygonIBO = 0;
    GLuint  m_lineIBO    = 0;
    GLsizei m_polygonIndexCount = 0;
    GLsizei m_lineIndexCount    = 0;
};