#version 300 es

layout(location = 0) in vec3 inputPosition;
layout(location = 1) in vec2 inputUV;

uniform mat4 u_viewProjection;

out vec2 v_uv;

void main()
{
    gl_Position = u_viewProjection * vec4(inputPosition, 1.0);
    v_uv = inputUV;
}