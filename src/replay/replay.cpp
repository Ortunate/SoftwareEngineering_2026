// 本文件实现带校验和、兼容标识与长度限制的录像二进制格式；整数统一小端，浮点保留 IEEE 位模式。
#include "replay/replay.hpp"
#include <bit>
#include <cmath>
#include <fstream>
#include <chrono>
#include <stdexcept>
#include <limits>
#include <atomic>
#include <iterator>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace koumakyou::replay {
namespace {
using Bytes=std::vector<std::uint8_t>;
constexpr std::uint32_t kMagic=0x3152504b, kVersion=2;
constexpr std::size_t kMaxBytes=65536+512+kMaxFrames*24;
struct Codec {
    Bytes bytes; std::size_t offset=0;
    void Put(std::uint32_t x) { for(int i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(x>>(8*i))); }
    std::uint32_t Get() { if(offset+4>bytes.size())throw std::runtime_error("录像文件截断"); std::uint32_t x=0;for(int i=0;i<4;++i)x|=std::uint32_t(bytes[offset++])<<(8*i);return x; }
    void String(const std::string& value) { RequireString(value.size());Put(static_cast<std::uint32_t>(value.size()));bytes.insert(bytes.end(),value.begin(),value.end()); }
    static void RequireString(std::size_t n) { if(n>512)throw std::runtime_error("录像字符串过长"); }
    std::string String() { const auto n=Get();RequireString(n);if(offset+n>bytes.size())throw std::runtime_error("录像字符串截断");std::string s(bytes.begin()+offset,bytes.begin()+offset+n);offset+=n;return s; }
    void Float(float x) { Put(std::bit_cast<std::uint32_t>(x)); }
    float Float() { return std::bit_cast<float>(Get()); }
};
std::uint32_t Hash(const Bytes& bytes,std::size_t count) {
    std::uint32_t h=2166136261u;for(std::size_t i=0;i<count;++i)h=(h^bytes[i])*16777619u;return h;
}
void Require(bool valid,const char* message) { if(!valid)throw std::runtime_error(message); }
void Validate(const Data& d) {
    std::string normalized;Require(NormalizeName(d.name,normalized),"录像名称无效");
    Require(!d.segments.empty() && d.segments.size()<=64,"录像片段过多");
    std::size_t boundary=0;
    for(const auto& segment:d.segments) {
        Require(segment.begin==boundary && segment.end>segment.begin && segment.end<=d.frames.size() && static_cast<unsigned>(segment.difficulty)<=4,"录像片段边界无效");
        Require(segment.lives<=1000 && segment.bombs<=1000 && segment.power>=8 && segment.power<=128 && segment.rank<=32,"录像入口资源无效");
        Require((d.route=="extra")== (segment.difficulty==Difficulty::Extra),"录像路线与难度不匹配");boundary=segment.end;
    }
    Require(d.segments.empty() || boundary==d.frames.size(),"录像片段不完整");
    const auto& h=d.header;
    Require(h.shot<4 && h.lives>=1 && h.lives<=1000 && h.bombs<=1000 && (!h.spell || (h.practice && !h.challenge)),"录像开局参数无效");
    Require(!d.frames.empty() && d.frames.size()<=kMaxFrames,"录像步数无效");
    Require(d.result.score<=std::uint64_t(std::numeric_limits<long long>::max()) && d.result.power>=8 && d.result.power<=128,"录像结果无效");
    for(const auto& f:d.frames)Require(std::isfinite(f.dt) && f.dt>0 && f.dt<=0.05F && std::isfinite(f.player.mouseDelta.x) && std::isfinite(f.player.mouseDelta.y) && std::isfinite(f.player.mouseSensitivityScale) && f.player.mouseSensitivityScale>=0.1F && f.player.mouseSensitivityScale<=3,"录像时间或输入无效");
}
Codec Encode(const Data& d) {
    Validate(d);Codec c;c.Put(kMagic);c.Put(kVersion);
    const std::string id=BuildId();c.Put(static_cast<std::uint32_t>(id.size()));for(char b:id)c.bytes.push_back(static_cast<std::uint8_t>(b));
    c.String(d.name);c.String(d.route);c.String(d.character);c.String(d.shot);c.String(d.rules);
    c.String(d.simulation.empty()?SimulationId():d.simulation);c.String(d.content.empty()?ContentId():d.content);c.String(d.build.empty()?BuildId():d.build);c.Put(d.continues);
    c.Put(static_cast<std::uint32_t>(d.segments.size()));
    for(const auto& s:d.segments) {
        c.String(s.stage);c.String(s.entry);c.String(s.target);for(auto v:{static_cast<std::uint32_t>(s.difficulty),s.begin,s.end,s.seed,s.lives,s.bombs,s.power,s.rank})c.Put(v);
        c.Put(static_cast<std::uint32_t>(s.score));c.Put(static_cast<std::uint32_t>(s.score>>32));
        c.Put(static_cast<std::uint32_t>(s.result.score));c.Put(static_cast<std::uint32_t>(s.result.score>>32));for(auto v:{s.result.lives,s.result.power,s.result.misses,s.result.bombs,std::uint32_t(s.result.won)})c.Put(v);
    }
    c.Put(0); // 扩展项数；类型高位标记必需项，未知必需项必须拒绝。
    const auto& h=d.header;for(auto v:{h.seed,h.shot,h.lives,h.bombs,h.phase})c.Put(v);
    c.Put(h.practice | (h.spell<<1) | (h.challenge<<2));c.Put(static_cast<std::uint32_t>(d.frames.size()));
    const auto& r=d.result;c.Put(static_cast<std::uint32_t>(r.score));c.Put(static_cast<std::uint32_t>(r.score>>32));for(auto v:{r.lives,r.power,r.misses,r.bombs,std::uint32_t(r.won)})c.Put(v);
    for(const auto& f:d.frames) {
        c.Float(f.dt);c.Float(f.player.mouseDelta.x);c.Float(f.player.mouseDelta.y);c.Float(f.player.mouseSensitivityScale);
        const auto& p=f.player;c.Put(p.forward|(p.backward<<1)|(p.moveLeft<<2)|(p.moveRight<<3)|(p.ascend<<4)|(p.descend<<5)|(p.focus<<6)|(p.fire<<7)|(p.bomb<<8)|(f.confirm<<9)|(f.fast<<10));
    }
    c.Put(Hash(c.bytes,c.bytes.size()));return c;
}
}
const char* BuildId() { return KOUMAKYOU_REPLAY_BUILD; }
const char* SimulationId() { return KOUMAKYOU_REPLAY_SIMULATION; }
const char* ContentId() { return KOUMAKYOU_REPLAY_CONTENT; }
bool Save(const std::filesystem::path& directory,const Data& data,std::filesystem::path& saved,std::string& error) {
    std::filesystem::path temporary;bool ownsTemporary=false;
    try {
        Require(data.continues==0,"续关录像不可保存");auto c=Encode(data);std::filesystem::create_directories(directory);
        static std::atomic<unsigned> serial=0;
        const auto now=std::chrono::system_clock::now();
        const auto time=std::chrono::system_clock::to_time_t(now);std::ostringstream name;
        name<<std::put_time(std::localtime(&time),"%Y%m%d-%H%M%S")<<"-"<<now.time_since_epoch().count()<<"-"<<serial++;
        const auto stem=name.str();
        const auto target=directory/(stem+".krep");temporary=directory/(stem+".tmp");
        Require(!std::filesystem::exists(target) && !std::filesystem::exists(temporary),"录像文件名冲突");
        std::ofstream stream(temporary,std::ios::binary);ownsTemporary=bool(stream);stream.write(reinterpret_cast<const char*>(c.bytes.data()),static_cast<std::streamsize>(c.bytes.size()));stream.close();
        Require(bool(stream),"录像写入失败");std::filesystem::rename(temporary,target);saved=target;error.clear();return true;
    } catch(const std::exception& e) { error=e.what();if(ownsTemporary){std::error_code ec;std::filesystem::remove(temporary,ec);}return false; }
}
bool Load(const std::filesystem::path& file,Data& out,std::string& error) {
    try {
        const auto size=std::filesystem::file_size(file);Require(size>=64 && size<=kMaxBytes,"录像文件长度无效");
        std::ifstream stream(file,std::ios::binary);Codec c;c.bytes.resize(static_cast<std::size_t>(size));stream.read(reinterpret_cast<char*>(c.bytes.data()),static_cast<std::streamsize>(size));Require(bool(stream),"录像读取失败");
        c.offset=c.bytes.size()-4;const auto checksum=c.Get();Require(checksum==Hash(c.bytes,c.bytes.size()-4),"录像校验失败");c.offset=0;
        Require(c.Get()==kMagic,"不是录像文件");const auto version=c.Get();Require(version!=1,"旧版录像请使用对应旧构建观看");Require(version==kVersion,"录像格式不兼容");const auto n=c.Get();Require(n<=512 && c.offset+n<=c.bytes.size(),"录像版本标识无效");
        std::string id(c.bytes.begin()+c.offset,c.bytes.begin()+c.offset+n);c.offset+=n;
        Data d;d.name=c.String();d.route=c.String();d.character=c.String();d.shot=c.String();d.rules=c.String();d.simulation=c.String();d.content=c.String();d.build=c.String();d.continues=c.Get();
        const auto segments=c.Get();Require(segments<=64,"录像片段过多");
        for(unsigned i=0;i<segments;++i) { Segment s;s.stage=c.String();s.entry=c.String();s.target=c.String();s.difficulty=static_cast<Difficulty>(c.Get());s.begin=c.Get();s.end=c.Get();s.seed=c.Get();s.lives=c.Get();s.bombs=c.Get();s.power=c.Get();s.rank=c.Get();const auto low=c.Get();s.score=low|(std::uint64_t(c.Get())<<32);const auto exitLow=c.Get();s.result.score=exitLow|(std::uint64_t(c.Get())<<32);s.result.lives=c.Get();s.result.power=c.Get();s.result.misses=c.Get();s.result.bombs=c.Get();const auto won=c.Get();Require(won<2,"片段结果无效");s.result.won=won;d.segments.push_back(s); }
        const auto extensions=c.Get();Require(extensions<=64,"录像扩展过多");
        for(unsigned i=0;i<extensions;++i) {const auto type=c.Get(),length=c.Get();Require(!(type&0x80000000u),"录像需要不支持的扩展");Require(length<=65536 && c.offset+length<=c.bytes.size(),"录像扩展截断");c.offset+=length;}
        auto& h=d.header;h.seed=c.Get();h.shot=c.Get();h.lives=c.Get();h.bombs=c.Get();h.phase=c.Get();auto flags=c.Get();Require(flags<8,"录像模式无效");h.practice=flags&1;h.spell=flags&2;h.challenge=flags&4;
        const auto count=c.Get();Require(count>0 && count<=kMaxFrames,"录像步数无效");auto& r=d.result;const auto low=c.Get();r.score=low|(std::uint64_t(c.Get())<<32);r.lives=c.Get();r.power=c.Get();r.misses=c.Get();r.bombs=c.Get();const auto won=c.Get();Require(won<2,"录像结果无效");r.won=won;
        Require(c.offset+std::size_t(count)*20+4==c.bytes.size(),"录像步数与文件长度不符");d.frames.resize(count);
        for(auto& f:d.frames) { f.dt=c.Float();f.player.mouseDelta.x=c.Float();f.player.mouseDelta.y=c.Float();f.player.mouseSensitivityScale=c.Float();flags=c.Get();Require(flags<2048,"录像输入标志无效");auto& p=f.player;p.forward=flags&1;p.backward=flags&2;p.moveLeft=flags&4;p.moveRight=flags&8;p.ascend=flags&16;p.descend=flags&32;p.focus=flags&64;p.fire=flags&128;p.bomb=flags&256;f.confirm=flags&512;f.fast=flags&1024; }
        Validate(d);out=std::move(d);error.clear();return true;
    } catch(const std::exception& e) { error=e.what();return false; }
}
}

