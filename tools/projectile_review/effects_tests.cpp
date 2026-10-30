// 本文件验证模型收缩、透明度由高到低及结束状态，不执行性能测试。
#include "effects_asset.hpp"
#include <iostream>
void require(bool v){if(!v)throw std::runtime_error("Spawn test failed");}
int main(){
 review::SpawnParams p;float lastScale=100,lastAlpha=0;
 for(int i=0;i<=100;i++){p.age=p.duration*i/100;auto s=review::SampleEffect(p);require(s.scale<=lastScale+1e-5F&&s.alpha>=lastAlpha-1e-5F);lastScale=s.scale;lastAlpha=s.alpha;}
 require(review::SampleEffect(p).visible&&lastAlpha==1);require(std::abs(lastScale-p.endMultiplier)<1e-5F);
 p.age=p.duration*2;require(review::SampleEffect(p).alpha==1&&review::SampleEffect(p).scale==p.endMultiplier);
 p.age=0;require(review::SampleEffect(p).alpha==0&&!review::SampleEffect(p).visible);
 p.age=0;require(review::SampleEffect(p).scale==p.startMultiplier);p.scale=2;require(review::SampleEffect(p).scale==p.startMultiplier*2);
 for(int bad=0;bad<5;bad++){auto q=p;if(bad==0)q.duration=0;if(bad==1)q.age=-1;if(bad==2)q.scale=0;if(bad==3)q.endMultiplier=q.startMultiplier+1;if(bad==4)q.rotation.w=0;bool rejected=false;try{review::SampleEffect(q);}catch(const std::invalid_argument&){rejected=true;}require(rejected);}
 std::cout<<"Input model shrink/reveal, settled state and parameter validation PASS\n";
}
