// 本文件实现前十成绩的校验、稳定排序及原子存盘，保存失败不会覆盖已有成绩。
#include "config/high_scores.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace koumakyou::score {
namespace {
bool ValidGroup(int shot,int difficulty) {return shot>=0 && shot<kShots && difficulty>=0 && difficulty<kDifficulties;}
bool ValidEntry(const Entry& e) {
    return e.value>=0 && ValidName(e.name) && e.misses>=0 && e.bombs>=0 && e.graze>=0 && e.completedAt>=0 && e.stage>=0 && e.stage<=7 && (!e.cleared || e.stage>0);
}
} // namespace
bool ValidName(std::string_view name) {
    return !name.empty() && name.size()<=kNameLength && name.find_first_not_of(' ')!=std::string_view::npos &&
        std::all_of(name.begin(),name.end(),[](unsigned char c){return c>=32 && c<=126;});
}
const Table& Profile::Get(bool challenge,int shot,int difficulty) const {
    static const Table empty{};
    return ValidGroup(shot,difficulty)?groups[challenge][shot][difficulty]:empty;
}
std::optional<int> Profile::Position(bool challenge,int shot,int difficulty,long long value) const {
    if(!ValidGroup(shot,difficulty) || value<0)return std::nullopt;
    const auto& table=Get(challenge,shot,difficulty);
    for(int i=0;i<kSlots;++i)if(!table[i] || value>=table[i]->value)return i;
    return std::nullopt;
}
bool Profile::Insert(bool challenge,int shot,int difficulty,const Entry& entry) {
    if(!ValidEntry(entry))return false;
    const auto position=Position(challenge,shot,difficulty,entry.value);
    if(!position)return false;
    auto& table=groups[challenge][shot][difficulty];
    for(int i=kSlots-1;i>*position;--i)table[i]=table[i-1];
    table[*position]=entry;lastName=entry.name;return true;
}
Profile Load(const std::filesystem::path& path) {
    Profile profile;std::ifstream input(path);
    std::string line;
    if(!std::getline(input,line))return profile;
    if(!line.empty() && line.back()=='\r')line.pop_back();
    const bool legacy=line=="KOUMAKYOU_SCORES 1";
    if(!legacy && line!="KOUMAKYOU_SCORES 2")return profile;
    while(std::getline(input,line)) {
        std::istringstream row(line);std::string kind,trailing;row>>kind;
        if(kind=="name") {
            std::string name;if(row>>std::quoted(name) && !(row>>trailing) && (name.empty() || ValidName(name)))profile.lastName=name;
        } else if(kind=="ending") {
            int id=-1;if(row>>id && !(row>>trailing) && id>=0 && id<6)profile.endingsSeen[id]=true;
        } else if(kind=="score") {
            int mode=-1,shot=-1,difficulty=-1,slot=-1;Entry entry;
            int cleared=0;
            const bool fields=bool(row>>mode>>shot>>difficulty>>slot>>entry.value>>entry.misses>>entry.bombs>>entry.graze>>entry.completedAt>>std::quoted(entry.name));
            const bool progress=legacy || bool(row>>entry.stage>>cleared);
            entry.cleared=cleared==1;
            if(fields && progress && cleared>=0 && cleared<=1 && !(row>>trailing) && mode>=0 && mode<2 && ValidGroup(shot,difficulty) && slot>=0 && slot<kSlots && ValidEntry(entry)) {
                auto& target=profile.groups[mode][shot][difficulty][slot];
                if(!target)target=entry;
            }
        }
    }
    bool hasScores=false;
    for(auto& mode:profile.groups)for(auto& shot:mode)for(auto& table:shot) {
        std::stable_sort(table.begin(),table.end(),[](const auto& a,const auto& b){return a && (!b || a->value>b->value);});
        hasScores=hasScores || table.front().has_value();
    }
    // 旧版可能仅为观看结局写入默认PLAYER；没有有效成绩时不能把它视为一次成功命名。
    if(legacy && !hasScores)profile.lastName.clear();
    return profile;
}
bool Save(const std::filesystem::path& path,const Profile& profile) {
    if(!profile.lastName.empty() && !ValidName(profile.lastName))return false;
    for(const auto& mode:profile.groups)for(const auto& shot:mode)for(const auto& table:shot) {
        bool emptySeen=false;long long previous=-1;
        for(const auto& e:table) {
            if(!e){emptySeen=true;continue;}
            if(emptySeen || !ValidEntry(*e) || (previous>=0 && previous<e->value))return false;
            previous=e->value;
        }
    }
    const auto temporary=std::filesystem::path(path.string()+".tmp");
    std::ofstream output(temporary,std::ios::trunc);if(!output)return false;
    output<<"KOUMAKYOU_SCORES 2\nname "<<std::quoted(profile.lastName)<<'\n';
    for(int id=0;id<6;++id)if(profile.endingsSeen[id])output<<"ending "<<id<<'\n';
    for(int mode=0;mode<2;++mode)for(int shot=0;shot<kShots;++shot)for(int difficulty=0;difficulty<kDifficulties;++difficulty)
        for(int slot=0;slot<kSlots;++slot) {
            const auto& entry=profile.groups[mode][shot][difficulty][slot];if(!entry)continue;
            const auto& e=*entry;
            output<<"score "<<mode<<' '<<shot<<' '<<difficulty<<' '<<slot<<' '<<e.value<<' '<<e.misses<<' '<<e.bombs<<' '
                  <<e.graze<<' '<<e.completedAt<<' '<<std::quoted(e.name)<<' '<<e.stage<<' '<<int(e.cleared)<<'\n';
        }
    output.flush();bool written=output.good();output.close();written=written&&!output.fail();
    std::error_code error;
    if(written)std::filesystem::rename(temporary,path,error);
    if(!written || error) {std::error_code ignored;std::filesystem::remove(temporary,ignored);return false;}
    return true;
}
} // namespace koumakyou::score
