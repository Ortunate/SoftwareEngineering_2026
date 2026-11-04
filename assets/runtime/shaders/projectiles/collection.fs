// 本文件绘制随视角变化的柔和实体高光、三维球壳与火焰，不使用模型预烘焙明暗。
#version 330
in vec3 n,world,local,center;
in float instanceScale;
in vec4 bodyTint,highlightTint,glowTint,fx;
uniform vec3 eye;
uniform int kind,role,pass,mode;
uniform float seconds;
out vec4 finalColor;
vec4 sampledColor;
void Shade(){
    vec3 N=normalize(n),V=normalize(eye-world);float facing=clamp(abs(dot(N,V)),0.0,1.0);
    // 弹体视为自发光材质：保留彩色轮廓，以视线夹角塑形，避免固定世界光向造成半面发黑。
    // 高光由变换后的法线逐片段求值；COLOR_0.r 的旧亮面不能再作为固定白色分区。
    float diffuse=.88+.12*facing;
    // 窄镜面高光叠加柔和底层反射，保留弹色，避免整面铺白产生粉质感。
    float highlight=.18*pow(facing,4.0)+.72*pow(facing,24.0);
    if(mode==1){sampledColor=vec4(vec3(.54,.60,.68)*(.45+.55*facing),1);return;}
    if(pass==2){sampledColor=vec4(glowTint.rgb,clamp(fx.x*.35,0,.7)*pow(1-facing,1.3)*glowTint.a);return;}
    // 泡的暗透内球只吸收少量背景光；碰撞几何独立于透明度。
    if(kind==9&&role==0){sampledColor=vec4(0,0,0,.32*fx.z*(.5+.5*facing));return;}
    if(role==1){
        if(!gl_FrontFacing && length(eye-center)>.5*instanceScale)discard;
        float rho=sqrt(max(0,1-facing*facing));
        float white=0,strength=0;
        if(kind==1){float core=1-smoothstep(.73,.77,rho);strength=max(core*1.55,smoothstep(.84,.90,rho)*(1-smoothstep(.975,1,rho))*1.3);white=max(core,.12);}
        if(kind==3){strength=.8*smoothstep(.68,.80,rho)*(1-smoothstep(.86,1,rho));white=1-smoothstep(.65,.86,rho);}
        if(kind==6){strength=1.05*smoothstep(.50,.72,rho)*(1-smoothstep(.78,1,rho));white=1-smoothstep(.62,.94,rho);}
        if(kind==9){strength=1.45*smoothstep(.69,.85,rho)*(1-smoothstep(.985,1,rho));white=smoothstep(.69,.82,rho);
            if(length(eye-center)<.5*instanceScale){strength=.10+.20*facing;white=.3;}}
        vec3 color=mix(bodyTint.rgb,highlightTint.rgb,white);
        sampledColor=vec4(color,min(1,strength*fx.z));return;
    }
    if(role==2){
        if(kind==9){float r=length(local);float white=smoothstep(.425,.47,r);
            float saturation=max(bodyTint.r,max(bodyTint.g,bodyTint.b))-min(bodyTint.r,min(bodyTint.g,bodyTint.b));
            // 端向观察时淡出丝束，避免前后端面在暗中心形成亮色方块；侧向仍保留真实厚度。
            float side=1-abs(dot(normalize(world-center),V));
            float alpha=smoothstep(.12,.7,side)*smoothstep(.19,.27,r)*fx.z;
            sampledColor=vec4(mix(bodyTint.rgb,highlightTint.rgb,white),alpha*mix(.08,1.4,smoothstep(.08,.4,saturation)));return;}
        float h=clamp((local.y+.34)/.9,0,1);float wave=.5+.5*sin(local.y*34+local.x*20-seconds*18.849556+fx.w);
        vec3 color=mix(vec3(1,.78,.18),bodyTint.rgb,smoothstep(.05,.55,h));
        color=mix(color,vec3(1,.19,.015),smoothstep(.5,1,h));
        sampledColor=vec4(color,(.55+.3*wave)*(.7+.3*facing)*(1-.45*h)*fx.z);return;
    }
    if(kind==4||kind==5||kind==8){sampledColor=vec4(mix(bodyTint.rgb*diffuse,highlightTint.rgb,highlight),1);return;}
    if(kind==1){sampledColor=vec4(highlightTint.rgb,1);return;}
    if(kind==3||kind==6){float rho=sqrt(max(0,1-facing*facing));
        float white=.12*(1-smoothstep(kind==3?.72:.48,1.0,rho))+.78*pow(facing,18.0);
        sampledColor=vec4(mix(bodyTint.rgb,highlightTint.rgb,white),1);return;}
    float center=highlight;
    if(kind==2)center=highlight;
    float bright=kind==0?.94:kind==7?1:.98;
    sampledColor=vec4(mix(bodyTint.rgb*diffuse,highlightTint.rgb,center*bright),1);
}

// 所有显示层共享实例显现系数，不改变原材质形状或危险体积。
void main(){Shade();finalColor=vec4(sampledColor.rgb,sampledColor.a*bodyTint.a);}
