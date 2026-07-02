#version 300 es
layout(location = 0) in vec3 inputPosition;
layout(location = 1) in vec4 inputColor;

uniform mat4  u_viewProjection;

out vec4 v_color;

void main()
{
    gl_Position = u_viewProjection * vec4(inputPosition.xy, inputPosition.z, 1.0);
    v_color = inputColor;
}