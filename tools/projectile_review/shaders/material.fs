// 金属/粗糙度实时材质：独立颜色、法线、ORM；棚灯反射随观察角度变化。
#version 330
in vec3 worldPosition;
in vec3 worldNormal;
in vec2 uv;
out vec4 finalColor;
uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform vec3 eye;
uniform float lightAngle;
uniform float exposure;
uniform int clay;
const float PI=3.14159265359;
vec3 fresnel(float c,vec3 f){return f+(1-f)*pow(clamp(1-c,0,1),5);}
vec3 lamp(vec3 n,vec3 v,vec3 l,vec3 radiance,vec3 color,float metal,float rough){
    vec3 h=normalize(v+l);float nv=max(dot(n,v),.0001),nl=max(dot(n,l),0),nh=max(dot(n,h),0);
    float a=rough*rough,a2=a*a,d=a2/(PI*pow(nh*nh*(a2-1)+1,2));
    float k=pow(rough+1,2)/8;float g=(nv/(nv*(1-k)+k))*(nl/(nl*(1-k)+k));
    vec3 f=fresnel(max(dot(h,v),0),mix(vec3(.04),color,metal));
    return ((1-f)*(1-metal)*color/PI+d*g*f/max(4*nv*nl,.0001))*radiance*nl;
}
vec3 rotateLight(vec3 p){float c=cos(lightAngle),s=sin(lightAngle);return vec3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
vec3 studio(vec3 direction,float rough){
    vec3 d=rotateLight(direction);
    vec3 result=mix(vec3(.045,.060,.085),vec3(.46,.52,.62),clamp(d.y*.5+.5,0,1));
    // 宽棚灯形成柔和金属高光；这是照明函数，不是画在底色中的假高光。
    float spread=mix(110.,5.,rough*rough);
    result+=vec3(3.5,3.2,2.8)*pow(max(dot(d,normalize(vec3(-.7,.7,1))),0),spread);
    result+=vec3(1.5,1.9,2.5)*pow(max(dot(d,normalize(vec3(1,.15,.3))),0),spread*.6);
    result+=vec3(2.0,1.8,1.5)*pow(max(dot(d,normalize(vec3(.1,1,-.7))),0),spread);
    return result;
}
void main(){
    vec3 albedo=pow(texture(texture0,uv).rgb,vec3(2.2));
    vec3 orm=texture(texture2,uv).rgb;
    float rough=clamp(orm.g,.12,.95),metal=orm.b,ao=orm.r;
    vec3 n=normalize(worldNormal),v=normalize(eye-worldPosition);
    if(clay==0){
        vec3 q1=dFdx(worldPosition),q2=dFdy(worldPosition);vec2 s1=dFdx(uv),s2=dFdy(uv);
        vec3 t=q1*s2.y-q2*s1.y;
        if(dot(t,t)>1e-16){t=normalize(t);vec3 b=normalize(-q1*s2.x+q2*s1.x);
            vec3 map=texture(texture1,uv).rgb*2-1;n=normalize(mat3(t,b,n)*map);}
    }else{albedo=vec3(.36,.39,.43);rough=.58;metal=0;ao=1;}
    vec3 f0=mix(vec3(.04),albedo,metal);
    vec3 result=albedo*(1-metal)*vec3(.24,.27,.32)*ao;
    result+=studio(reflect(-v,n),rough)*fresnel(max(dot(n,v),0),f0)*(.65+.35*ao);
    result+=lamp(n,v,normalize(rotateLight(vec3(-1.5,2,2))),vec3(3.5,3.3,3.0),albedo,metal,rough);
    result+=lamp(n,v,normalize(rotateLight(vec3(2,.6,1))),vec3(1.5,1.9,2.5),albedo,metal,rough);
    result+=lamp(n,v,normalize(rotateLight(vec3(-.5,1,-2))),vec3(2.5,2.1,1.7),albedo,metal,rough);
    result*=exposure;
    result=(result*(2.51*result+.03))/(result*(2.43*result+.59)+.14);
    finalColor=vec4(pow(clamp(result,0,1),vec3(1/2.2)),1);
}
