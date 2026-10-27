// 固定白亮面遮罩、每实例颜色和统一缩放；光效只改变可见壳层。
#version 330
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in float mask;
layout(location=3) in vec4 posScale;
layout(location=4) in vec4 rotation;
layout(location=5) in vec4 body;
layout(location=6) in vec4 highlight;
layout(location=7) in vec4 glow;
layout(location=8) in vec4 effects;
uniform mat4 viewProjection;
uniform int glowPass;
out vec3 n;
out float highlightMask;
out vec4 bodyTint;
out vec4 highlightTint;
out vec4 glowTint;
out float strength;
vec3 rotate(vec3 p){return p+2*cross(rotation.xyz,cross(rotation.xyz,p)+rotation.w*p);}
void main(){
    vec3 local=position+normal*(glowPass==1?effects.y:0);
    gl_Position=viewProjection*vec4(posScale.xyz+rotate(local)*posScale.w,1);
    n=rotate(normal);highlightMask=mask;bodyTint=body;highlightTint=highlight;glowTint=glow;strength=effects.x;
}
