// 本文件用确定性 Alpha 合成检查掉落物、演员、弹幕、激光与特效的实际 GPU 遮挡，无需音频或游戏配置。
#include "actor_fixture.hpp"
#include "render/sprite_renderer.hpp"
#include "resources/asset_manager.hpp"
#include "stages/stage1/content.hpp"
#include "rlgl.h"
#include <array>
#include <bit>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
using namespace koumakyou;
constexpr Color kRed{255,0,0,255};
/// @brief 直接初始化行为演员作为渲染输入；不运行关卡、攻击或掉落逻辑。
void InitializeActor(behavior::Actor& actor,VisualId visual,Vector3 position) {
    actor.Activate(testing::SpawnAt(position,visual));actor.presentation.Update(1);
}
/// @brief 测试纹理/档案所有者；颜色刻意简单，期望值从标准 Alpha 公式计算，不依赖绘制实现。
struct Fixture {
    std::vector<VisualResource> visuals;
    std::shared_ptr<anm::Archive> archive=std::make_shared<anm::Archive>();
    BulletSystem bullets;
    ActorEffectSystem effects;
    SpriteRenderer renderer;
    Camera3D camera{{0,0,-10},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    std::filesystem::path output;
    explicit Fixture(std::filesystem::path directory):output(std::move(directory)) {
        archive->width=archive->height=8;
        archive->sprites={{0,{0,0,8,8}}};
        archive->scripts={{0,{{.opcode=1},{.opcode=15}}}};
        Add(kRed);Add({0,255,0,128},true);Add({0,0,255,128});Add({0,255,0,128});Add(kRed,false,false);
    }
    ~Fixture(){for(auto& visual:visuals)UnloadTexture(visual.texture);}
    /// @brief 纹理边长为 8 物理像素；padding 创建透明矩形外框，native 选择 ANM 或普通 PNG 路径。
    void Add(Color color,bool padding=false,bool native=true) {
        Image image=GenImageColor(8,8,color);
        if(padding)for(int y=0;y<8;++y)for(int x=0;x<8;++x)
            if(x<2 || x>=6 || y<2 || y>=6)ImageDrawPixel(&image,x,y,{color.r,color.g,color.b,0});
        const auto texture=LoadTextureFromImage(image);UnloadImage(image);
        SetTextureFilter(texture,TEXTURE_FILTER_BILINEAR);SetTextureWrap(texture,TEXTURE_WRAP_REPEAT);
        VisualResource visual;visual.texture=texture;visual.worldHeight=4;visual.bulletDiameterScale=4;
        if(native){visual.archive=archive;visual.animation={battle::Atlas::Bullets,0};}
        visuals.push_back(visual);
    }
    /// @brief 通过正式 SpriteRenderer 画到 256×256；probe 检查返回后的深度/剔除/混合，blocker 放置前景墙。
    Image Render(const char* name,std::span<const behavior::Actor*> enemies={},std::span<const PickupSprite> pickups={},bool probe=false,bool blocker=false) {
        auto target=LoadRenderTexture(256,256);
        BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);BeginMode3D(camera);
        if(blocker)DrawCube({0,0,-2},1,1,0.2F,WHITE);
        renderer.Draw(camera,bullets,enemies,nullptr,pickups,visuals,{},effects);
        if(probe) {
            DrawCube({0,0,4},1,1,0.2F,BLUE);
            DrawCube({0,0,6},1,1,0.2F,kRed);
            DrawCube({0,0,3},1,1,0.2F,{0,255,0,128});
        }
        EndMode3D();EndTextureMode();EndDrawing();
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);UnloadRenderTexture(target);
        if(!output.empty())ExportImage(image,(output/(std::string(name)+".png")).string().c_str());
        return image;
    }
};
/// @brief 采样最终帧的窗口像素；容差 2 覆盖 GPU 的 8 位 Alpha 舍入，失败包含具体颜色便于定位。
void Expect(const Image& image,int x,int y,Color expected,const char* reason) {
    const auto actual=GetImageColor(image,x,y);
    if(std::abs(int(actual.r)-expected.r)>2 || std::abs(int(actual.g)-expected.g)>2 || std::abs(int(actual.b)-expected.b)>2)
        throw std::runtime_error(std::string(reason)+": expected "+std::to_string(expected.r)+","+std::to_string(expected.g)+","+std::to_string(expected.b)+
                                 " got "+std::to_string(actual.r)+","+std::to_string(actual.g)+","+std::to_string(actual.b));
}
void CheckOverlaps(Fixture& f) {
    behavior::Actor enemy;InitializeActor(enemy,0,{0,0,1});std::array<const behavior::Actor*,1> actors{&enemy};
    std::array<PickupSprite,1> pickups{{{{0,0,-1},1,1}}};
    Image image=f.Render("pickup-front",actors,pickups);
    Expect(image,173,128,kRed,"transparent pickup border hid enemy");
    Expect(image,128,128,{127,128,0,255},"pickup Alpha did not blend over enemy");UnloadImage(image);
    f.visuals[1].archive.reset();image=f.Render("png-pickup-front",actors,pickups);
    Expect(image,173,128,kRed,"PNG pickup transparent border hid enemy");
    Expect(image,128,128,{127,128,0,255},"PNG pickup soft Alpha was clipped");UnloadImage(image);
    f.visuals[1].archive=f.archive;
    image=f.Render("opaque-scenery",actors,pickups,false,true);
    Expect(image,128,128,WHITE,"transparent pass ignored opaque scenery depth");UnloadImage(image);
    pickups[0].position.z=2;image=f.Render("pickup-behind",actors,pickups);
    Expect(image,128,128,kRed,"rear pickup appeared through enemy");UnloadImage(image);

    std::array<PickupSprite,2> stacked{{{{0,0,-1},3,1},{{0,0,0},2,1}}};
    image=f.Render("stacked-pickups",actors,stacked);
    Expect(image,128,128,{63,128,64,255},"pickups depended on insertion order");UnloadImage(image);

    auto wide=std::make_shared<anm::Archive>(*f.archive);wide->sprites[0].source.width=32;
    f.visuals[3].archive=wide;
    behavior::Actor side;InitializeActor(side,3,{5,0,0});std::array<const behavior::Actor*,2> slanted{&side,&enemy};
    image=f.Render("camera-depth",slanted);
    Expect(image,128,128,{127,128,0,255},"radial distance reversed view-depth order");UnloadImage(image);
    f.visuals[3].archive=f.archive;

    auto shifted=std::make_shared<anm::Archive>(*f.archive);
    shifted->scripts[0].instructions.insert(shifted->scripts[0].instructions.begin()+1,
                                           {.opcode=17,.args={0,0,std::bit_cast<std::uint32_t>(-4.0F)}});
    f.visuals[3].archive=shifted;
    behavior::Actor movedByAnm,wall;InitializeActor(movedByAnm,3,{0,0,1});InitializeActor(wall,0,{0,0,0});
    std::array<const behavior::Actor*,2> displaced{&movedByAnm,&wall};
    image=f.Render("anm-local-depth",displaced);
    Expect(image,128,128,{127,128,0,255},"ANM local displacement was omitted from sorting");UnloadImage(image);
    f.visuals[3].archive=f.archive;

    auto* bullet=f.bullets.Spawn(BulletOwner::Player,{0,0,-1},{},1,WHITE,2);
    image=f.Render("player-bullet",actors);
    Expect(image,128,128,{127,0,128,255},"player bullet overlap changed");UnloadImage(image);
    bullet->color.a=51;image=f.Render("player-bullet-tint",actors);
    Expect(image,128,128,{229,0,26,255},"player tint/texture Alpha stopped multiplying");UnloadImage(image);bullet->active=false;
    bullet=f.bullets.Spawn(BulletOwner::Enemy,{0,0,-1},{},1,WHITE,2);
    image=f.Render("enemy-bullet",actors);
    Expect(image,128,128,{127,0,128,255},"enemy bullet overlap changed");UnloadImage(image);bullet->active=false;

    behavior::Actor legacy;InitializeActor(legacy,4,{0,0,1});actors[0]=&legacy;pickups[0].position.z=-1;
    image=f.Render("legacy-actor",actors,pickups);
    Expect(image,173,128,kRed,"PNG actor border regression");UnloadImage(image);
    actors[0]=&enemy;
    ActorPresentation presentation;presentation.Update(1);
    f.effects.Spawn(0,{0,0,1},0.5F,presentation,ActorExitReason::Retreat);
    image=f.Render("effect-overlap",{},pickups);
    Expect(image,173,128,kRed,"effect snapshot was clipped by pickup");UnloadImage(image);f.effects.Clear();

    // 视线前方的绿激光应覆盖红演员；即使线段后半在演员后面，也不能整束先画。
    auto* beam=f.bullets.SpawnBeam({-2,0,-2},BulletSystem::DirectionTo({-2,0,-2},{2,0,2}),std::sqrt(32.0F),0.25F,0,10,WHITE);
    beam->visual=3;beam->age=1;
    image=f.Render("crossing-beam",actors);
    Expect(image,150,128,{127,128,0,255},"front beam segment was hidden by actor");
    Expect(image,90,128,kRed,"rear beam segment painted over actor");UnloadImage(image);beam->active=false;

    // 透明演员仍会产生切分平面；切分前后的整束图像应相同，不能拉伸、重复或漏掉 UV。
    Image gradient=GenImageColor(8,8,BLACK);
    for(int y=0;y<8;++y)for(int x=0;x<8;++x)
        ImageDrawPixel(&gradient,x,y,{0,static_cast<unsigned char>(y*255/7),static_cast<unsigned char>(255-y*255/7),128});
    const auto gradientTexture=LoadTextureFromImage(gradient);UnloadImage(gradient);
    SetTextureFilter(gradientTexture,TEXTURE_FILTER_BILINEAR);SetTextureWrap(gradientTexture,TEXTURE_WRAP_REPEAT);
    const auto previous=f.visuals[3].texture;f.visuals[3].texture=gradientTexture;beam->active=true;
    Image continuous=f.Render("beam-uv-continuous");
    behavior::Actor ghost;InitializeActor(ghost,0,{0,0,1});ghost.presentation.Reset();std::array<const behavior::Actor*,1> ghosts{&ghost};
    image=f.Render("beam-uv-split",ghosts);
    for(int y=0;y<256;++y)for(int x=0;x<256;++x)
        Expect(image,x,y,GetImageColor(continuous,x,y),"splitting beam changed its UV/coverage");
    UnloadImage(image);UnloadImage(continuous);beam->active=false;
    f.visuals[3].texture=previous;UnloadTexture(gradientTexture);

    f.visuals[3].archive.reset();f.visuals[3].glow=true;
    behavior::Actor near;InitializeActor(near,0,{0,0,-4});std::array<const behavior::Actor*,1> nearActors{&near};
    bullet=f.bullets.Spawn(BulletOwner::Enemy,{0,0,-3},{},1,WHITE,3);
    image=f.Render("glow-behind-actor",nearActors);
    Expect(image,128,128,kRed,"rear glow painted over opaque foreground actor");UnloadImage(image);bullet->active=false;
    f.visuals[3].archive=f.archive;f.visuals[3].glow=false;

    auto additive=std::make_shared<anm::Archive>(*f.archive);
    additive->scripts[0].instructions.insert(additive->scripts[0].instructions.begin()+1,{.opcode=13});
    f.visuals[3].archive=additive;
    bullet=f.bullets.Spawn(BulletOwner::Enemy,{0,0,-3},{},1,WHITE,3);
    image=f.Render("additive-behind-actor",nearActors);
    Expect(image,128,128,kRed,"rear additive effect appeared through actor");UnloadImage(image);bullet->active=false;
    beam=f.bullets.SpawnBeam({-2,0,-2},BulletSystem::DirectionTo({-2,0,-2},{2,0,2}),std::sqrt(32.0F),0.25F,0,10,WHITE);
    beam->visual=3;beam->age=1;
    image=f.Render("additive-crossing-beam",actors);
    Expect(image,150,128,{255,128,0,255},"native additive beam failed to cover actor");
    Expect(image,90,128,kRed,"native additive rear beam appeared through actor");UnloadImage(image);beam->active=false;

    beam=f.bullets.SpawnBeam({-2,0,0},{1,0,0},4,0.25F,5,10,WHITE);beam->age=1;
    image=f.Render("legacy-warning-front",actors);
    bool warningVisible=false;
    for(int y=126;y<=129;++y) {
        const auto pixel=GetImageColor(image,128,y);
        if(pixel.g<20)continue;
        warningVisible=true;
        if(pixel.r<253)throw std::runtime_error("legacy warning wrote depth instead of blending over red actor");
    }
    if(!warningVisible)throw std::runtime_error("legacy warning disappeared");
    UnloadImage(image);beam->active=false;

    // 渲染器返回后必须恢复深度写入；透明对象不能把后绘制的不透明物永久挡住。
    image=f.Render("depth-state",{},pickups,true);
    Expect(image,128,128,{0,188,120,255},"transparent pass leaked depth/culling/blend state");UnloadImage(image);
}
/// @brief 用正式一面高清素材输出 P 点、分数点、敌人及激光的近/远重叠审阅图，不改变素材或动画。
void ReviewRealAssets(const std::filesystem::path& root,const std::filesystem::path& output) {
    AssetManager assets(root);stage1::Stage1Content content;assets.LoadStage(content.Assets());
    const auto& visuals=assets.Stage().visuals;
    SpriteRenderer renderer;BulletSystem bullets;ActorEffectSystem effects;
    behavior::Actor enemy;InitializeActor(enemy,stage1::kFairy,{0,0,0.5F});std::array<const behavior::Actor*,1> actors{&enemy};
    const Camera3D camera{{0,0,-5},{0,0,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    std::array<PickupSprite,2> pickups{{{{-0.05F,-0.15F,0},stage1::kPowerPoint,1},{{0.3F,0,0.1F},stage1::kScorePoint,1}}};
    auto target=LoadRenderTexture(768,768);
    for(int scene=0;scene<3;++scene) {
        if(scene==1)for(auto& pickup:pickups)pickup.position.z+=1;
        if(scene==2) {
            auto* beam=bullets.SpawnBeam({-1.2F,0,-0.5F},BulletSystem::DirectionTo({-1.2F,0,-0.5F},{1.2F,0,1.5F}),
                                         std::sqrt(9.76F),0.12F,0,10,WHITE);
            beam->visual=stage1::kNativeLaserBase+2;beam->age=1;
        }
        BeginDrawing();BeginTextureMode(target);ClearBackground({24,30,40,255});BeginMode3D(camera);
        renderer.Draw(camera,bullets,actors,nullptr,pickups,visuals,assets.Stage().shaders,effects);
        EndMode3D();EndTextureMode();EndDrawing();
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
        if(!output.empty())ExportImage(image,(output/("real-assets-"+std::to_string(scene)+".png")).string().c_str());
        UnloadImage(image);
    }
    UnloadRenderTexture(target);
}
}
/// @brief argv[1] 可选为截图目录、argv[2] 可选为 runtime 资源根目录；需图形会话，不加载音频或设置。
/// 未提供资源根时只运行独立像素回归，任何失败返回非零。
int main(int argc,char** argv) {
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED|FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(256,256,"Sprite overlap regression");SetTargetFPS(0);
    int result=0;
    try {
        std::filesystem::path output=argc>1 ? std::filesystem::absolute(argv[1]) : std::filesystem::path{};
        if(!output.empty())std::filesystem::create_directories(output);
        Fixture fixture(output);CheckOverlaps(fixture);
        if(argc>2)ReviewRealAssets(std::filesystem::absolute(argv[2]),output);
        std::cout<<"Sprite overlap regression passed\n";
    } catch(const std::exception& error){std::cerr<<error.what()<<"\n";result=1;}
    CloseWindow();return result;
}
