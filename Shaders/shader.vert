#version 450    // Use GLSL 4.5

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

layout(binding = 0) uniform UBOViewProjection {
    mat4 projection;
    mat4 view;
} uboViewProjection;

layout(binding = 1) uniform UBOModel {
    mat4 model;
} uboModel;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = uboViewProjection.projection * uboViewProjection.view * uboModel.model * vec4(pos, 1.0);
    fragColor = col;
}