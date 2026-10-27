// 本文件通过包围体和三角形 BVH 查询封闭实体距离，并保留尾环真实孔洞。
#include "collision.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <stdexcept>
namespace review {
namespace {
float axis(Vector3 v,int a){return a==0?v.x:a==1?v.y:v.z;}
Bounds empty(){return {{INFINITY,INFINITY,INFINITY},{-INFINITY,-INFINITY,-INFINITY}};}
void include(Bounds& b,Vector3 p){b.lo=Vector3Min(b.lo,p);b.hi=Vector3Max(b.hi,p);}
float boxDistance(Bounds b,Vector3 p){return Vector3DistanceSqr(p,Vector3Clamp(p,b.lo,b.hi));}
bool rayBox(Bounds b,Vector3 p,Vector3 d){
    float near=0,far=INFINITY;
    for(int i=0;i<3;++i){
        float v=axis(d,i),q=axis(p,i);
        if(std::abs(v)<1e-12F){if(q<axis(b.lo,i)||q>axis(b.hi,i))return false;continue;}
        float a=(axis(b.lo,i)-q)/v,c=(axis(b.hi,i)-q)/v;
        if(a>c)std::swap(a,c);near=std::max(near,a);far=std::min(far,c);
        if(near>far)return false;
    }return far>0;
}
int build(Part& p,int begin,int end){
    const int index=static_cast<int>(p.nodes.size());p.nodes.emplace_back();
    Bounds box=empty(),centers=empty();
    for(int i=begin;i<end;++i){const auto& t=p.triangles[p.order[i]];include(box,t.a);include(box,t.b);include(box,t.c);include(centers,Vector3Scale(Vector3Add(Vector3Add(t.a,t.b),t.c),1.F/3));}
    p.nodes[index].box=box;
    if(end-begin<=8){p.nodes[index].begin=begin;p.nodes[index].count=end-begin;return index;}
    Vector3 extent=Vector3Subtract(centers.hi,centers.lo);
    int a=extent.x>extent.y?0:1;if(extent.z>axis(extent,a))a=2;
    int mid=(begin+end)/2;
    std::nth_element(p.order.begin()+begin,p.order.begin()+mid,p.order.begin()+end,[&](int l,int r){
        const auto& t=p.triangles[l];const auto& u=p.triangles[r];
        return axis(Vector3Add(Vector3Add(t.a,t.b),t.c),a)<axis(Vector3Add(Vector3Add(u.a,u.b),u.c),a);
    });
    int left=build(p,begin,mid),right=build(p,mid,end);
    p.nodes[index].left=left;p.nodes[index].right=right;return index;
}
// 最近点按三角形顶点、边和面区域分类；不把细长刃体替换为包围盒。
float triangleDistance(Triangle t,Vector3 p){
    Vector3 ab=Vector3Subtract(t.b,t.a),ac=Vector3Subtract(t.c,t.a),ap=Vector3Subtract(p,t.a);
    float d1=Vector3DotProduct(ab,ap),d2=Vector3DotProduct(ac,ap);
    if(d1<=0&&d2<=0)return Vector3LengthSqr(ap);
    Vector3 bp=Vector3Subtract(p,t.b);float d3=Vector3DotProduct(ab,bp),d4=Vector3DotProduct(ac,bp);
    if(d3>=0&&d4<=d3)return Vector3LengthSqr(bp);
    float vc=d1*d4-d3*d2;
    if(vc<=0&&d1>=0&&d3<=0)return Vector3DistanceSqr(p,Vector3Add(t.a,Vector3Scale(ab,d1/(d1-d3))));
    Vector3 cp=Vector3Subtract(p,t.c);float d5=Vector3DotProduct(ab,cp),d6=Vector3DotProduct(ac,cp);
    if(d6>=0&&d5<=d6)return Vector3LengthSqr(cp);
    float vb=d5*d2-d1*d6;
    if(vb<=0&&d2>=0&&d6<=0)return Vector3DistanceSqr(p,Vector3Add(t.a,Vector3Scale(ac,d2/(d2-d6))));
    float va=d3*d6-d5*d4;
    if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return Vector3DistanceSqr(p,Vector3Add(t.b,Vector3Scale(Vector3Subtract(t.c,t.b),(d4-d3)/((d4-d3)+(d5-d6)))));
    float denom=1/(va+vb+vc);
    return Vector3DistanceSqr(p,Vector3Add(t.a,Vector3Add(Vector3Scale(ab,vb*denom),Vector3Scale(ac,vc*denom))));
}
void nearest(const Part& part,int index,Vector3 p,float& best){
    const auto& n=part.nodes[index];if(boxDistance(n.box,p)>best)return;
    if(n.count){for(int i=n.begin;i<n.begin+n.count;++i)best=std::min(best,triangleDistance(part.triangles[part.order[i]],p));return;}
    int a=n.left,b=n.right;if(boxDistance(part.nodes[a].box,p)>boxDistance(part.nodes[b].box,p))std::swap(a,b);
    nearest(part,a,p,best);nearest(part,b,p,best);
}
void crossings(const Part& part,int index,Vector3 p,Vector3 dir,std::vector<float>& hits){
    const auto& n=part.nodes[index];if(!rayBox(n.box,p,dir))return;
    if(!n.count){crossings(part,n.left,p,dir,hits);crossings(part,n.right,p,dir,hits);return;}
    for(int i=n.begin;i<n.begin+n.count;++i){
        auto t=part.triangles[part.order[i]];
        Vector3 e1=Vector3Subtract(t.b,t.a),e2=Vector3Subtract(t.c,t.a),h=Vector3CrossProduct(dir,e2);
        float det=Vector3DotProduct(e1,h);if(std::abs(det)<1e-12F)continue;
        Vector3 s=Vector3Subtract(p,t.a);float u=Vector3DotProduct(s,h)/det;
        if(u<-1e-6F||u>1.000001F)continue;
        Vector3 q=Vector3CrossProduct(s,e1);float v=Vector3DotProduct(dir,q)/det;
        if(v<-1e-6F||u+v>1.000001F)continue;
        float distance=Vector3DotProduct(e2,q)/det;if(distance>1e-7F)hits.push_back(distance);
    }
}
bool inside(const Part& part,Vector3 p,std::vector<float>& hits){
    if(boxDistance(part.nodes[0].box,p)>0)return false;
    hits.clear();crossings(part,0,p,{.819172F,.371391F,.436214F},hits);
    std::sort(hits.begin(),hits.end());
    int unique=0;float prev=-1;
    // 射线恰好经过共享边时只计一次，避免三角剖分改变实体内外。
    for(float t:hits)if(t-prev>1e-6F){++unique;prev=t;}
    return unique%2==1;
}
template<class T> T read(std::ifstream& stream){T t{};if(!stream.read(reinterpret_cast<char*>(&t),sizeof(t)))throw std::runtime_error("Truncated collision asset");return t;}
}
CollisionAsset::CollisionAsset(const std::filesystem::path& path){
    std::ifstream stream(path,std::ios::binary);char magic[8]{};
    if(!stream.read(magic,8)||std::memcmp(magic,"KCOL0001",8))throw std::runtime_error("Invalid collision asset: "+path.string());
    unsigned count=read<unsigned>(stream);if(!count||count>128)throw std::runtime_error("Invalid collision part count");
    for(unsigned i=0;i<count;++i){
        Part p;unsigned n=read<unsigned>(stream);if(!n||n>1000000)throw std::runtime_error("Invalid collision triangle count");
        for(unsigned j=0;j<n;++j){Triangle t=read<Triangle>(stream);
            for(auto v:{t.a,t.b,t.c})if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))throw std::runtime_error("Nonfinite collision vertex");
            if(Vector3LengthSqr(Vector3CrossProduct(Vector3Subtract(t.b,t.a),Vector3Subtract(t.c,t.a)))<1e-20F)throw std::runtime_error("Degenerate collision triangle");
            p.triangles.push_back(t);
            for(auto v:{t.a,t.b,t.c})bound_=std::max(bound_,Vector3Length(v));
        }
        p.order.resize(n);std::iota(p.order.begin(),p.order.end(),0);build(p,0,n);parts_.push_back(std::move(p));
    }
    if(stream.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing collision data");
}
float CollisionAsset::DistanceSquared(Vector3 point)const{
    thread_local CollisionScratch scratch;
    scratch.Prepare(MaxPartTriangles());
    return DistanceSquared(point,scratch);
}
std::size_t CollisionAsset::MaxPartTriangles()const{
    std::size_t n=0;for(const auto& p:parts_)n=std::max(n,p.triangles.size());return n;
}
float CollisionAsset::DistanceSquared(Vector3 point,CollisionScratch& scratch)const{
    if(scratch.crossings.capacity()<MaxPartTriangles())throw std::invalid_argument("Prepare collision scratch before querying");
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))throw std::invalid_argument("Nonfinite query");
    float best=INFINITY;
    for(const auto& p:parts_){if(inside(p,point,scratch.crossings))return 0;nearest(p,0,point,best);if(best<1e-16F)return 0;}
    return best;
}
float CollisionAsset::WorldDistanceSquared(Vector3 point,Matrix m)const{
    Vector3 x{m.m0,m.m1,m.m2},y{m.m4,m.m5,m.m6},z{m.m8,m.m9,m.m10};
    float s=Vector3Length(x),sy=Vector3Length(y),sz=Vector3Length(z);
    if(!(s>0)||!std::isfinite(s)||std::abs(s-sy)>s*1e-4F||std::abs(s-sz)>s*1e-4F||
       std::abs(Vector3DotProduct(x,y))>s*s*1e-4F||std::abs(Vector3DotProduct(x,z))>s*s*1e-4F||std::abs(Vector3DotProduct(y,z))>s*s*1e-4F||
       Vector3DotProduct(Vector3CrossProduct(x,y),z)<=0)throw std::invalid_argument("Expected positive uniform scale and rotation");
    return DistanceSquared(Vector3Transform(point,MatrixInvert(m)))*s*s;
}
bool CollisionAsset::Touches(Vector3 p,float r)const{if(!std::isfinite(r)||r<0)throw std::invalid_argument("Invalid probe radius");return DistanceSquared(p)<=r*r;}
int CollisionAsset::TriangleCount()const{int n=0;for(const auto& p:parts_)n+=static_cast<int>(p.triangles.size());return n;}
} // namespace review
