// 本文件以视角相关柔光绘制实体与可选加色壳；不使用固定亮面遮罩。
#version 330
in vec3 n,world;
uniform vec3 eye;
in vec4 bodyTint;
in vec4 highlightTint;
in vec4 glowTint;
in float strength;
uniform int glowPass;
uniform int clay;
out vec4 finalColor;
void main(){
    float facing=abs(dot(normalize(n),normalize(eye-world)));
    float diffuse=.82+.18*facing;
    if(glowPass==1){finalColor=vec4(glowTint.rgb,clamp(strength*.45,0,.8)*glowTint.a);return;}
    if(clay==1){finalColor=vec4(vec3(.52,.57,.63)*(.45+.55*facing),1);return;}
    // 与普通弹体集合保持一致，旋转时由表面朝向决定高光，而非顶点颜色。
    float weight=.72*pow(facing,6.0);
    finalColor=vec4(mix(bodyTint.rgb*diffuse,highlightTint.rgb,weight),1);
}
