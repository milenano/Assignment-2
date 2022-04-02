#version 430

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec3 outColor;

uniform layout(binding = 0) sampler2D s_Image;

uniform float u_Filter[25];
uniform vec2 u_PixelSize;

void main() {
    vec3 accumulator = vec3(0);
    for(int ix = -2; ix <= 2; ix++) {
        for (int iy = -2; iy <= 2; iy++) {
            int index =  (iy + 2) * 5 + (ix + 2);
            vec2 uv = inUV + vec2(u_PixelSize.x * ix, u_PixelSize.y * iy);
            accumulator += texture(s_Image, uv).rgb * u_Filter[index];
        }
    }
    outColor = accumulator;
}
