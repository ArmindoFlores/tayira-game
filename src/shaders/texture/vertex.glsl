#version 330 core
layout(location = 0) in vec2 aPos;      // 0..1
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform vec2 uPos;       // pixel position
uniform vec2 uSize;      // pixel size
uniform vec2 uScreen;    // screen width, height

void main() {
    vec2 pixelPos = aPos * uSize + uPos;
    vec2 ndc = (pixelPos / uScreen) * 2.0 - 1.0;
    ndc.y = -ndc.y; // flip if top-left is origin
    gl_Position = vec4(ndc, 0.0, 1.0);
    TexCoord = aTexCoord;
}