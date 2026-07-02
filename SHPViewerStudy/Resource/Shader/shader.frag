#version 300 es
precision mediump float;

uniform float u_colorMultiplier;  // 기본 1.0, 라인 드로우 시 0.6

in  vec4 v_color;
out vec4 fragColor;

void main()
{
    fragColor = vec4(v_color.rgb * u_colorMultiplier, v_color.a);
}