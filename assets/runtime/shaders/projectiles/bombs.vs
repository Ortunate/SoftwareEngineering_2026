// 本文件对 Bomb 模型应用可调圆台和流光变换，几何轴为 +Y。
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
uniform mat4 mvp;
uniform mat4 matModel;
uniform int kind;
uniform int layer;
uniform float beamLength;
uniform float nearRadius;
uniform float farRadius;
uniform float seconds;
out vec3 local;
out vec3 world;
out vec3 normal;
out vec2 uv;
void main(){vec3 p=vertexPosition;local=p;uv=vertexTexCoord;
 if(kind==3){if(layer==2){float angle=seconds*.25;mat2 rot=mat2(cos(angle),-sin(angle),sin(angle),cos(angle));p.xz=rot*p.xz;}float r=mix(nearRadius,farRadius,p.y);p=vec3(p.x*r,p.y*beamLength,p.z*r);}
 world=vec3(matModel*vec4(p,1));normal=normalize(mat3(matModel)*vertexNormal);gl_Position=mvp*vec4(p,1);}
