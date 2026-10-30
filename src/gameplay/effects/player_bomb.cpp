// 本文件逐帧复现四 Bomb 的伤害通道和攻击时序，并把原二维区域扩展为有体积的三维演出。
#include "gameplay/effects/player_bomb.hpp"
#include "gameplay/arena.hpp"
#include "app/math.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
namespace koumakyou {
namespace {
using namespace game_math;
constexpr float kPi=std::numbers::pi_v<float>;
constexpr std::array<PlayerBombDefinition,4> kBombs{{
    {ShotType::HomingAmulet,"灵符「梦想封印」",battle::Atlas::Reimu,133,4,300,360,0xd8d0,0xe0f0},
    {ShotType::Needle,"梦符「封魔阵」",battle::Atlas::Reimu,137,4,140,200,0xe280,0xe860},
    {ShotType::MagicMissile,"魔符「Stardust Reverie」",battle::Atlas::Marisa,5,3,250,310,0xe9a0,0xed80},
    {ShotType::IllusionLaser,"恋符「Master Spark」",battle::Atlas::Marisa,8,4,300,360,0xef20,0xf310},
}};
Vector3 Cross(Vector3 a,Vector3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
struct Basis { Vector3 position,forward,right,up; };
Basis View(const Camera3D& camera) {
    const auto forward=Normalize(Subtract(camera.target,camera.position));
    const auto right=Normalize(Cross(forward,camera.up));
    if(BulletSystem::Length(forward)<0.5F || BulletSystem::Length(right)<0.5F)
        throw std::invalid_argument("Invalid Bomb camera basis");
    return {camera.position,forward,right,Normalize(Cross(right,forward))};
}
float Component(Vector3 v,int axis) { return axis==0 ? v.x : axis==1 ? v.y : v.z; }
void SetComponent(Vector3& v,int axis,float value) { if(axis==0)v.x=value;else if(axis==1)v.y=value;else v.z=value; }
BombVolume Sphere(Vector3 position,float radius) { BombVolume v;v.origin=position;v.radius=radius;return v; }
BombVolume MasterSparkVolume(const Basis& view) {
    BombVolume volume;volume.shape=BombVolume::Shape::Frustum;volume.origin=view.position;
    float length=40,radius=0;
    // 盒内任一点都是八角的凸组合；以八角到束轴的最大距离为截面半径，可覆盖整个正向半场。
    // 端面固定在角色位置，不向后平移，因而身后仍是盲区；不能只扩大远端半径而漏掉近处两侧。
    for(float x:{-kArenaHalfExtents.x,kArenaHalfExtents.x})
        for(float y:{-kArenaHalfExtents.y,kArenaHalfExtents.y})
            for(float z:{-kArenaHalfExtents.z,kArenaHalfExtents.z}) {
                const auto offset=Subtract(Vector3{x,y,z},view.position);
                const float along=offset.x*view.forward.x+offset.y*view.forward.y+offset.z*view.forward.z;
                radius=std::max(radius,BulletSystem::Length(Subtract(offset,Scale(view.forward,along))));
                length=std::max(length,along+1);
            }
    volume.end=Add(view.position,Scale(view.forward,length));
    volume.radius=radius+0.5F;volume.endRadius=volume.radius*1.06F;
    return volume;
}
Vector3 StarDestination(int index) {
    const auto cell=[](int i,float half) { return -half+(i+0.5F)*2*half/kBombStarGrid; };
    return {cell(index%kBombStarGrid,kArenaHalfExtents.x),
            cell(index/kBombStarGrid%kBombStarGrid,kArenaHalfExtents.y),
            cell(index/(kBombStarGrid*kBombStarGrid),kArenaHalfExtents.z)};
}
}
const PlayerBombDefinition& BombDefinition(ShotType shot) {
    const int index=static_cast<int>(shot);
    if(index<0 || index>=4)throw std::invalid_argument("Invalid Bomb shot type");
    return kBombs[index];
}
void PlayerBombEffect::Clear() { *this=PlayerBombEffect{}; }
void PlayerBombEffect::Start(ShotType shot,const Camera3D& camera,const anm::Archive* archive,std::uint32_t seed) {
    const auto& definition=BombDefinition(shot);const auto view=View(camera);
    if(archive)for(int layer=0;layer<definition.layers;++layer)
        if(!archive->FindScript(definition.firstScript+layer))throw std::invalid_argument("Missing Bomb ANM script");
    Clear();active_=true;shot_=shot;archive_=archive;random_=seed ? seed : 1;
    forward_=view.forward;right_=view.right;up_=view.up;anchor_=view.position;
    orbBorn_.fill(-1);orbBurst_.fill(-1);
    if(shot!=ShotType::HomingAmulet)sounds_[soundCount_++]=shot==ShotType::IllusionLaser ? 19 : 6;
    if(shot!=ShotType::HomingAmulet) {
        const int count=shot==ShotType::MagicMissile ? kBombStarCount : 4;
        for(int i=0;i<count;++i) {
            if(archive_)animations_[i].Start(*archive_,definition.firstScript+i%definition.layers);
            positions_[i]=anchor_;
            // 固定世界格心而非少量视线方向：即使从角落释放，180 帧后也能铺满同一组空间区域。
            if(shot==ShotType::MagicMissile)velocities_[i]=Scale(Subtract(StarDestination(i),anchor_),1.0F/kBombStarTravelFrames);
        }
    }
    Sample(camera);
}
void PlayerBombEffect::StartOrb(int index,const Camera3D& camera) {
    const auto view=View(camera);
    positions_[index]=Add(view.position,Subtract(Scale(view.forward,1.1F),Scale(view.up,0.35F)));
    random_=random_*1664525U+1013904223U;const float angle=(random_>>8)/16777216.0F*2*kPi;
    velocities_[index]=Scale(Add(Scale(view.right,std::cos(angle)),Scale(view.up,std::sin(angle))),4*kBombWorldPixel);
    orbBorn_[index]=frame_;sounds_[soundCount_++]=13;
    if(archive_)for(int layer=0;layer<4;++layer)animations_[index*4+layer].Start(*archive_,133+layer);
}
void PlayerBombEffect::Update(float seconds,const Camera3D& camera,std::span<const Vector3> targets,
                              const std::function<void(PlayerBombEffect&)>& resolve) {
    if(!std::isfinite(seconds) || seconds<0)throw std::invalid_argument("Invalid Bomb delta time");
    soundCount_=0;if(!Active())return;
    fractionalFrames_+=static_cast<double>(seconds)*60;
    while(fractionalFrames_+1e-5>=1 && Active()) {
        fractionalFrames_=std::max(0.0,fractionalFrames_-1);Tick(camera,targets);
        if(Active()) { Sample(camera);if(resolve)resolve(*this); }
    }
    if(Active())Sample(camera);
}
void PlayerBombEffect::ReportHit(int region,int damage) {
    if(Active() && shot_==ShotType::HomingAmulet && region>0 && region<8 && orbBurst_[region]<0)
        orbDamage_[region]=std::min(100,orbDamage_[region]+std::clamp(damage,0,100));
}
std::array<int,8> PlayerBombEffect::ContactDamage(Vector3 center,float radius) const {
    std::array<int,8> channels{};
    for(const auto& attack:Attacks())if(attack.volume.IntersectsSphere(center,radius))
        channels[attack.region]=std::max(channels[attack.region],attack.damage);
    return channels;
}
void PlayerBombEffect::Tick(const Camera3D& camera,std::span<const Vector3> targets) {
    if(++frame_>=BombDefinition(shot_).durationFrames) { Clear();return; }
    if(shot_==ShotType::HomingAmulet) {
        if(frame_>=80 && frame_<180 && frame_%16==0)StartOrb((frame_-60)/16,camera);
        for(int orb=1;orb<8;++orb) {
            if(orbBorn_[orb]<0)continue;
            auto& position=positions_[orb];auto& velocity=velocities_[orb];
            if(orbBurst_[orb]<0) {
                // 原作无敌机时回到自机附近盘旋，不能凭空锁定视线前方的固定点。
                Vector3 target=camera.position;
                float nearest=std::numeric_limits<float>::max();
                for(const auto candidate:targets)if(const float distance=DistanceSquared(position,candidate);distance<nearest)
                    { nearest=distance;target=candidate; }
                const float speed=std::max(kBombWorldPixel,BulletSystem::Length(velocity));
                const auto offset=Subtract(target,position);
                // 原式将吸引位移限制为 speed/8；贴近目标时不能继续给固定的归一化加速度。
                const auto acceleration=Scale(offset,1/std::max(1.0F,BulletSystem::Length(offset)/(speed/8)));
                const auto accelerated=Add(velocity,acceleration);
                velocity=Scale(Normalize(accelerated),std::clamp(BulletSystem::Length(accelerated),kBombWorldPixel,10*kBombWorldPixel));
                if(orbDamage_[orb]>=100 || frame_>=270) {
                    orbBurst_[orb]=frame_;sounds_[soundCount_++]=15;
                    if(archive_)for(int layer=0;layer<4;++layer)animations_[orb*4+layer].Interrupt(1);
                }
            }
            position=Add(position,velocity);
            if(archive_ && frame_!=orbBorn_[orb])for(int layer=0;layer<4;++layer)animations_[orb*4+layer].Advance(1.0/60);
        }
    } else {
        const int count=shot_==ShotType::MagicMissile ? kBombStarCount : 4;
        for(int i=0;i<count;++i) {
            // 5/6 号原脚本在 220 帧结束；扩展后的星群需继续可见，统一在末 30 帧淡出，避免隐形攻击。
            if(archive_ && (shot_!=ShotType::MagicMissile || frame_<219))animations_[i].Advance(1.0/60);
            if(shot_==ShotType::MagicMissile)
                positions_[i]=Add(anchor_,Scale(velocities_[i],static_cast<float>(std::min(frame_,kBombStarTravelFrames))));
        }
    }
}
void PlayerBombEffect::Sample(const Camera3D& camera) {
    spriteCount_=attackCount_=cancelCount_=0;
    const auto attack=[&](BombVolume volume,int region,int damage,bool hurt=true,bool cancel=true) {
        if(hurt)attacks_[attackCount_++]={volume,region,damage};
        if(cancel)cancelVolumes_[cancelCount_++]=volume;
    };
    const auto emit=[&](int animation,int layer,BombVolume volume,PlayerBombSprite::Shape shape,float height=1,bool trail=false) {
        if(!archive_)return;
        const auto& frame=animations_[animation].Sample();
        if(!frame.visible || animations_[animation].Finished())return;
        auto& sprite=sprites_[spriteCount_++];sprite={};sprite.layer=layer;sprite.frame=frame;
        sprite.frame.position=sprite.frame.positionOffset={};
        sprite.shape=shape;sprite.volume=volume;sprite.position=volume.origin;sprite.end=volume.end;
        // 模型炮口位于视点前下方；攻击圆台保持原始覆盖范围，不能用覆盖半场的判定半径撑大可见光束。
        if(shape==PlayerBombSprite::Shape::Beam){const auto view=View(camera);sprite.position=Add(volume.origin,Add(Scale(view.forward,.9F),Scale(view.up,-.6F)));}
        sprite.height=height;sprite.width=2*volume.endRadius;sprite.ageSeconds=frame_/60.0F;
        sprite.trail=trail;
        if(shot_==ShotType::MagicMissile) {
            const float fade=std::clamp((250-frame_)/30.0F,0.0F,1.0F)*(trail ? 0.35F : 0.7F);
            const int alpha=frame_>=180 ? std::max<int>(96,sprite.frame.color.a) : sprite.frame.color.a;
            sprite.frame.color.a=static_cast<unsigned char>(alpha*fade);
        }
    };
    if(shot_==ShotType::HomingAmulet) {
        // 原作 8 号初始保护区只清弹；并非对全场瞬间扣除固定生命。
        if(frame_==0)attack(Sphere(anchor_,128*kBombWorldPixel),0,0,false);
        for(int orb=1;orb<8;++orb)if(orbBorn_[orb]>=0) {
            const bool burst=orbBurst_[orb]>=0;
            const auto volume=Sphere(positions_[orb],(burst ? 128 : 24)*kBombWorldPixel);
            if(!burst || frame_==orbBurst_[orb])attack(volume,orb,burst ? 200 : 8);
            if(!burst || frame_-orbBurst_[orb]<29)for(int layer=0;layer<4;++layer)
                emit(orb*4+layer,layer,volume,PlayerBombSprite::Shape::Sphere,280*0.012F);
        }
    } else if(shot_==ShotType::Needle) {
        // 蓄力 60 帧不伤害/清弹；以施放点为中心长出六面法阵，向场地边界扩展。
        // 横向尺寸也随外扩增长，不能用一开始贯穿全场的薄盒提前伤害远处目标。
        // Z 两面复用 X 通道，仍保留原来的四份伤害上限；施放后移动/转头不搬动法阵。
        const float progress=std::clamp((frame_-60)/79.0F,0.0F,1.0F);
        for(int axis=0;axis<3;++axis)for(int side=0;side<2;++side) {
            BombVolume volume;volume.shape=BombVolume::Shape::Slab;
            volume.origin=Scale(anchor_,1-progress);
            volume.halfExtents={std::max(0.5F,kArenaHalfExtents.x*progress),
                                std::max(0.5F,kArenaHalfExtents.y*progress),
                                std::max(0.5F,kArenaHalfExtents.z*progress)};
            SetComponent(volume.origin,axis,Component(volume.origin,axis)+
                         (side==0 ? 1 : -1)*Component(kArenaHalfExtents,axis)*progress);
            SetComponent(volume.halfExtents,axis,0.35F+(31*kBombWorldPixel-0.35F)*progress);
            const int region=(axis==1 ? 1 : 0)+(side==0 ? 0 : 2);
            if(frame_>60)attack(volume,region,8,frame_%2!=0);
            emit(region,region,volume,PlayerBombSprite::Shape::Slab);
        }
    } else if(shot_==ShotType::MagicMissile) {
        for(int i=0;i<kBombStarCount;++i) {
            const auto volume=Sphere(positions_[i],64*kBombWorldPixel);
            if(frame_>0 && frame_%3!=0)attack(volume,i%8,8);
            emit(i,i%3,volume,PlayerBombSprite::Shape::Star,3.6F);
            auto trail=volume;trail.origin=Subtract(volume.origin,Scale(velocities_[i],6));
            emit(i,i%3,trail,PlayerBombSprite::Shape::Star,1.8F,true);
        }
    } else {
        const auto view=View(camera);
        // 四种原色是同一根巨束的材质层，几何轴线和出射点完全相同，不能再生成四根分叉激光。
        const auto volume=MasterSparkVolume(view);
        if(frame_>0 && frame_%4!=0)attack(volume,0,12);
        for(int layer=0;layer<4;++layer) {
            emit(layer,layer,volume,PlayerBombSprite::Shape::Beam);
        }
    }
}
unsigned char PlayerBombEffect::BackdropAlpha() const {
    if(!Active())return 0;
    const int remaining=BombDefinition(shot_).durationFrames-frame_;
    // 恋札从光束内部观察时需要辨认全场目标，背景仅轻度压暗，避免光线以外的场景陷入黑屏。
    const int maximum=shot_==ShotType::IllusionLaser ? 96 : 176;
    return static_cast<unsigned char>(maximum*std::clamp(std::min(frame_,remaining)/60.0F,0.0F,1.0F));
}
} // namespace koumakyou
