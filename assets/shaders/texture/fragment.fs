#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec4 vColor;

uniform sampler2D uTexture;
uniform float uAlphaClip;

void main() {
    vec4 texel = texture(uTexture, TexCoord);
    if (texel.a <= uAlphaClip) discard;
    FragColor = texel * vColor;
}
