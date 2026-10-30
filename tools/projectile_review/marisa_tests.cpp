// 本文件验证实际导出模型的双弹间隙、导弹实体翼片和爆破内球；不创建图形或性能场景。
#include "collision.hpp"
#include "marisa_asset.hpp"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char**argv){try{
 std::filesystem::path root=argc>1?argv[1]:".";
 review::CollisionAsset twin(root/"assets/projectiles/marisa_primary/collision.bin");
 require(!twin.Touches({0,0,0},.025F),"twin gap must be clear");require(twin.Touches({.17F,0,0},.01F),"right core");require(twin.Touches({-.17F,0,0},.01F),"left core");require(!twin.Touches({.29F,-.4F,0},.01F),"aura excluded");
 review::CollisionAsset missile(root/"assets/projectiles/marisa_missile/collision.bin");
 require(missile.Touches({0,0,0},.01F),"missile center");require(missile.Touches({.4F,.07F,0},.015F),"X wing");require(missile.Touches({0,.07F,.4F},.015F),"Z wing");require(!missile.Touches({.8F,0,0},.01F),"missile outside");
 review::CollisionAsset explosion(root/"assets/projectiles/marisa_explosion/collision.bin");
 require(explosion.Touches({0,0,0},0),"explosion center is dangerous");require(explosion.Touches({.49F,0,0},.015F),"explosion surface");require(!explosion.Touches({.65F,0,0},.02F),"flames are decorative");
 review::MarisaInstance sample;sample.kind=3;sample.age=6/60.F;auto visual=review::SampleMarisa(sample);require(std::abs(visual.nativeOpacity-96/255.F)<1e-5F,"explosion frame 6");sample.age=13/60.F;require(std::abs(review::SampleMarisa(sample).opacity-.5F)<1e-5F,"explosion half fade");sample.age=20/60.F;require(!review::SampleMarisa(sample).alive,"explosion ends at 20");sample.kind=2;sample.age=10/60.F;require(std::abs(review::SampleMarisa(sample).nativeOpacity-64/255.F)<1e-5F,"laser half fade in");sample.age=2.25F;require(std::abs(review::SampleMarisa(sample).opacity-.5F)<1e-5F,"laser half fade out");
 sample={};sample.kind=1;auto original=review::SampleMarisa(sample);sample.trailLengthScale=2;sample.trailWidthScale=.5F;auto changed=review::SampleMarisa(sample);require(original.bodyScale==changed.bodyScale&&original.opacity==changed.opacity,"trail size must not alter body scale or lifecycle");sample.trailLengthScale=0;bool rejected=false;try{review::SampleMarisa(sample);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"invalid trail length rejected");
 std::cout<<"PASS Marisa mesh collision probes\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
