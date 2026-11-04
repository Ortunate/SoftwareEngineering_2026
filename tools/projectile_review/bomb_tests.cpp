// 本文件核验 Bomb 五角星真实凹角、装饰排除和游戏圆台端面；不执行性能场景。
#include "collision.hpp"
#include "bomb_asset.hpp"
#include "gameplay/effects/bomb_volume.hpp"
#include <iostream>
void require(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char**argv){try{std::filesystem::path root=argc>1?argv[1]:".";
 review::CollisionAsset star(root/"assets/projectiles/bomb_star/collision.bin");require(star.Touches({0,0,0},0),"star center");require(star.Touches({0,.63F,0},.02F),"star top tip");require(!star.Touches({-.294F,.404F,0},.01F),"star concavity must remain empty");require(!star.Touches({0,-.9F,0},.01F),"trail excluded");
 review::CollisionAsset seal(root/"assets/projectiles/bomb_seal/collision.bin");require(seal.Touches({0,0,0},0),"seal inner sphere");require(!seal.Touches({.6F,0,0},.01F),"seal outer shell excluded");
 koumakyou::BombVolume volume;volume.shape=koumakyou::BombVolume::Shape::Frustum;volume.origin={0,0,0};volume.end={0,4,0};volume.radius=.3F;volume.endRadius=1.05F;require(volume.IntersectsSphere({0,0,0},0),"beam start cap");require(volume.IntersectsSphere({0,4,0},0),"beam end cap");require(!volume.IntersectsSphere({0,-.2F,0},.01F),"beam behind excluded");require(volume.IntersectsSphere({.6F,2,0},.01F),"beam middle");
 review::BombAssetInstance p;review::ValidateBomb(p);p.length=0;bool rejected=false;try{review::ValidateBomb(p);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"zero beam length rejected");std::cout<<"PASS Bomb model and attack volume boundaries\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
