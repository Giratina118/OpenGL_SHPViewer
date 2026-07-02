#pragma once

#include <string>
#include <GLES3/gl3.h>

class Shader
{
private:
	GLuint m_shaderProgram = 0; // 링크된 프로그램의 ID. 0=미생성

	std::string LoadFile(const std::string& path); // 셰이더 파일 불러오기
	GLuint CompileShader(GLenum shaderType, const std::string& source); // shader 생성 (GLSL -> GPU 컴파일)


public:
	Shader();
	~Shader();

	// Vertex/Fragment Shader 파일 로드 및 Program 생성
	bool CreateProgram(const std::string& vertexPath, const std::string& fragmentPath);
	void UseProgram() { glUseProgram(m_shaderProgram); }  // 그리기 직전 호출. 현재 GL 상태에 이 프로그램 바인딩
	GLuint GetProgram() const { return m_shaderProgram; } // uniform location 가져올 때 사용
	 
};