#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 iStart;
layout(location = 2) in vec2 iDir;
layout(location = 3) in float iLength;
layout(location = 4) in float iWidth;
layout(location = 5) in vec3 iColor;
layout(location = 6) in float iDepth;

out vec4 vColor;

uniform vec2 uScreen;
uniform vec2 uPan;

void main() {
    vec2 t = normalize(iDir);
    vec2 n = vec2(-t.y, t.x);

    vec2 scaled = vec2(aPos.x * iLength, aPos.y * iWidth);
    vec2 center = iStart + 0.5 * iLength * t;
    vec2 pixelPos = center + t * scaled.x + n * scaled.y - uPan;
    vec2 ndc = (pixelPos / uScreen) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, iDepth, 1.0);
    vColor = vec4(iColor, 1.0);
}
