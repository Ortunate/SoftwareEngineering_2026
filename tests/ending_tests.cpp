// 本文件验证原生结局分支、独立内容时钟、前十命名成绩的分组与原子持久化，不需要图形或本机原作。
#include "narrative/ending.hpp"
#include "config/high_scores.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {
using namespace koumakyou;
void CheckSelection() {
    for(int shot=0;shot<4;++shot) {
        ending::Request r{shot,1,0,0,false,true};
        assert(ending::Select(r)==static_cast<ending::Id>(shot));
        for(int difficulty:{1,2,3}) {r.difficulty=difficulty;r.misses=200;assert(ending::Select(r)==static_cast<ending::Id>(shot));}
        const auto bad=shot<2?ending::Id::ReimuBad:ending::Id::MarisaBad;
        r.difficulty=0;assert(ending::Select(r)==bad);
        r.difficulty=1;r.continues=1;assert(ending::Select(r)==bad);
        r.continues=0;r.challenge=true;r.misses=5;assert(ending::Select(r)==static_cast<ending::Id>(shot));
        r.misses=6;assert(ending::Select(r)==bad);
        r.completedMainline=false;assert(!ending::Select(r));
        r.completedMainline=true;r.difficulty=4;assert(!ending::Select(r));
    }
    assert(!ending::Select({-1,1,0,0,false,true}));
    assert(!ending::Select({4,1,0,0,false,true}));
    assert(!ending::Select({0,1,-1,0,false,true}));
}
void CheckPlayback() {
    // 从六份独立 END 的所有等待参数及原生 70 帧文字延迟求得；不从生产演出表推导期望总时长。
    constexpr std::array frames{24740,28550,34900,36170,6890,6960};
    constexpr std::array firstBackground{0,0,7,7,14,14};
    constexpr std::array textBlocks{20,23,28,29,5,6};
    for(int id=0;id<6;++id) {
        const auto variant=static_cast<ending::Id>(id);int texts=0;
        const auto events=ending::Events(variant);
        assert(events.back().op==ending::Op::Finish);
        for(const auto& e:events) {
            if(e.op==ending::Op::Text)++texts;
            if(e.op==ending::Op::Background)assert(e.argument>=0 && e.argument<15);
            assert(e.text.find("ED_END")==std::string_view::npos);
        }
        assert(texts==textBlocks[id]);
        assert(!ending::Assets(variant).stageBgm.empty());
        for(int fps:{30,60,120,144,240}) {
            ending::Player player;player.Start(variant,false);
            assert(player.Background()==firstBackground[id] && player.WhiteOpacity()==1);
            const int updates=(frames[id]*fps+59)/60;
            for(int i=0;i<updates-1;++i)player.Update(1.0/fps);
            assert(!player.Finished());player.Update(1.0/fps);assert(player.Finished());
        }
    }
    ending::Player player;player.Start(ending::Id::ReimuA,false);
    player.Update(2);assert(player.WhiteOpacity()==1);
    player.Update(1.5);assert(std::abs(player.WhiteOpacity()-0.5F)<1e-6F);
    player.Update(1.5);assert(!player.Lines().empty() && player.WhiteOpacity()==0);
    const auto before=player.EventIndex();player.Update(0);assert(player.EventIndex()==before);
    assert(!player.Skip());player.Update(1.0/60,{true,false,true});assert(!player.Finished());
    player.Start(ending::Id::ReimuA,true);assert(player.Skip() && player.Finished());assert(!player.Skip());
    player.Start(ending::Id::ReimuA,true);
    for(int i=0;i<60*20 && !player.Finished();++i)player.Update(1.0/60,{false,true,true});
    assert(player.Finished());
    bool rejected=false;try{player.Update(std::numeric_limits<double>::infinity());}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
}
void CheckRecords() {
    score::Profile profile;
    for(int i=0;i<10;++i)assert(profile.Insert(false,0,1,{1000-i*10,"PLAYER",1,2,3,100}));
    assert(!profile.Position(false,0,1,1));
    assert(profile.Position(false,0,1,950)==5);
    assert(profile.Insert(false,0,1,{950,"TIE",2,3,4,200}));
    assert(profile.Get(false,0)[5]->name=="TIE" && profile.Get(false,0)[6]->name=="PLAYER");
    assert(profile.Get(true,0)[0]==std::nullopt && profile.Get(false,1)[0]==std::nullopt && profile.Get(false,0,2)[0]==std::nullopt);
    assert(!profile.Insert(false,0,1,{2000,"名字",0,0,0,0}));
    assert(!profile.Insert(false,0,1,{2000,"123456789",0,0,0,0}));
    assert(!profile.Position(false,4,1,9999));
    assert(score::ValidName("A B_123!") && !score::ValidName("        ") && !score::ValidName("A\nB"));
    assert(profile.Insert(true,3,1,{9223372036854770000LL,"MAX",0,0,0,200}));
    const auto path=std::filesystem::temp_directory_path()/
        ("koumakyou-ending-unit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(path));
    const auto file=path/"scores.dat";profile.endingsSeen[3]=true;
    assert(score::Save(file,profile));
    const auto loaded=score::Load(file);
    assert(loaded.Get(false,0)[5]->name=="TIE" && loaded.Get(true,3)[0]->value==9223372036854770000LL);
    assert(loaded.endingsSeen[3] && loaded.lastName=="MAX");
    std::ifstream input(file);const std::string original((std::istreambuf_iterator<char>(input)),{});input.close();
    auto bad=profile;bad.groups[0][0][1][0]->name="\n";
    assert(!score::Save(file,bad));
    std::ifstream after(file);assert(std::string((std::istreambuf_iterator<char>(after)),{})==original);after.close();
    assert(!score::Save(file/"cannot-create",profile));
    assert(score::Load(file).Get(false,0)[5]->name=="TIE");
    std::ofstream corrupt(path/"corrupt.dat");
    corrupt<<"KOUMAKYOU_SCORES 1\r\nscore 0 0 1 0 -1 0 0 0 0 \"BAD\"\r\n"
           <<"score 0 0 1 1 400 0 0 0 0 \"VALID\"\r\nscore 4 0 1 0 900 0 0 0 0 \"BAD\"\r\n";
    corrupt.close();const auto filtered=score::Load(path/"corrupt.dat");assert(filtered.Get(false,0)[0]->name=="VALID");
    assert(!filtered.Get(false,0)[1]);
    std::filesystem::remove_all(path);
}
} // namespace
/// @brief 无窗口入口；覆盖六结局、Miss 5/6 边界、五种显示帧率和持久化失败保护。
int main() {CheckSelection();CheckPlayback();CheckRecords();return 0;}
