#version 430

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

uniform layout(binding = 0) sampler2D s_Sampler;


void main() {
	// Calculate noise and sample texture
    float amount = 0.1; 
    float noise = (fract(sin(dot(inUV, vec2(12.9898,78.233)*2.0)) * 43758.5453));
    vec4 tex = texture(s_Sampler, inUV);


    vec4 result = tex - noise * amount;

    // Output to screen
    outColor = result;
}