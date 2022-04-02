#version 440

// Include our common vertex shader attributes and uniforms
#include "../fragments/vs_common.glsl"

layout(location = 6) in vec4 inTextureWeights;

#include "../fragments/math_constants.glsl"

layout(location = 7) out vec4 outTextureWeights;

void main() {

	gl_Position = u_ModelViewProjection * vec4(inPosition, 1.0);

	// Pass vertex pos in world space to frag shader
	outViewPos = (u_ModelView * vec4(inPosition, 1.0)).xyz;
	// Normals
	outNormal = (u_View * vec4(mat3(u_NormalMatrix) * inNormal, 1)).xyz;
	// Pass our UV coords to the fragment shader
	outUV = inUV;
	///////////
	outColor = inColor;
	
    // We use a TBN matrix for tangent space normal mapping
    vec3 T = normalize((u_View * vec4(mat3(u_NormalMatrix) * inTangent, 0)).xyz);
    vec3 B = normalize((u_View * vec4(mat3(u_NormalMatrix) * inBiTangent, 0)).xyz);
    vec3 N = normalize((u_View * vec4(mat3(u_NormalMatrix) * inNormal, 0)).xyz);
    mat3 TBN = mat3(T, B, N);

	// We now rotate our tangent space matrices to be view-dependant 
	outTBN = TBN;

	float overrideWeight = clamp(dot(inTextureWeights, vec4(1)), 0, 1);
	outTextureWeights = mix(inTextureWeights, vec4(0.25), overrideWeight);
}

