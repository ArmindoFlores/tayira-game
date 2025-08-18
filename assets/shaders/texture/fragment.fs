#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec4 vColor;
uniform sampler2D uTexture;

void main() {
    vec4 texel = texture(uTexture, TexCoord);
    if (texel.a < 0.5) discard;
    FragColor = texture(uTexture, TexCoord) * vColor;
}
