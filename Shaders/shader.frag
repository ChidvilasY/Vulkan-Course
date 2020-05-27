#version 450    // Use GLSL 4.5

layout(location = 0) in vec3 fragColor; // Interpolated color from vertices (location must match)

// above location is not the same!
layout(location = 0) out vec4 outColor; // Final output color (must also have location)

void main() {
    outColor = vec4(fragColor, 1.0);
}