namespace koumakyou::replay {
bool NormalizeName(const std::string& input,std::string& output) {
    const auto first=input.find_first_not_of(" "),last=input.find_last_not_of(" ");if(first==std::string::npos)return false;
    const auto value=input.substr(first,last-first+1);unsigned count=0;
    for(std::size_t i=0;i<value.size();) {
        auto b=static_cast<unsigned char>(value[i++]);unsigned cp=b,n=0;
        if(b>=0xc2 && b<=0xdf){cp=b&31;n=1;}else if(b>=0xe0 && b<=0xef){cp=b&15;n=2;}else if(b>=0xf0 && b<=0xf4){cp=b&7;n=3;}else if(b>=128)return false;
        for(unsigned j=0;j<n;++j){if(i>=value.size())return false;auto c=static_cast<unsigned char>(value[i++]);if((c&192)!=128)return false;cp=(cp<<6)|(c&63);}
        if((n==1 && cp<128)||(n==2 && cp<2048)||(n==3 && cp<65536)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||cp<32||(cp>=127&&cp<=159)||++count>32)return false;
    }
    output=value;return true;
}
void BeginSession(Data& data,const Header& header) { data={};data.header=header;data.character=header.shot<2?"reimu":"marisa";data.shot=std::array<const char*,4>{"reimu-a","reimu-b","marisa-a","marisa-b"}.at(header.shot);data.rules=header.challenge?"challenge":"standard"; }
void BeginStage(Data& data,Segment s) { if(!data.segments.empty() && data.segments.back().end==0)throw std::invalid_argument("上一片段未结束");s.begin=static_cast<unsigned>(data.frames.size());s.end=0;data.segments.push_back(s); }
void EndStage(Data& data,const Result& result) { if(data.segments.empty() || data.segments.back().end!=0)throw std::invalid_argument("没有打开的片段");data.segments.back().end=static_cast<unsigned>(data.frames.size());data.segments.back().result=result; }
void FinishSession(Data& data,const Result& result) { if(!data.segments.empty() && data.segments.back().end==0)EndStage(data,result);data.result=result; }
bool CanPlay(const Data& d,std::string& error) {
    if(d.simulation!=SimulationId() || d.content!=ContentId()){error="模拟或内容版本不兼容";return false;}
    if(d.segments.size()!=1 || d.segments[0].stage!="stage1" || d.segments[0].difficulty!=Difficulty::Normal || (d.route!="stage1-prototype" && d.route!="stage-practice" && d.route!="spell-practice")){error="录像内容尚未实现";return false;}
    const auto& h=d.header;const auto& segment=d.segments.front();
    const auto expectedShot=std::array<const char*,4>{"reimu-a","reimu-b","marisa-a","marisa-b"}.at(h.shot);
    if(d.shot!=expectedShot || d.character!=(h.shot<2?"reimu":"marisa") || d.rules!=(h.challenge?"challenge":"standard") ||
       d.continues!=0 || d.route!=(h.spell?"spell-practice":h.practice?"stage-practice":"stage1-prototype") || h.lives>5 || h.bombs>3 || (h.phase!=2 && h.phase!=4) || segment.seed!=h.seed || segment.lives!=h.lives || segment.bombs!=h.bombs || segment.score!=0 ||
       segment.power!=(h.spell?128u:8u) || segment.rank!=16 || segment.target!=(h.spell?(h.phase==2?"rumia-night-bird":"rumia-demarcation"):"") || segment.entry!=(h.spell?"spell":"stage")) {error="录像入口配置与当前内容不匹配";return false;}
    error.clear();return true;
}
}
