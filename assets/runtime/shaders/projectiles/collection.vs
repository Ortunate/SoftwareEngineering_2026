// 本文件变换弹体法线、实例颜色和有界火焰形变；壳层与外焰角色由索引区间选择。
#version 330
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 maskPhase;
layout(location=3) in vec4 posScale;
layout(location=4) in vec4 rotation;
layout(location=5) in vec4 body;
layout(location=6) in vec4 highlight;
layout(location=7) in vec4 glow;
layout(location=8) in vec4 effects;
uniform mat4 viewProjection;
uniform float seconds;
uniform int role,pass,kind;
out vec3 n,world,local,center;
out float instanceScale;
out vec4 bodyTint,highlightTint,glowTint,fx;
vec3 rotate(vec3 p){return p+2*cross(rotation.xyz,cross(rotation.xyz,p)+rotation.w*p);}
void main(){
    vec3 p=position;
    if(role==2&&kind==7){float height=clamp((p.y+.34)/.9,0,1);float a=seconds*18.849556+effects.w+maskPhase.y*6.283185;
        p.x+=height*height*(.058*sin(a+height*4)+.025*sin(a*2));p.z+=height*height*.035*cos(a+height*3);p.y+=height*.025*sin(a+1.2);}
    if(pass==2)p+=normal*effects.y;
    world=posScale.xyz+rotate(p)*posScale.w;local=p;n=rotate(normal);
    center=posScale.xyz;instanceScale=posScale.w;
    bodyTint=body;highlightTint=highlight;glowTint=glow;fx=effects;
    gl_Position=viewProjection*vec4(world,1);
}
