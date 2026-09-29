// 本文件负责从立方体贴图采样红雾天空盒颜色。

#version 330

in vec3 direction;
uniform samplerCube environmentMap;
out vec4 finalColor;

void main() {
    finalColor = vec4(texture(environmentMap, direction).rgb, 1.0);
}
