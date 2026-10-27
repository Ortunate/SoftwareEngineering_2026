// 独立查看器的世界空间顶点与法线传递。
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 worldPosition;
out vec3 worldNormal;
out vec2 uv;
void main(){
    worldPosition=vec3(matModel*vec4(vertexPosition,1));
    worldNormal=normalize(vec3(matNormal*vec4(vertexNormal,0)));
    uv=vertexTexCoord;
    gl_Position=mvp*vec4(vertexPosition,1);
}
