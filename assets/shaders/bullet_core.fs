// 本文件负责裁剪弹幕贴图的透明区域并输出写入深度的实心部分。

#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;

void main() {
    finalColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (finalColor.a < 0.55) discard;
    finalColor.a = 1.0;
}
