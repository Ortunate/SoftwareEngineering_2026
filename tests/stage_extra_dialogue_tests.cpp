// 本文件核对Extra六段对白的机体分派、真实等待/结算时刻与演出边界；原文逐写入另由独立MSG工具验证。
#include "stages/stage_extra/dialogue.hpp"
#include "stages/stage_extra/content.hpp"
#include "render/dialogue_motion.hpp"
#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

/// @brief 覆盖两角色六段分支与四机体分派；期望帧取自MSG独立展开，不从生产表推导。
void RunStageExtraDialogueTests() {
    using namespace koumakyou;
    using namespace stage_extra;
    const StageExtraContent content;
    for(int previous:{0,5,10,20})for(int card=0;card<=135;++card) {
        const int expected=card<122||card>134?previous:card==131?10:previous+1;
        assert(content.CaptureProgress(previous,card)==expected);
    }
    int progress=0;
    for(int card=119;card<=130;++card)progress=content.CaptureProgress(progress,card);
    assert(progress==9);progress=content.CaptureProgress(progress,131);assert(progress==10);
    for(int card=132;card<=134;++card)progress=content.CaptureProgress(progress,card);
    assert(progress==13);
    for(int shot=0;shot<4;++shot)for(int captures:{0,5,6,10,13})for(bool challenge:{false,true})for(int misses:{-2,0,2,3,99}) {
        const auto choice=static_cast<ShotType>(shot);
        const auto main=content.ResolveDialogueFlow(DialogueScene::FinalBoss,true,choice,{captures,challenge,misses});
        const bool qualifies=captures>=6&&(!challenge||misses<3);
        assert(main.skip==qualifies&&main.finishStage==!qualifies);
        assert(main.awardFrame==(qualifies?-1:shot<2?8206:11016));
        const auto encore=content.ResolveDialogueFlow(DialogueScene::EncoreBoss,true,choice,{captures,challenge,misses});
        assert(!encore.skip&&encore.finishStage&&encore.awardFrame==(shot<2?9803:13412));
        for(auto scene:{DialogueScene::FinalBoss,DialogueScene::Midboss,DialogueScene::EncoreBoss}) {
            const auto before=content.ResolveDialogueFlow(scene,false,choice,{captures,challenge,misses});
            assert(!before.skip&&!before.finishStage&&before.awardFrame==-1);
            assert(!content.SceneDialogue(scene,false,choice).empty());
        }
    }
    constexpr std::array sections{DialogueSection::MainBefore,DialogueSection::MainAfter,DialogueSection::Midboss,
        DialogueSection::EncoreBefore,DialogueSection::EncoreAfter,DialogueSection::EncoreInterlude};
    constexpr std::array<std::array<int,6>,2> counts{{{39,20,4,7,23,0},{27,26,4,7,32,0}}};
    constexpr std::array<std::array<int,6>,2> ends{{{15923,8566,373,1659,10163,86},{11106,11376,373,1659,13772,86}}};
    constexpr std::array<std::array<int,6>,2> clears{{{-1,8206,-1,-1,9803,-1},{-1,11016,-1,-1,13412,-1}}};
    for(int shot=0;shot<4;++shot) {
        const int character=shot>=2;
        for(std::size_t section=0;section<sections.size();++section) {
            const auto& script=ExtraDialogue(sections[section],static_cast<ShotType>(shot));
            assert(script.lines.size()==std::size_t(counts[character][section]));
            assert(script.endFrame==ends[character][section]&&script.clearFrame==clears[character][section]);
            float total=0;
            for(const auto& line:script.lines) {
                total+=line.duration;
                assert(line.confirmDelay+1e-6F>=line.textDelay+line.line2Delay+8.0F/60);
            }
            if(!script.lines.empty())assert(std::abs(total*60-script.endFrame)<.01F);
            int previous=-1,signal=0;
            for(const auto& cue:script.cues) {
                assert(cue.frame>=previous&&cue.frame<=script.endFrame);previous=cue.frame;
                if(cue.kind==DialogueCueKind::StageSignal)assert(cue.value==++signal);
                if(cue.kind==DialogueCueKind::StageMusic)assert(cue.value>=-1&&cue.value<=2);
            }
        }
        // 前奏的主战放行发生在长对白中；锁定的180帧入场后重新接受输入，不能等待对白结束才创建角色。
        const auto& main=ExtraDialogue(DialogueSection::MainBefore,static_cast<ShotType>(shot));
        const int release=character?2065:2066;
        assert(DialogueFlagAt(main.cues,DialogueCueKind::InputEnabled,(release-1)/60.0F,true));
        assert(!DialogueFlagAt(main.cues,DialogueCueKind::InputEnabled,release/60.0F,true));
        assert(DialogueFlagAt(main.cues,DialogueCueKind::InputEnabled,(release+180)/60.0F,true));
        assert(std::count_if(main.cues.begin(),main.cues.end(),[&](const auto& cue){
            return cue.kind==DialogueCueKind::StageSignal&&cue.frame==release&&cue.value==1;})==1);
        ui::DialogueMotion mainMotion;
        mainMotion.Seek(main.cues,(release+60)/60.0F);assert(mainMotion.Sample(true).slide==1);
        mainMotion.Seek(main.cues,(release+90)/60.0F);assert(mainMotion.Sample(true).slide==0&&mainMotion.Sample(true).opacity==1);
        // 追加战前的音乐转换与放行不在同一时刻；空字清除区间仍属于真实演出。
        const auto& encore=ExtraDialogue(DialogueSection::EncoreBefore,static_cast<ShotType>(shot));
        assert(encore.cues.front().kind==DialogueCueKind::StageMusic&&encore.cues.front().value==-1);
        assert(encore.lines[3].text[0]=='\0'&&encore.lines[3].speaker[0]=='\0');
        assert(std::count_if(encore.cues.begin(),encore.cues.end(),[](const auto& cue){
            return cue.kind==DialogueCueKind::StageMusic&&cue.frame==943&&cue.value==2;})==1);
        assert(std::count_if(encore.cues.begin(),encore.cues.end(),[](const auto& cue){
            return cue.kind==DialogueCueKind::StageSignal&&cue.frame==999&&cue.value==1;})==1);
        const auto& interlude=ExtraDialogue(DialogueSection::EncoreInterlude,static_cast<ShotType>(shot));
        assert(interlude.lines.empty()&&interlude.cues[0].value==-1&&interlude.cues[1].value==2);
        assert(!DialogueFlagAt(interlude.cues,DialogueCueKind::StageSignal,55.5F/60));
        assert(DialogueFlagAt(interlude.cues,DialogueCueKind::StageSignal,56.0F/60));
        ui::DialogueMotion motion;
        motion.Seek(encore.cues,942.0F/60);assert(motion.Sample(false).slide==0);
        motion.Seek(encore.cues,958.0F/60);assert(std::abs(motion.Sample(false).slide-.75F)<1e-5F);
        motion.Seek(encore.cues,973.0F/60);assert(motion.Sample(false).slide==1&&motion.Sample(false).opacity==0);
        for(auto section:{DialogueSection::MainAfter,DialogueSection::EncoreAfter}) {
            const auto& after=ExtraDialogue(section,static_cast<ShotType>(shot));
            assert(after.endFrame-after.clearFrame==360); // MSG9之后仍等待，11之后静态记录不可执行。
        }
    }
    for(int bad:{-1,4,99}) {
        bool rejected=false;
        try {(void)ExtraDialogue(DialogueSection::MainBefore,static_cast<ShotType>(bad));}
        catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    }
    assert(ExtraDialogue(DialogueSection::MainAfter,ShotType::HomingAmulet).lines[16].duration==1.0F/60);
    bool rejected=false;
    try {(void)ExtraDialogue(static_cast<DialogueSection>(99),ShotType::HomingAmulet);}
    catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    std::cout<<"Extra twelve dialogue branches and four shot mappings passed\n";
}
