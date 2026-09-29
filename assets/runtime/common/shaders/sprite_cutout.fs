// 本文件负责裁剪角色贴图的透明区域，保留可见像素。

#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;

void main() {
    finalColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (finalColor.a < 0.08) discard;
}
