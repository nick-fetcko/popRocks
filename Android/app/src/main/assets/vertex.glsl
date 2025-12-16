#version 300 es
layout (location = 0) in highp vec2 pos; // <vec2 pos>

uniform highp mat4 projection;

void main()
{
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}  