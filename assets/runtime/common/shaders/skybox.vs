// 本文件负责移除相机平移，只用视角旋转定位天空盒。

#version 330

in vec3 vertexPosition;
uniform mat4 matProjection;
uniform mat4 matView;
out vec3 direction;

void main() {
    direction = vertexPosition;
    mat4 rotationOnlyView = mat4(mat3(matView));
    gl_Position = matProjection * rotationOnlyView * vec4(vertexPosition, 1.0);
}
