#version 300 es
layout (location = 0) in highp vec2 pos; // <vec2 pos, vec4 col, float rot>
layout (location = 1) in highp vec4 col;
layout (location = 2) in highp float rot;

out highp vec4 color;

uniform highp mat4 projection;
uniform highp vec2 screenSize;
uniform highp float radius;

void main()
{
    mat4 rotationMatrix = mat4 (cos(radians(360.0) - rot), sin(radians(360.0)- rot), 0.0, 0.0, -sin(radians(360.0) - rot), cos(radians(360.0)- rot), 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    mat4 centerMatrix = mat4(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, screenSize.x / 2.0, screenSize.y / 2.0, 0.0, 1.0);
    mat4 offsetMatrix = mat4(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, radius * sin(rot), radius * cos(rot), 0.0, 1.0);

    gl_Position = (projection * centerMatrix * offsetMatrix * rotationMatrix * vec4(pos, 0.0, 1.0));
    color = col;
}