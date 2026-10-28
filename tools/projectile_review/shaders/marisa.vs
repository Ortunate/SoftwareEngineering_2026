// 本文件将普通魔理沙模型和参数化激光变换到世界空间，保留本地轴向供光效渐变。
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform int kind;
uniform int effect;
uniform float beamLength;
uniform float radius;
uniform float phase;
uniform float growth;
uniform float trailLengthScale;
uniform float trailWidthScale;
out float trailProgress;
out vec3 localPosition;
out vec3 worldPosition;
out vec3 worldNormal;
void main(){
 vec3 p=vertexPosition;vec3 n=vertexNormal;trailProgress=0.0;
 if(effect==1&&kind<2){
  // 每根双弹围绕自己的中心缩放，避免把间距当作尾宽放大；根部锚点保持固定。
  float anchor=kind==0?.7:-.3;float extent=kind==0?2.55:1.6;
  trailProgress=clamp((anchor-p.y)/extent,0.0,1.0);
  float center=kind==0?(p.x<0.0?-.17:.17):0.0;
  p.x=center+(p.x-center)*trailWidthScale;p.z*=trailWidthScale;p.y=anchor+(p.y-anchor)*trailLengthScale;
  n=normalize(vertexNormal/vec3(trailWidthScale,trailLengthScale,trailWidthScale));
 }
 if(kind==2){float cap=p.y<0.0?p.y:p.y>1.0?p.y-1.0:0.0;float r=radius*(effect==1?1.5:1.0);p=vec3(p.x*r,clamp(p.y,0.0,1.0)*beamLength+cap*r,p.z*r);}
 if(kind==3&&effect==1){float a=phase*.4;mat2 rot=mat2(cos(a),-sin(a),sin(a),cos(a));p.xz=rot*p.xz;p*=growth;}
 localPosition=p;worldPosition=vec3(matModel*vec4(p,1));worldNormal=normalize(mat3(matModel)*n);gl_Position=mvp*vec4(p,1);
}
