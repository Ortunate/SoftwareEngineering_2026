// 白亮芯、彩色边与可关闭外光；第二套以轴向圆润亮度表现光斑特征。
#version 330
in vec3 world,n;
in vec4 tint,fx;
in float axial;
flat in float inside;
uniform vec3 eye;
uniform float clipFrom,clipTo;
uniform int pass,variant,mode;
out vec4 finalColor;
void main(){
 if(axial<clipFrom||axial>clipTo)discard;
 float face=abs(dot(normalize(n),normalize(eye-world)));
 // 只保留朝向相机的一面，避免透明前后表面重复累加。
 if(!gl_FrontFacing&&inside<.5)discard;
 float center=smoothstep(.18,.83,face),profile=variant==0?1:mix(.55,1,pow(max(sin(axial*3.141593),0),.4));
 if(mode==1){finalColor=vec4(vec3(.55,.62,.7)*(.4+.6*face),.8);return;}
 vec3 c=mix(tint.rgb,vec3(1),center*profile);
 float a=pass==0?.85:pass==1?.24*pow(1-face,1.3):fx.x*.12*pow(1-face,1.5);
 finalColor=vec4(pass==0?c:tint.rgb,a*(fx.y>.5?1:.55)*tint.a);
}
