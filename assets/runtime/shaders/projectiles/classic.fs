// 无 PBR、无纹理采样；纯色面和固定亮面表现原版轮廓，加色壳独立开关。
#version 330
in vec3 n;
in float highlightMask;
in vec4 bodyTint;
in vec4 highlightTint;
in vec4 glowTint;
in float strength;
uniform int glowPass;
uniform int clay;
out vec4 finalColor;
void main(){
    vec3 normal=normalize(n);
    float diffuse=.80+.20*max(dot(normal,normalize(vec3(-.5,.7,1))),0);
    if(glowPass==1){finalColor=vec4(glowTint.rgb,clamp(strength*.45,0,.8)*glowTint.a);return;}
    if(clay==1){finalColor=vec4(vec3(.52,.57,.63)*(.45+.55*max(dot(normal,normalize(vec3(-.5,.7,1))),0)),1);return;}
    float weight=clamp(highlightMask,0,1);
    finalColor=vec4(mix(bodyTint.rgb*diffuse,highlightTint.rgb,weight),1);
}
