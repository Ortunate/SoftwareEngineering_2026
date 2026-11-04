// 本文件为开口圆锥侧面提供轴向透明度渐变；不改变针弹碰撞。
#version 330
in float axial;
uniform vec4 colDiffuse;
out vec4 finalColor;
void main(){float t=clamp(axial,0.0,1.0);float alpha=smoothstep(0.0,0.09,t)*(1.0-smoothstep(0.78,1.0,t));finalColor=vec4(colDiffuse.rgb,alpha*colDiffuse.a);}
