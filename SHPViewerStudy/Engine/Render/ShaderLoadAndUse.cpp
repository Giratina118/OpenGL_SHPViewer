#include <pch.h>
#include <fstream>
#include <sstream>
#include "ShaderLoadAndUse.h"

Shader::Shader()
{
}

Shader::~Shader()
{
	if (m_shaderProgram != 0)
	{
		glDeleteProgram(m_shaderProgram);
		m_shaderProgram = 0;
	}
}

// Vertex/Fragment Shader 파일 로드 및 Program 생성
bool Shader::CreateProgram(const std::string& vertexPath, const std::string& fragmentPath)
{
	// shader 파일 로드
	std::string vertexSource = LoadFile(vertexPath);
	std::string fragmentSource = LoadFile(fragmentPath);

	// shader 생성
	GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
	GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

	// program 생성
	m_shaderProgram = glCreateProgram();
	glAttachShader(m_shaderProgram, vertexShader);
	glAttachShader(m_shaderProgram, fragmentShader);
	glLinkProgram(m_shaderProgram);

	// 링크 체크
	GLint linkResult;
	glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &linkResult);
	if (linkResult == GL_FALSE) {
		char log[1024];
		glGetProgramInfoLog(m_shaderProgram, 1024, nullptr, log);
		OutputDebugStringA(log);
		return false;
	}
	return true;
}

// 셰이더 파일 불러오기
std::string Shader::LoadFile(const std::string& filePath)
{
	std::ifstream shaderFile(filePath); // 파일 열기

	if (!shaderFile.is_open()) { // 파일 열렸는지 검사
		TRACE(L"셰이더 파일 열기 실패\n");
		return "";
	}

	std::stringstream shaderStream;		// 문자열 버퍼 생성

	// 파일 전체 내용을 문자열 버퍼로 복사
	shaderStream << shaderFile.rdbuf(); // .rdbuf(): 파일 내부 버퍼 전체
	shaderFile.close();
	return shaderStream.str(); // 최종 문자열 추출
}

// shader 생성 (GLSL -> GPU 컴파일)
GLuint Shader::CompileShader(GLenum shaderType, const std::string& source)
{
	GLuint shader = glCreateShader(shaderType); // GPU 내부에 shader 객체 생성
	const char* shaderSource = source.c_str();  // OpenGL API는 c기반 -> string을 c로 변환
	glShaderSource(shader, 1, &shaderSource, nullptr);
	glCompileShader(shader); // GLSL -> GPU 코드 컴파일

	GLint compileResult;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult); // 컴파일 성공 여부 검사
	if (compileResult == GL_FALSE) {
		GLchar errorLog[1024];
		glGetShaderInfoLog(shader, 1024, nullptr, errorLog);  // GPU 컴파일 에러 로그 가져옴
		TRACE(L"Shader Compile 실패\n");

		OutputDebugStringA(errorLog);
		OutputDebugStringA("\n");
		return 0;
	}
	TRACE(L"Shader Compile 성공\n");

	return shader;
}