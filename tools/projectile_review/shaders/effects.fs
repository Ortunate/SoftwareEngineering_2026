// 本文件为传入模型提供统一透明度；普通模型保留纹理色，敌弹可选择顶点高光遮罩约定。
#version 330
in vec2 uv;
in vec4 color;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec4 tint;
uniform float opacity;
uniform int projectileMask;
out vec4 finalColor;
void main(){
 vec4 surface=texture(texture0,uv)*colDiffuse;
 if(projectileMask==1){surface.rgb*=mix(tint.rgb,vec3(1),color.r*.85);surface.a*=color.g>.2?.45:1.;}
 else surface*=color*vec4(tint.rgb,1);
 finalColor=vec4(surface.rgb,surface.a*opacity*tint.a);
}
