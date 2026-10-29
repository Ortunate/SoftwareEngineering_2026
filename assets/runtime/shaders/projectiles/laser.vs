// 参数化胶囊：中段独立伸长，端部仅按半径缩放。
#version 330
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 origin;
layout(location=3) in vec4 direction;
layout(location=4) in vec4 color;
layout(location=5) in vec4 effects;
uniform mat4 vp;
uniform int pass;
uniform vec3 eye;
out vec3 world,n;
out vec4 tint,fx;
out float axial;
flat out float inside;
void main(){
 vec3 d=normalize(direction.xyz),u=normalize(cross(abs(d.y)<.95?vec3(0,1,0):vec3(1,0,0),d)),v=cross(d,u);
 float r=direction.w*(pass==0?1:pass==1?1.28:1.45);
 vec3 relative=eye-origin.xyz;float along=clamp(dot(relative,d),0,origin.w);
 inside=length(relative-d*along)<r?1:0;
 float y=position.y<0?position.y*r:position.y>1?origin.w+(position.y-1)*r:position.y*origin.w;
 world=origin.xyz+d*y+(u*position.x+v*position.z)*r;n=u*normal.x+d*normal.y+v*normal.z;tint=color;fx=effects;axial=clamp(y/max(origin.w,.001),0,1);gl_Position=vp*vec4(world,1);
}
