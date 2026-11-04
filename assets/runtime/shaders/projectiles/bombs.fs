// 本文件绘制 Bomb 的纯色实体、半透明外壳和流光；材质层不会生成攻击体积。
#version 330
in vec3 local;
in vec3 world;
in vec3 normal;
in vec2 uv;
uniform vec3 eye;
uniform int kind;
uniform int layer;
uniform int palette;
uniform int clay;
uniform float seconds;
uniform float opacity;
uniform int clipEnabled;
uniform float clipFrom,clipTo;
uniform float strength;
uniform vec3 tintMultiplier;
out vec4 finalColor;
void main(){if(kind==3&&clipEnabled==1&&(local.y<clipFrom||local.y>clipTo))discard;float facing=clamp(abs(dot(normalize(normal),normalize(eye-world))),0.0,1.0);float rim=pow(1-facing,1.5);
 vec3 colors[4]=vec3[4](vec3(.25,.22,1),vec3(.1,1,1),vec3(.25,1,.1),vec3(1,.15,.18));
 // player00 ANM 133–136 的原始 RGB：3030ff / 30ffff / 30ff30 / ff3030。
 if(kind==0){colors[0]=vec3(48,48,255)/255.0;colors[1]=vec3(48,255,255)/255.0;colors[2]=vec3(48,255,48)/255.0;colors[3]=vec3(255,48,48)/255.0;}
 if(kind==3){colors[0]=vec3(1,.5,.5);colors[1]=vec3(.5,1,.5);colors[2]=vec3(.5,.5,1);colors[3]=vec3(.5,1,1);}
 float sector=mod((atan(local.z,local.x)+3.14159265)*.63661977+seconds*.1+local.y*.3,4.0);int index=int(floor(sector));
 vec3 tint=palette>=0?colors[palette%4]:mix(colors[index],colors[(index+1)%4],smoothstep(0.0,1.0,fract(sector)));
 float a=opacity;vec3 c=mix(tint,vec3(1),.18*pow(facing,4.0)+.72*pow(facing,24.0));
 if(kind==0){if(layer==0)c=mix(tint,vec3(1),.55);else{c=mix(tint,vec3(1),.2);a*=.6*pow(facing,3.5);}}
 if(kind==2){
  tint=palette%3==0?vec3(.45,.4,1):palette%3==1?vec3(.4,1,.35):vec3(1,.35,.4);
  // 星形实体以真实法线塑形，避免本地角度分段形成不随观察方向变化的暗瓣。
  c=mix(tint*(.88+.12*facing),vec3(1),.18*pow(facing,4.0)+.62*pow(facing,24.0));
  if(layer>0){c=tint;a*=layer==1?.15:.24;}
 }
 if(kind==3){
  float endFade=smoothstep(0.0,.025,local.y)*(1.0-smoothstep(.75,1.0,local.y));
  if(layer==0){c=mix(vec3(1,.92,.65),vec3(1),.65);a*=endFade*(.18+.24*pow(facing,2));}
  if(layer==1){c=tint;a*=endFade*(.035+.2*rim);}
  if(layer==2){
   // 螺旋带横向软边与中心亮芯，向前流动的脉冲不形成封闭白盘。
   float soft=pow(max(0.0,sin(uv.x*3.14159265)),1.4);
   float core=pow(max(0.0,sin(uv.x*3.14159265)),9.0);
   c=mix(tint,vec3(1,.96,.8),.25+.65*core);
   float pulse=.8+.2*sin(local.y*26-seconds*10);
   a*=endFade*soft*pulse*.85;
  }
 }
 if(clay==1){c=vec3(.7)*(.3+.7*facing);a=1;}
 if(layer>0)a*=strength;finalColor=vec4(c*tintMultiplier,a);}
