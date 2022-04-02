#version 450

layout (location = 0) in uint  inType;
layout (location = 1) in vec3  inPosition;
layout (location = 3) in vec4  inColor;
layout (location = 5) in vec4  inMetaData;

layout (location = 0) out vec4 fragColor;
layout (location = 1) out flat uint outType;
layout (location = 2) out vec3 outPosition;
layout (location = 3) out vec4  outMetaData;

#include "../fragments/frame_uniforms.glsl"

void main() {
    outPosition = inPosition;
    fragColor = inColor;
    outType = inType;
    outMetaData = inMetaData;
}