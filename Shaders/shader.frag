#version 450    // Use GLSL 4.5

// above location is not the same!
layout(location = 0) out vec4 outColor; // Final output color (must also have location)

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTex;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;

void main() {
    outColor = texture(textureSampler, fragTex);
}
