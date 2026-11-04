// 本文件绘制自机弹的抛光主体、原纹理、独立外辉光和球形爆破；透明度按原 ANM 时间设置。
#version 330
in vec3 localPosition;
in float trailProgress;
in vec2 surfaceUv;
uniform float phase;
in vec3 worldPosition;
in vec3 worldNormal;
uniform int kind;
uniform int effect;
uniform int clay;
uniform float opacity;
uniform int clipEnabled;
uniform int clipGeometry;
uniform float clipFrom,clipTo,beamLength;
uniform float strength;
uniform vec3 eye;
uniform vec3 tintMultiplier;
uniform vec4 colDiffuse;
uniform sampler2D texture0;
out vec4 finalColor;
void main(){
 // 独立审阅器保留片元裁剪。游戏由硬件逐采样点裁剪，不能再按片元中心 discard，否则 MSAA 切口会缺样形成接缝。
 if(kind==2&&clipEnabled==1&&clipGeometry==0&&((clipFrom>0.0&&localPosition.y<clipFrom*beamLength)||(clipTo<1.0&&localPosition.y>clipTo*beamLength)))discard;
 float facing=clamp(abs(dot(normalize(worldNormal),normalize(eye-worldPosition))),0.0,1.0);
 float rim=pow(1.0-facing,1.8);
 vec4 surface=kind==4?texture(texture0,surfaceUv)*colDiffuse:vec4(1);
 vec3 tint=kind==4?surface.rgb:kind==0?vec3(.25,.8,1):kind==2?vec3(1,.23,.36):vec3(.3,1,.045);
 // kind=4 为灵梦实体层，保留 glTF 材质底色；所有自机弹共用视角相关的抛光反射。
 float gloss=.18*pow(facing,4.0)+.72*pow(facing,24.0);
 vec3 color=mix(tint*(.88+.12*facing),vec3(1),kind==4?.18*gloss:gloss);float alpha=opacity*surface.a;
 if(kind==0){float head=smoothstep(-.85,.5,localPosition.y);color=mix(tint,vec3(1),.55*head+.4*gloss);}
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
