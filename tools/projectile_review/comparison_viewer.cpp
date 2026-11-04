// 本文件并排审阅原作 DDS/ANM 与正式 3D 材质，提供轨道相机和全目录图形自检；不启动游戏或写入玩家记录。
#include "comparison_catalog.hpp"
#include "resources/asset_manager.hpp"
#include "resources/projectile_assets.hpp"
#include "render/combat_renderer.hpp"
#include "render/projectiles/model_draw.hpp"
#include "render/projectiles/marisa_asset.hpp"
#include "render/projectiles/marisa_beam_geometry.hpp"
#include "render/projectiles/bomb_renderer.hpp"
#include "render/bomb_geometry.hpp"
#include "external/glad.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using namespace koumakyou;
using namespace koumakyou::comparison;
namespace {
constexpr int kWidth=1440,kHeight=920;
constexpr Rectangle kRawPanel{260,148,410,536},kModelPanel{690,148,730,536};
constexpr Color kBackground{17,24,35,255},kText{226,232,241,255},kMuted{151,166,185,255},kAccent{104,215,201,255};
const char* kWords="弹体素材与建模对照原作当前材质颜色档位素材层附加副机模型固定坐标场景轨道视角鼠标拖动旋转滚轮缩放重置正面侧面背面俯视暂停播放逐帧线框网格深浅背景原图脚本着色局内透明度审阅亮度返回截图已保存未找到独立网格采用正式平面几何仅展示单体而非整场演出适配窗口取景不代表相对尺寸纹理显示动画源图保留原始透明通道全部变体上一项下一项";
struct Options {fs::path root,output;bool verify=false;int entry=0,variant=2,frames=0;std::string view="orbit";};
Options Parse(int argc,char** argv){
 if(argc<2)throw std::runtime_error("Usage: projectile_comparison REPOSITORY [--verify] [--capture-dir DIR] [--entry KEY] [--variant N] [--view orbit|front|side|back|top] [--frames N]");
 Options o;bool explicitVariant=false;o.root=fs::absolute(argv[1]);o.output=o.root/"output/projectile-review/comparison";
 for(int i=2;i<argc;++i){std::string a=argv[i];if(a=="--verify"){o.verify=true;continue;}
  if(i+1==argc)throw std::runtime_error("Missing value for "+a);std::string value=argv[++i];
  if(a=="--capture-dir")o.output=fs::absolute(value);
  else if(a=="--variant"){o.variant=std::stoi(value);explicitVariant=true;}
  else if(a=="--frames")o.frames=std::stoi(value);
  else if(a=="--view")o.view=value;
  else if(a=="--entry"){
   auto found=std::find_if(kEntries.begin(),kEntries.end(),[&](const auto& e){return value==e.key;});
   if(found==kEntries.end())throw std::runtime_error("Unknown entry: "+value);o.entry=int(found-kEntries.begin());if(!explicitVariant)o.variant=0;
  }else throw std::runtime_error("Unknown argument: "+a);
 }
 if(o.variant<0||o.variant>=kEntries[o.entry].variants||o.frames<0)throw std::runtime_error("Invalid variant or frame count");
 if(o.view!="orbit"&&o.view!="front"&&o.view!="side"&&o.view!="back"&&o.view!="top")throw std::runtime_error("Unknown camera view");
 return o;
}
/// @brief raylib 5.5 只接受独立 SFNT；将 TTC 首字库在内存中重排表偏移，不落盘或分发系统字体。
std::vector<unsigned char> StandaloneFont(std::vector<unsigned char> data){
 if(data.size()<16||std::string(reinterpret_cast<char*>(data.data()),4)!="ttcf")return data;
 const auto read32=[&](std::size_t p){if(p+4>data.size())throw std::runtime_error("Truncated TTC table");return std::uint32_t(data[p])<<24|std::uint32_t(data[p+1])<<16|std::uint32_t(data[p+2])<<8|data[p+3];};
 const std::size_t first=read32(12);if(first+12>data.size())throw std::runtime_error("Invalid TTC face");
 const int count=int(data[first+4])*256+data[first+5];const std::size_t header=12+16*count;
 if(first+header>data.size())throw std::runtime_error("Truncated TTC directory");
 std::vector<unsigned char> result(data.begin()+first,data.begin()+first+header);
 for(int i=0;i<count;++i){
  const auto table=first+12+16*i;const auto start=read32(table+8),length=read32(table+12);
  if(std::uint64_t(start)+length>data.size())throw std::runtime_error("Invalid TTC table range");
  result.resize((result.size()+3)&~std::size_t(3));const auto offset=result.size();
  result.insert(result.end(),data.begin()+start,data.begin()+start+length);
  for(int byte=0;byte<4;++byte)result[12+16*i+8+byte]=static_cast<unsigned char>(offset>>(24-8*byte));
 }
 return result;
}
/// @brief 仅借用已分发字体或系统字体文件；字形纹理由本工具独占，不改游戏字体子集。
Font LoadReviewFont(const fs::path& root){
 std::string words=kWords;
 for(const char* name:{"comparison_viewer.cpp","comparison_catalog.hpp"}){std::ifstream file(root/"tools/projectile_review"/name);words.append(std::istreambuf_iterator<char>(file),{});}
 for(const auto& e:kEntries){words+=e.title;words+=e.key;}
 for(int c=32;c<127;++c)words+=char(c);
 int count=0;int* loaded=LoadCodepoints(words.c_str(),&count);std::vector<int> codes(loaded,loaded+count);UnloadCodepoints(loaded);
 std::sort(codes.begin(),codes.end());codes.erase(std::unique(codes.begin(),codes.end()),codes.end());
 const std::array<fs::path,7> fonts{{"/System/Library/Fonts/PingFang.ttc","/System/Library/Fonts/STHeiti Medium.ttc","/System/Library/Fonts/Supplemental/Songti.ttc","C:/Windows/Fonts/msyh.ttc",
  "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",root/"assets/runtime/common/ui_font.otf",root/"assets/runtime/common/ui_font_bold.otf"}};
 for(const auto& file:fonts)if(fs::exists(file)){
  std::ifstream stream(file,std::ios::binary);std::vector<unsigned char> bytes(std::istreambuf_iterator<char>(stream),{});bytes=StandaloneFont(std::move(bytes));
  auto font=LoadFontFromMemory(".ttf",bytes.data(),int(bytes.size()),40,codes.data(),int(codes.size()));
  if(IsFontValid(font)&&font.texture.id!=GetFontDefault().texture.id){SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);return font;}
 }
 return GetFontDefault();
}
void SaveTexture(RenderTexture2D target,const fs::path& file){
 fs::create_directories(file.parent_path());Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
 const bool saved=ExportImage(image,file.string().c_str());UnloadImage(image);if(!saved)throw std::runtime_error("Cannot write "+file.string());
}
int Ink(RenderTexture2D target){
 Image image=LoadImageFromTexture(target.texture);auto colors=LoadImageColors(image);int count=0;
 for(int i=0;i<image.width*image.height;++i)if(std::abs(int(colors[i].r)-kBackground.r)+std::abs(int(colors[i].g)-kBackground.g)+std::abs(int(colors[i].b)-kBackground.b)>12)++count;
 UnloadImageColors(colors);UnloadImage(image);return count;
}
class Viewer {
public:
 explicit Viewer(const Options& options):options_(options),assets_(options.root/"assets/runtime"),pool_(std::make_unique<BulletSystem>()){
  // 清单存储覆盖 AssetManager 的借用期；按目录收集素材，不加载任何关卡时间轴、音频或玩家配置。
  for(std::size_t i=0;i<kEntries.size();++i){offset_[i]=definitions_.size();for(int v=0;v<kEntries[i].variants;++v){
   VisualDefinition d;d.id=static_cast<VisualId>(definitions_.size());d.mode=VisualMode::Bullet;d.animation=Reference(kEntries[i],v);definitions_.push_back(d);
  }}
  assets_.LoadStage({definitions_,{},{},{}});if(!assets_.Projectiles())throw std::runtime_error("Projectile models not loaded");
  font_=LoadReviewFont(options.root);raw_=LoadRenderTexture(int(kRawPanel.width),int(kRawPanel.height));
  model_=LoadRenderTexture(int(kModelPanel.width),int(kModelPanel.height));canvas_=LoadRenderTexture(kWidth,kHeight);
  if(!IsRenderTextureValid(raw_)||!IsRenderTextureValid(model_)||!IsRenderTextureValid(canvas_))throw std::runtime_error("Cannot create comparison render targets");
  Select(options.entry,options.variant);Preset(options.view);
 }
 ~Viewer(){UnloadRenderTexture(raw_);UnloadRenderTexture(model_);UnloadRenderTexture(canvas_);if(font_.texture.id!=GetFontDefault().texture.id)UnloadFont(font_);}
 void Run(){int frames=0;while(!WindowShouldClose()){
  Input();if(playing_)frame_=std::fmod(frame_+GetFrameTime()*60,FrameCount(Selected()));Render(false);
  BeginDrawing();ClearBackground({8,12,19,255});const auto r=WindowRect();DrawTexturePro(canvas_.texture,{0,0,float(kWidth),-float(kHeight)},r,{0,0},0,WHITE);EndDrawing();
  if(IsKeyPressed(KEY_F12))Capture("manual-"+std::to_string(captures_++));
  if(options_.frames>0&&++frames>=options_.frames)break;
 }}
 void Verify(){
  fs::create_directories(options_.output);std::ofstream report(options_.output/"report.json");
  report<<"{\n  \"entries\":25, \"variants\":"<<definitions_.size()<<",\n  \"samples\":[\n";bool first=true;int samples=0;
  std::set<int> models;
  for(std::size_t e=0;e<kEntries.size();++e)for(int v=0;v<kEntries[e].variants;++v){
   Select(int(e),v);
   for(int view=0;view<(v==0?5:1);++view){
    const char* name=view==0?"orbit":view==1?"front":view==2?"side":view==3?"back":"top";Preset(name);Render(true);
    const int sourceInk=Ink(raw_),modelInk=Ink(model_);
    if(sourceInk<25||modelInk<25)throw std::runtime_error("Empty comparison: "+std::string(Selected().key)+" variant="+std::to_string(v)+" view="+name);
    if(!first)report<<",\n";first=false;const auto& selection=Visual().animation;
    report<<"    {\"entry\":"<<std::quoted(Selected().key)<<",\"variant\":"<<v<<",\"view\":"<<std::quoted(name)
     <<",\"atlas\":"<<std::quoted(std::string(battle::Describe(selection.atlas).name))<<",\"script\":"<<selection.script
     <<",\"sprite\":"<<NativeFrame().sprite<<",\"reference_pixels\":"<<sourceInk<<",\"model_pixels\":"<<modelInk<<"}";
    if(v==0&&view==0)Capture(Selected().key);
    if(Selected().model==ProjectileAssetId::BombSeal&&view==0)Capture("bomb-seal-color-"+std::to_string(v));
    ++samples;
   }
   if(Selected().model!=ProjectileAssetId::Invalid)models.insert(int(Selected().model));
  }
  if(definitions_.size()!=184||models.size()!=ProjectileModels.size())throw std::runtime_error("Incomplete projectile catalog");
  report<<"\n  ],\n  \"model_resources\":"<<models.size()<<", \"rendered_samples\":"<<samples<<", \"passed\":true\n}\n";
  std::cout<<"PASS: 25 entries, 184 variants, 24 model resources, "<<samples<<" nonempty orbit/reference samples\n";
 }
private:
 const Entry& Selected()const{return kEntries[selected_];}
 const VisualResource& Visual()const{return assets_.GetVisual(static_cast<VisualId>(offset_[selected_]+variant_));}
 anm::Frame NativeFrame(int variant=-1)const{
  const auto& v=variant<0?Visual():assets_.GetVisual(static_cast<VisualId>(offset_[selected_]+variant));anm::Player animation;
  animation.Start(*v.archive,v.animation.script,v.animation.spriteOffset,v.animation.initialSprite);animation.Advance(frame_/60.0);
  return animation.Sample();
 }
 void Select(int entry,int variant){selected_=(entry+int(kEntries.size()))%int(kEntries.size());variant_=variant;frame_=float(InitialFrame(Selected()));zoom_=1;notice_.clear();}
 void Preset(const std::string& name){yaw_=name=="side"?PI/2:name=="back"?PI:name=="orbit"?.25F:0;pitch_=name=="top"?1.35F:name=="orbit"?.12F:0;roll_=0;}
 Rectangle WindowRect()const{const float s=std::min(GetScreenWidth()/float(kWidth),GetScreenHeight()/float(kHeight));return {(GetScreenWidth()-kWidth*s)/2,(GetScreenHeight()-kHeight*s)/2,kWidth*s,kHeight*s};}
 Vector2 Mouse()const{const auto r=WindowRect();auto p=GetMousePosition();return {(p.x-r.x)*kWidth/r.width,(p.y-r.y)*kHeight/r.height};}
 bool Click(Rectangle r)const{return IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&CheckCollisionPointRec(Mouse(),r);}
 void Label(const std::string& text,float x,float y,float size,Color color=kText)const{DrawTextEx(font_,text.c_str(),{x,y},size,size*.015F,color);}
 Rectangle VariantRect(int v)const{return {274.F+(v%8)*48,596.F+(v/8)*36,43,30};}
 void Input(){
  if(IsKeyPressed(KEY_TAB)||IsKeyPressed(KEY_DOWN))Select(selected_+(IsKeyDown(KEY_LEFT_SHIFT)?-1:1),0);
  if(IsKeyPressed(KEY_UP))Select(selected_-1,0);
  for(int i=0;i<int(kEntries.size());++i)if(Click({14,136.F+i*25,230,24}))Select(i,0);
  if(IsKeyPressed(KEY_C)||IsKeyPressed(KEY_RIGHT))variant_=(variant_+1)%Selected().variants;
  if(IsKeyPressed(KEY_LEFT))variant_=(variant_+Selected().variants-1)%Selected().variants;
  for(int v=0;v<Selected().variants;++v)if(Click(VariantRect(v)))variant_=v;
  if(IsKeyPressed(KEY_SPACE))playing_=!playing_;
  if(IsKeyPressed(KEY_COMMA)){playing_=false;frame_=std::max(0.F,frame_-1);}
  if(IsKeyPressed(KEY_PERIOD)){playing_=false;frame_=std::min(float(FrameCount(Selected())-1),frame_+1);}
  if(IsKeyPressed(KEY_R)){zoom_=1;Preset("orbit");}
  if(IsKeyPressed(KEY_Q))roll_-=PI/12;if(IsKeyPressed(KEY_E))roll_+=PI/12;
  if(IsKeyPressed(KEY_ONE))Preset("front");if(IsKeyPressed(KEY_TWO))Preset("side");if(IsKeyPressed(KEY_THREE))Preset("back");if(IsKeyPressed(KEY_FOUR))Preset("top");
  if(IsKeyPressed(KEY_W))wire_=!wire_;if(IsKeyPressed(KEY_G))grid_=!grid_;if(IsKeyPressed(KEY_B))light_=!light_;
  if(IsKeyPressed(KEY_N))tinted_=!tinted_;if(IsKeyPressed(KEY_A))gameAlpha_=!gameAlpha_;
  if(CheckCollisionPointRec(Mouse(),kModelPanel)){
   if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){const auto d=GetMouseDelta();const float scale=kWidth/WindowRect().width;yaw_-=d.x*scale*.008F;pitch_=std::clamp(pitch_+d.y*scale*.006F,-1.45F,1.45F);}
   zoom_=std::clamp(zoom_*std::pow(.88F,GetMouseWheelMove()),.25F,6.F);
  }
  if(Click({276,790,300,22}))playing_=!playing_;
  if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&CheckCollisionPointRec(Mouse(),{276,828,1120,24})){playing_=false;frame_=std::clamp((Mouse().x-276)/1120,0.F,1.F)*(FrameCount(Selected())-1);}
 }
 Bullet PreviewBullet()const{
  const auto& e=Selected();Bullet b;b.active=true;b.owner=e.kind==Kind::Enemy?BulletOwner::Enemy:BulletOwner::Player;
  b.position={0,0,0};b.SetVelocity({0,1,0});b.radius=e.kind==Kind::Enemy?battle::EnemyBulletHitRadius(e.index):.2F;
  b.nativeType=e.kind==Kind::Enemy?e.index:-1;b.nativeColor=variant_;b.age=frame_/60;b.maxAge=3;b.color={255,255,255,static_cast<unsigned char>(gameAlpha_?51:255)};
  if(e.model==ProjectileAssetId::MarisaLaser){b.playerShotKind=PlayerShotKind::Laser;b.SetVelocity({});b.SetBeamDirection({0,1,0});b.position={0,-1.5F,0};b.beamLength=3;b.radius=(10+variant_*5)*native_sizes::kWorldPerPixel*.5F;b.laserFadeAt=2;b.maxAge=2.5F;}
  return b;
 }
 struct Framing {Vector3 center{};float radius=1;};
 Framing Bounds(const Bullet& b)const{
  const auto& e=Selected();const auto* assets=assets_.Projectiles();
  if((e.kind==Kind::Enemy||e.kind==Kind::Player)&&ProjectileFrameBuilder::Resolve(b,Visual()).id!=e.model)
   throw std::runtime_error("Catalog/model binding mismatch: "+std::string(e.key));
  if(e.kind==Kind::Laser||e.model==ProjectileAssetId::BombSpark)return {{0,0,0},2.1F};
  if(e.kind==Kind::Bomb||e.kind==Kind::Option)return {{0,0,0},e.kind==Kind::Bomb?1.25F:.7F};
  if(e.model==ProjectileAssetId::MarisaLaser){const auto g=projectiles::SampleMarisaBeamGeometry(b,variant_);return {Vector3Scale(Vector3Add(g.origin,g.end),.5F),g.length*.5F+g.haloRadius};}
  const auto binding=ProjectileFrameBuilder::Resolve(b,Visual());const auto pose=ProjectileFrameBuilder::Pose(b,binding,*assets);
  BoundingBox box{{INFINITY,INFINITY,INFINITY},{-INFINITY,-INFINITY,-INFINITY}};
  for(const auto& model:assets->Get(e.model).layers)if(model.meshCount){const auto local=GetModelBoundingBox(model);float scale=pose.scale;
   if(e.model==ProjectileAssetId::MarisaExplosion){projectiles::MarisaInstance s;s.kind=3;s.tier=variant_;s.age=20.F/60;scale*=projectiles::SampleMarisa(s).growth;}
   for(int bits=0;bits<8;++bits){Vector3 p{bits&1?local.max.x:local.min.x,bits&2?local.max.y:local.min.y,bits&4?local.max.z:local.min.z};p=Vector3Add(pose.position,Vector3RotateByQuaternion(Vector3Scale(p,scale),pose.rotation));box.min=Vector3Min(box.min,p);box.max=Vector3Max(box.max,p);}
  }
  return {Vector3Scale(Vector3Add(box.min,box.max),.5F),std::max(.1F,Vector3Distance(box.min,box.max)*.5F)};
 }
 void DrawModelView(const Camera3D& camera,const Bullet& b){
  using namespace projectile_detail;
  const auto& e=Selected();auto* a=assets_.Projectiles();const auto& v=Visual();const float aspect=kModelPanel.width/kModelPanel.height;
  if(e.kind==Kind::Enemy){
   const auto pose=ProjectileFrameBuilder::Pose(b,ProjectileFrameBuilder::Resolve(b,v),*a);projectiles::CollectionInstance item;item.type=e.index;
   item.transform.position=pose.position;item.transform.rotation=pose.rotation;item.transform.scale=pose.scale;item.transform.appearance=projectiles::AppearanceFor(e.index,variant_);
   a->Collection().Prepare(std::span(&item,1),camera,aspect,false);a->Collection().Draw(frame_/60,true);
  }else if(e.kind==Kind::Laser){
   projectiles::LaserInstance beam;beam.origin={0,-1.5F,0};beam.direction={0,1,0};beam.length=3;beam.radius=.09F;beam.variant=e.index;beam.color=variant_;
   beam.age=1;beam.warning=beam.growEnd=0;beam.dangerEnd=10;beam.end=11;beam.fadeSeconds=0;
   a->Lasers().Draw(std::span(&beam,1),camera,aspect);
  }else if(e.kind==Kind::Player){
   combat_.Prepare(a,*pool_,{},camera,aspect);if(b.IsBeam())combat_.DrawBeam(b,v,0,1);else combat_.DrawBullet(b,v);
  }else if(e.kind==Kind::Option){
   // 核心固定在世界空间，允许观察背面与厚度；不使用生产环境中始终朝向相机的副机姿态。
   ProjectilePose pose;const auto frame=NativeFrame();Model body=a->Get(e.model).layers[0];const auto box=GetModelBoundingBox(body);pose.scale=1.2F/Vector3Distance(box.min,box.max);
   float angle=frame.rotation.z;if(e.index==1&&variant_==1)angle=-angle;
   pose.rotation=QuaternionFromAxisAngle({0,0,1},-angle);
   if(e.index==0&&variant_==0)body.transform=MatrixMultiply(body.transform,MatrixScale(-1,1,1));
   Color tint=frame.color;tint.a=gameAlpha_?static_cast<unsigned char>(51.F*frame.color.a/255):255;
   BeginBlendMode(BLEND_ALPHA);rlDrawRenderBatchActive();rlDisableDepthMask();DrawLayer(body,{},pose,tint);EndBlendMode();
  }else{
   PlayerBombSprite p;p.frame=NativeFrame();p.frame.visible=true;p.frame.color.a=gameAlpha_?p.frame.color.a:192;p.ageSeconds=frame_/60;p.height=1.2F;p.volume.radius=.7F;
   if(e.index==0||e.index==2)p.layer=variant_; // 封印四色及星群三色均沿用正式层号选择 palette。
   p.shape=e.index==0?PlayerBombSprite::Shape::Sphere:e.index==1?PlayerBombSprite::Shape::Slab:e.index==2?PlayerBombSprite::Shape::Star:PlayerBombSprite::Shape::Beam;
   if(e.index==1){p.volume.shape=BombVolume::Shape::Slab;p.volume.halfExtents={1,1,.035F};}
   if(e.index==3){p.position={0,-1.5F,0};p.end={0,1.5F,0};p.volume.origin=p.position;p.volume.end=p.end;p.volume.radius=.3F;p.volume.endRadius=1.05F;p.volume.shape=BombVolume::Shape::Frustum;}
   if(!BombRenderer::Draw(a,camera,{&p,kInvalidVisualId},v,0,1)){
    rlDrawRenderBatchActive();rlDisableDepthMask();rlDisableBackfaceCulling();DrawBombGeometry(v.texture,*v.archive,p);
   }
  }
 }
 void DrawReference(Rectangle destination,int variant=-1){
  const auto frame=NativeFrame(variant);if(frame.sprite<0||frame.source.width<=0||frame.source.height<=0)throw std::runtime_error("Native sprite missing: "+std::string(Selected().key));
  const auto& v=Visual();const float sx=float(v.texture.width)/v.archive->width,sy=float(v.texture.height)/v.archive->height;
  const Rectangle source{frame.source.x*sx,frame.source.y*sy,frame.source.width*sx,frame.source.height*sy};
  const float scale=std::min(destination.width/source.width,destination.height/source.height);
  const Rectangle target{destination.x+(destination.width-source.width*scale)/2,destination.y+(destination.height-source.height*scale)/2,source.width*scale,source.height*scale};
  Color tint=tinted_?frame.color:WHITE;tint.a=255;BeginBlendMode(BLEND_ALPHA);DrawTexturePro(v.texture,source,target,{0,0},0,tint);EndBlendMode();
 }
 void Render(bool verify){
  const auto bullet=PreviewBullet();const auto fit=Bounds(bullet);const float distance=fit.radius*3.6F*zoom_;
  Camera3D camera{Vector3Add(fit.center,{distance*std::sin(yaw_)*std::cos(pitch_),distance*std::sin(pitch_),distance*std::cos(yaw_)*std::cos(pitch_)}),fit.center,{0,1,0},40,CAMERA_PERSPECTIVE};
  camera.up=Vector3RotateByAxisAngle(camera.up,Vector3Normalize(Vector3Subtract(camera.target,camera.position)),roll_);
  BeginTextureMode(raw_);ClearBackground(kBackground);
  if(!verify)for(int y=0;y<raw_.texture.height;y+=24)for(int x=0;x<raw_.texture.width;x+=24)if((x/24+y/24)%2)DrawRectangle(x,y,24,24,{23,32,45,255});
  DrawReference({28,46,kRawPanel.width-56,370});EndTextureMode();
  BeginTextureMode(model_);ClearBackground(verify?kBackground:light_?Color{197,207,216,255}:kBackground);BeginMode3D(camera);
  if(grid_&&!verify){rlPushMatrix();rlTranslatef(fit.center.x,fit.center.y-fit.radius*.7F,fit.center.z);DrawGrid(12,fit.radius*.25F);rlPopMatrix();}
  rlDrawRenderBatchActive();rlDisableBackfaceCulling();if(wire_)glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
  DrawModelView(camera,bullet);rlDrawRenderBatchActive();glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);rlEnableDepthMask();rlEnableBackfaceCulling();EndMode3D();EndTextureMode();
  DrawCanvas(camera);
 }
 void DrawCanvas(const Camera3D& camera){
  BeginTextureMode(canvas_);ClearBackground({10,16,25,255});
  Label("弹体素材与 3D 建模对照",22,22,30);Label("原作 DDS / ANM  ×  当前游戏材质",23,66,18,kMuted);
  Label(std::string(Selected().title)+"  /  "+Selected().key,264,105,23,kAccent);
  for(int i=0;i<int(kEntries.size());++i){if(i==selected_)DrawRectangleRounded({14,136.F+i*25,230,24},.2F,4,{33,62,74,255});Label(std::to_string(i+1)+"  "+kEntries[i].title,22,139.F+i*25,16,i==selected_?kText:kMuted);}
  DrawTextureRec(raw_.texture,{0,0,float(raw_.texture.width),-float(raw_.texture.height)},{kRawPanel.x,kRawPanel.y},WHITE);
  DrawTextureRec(model_.texture,{0,0,float(model_.texture.width),-float(model_.texture.height)},{kModelPanel.x,kModelPanel.y},WHITE);
  Label("原作素材图",274,157,20);Label("3D 模型 / 鼠标拖动旋转",704,157,20);
  // 按钮是同图集中的真实素材缩略图，颜色/档位/图层总数均来自显式目录。
  for(int v=0;v<Selected().variants;++v){const auto r=VariantRect(v);DrawRectangleRounded(r,.15F,4,v==variant_?Color{42,108,111,255}:Color{34,47,65,255});
   DrawReference({r.x+3,r.y+2,r.width-6,r.height-4},v);Label(std::to_string(v),r.x+2,r.y+15,12,WHITE);
  }
  const auto frame=NativeFrame();const auto& s=Visual().animation;const auto& d=battle::Describe(s.atlas);
  Label("变体 "+std::to_string(variant_+1)+" / "+std::to_string(Selected().variants)+"    ANM "+std::to_string(s.script)+"   sprite "+std::to_string(frame.sprite),270,698,16,kMuted);
  Label(std::string(d.name)+".dds  "+std::to_string(int(frame.source.width))+" × "+std::to_string(int(frame.source.height)),270,724,16,kMuted);
  const std::string model=Selected().model==ProjectileAssetId::Invalid?"无独立网格：使用正式平面几何":std::string(DescribeProjectileModel(Selected().model).name)+".gltf";
  Label("ANM scale "+std::to_string(frame.scale.x).substr(0,4)+" x "+std::to_string(frame.scale.y).substr(0,4)+"    alpha "+std::to_string(frame.color.a),270,745,12,kMuted);
  Label(model,704,698,16,kMuted);
  Label("Q/E 视角滚转  /  模型固定世界轴",704,745,12,kMuted);
  Label(Selected().kind==Kind::Bomb?(Selected().index==3?"左：原作素材层 / 右：组合单体模型":"对应素材层色的单体 / 非整场演出"):"自动适配取景 / 不代表弹种间相对尺寸",704,724,16,kMuted);
  Label("Tab / ↑↓  弹种    C / ←→  变体    1 正面  2 侧面  3 背面  4 俯视    R 重置",270,762,17);
  Label(std::string("Space  ")+(playing_?"暂停":"播放")+"    , / . 逐帧    W 线框    G 网格    B 深浅背景    滚轮缩放",276,790,17);
  DrawRectangleRounded({276,836,1120,6},.5F,4,{45,63,83,255});const float progress=frame_/std::max(1,FrameCount(Selected())-1);
  DrawCircle(int(276+1120*progress),839,7,kAccent);
  Label("frame "+std::to_string(int(frame_))+" / "+std::to_string(FrameCount(Selected())-1)+"  @60Hz",276,858,16,kMuted);
  Label(std::string("N ")+(tinted_?"脚本着色":"原图")+"    A "+(gameAlpha_?"局内透明度":"审阅亮度")+"    F12 截图",704,858,16,kMuted);
  Label("25 项 / 184 变体",22,795,17,kAccent);Label("含副机附加项",22,825,16,kMuted);
  if(!notice_.empty())Label(notice_,22,880,14,kAccent);
  EndTextureMode();(void)camera;
 }
 void Capture(const std::string& name){SaveTexture(canvas_,options_.output/(name+".png"));notice_="截图已保存: "+name+".png";}
 // definitions_ 在 assets_ 前构造、后销毁，保证异步加载器/动画借用始终有效。
 std::vector<VisualDefinition> definitions_;std::array<std::size_t,25> offset_{};Options options_;AssetManager assets_;
 std::unique_ptr<BulletSystem> pool_;CombatRenderer combat_;Font font_{};RenderTexture2D raw_{},model_{},canvas_{};
 int selected_=0,variant_=0,captures_=0;float frame_=30,yaw_=.25F,pitch_=.12F,roll_=0,zoom_=1;
 bool playing_=false,wire_=false,grid_=true,light_=false,tinted_=true,gameAlpha_=false;std::string notice_;
};
}
int main(int argc,char** argv){int result=0;try{
 const auto options=Parse(argc,argv);SetTraceLogLevel(LOG_WARNING);
 SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_WINDOW_RESIZABLE|(options.verify?FLAG_WINDOW_HIDDEN|FLAG_WINDOW_ALWAYS_RUN:0));
 InitWindow(1280,818,"Projectile comparison | DDS / ANM + 3D");SetWindowMinSize(1000,640);SetTargetFPS(options.verify?0:60);
 if(!IsWindowReady())throw std::runtime_error("Cannot create OpenGL window");
 BeginDrawing();ClearBackground(kBackground);DrawText("Loading original DDS / ANM and production 3D assets...",32,42,22,RAYWHITE);EndDrawing();
 {Viewer viewer(options);if(options.verify)viewer.Verify();else viewer.Run();}
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}
 if(IsWindowReady())CloseWindow();return result;
}
