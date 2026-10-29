// 本文件保留传入网格的纹理坐标和顶点颜色，变换由公用收缩函数提供。
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 uv;
out vec4 color;
void main(){uv=vertexTexCoord;color=vertexColor;gl_Position=mvp*vec4(vertexPosition,1);}
