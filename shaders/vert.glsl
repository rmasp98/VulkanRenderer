#version 450

layout (push_constant) uniform constants {
    mat4 mvp;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColour;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragColour;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    fragColour = inColour;
    fragTexCoord = inUV;
}
