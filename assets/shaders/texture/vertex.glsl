#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 iPosSize;
layout(location = 3) in vec4 iUV;

out vec2 TexCoord;
out vec4 vColor;

uniform vec2 uScreen;
uniform vec2 uPan;
uniform vec4 uColor;

void main() {
    vec2 pixelPos = aPos * iPosSize.zw + iPosSize.xy - uPan;
    vec2 ndc = (pixelPos / uScreen) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);

    vec2 uv = mix(iUV.xy, iUV.zw, aPos);
    TexCoord = uv;
    vColor = uColor;
}
