// 本文件安全解析 ANM 原始字节，并执行精灵、循环、中断及变换/插值指令。
#include "resources/animation.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <unordered_map>

namespace koumakyou::anm {
namespace {
constexpr std::array<int,32> kSizes{0,4,8,4,4,4,0,0,0,12,12,8,8,0,0,0,8,12,16,16,16,0,4,0,0,4,4,4,4,4,12,4};
std::uint32_t Word(std::span<const unsigned char> bytes, std::size_t at) {
    if(at>bytes.size() || bytes.size()-at<4)throw std::runtime_error("Truncated ANM word");
    return std::uint32_t(bytes[at]) | std::uint32_t(bytes[at+1])<<8 |
           std::uint32_t(bytes[at+2])<<16 | std::uint32_t(bytes[at+3])<<24;
}
float Real(std::span<const unsigned char> bytes,std::size_t at) {
    const float value=std::bit_cast<float>(Word(bytes,at));
    if(!std::isfinite(value))throw std::runtime_error("Non-finite ANM coordinate");
    return value;
}
float Lerp(float from,float to,float amount) { return from+(to-from)*amount; }
}
int Instruction::Int(int index) const { return std::bit_cast<std::int32_t>(args.at(index)); }
float Instruction::Float(int index) const { return std::bit_cast<float>(args.at(index)); }
const Sprite* Archive::FindSprite(int id) const {
    const auto it=std::lower_bound(sprites.begin(),sprites.end(),id,[](const Sprite& s,int value){return s.id<value;});
    return it!=sprites.end() && it->id==id ? &*it : nullptr;
}
const Script* Archive::FindScript(int id) const {
    const auto it=std::lower_bound(scripts.begin(),scripts.end(),id,[](const Script& s,int value){return s.id<value;});
    return it!=scripts.end() && it->id==id ? &*it : nullptr;
}
Archive LoadArchive(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("Cannot open ANM: "+path.string());
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file),{}};
    if(bytes.size()<64 || bytes.size()>1024*1024)throw std::runtime_error("Invalid ANM size");
    const auto n=Word(bytes,0),m=Word(bytes,4);
    if(n>4096 || m>4096 || 64+4*n+8*m>bytes.size() || Word(bytes,40)!=0 || Word(bytes,56)!=0)
        throw std::runtime_error("Unsupported ANM header");
    Archive archive; archive.width=Word(bytes,12);archive.height=Word(bytes,16);
    if(archive.width<=0 || archive.height<=0)throw std::runtime_error("Invalid ANM dimensions");
    for(std::uint32_t i=0;i<n;++i) {
        const auto at=Word(bytes,64+4*i);
        Sprite sprite{static_cast<int>(Word(bytes,at)),{Real(bytes,at+4),Real(bytes,at+8),Real(bytes,at+12),Real(bytes,at+16)}};
        if(sprite.source.width<=0 || sprite.source.height<=0)throw std::runtime_error("Empty ANM sprite");
        archive.sprites.push_back(sprite);
    }
    std::sort(archive.sprites.begin(),archive.sprites.end(),[](auto& a,auto& b){return a.id<b.id;});
    for(std::size_t i=1;i<archive.sprites.size();++i)
        if(archive.sprites[i-1].id==archive.sprites[i].id)throw std::runtime_error("Duplicate ANM sprite");
    for(std::uint32_t i=0;i<m;++i) {
        const auto table=64+4*n+8*i;
        const int id=static_cast<int>(Word(bytes,table));
        const auto start=Word(bytes,table+4);
        const std::size_t end=i+1<m ? Word(bytes,table+12) : bytes.size();
        if(start>=end || end>bytes.size())throw std::runtime_error("Invalid ANM script offset");
        Script script;script.id=id;
        std::size_t at=start;bool sentinel=false;
        while(at+4<=end) {
            const unsigned op=bytes[at+2],size=bytes[at+3];
            if(op>=kSizes.size() || size!=static_cast<unsigned>(kSizes[op]) || at+4+size>end)
                throw std::runtime_error("Invalid ANM opcode or arguments");
            Instruction instruction;
            instruction.frame=static_cast<std::int16_t>(bytes[at]|bytes[at+1]<<8);
            instruction.opcode=op;instruction.offset=at-start;
            for(unsigned arg=0;arg<size/4;++arg)instruction.args[arg]=Word(bytes,at+4+arg*4);
            script.instructions.push_back(instruction);at+=4+size;
            if(op==0){sentinel=true;break;}
        }
        if(!sentinel)throw std::runtime_error("ANM script missing sentinel");
        for(const auto& instruction:script.instructions) {
            if(instruction.opcode==5 && std::none_of(script.instructions.begin(),script.instructions.end(),
                [&](const Instruction& other){return other.offset==instruction.args[0];}))
                throw std::runtime_error("ANM jump is not an instruction boundary");
            if(instruction.opcode==1 && !archive.FindSprite(instruction.Int(0)))
                throw std::runtime_error("ANM script references missing sprite");
        }
        // 原加载器按编号覆盖；Extra 图集中重复编号 71 的后项必须取代前项。
        const auto existing=std::find_if(archive.scripts.begin(),archive.scripts.end(),[&](const auto& s){return s.id==id;});
        if(existing!=archive.scripts.end())*existing=std::move(script);
        else archive.scripts.push_back(std::move(script));
    }
    std::sort(archive.scripts.begin(),archive.scripts.end(),[](auto& a,auto& b){return a.id<b.id;});
    return archive;
}
void Player::SetSprite(int id) {
    const auto* sprite=archive_ ? archive_->FindSprite(id) : nullptr;
    if(!sprite)throw std::runtime_error("Animation references unavailable sprite");
    frame_.sprite=id;frame_.source=sprite->source;
}
void Player::SetRotationZ(float radians) {
    if(!std::isfinite(radians))throw std::invalid_argument("Non-finite animation rotation");
    frame_.rotation.z=radians;
}
void Player::Start(const Archive& archive,int id,int offset,int initial,std::uint32_t seed) {
    *this=Player{};archive_=&archive;script_=archive.FindScript(id);
    if(!script_)throw std::runtime_error("Unknown ANM script: "+std::to_string(id));
    spriteOffset_=offset;random_=seed ? seed : 1;finished_=false;
    if(initial>=0){SetSprite(initial);frame_.visible=true;}
    Tick();
}
bool Player::Interrupt(int label) {
    if(!script_ || finished_)return false;
    std::size_t target=script_->instructions.size(),fallback=target;
    for(std::size_t i=0;i<script_->instructions.size();++i) {
        const auto& op=script_->instructions[i];
        if(op.opcode==22) {
            if(op.Int(0)==label){target=i+1;break;}
            if(op.Int(0)==-1)fallback=i+1;
        }
        if(op.opcode==0 || op.opcode==15)break;
    }
    if(target==script_->instructions.size())target=fallback;
    if(target>=script_->instructions.size())return false;
    pc_=target;clock_=script_->instructions[pc_].frame;halted_=false;frame_.visible=true;
    Tick();return true;
}
void Player::Advance(double seconds) {
    if(!std::isfinite(seconds) || seconds<0)throw std::invalid_argument("Invalid animation delta time");
    remainder_+=seconds*60;
    // 加容差抵消 float(1/60) 的表示误差；余量仍保留，任意更新频率不会改变帧数。
    while(!finished_ && remainder_+1e-6>=1) { remainder_-=1;Tick(); }
    if(finished_)remainder_=0;
}
void Player::Tick() {
    if(finished_ || !script_)return;
    int budget=4096;
    while(!halted_ && pc_<script_->instructions.size() && script_->instructions[pc_].frame<=clock_) {
        if(--budget==0)throw std::runtime_error("ANM zero-time loop exceeded budget");
        const auto& op=script_->instructions[pc_++];
        const auto f=[&](int i){const float v=op.Float(i);if(!std::isfinite(v))throw std::runtime_error("Invalid ANM float");return v;};
        const auto vector=[&](){return Vector3{f(0),f(1),f(2)};};
        switch(op.opcode) {
        case 0: frame_.visible=false;finished_=true;return;
        case 15: finished_=true;return;
        case 1: SetSprite(op.Int(0)+spriteOffset_);frame_.visible=true;break;
        case 2: frame_.scale={f(0),f(1)};break;
        case 3: frame_.color.a=static_cast<unsigned char>(op.Int(0)&255);break;
        case 4: frame_.color.r=(op.args[0]>>16)&255;frame_.color.g=(op.args[0]>>8)&255;frame_.color.b=op.args[0]&255;break;
        case 5: {
            const auto& ops=script_->instructions;
            pc_=std::find_if(ops.begin(),ops.end(),[&](const auto& i){return i.offset==op.args[0];})-ops.begin();
            if(pc_>=ops.size())throw std::runtime_error("Invalid ANM jump");
            clock_=ops[pc_].frame;break;
        }
        case 6: case 22: break;
        case 7: flipX_=!flipX_;frame_.scale.x=-frame_.scale.x;break;
        case 8: flipY_=!flipY_;frame_.scale.y=-frame_.scale.y;break;
        case 9: frame_.rotation=vector();break;
        case 10: rotationSpeed_=vector();break;
        case 11: scaleSpeed_={f(0),f(1)};scaleDuration_=0;break;
        case 12: alphaStart_=frame_.color.a;alphaTarget_=op.Int(0)&255;alphaDuration_=op.Int(1);alphaTime_=0;break;
        case 13: frame_.additive=true;break;
        case 14: frame_.additive=false;break;
        case 16: random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
                 SetSprite(op.Int(0)+spriteOffset_+static_cast<int>(random_%std::max(1,op.Int(1))));frame_.visible=true;break;
        case 17: (positionOffset_ ? frame_.positionOffset : frame_.position)=vector();break;
        case 18: case 19: case 20:
            positionStart_=positionOffset_ ? frame_.positionOffset : frame_.position;positionTarget_=vector();positionDuration_=op.Int(3);positionTime_=0;positionMode_=op.opcode;break;
        case 21: halted_=true;break;
        case 23: frame_.topLeft=true;break;
        case 24: frame_.visible=false;halted_=true;break;
        case 25: positionOffset_=op.Int(0)!=0;break;
        case 26: frame_.autoRotate=op.Int(0);break;
        case 27: frame_.uvScroll.x=std::fmod(frame_.uvScroll.x+f(0)+1,1);break;
        case 28: frame_.uvScroll.y=std::fmod(frame_.uvScroll.y+f(0)+1,1);break;
        case 29: frame_.visible=op.Int(0)!=0;break;
        case 30: scaleStart_=frame_.scale;scaleTarget_={f(0),f(1)};scaleDuration_=op.args[2]&65535;scaleTime_=0;break;
        case 31: frame_.disableZWrite=op.Int(0)!=0;break;
        default: throw std::runtime_error("Unsupported animation instruction");
        }
    }
    frame_.rotation.x=std::remainder(frame_.rotation.x+rotationSpeed_.x,2*PI);
    frame_.rotation.y=std::remainder(frame_.rotation.y+rotationSpeed_.y,2*PI);
    frame_.rotation.z=std::remainder(frame_.rotation.z+rotationSpeed_.z,2*PI);
    if(scaleDuration_>0) {
        const float t=std::min(1.0F,static_cast<float>(++scaleTime_)/scaleDuration_);
        frame_.scale={Lerp(scaleStart_.x,scaleTarget_.x,t)*(flipX_?-1:1),
                      Lerp(scaleStart_.y,scaleTarget_.y,t)*(flipY_?-1:1)};
        if(t==1){scaleDuration_=0;scaleSpeed_={};}
    } else {frame_.scale.x+=scaleSpeed_.x;frame_.scale.y+=scaleSpeed_.y;}
    if(alphaDuration_>0) {
        const float t=std::min(1.0F,static_cast<float>(++alphaTime_)/alphaDuration_);
        frame_.color.a=static_cast<unsigned char>(Lerp(alphaStart_,alphaTarget_,t));
        if(t==1)alphaDuration_=0;
    }
    if(positionDuration_>0) {
        float t=std::min(1.0F,static_cast<float>(positionTime_++)/positionDuration_);
        if(positionMode_==19)t=1-(1-t)*(1-t);
        if(positionMode_==20)t=1-std::pow(1-t,4);
        (positionOffset_ ? frame_.positionOffset : frame_.position)=Vector3{Lerp(positionStart_.x,positionTarget_.x,t),Lerp(positionStart_.y,positionTarget_.y,t),Lerp(positionStart_.z,positionTarget_.z,t)};
        if(t==1)positionDuration_=0;
    }
    if(!halted_)++clock_;
}
} // namespace koumakyou::anm
