// 本文件将针弹圆锥光效的本地轴向坐标传递给渐隐着色器。
#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
out float axial;
void main(){axial=(1.1-vertexPosition.y)/2.2;gl_Position=mvp*vec4(vertexPosition,1.0);}
