#version 320 es
layout (location = 0) in highp vec2 pos; // <vec2 pos, vec2 tex>
layout (location = 1) in highp vec2 tex;
out highp vec2 texCoords;

uniform highp mat4 projection;

void main()
{
    gl_Position = projection * vec4(pos, 0.0, 1.0);
    texCoords = tex;
}  