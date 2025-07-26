#version 330 core
layout (location = 0) in vec2 pos; // <vec2 pos, vec4 col, float rot>
layout (location = 1) in vec4 col;
layout (location = 2) in float rot;

out vec4 color;

uniform mat4 projection;
uniform vec2 screenSize;
uniform float radius;

void main()
{
    mat4 rotationMatrix = mat4 (cos(radians(360.0) - rot), sin(radians(360.0)- rot), 0.0, 0.0, -sin(radians(360.0) - rot), cos(radians(360.0)- rot), 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    mat4 centerMatrix = mat4(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, screenSize.x / 2.0, screenSize.y / 2.0, 0.0, 1.0);
    mat4 offsetMatrix = mat4(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, radius * sin(rot), radius * cos(rot), 0.0, 1.0);

    gl_Position = (projection * centerMatrix * offsetMatrix * rotationMatrix * vec4(pos, 0.0, 1.0));
    color = col;
}