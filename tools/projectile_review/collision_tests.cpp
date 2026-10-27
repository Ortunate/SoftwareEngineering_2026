// 本文件验证真实苦无资产的环孔、刃体、柄体与变换，不创建图形窗口。
#include "collision.hpp"
#include "raymath.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Expected collision asset path");
    review::CollisionAsset shape(argv[1]);
    require(shape.Parts().size()==7,"Expected seven independently closed components");
    require(shape.Touches({0,.20F,0},.001F),"Blade interior must collide");
    require(shape.Touches({0,-.20F,0},.001F),"Handle interior must collide");
    require(shape.Touches({.071F,-.410F,0},.005F),"Ring wall must collide");
    require(!shape.Touches({0,-.410F,0},.020F),"Ring hole must stay empty");
    require(!shape.Touches({.12F,.20F,0},.01F),"Outside blade must stay empty");
    require(!shape.Touches({0,.20F,.06F},.01F),"Thin blade must not become a thick box");
    require(shape.Touches({0,.499F,0},.003F),"Blade tip must collide");
    for(int i=0;i<=80;++i){float z=-.2F+i*.005F;require(!shape.Touches({0,-.410F,z},.020F),"Small probe must pass through ring in depth");}
    require(shape.Touches({0,-.410F,0},.060F),"Probe wider than hole must touch ring");
    Vector3 p{.12F,.20F,.03F};float d=shape.DistanceSquared(p);
    for(float s:{.2F,1.F,3.F})for(float a:{0.F,.7F,1.8F}){
        Matrix transform=MatrixMultiply(MatrixMultiply(MatrixScale(s,s,s),MatrixRotateXYZ({a,.3F,-.2F})),MatrixTranslate(2,-3,4));
        float actual=shape.WorldDistanceSquared(Vector3Transform(p,transform),transform);
        require(std::abs(actual-d*s*s)<1e-5F,"Rotation/translation/uniform scale distance mismatch");
    }
    bool rejected=false;try{shape.WorldDistanceSquared(p,MatrixScale(1,2,1));}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Nonuniform scales must not silently distort collision");
    std::cout<<"PASS: closed parts, blade, grip, ring hole, 81 through-hole samples, tip, probe size and transforms; "<<shape.TriangleCount()<<" triangles\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
