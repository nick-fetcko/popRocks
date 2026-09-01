#version 300 es
layout (location = 0) in highp vec2 pos; // <vec2 pos, vec4 col>
layout (location = 1) in highp vec4 col;
out highp vec4 color;

uniform highp mat4 projection;

void main()
{
    gl_Position = projection * vec4(pos, 0.0, 1.0);
    color = col;
}  