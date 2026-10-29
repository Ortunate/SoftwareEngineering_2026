// 本文件绘制纯色主体、独立外辉光和球形爆破边缘；透明度由调用方按原 ANM 时间设置。
#version 330
in vec3 localPosition;
in float trailProgress;
uniform float phase;
in vec3 worldPosition;
in vec3 worldNormal;
uniform int kind;
uniform int effect;
uniform int clay;
uniform float opacity;
uniform int clipEnabled;
uniform float clipFrom,clipTo,beamLength;
uniform float strength;
uniform vec3 eye;
uniform vec3 tintMultiplier;
out vec4 finalColor;
void main(){
 if(kind==2&&clipEnabled==1&&((clipFrom>0.0&&localPosition.y<clipFrom*beamLength)||(clipTo<1.0&&localPosition.y>clipTo*beamLength)))discard;
 float facing=abs(dot(normalize(worldNormal),normalize(eye-worldPosition)));
 float rim=pow(1.0-facing,1.8);
 vec3 tint=kind==0?vec3(.25,.8,1):kind==2?vec3(1,.23,.36):vec3(.3,1,.045);
 vec3 color=mix(tint,vec3(1),.25+.7*pow(facing,4));float alpha=opacity;
 if(kind==0){float head=smoothstep(-.85,.5,localPosition.y);color=mix(tint,vec3(1),head);}
 if(kind==3){color=mix(vec3(.04,.13,.025),vec3(.45,1,.1),rim);alpha*=.16+.75*rim;}
 if(effect==1){color=tint;alpha=opacity*strength*(.08+.6*rim);if(kind==3){float outer=smoothstep(.54,.69,length(localPosition));color=mix(vec3(.18,1,.015),vec3(.85,1,.24),outer*.6+rim*.4);alpha=opacity*strength*(.2+1.2*rim);}}
 if(effect==1&&kind<2){
  // 边缘趋于透明而非描亮轮廓；轴向包络在根部和尾端归零，消除封口与硬壳。
  float t=trailProgress;
  float envelope=smoothstep(0.0,.15,t)*(1.0-smoothstep(.28,1.0,t));
  float softEdge=pow(facing,1.6);
  float flow=1.0+.1*sin(6.2831853*(t-phase/.6));
  color=mix(kind==0?vec3(.72,.94,1):vec3(.92,1,.55),tint,smoothstep(.1,.65,t));
  alpha=opacity*strength*envelope*softEdge*flow;
 }
 if(clay==1){color=vec3(.65)*(.35+.65*facing);alpha=1;}
 finalColor=vec4(color*tintMultiplier,alpha);
}
