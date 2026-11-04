// 本文件负责在隐藏图形窗口中验证自机射击、本体碰撞、Game 战斗演出、结算及资源生命周期。

#include "gameplay/bosses/rumia.hpp"
#include "boss_fixture.hpp"
#include "actor_fixture.hpp"
#include "app/game.hpp"
#include "app/math.hpp"
#include "rlgl.h"
#include "stages/stage1/behaviors.hpp"
#include "stages/stage1/boss_config.hpp"
#include "stages/stage1/content.hpp"
#include "stages/stage2/boss_config.hpp"
#include "stages/stage2/content.hpp"
#include "stages/stage3/boss_config.hpp"
#include "stages/stage3/content.hpp"
#include "gameplay/bosses/meiling.hpp"
#include "gameplay/bosses/cirno.hpp"
#include "gameplay/bosses/daiyousei.hpp"
#include "stages/stage4/boss_config.hpp"
#include "stages/stage5/boss_config.hpp"
#include "stages/stage6/boss_config.hpp"
#include "stages/stage6/behaviors.hpp"
#include "stages/stage_extra/boss_config.hpp"
#include "stages/stage_extra/manifest.hpp"
#include "stages/stage_extra/ids.hpp"
#include "stages/stage_extra/dialogue.hpp"
#include "stages/stage_extra/timeline.hpp"
#include "stages/stage_extra/content.hpp"
#include "gameplay/bosses/sakuya.hpp"
#include "gameplay/bosses/remilia.hpp"
#include "stages/stage4/content.hpp"
#include "stages/stage5/content.hpp"
#include "stages/stage6/content.hpp"
#include "gameplay/bosses/koakuma.hpp"
#include "gameplay/bosses/patchouli.hpp"
#include "gameplay/bosses/flandre.hpp"
#include "ui/typography.hpp"
#include "render/gameplay_layout.hpp"
#include "render/boss_hud.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <array>
#include <utility>
#include <vector>
#include <cstring>

namespace koumakyou {
/// @brief Extra已接入段的隔离素材夹具：本体明确选择ExtraEnemies/Flandre，公共素材借用既有清单。
/// 可选择正式清单及BGM，但不提供时间轴或正式练习入口；清单存储覆盖异步借用期。
class ExtraBattleFixtureContent final : public stage_assets::NativeBattleContent {
public:
    explicit ExtraBattleFixtureContent(battle::Selection boss={battle::Atlas::ExtraEnemies,65},bool formalAssets=false,
        std::span<const DialogueCue> cues={}):cues_(cues),formalAssets_(formalAssets) {
        if(formalAssets) {manifest_=stage_extra::StageExtraManifest();return;}
        const auto& source=source_.Assets();visuals_.assign(source.visuals.begin(),source.visuals.end());
        for(auto& visual:visuals_) {
            if(visual.id==stage_assets::kBoss)visual.animation=boss;
            if(visual.id==stage_assets::kOrb)visual.animation={battle::Atlas::ExtraEnemies,13};
            if(visual.id==stage_extra::kPatchouli)visual.animation={battle::Atlas::ExtraEnemies,65};
        }
        manifest_.visuals=visuals_;
    }
    const StageAssetManifest& Assets() const override {return manifest_;}
    std::span<const StageEvent> Events() const override {return {};}
    const bosses::Battle& PracticeBoss() const override {throw std::logic_error("Extra fixture has no playable entry");}
    std::span<const DialogueLine> Dialogue(bool=false,ShotType=ShotType::HomingAmulet) const override {return {};}
    /// @brief 可选演出表借用测试函数的静态存储；仅验证公共对白音乐执行，不构造正式Extra流程。
    std::span<const DialogueCue> DialogueCues(bool,ShotType) const override {return cues_;}
    float DialogueMinimumDuration(DialogueScene,bool,ShotType) const override {return cues_.empty()?0:(cues_.back().frame+120)/60.0F;}
    float DialogueLineDuration() const override {return 5;}
    VisualId BossPortraitVisual(int expression=0) const override {
        return formalAssets_?stage_extra::kFlandrePortraits[expression>=0&&expression<4?expression:0]:source_.BossPortraitVisual(expression);
    }
    VisualId SpellPortraitVisual(int card) const override {
        return formalAssets_?(card>=119&&card<=121?stage_extra::kPatchouliPortraits[0]:stage_extra::kFlandrePortraits[2]):source_.SpellPortraitVisual();
    }
    const char* StartTitle() const override {return "Extra segment validation";}
    const char* ClearTitle() const override {return "Extra fixture";}
private:
    stage4::Stage4Content source_;
    std::vector<VisualDefinition> visuals_;StageAssetManifest manifest_{};
    std::span<const DialogueCue> cues_;
    bool formalAssets_;
};

/// @brief 借用正式素材及六段实际对白的独立播放夹具；不提供出生表、条件判定或可玩入口。
class ExtraDialogueFixtureContent final : public stage_assets::NativeBattleContent {
public:
    explicit ExtraDialogueFixtureContent(stage_extra::DialogueSection section):section_(section) {}
    const StageAssetManifest& Assets() const override {return stage_extra::StageExtraManifest();}
    std::span<const StageEvent> Events() const override {return {};}
    const bosses::Battle& PracticeBoss() const override {throw std::logic_error("Extra dialogue fixture has no playable entry");}
    std::span<const DialogueLine> Dialogue(bool,ShotType shot) const override {return stage_extra::ExtraDialogue(section_,shot).lines;}
    std::span<const DialogueCue> DialogueCues(bool,ShotType shot) const override {return stage_extra::ExtraDialogue(section_,shot).cues;}
    std::span<const DialogueLine> SceneDialogue(DialogueScene,bool after,ShotType shot) const override {return Dialogue(after,shot);}
    std::span<const DialogueCue> SceneDialogueCues(DialogueScene,bool after,ShotType shot) const override {return DialogueCues(after,shot);}
    float DialogueMinimumDuration(DialogueScene,bool,ShotType shot) const override {return stage_extra::ExtraDialogue(section_,shot).endFrame/60.0F;}
    float DialogueLineDuration() const override {return 5;}
    VisualId BossPortraitVisual(int expression=0) const override {
        const auto& ids=section_==stage_extra::DialogueSection::Midboss?stage_extra::kPatchouliPortraits:stage_extra::kFlandrePortraits;
        return ids[expression>=0&&expression<4?expression:0];
    }
    std::array<std::string_view,2> BossIntroduction() const override {return stage_extra::kFlandreIntroduction;}
    const char* StartTitle() const override {return "Extra dialogue validation";}
    const char* ClearTitle() const override {return "Extra fixture";}
private:
    stage_extra::DialogueSection section_;
};

/// @brief 仅此测试定义的友元探针；调用正式游戏路径验证效果不重复奖励或保留战斗对象。
struct GameLifecycleProbe {
    /// @brief 正式Extra纹理在三维通道中的暂停、重复绘制和卸载检查；截图覆盖四个观察位置及竖屏。
    static void TestExtraScenery(Game& game,const std::string& prefix) {
        game.EnterMainMenu();game.previewMode_=true;game.practiceMode_=game.spellPractice_=false;
        game.runDifficulty_=native_combat::Difficulty::Normal;
        game.stageContent_=std::make_unique<stage_extra::StageExtraContent>();
        game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.stage_=StageDirector{};
        const auto atlas=game.assets_.GetVisual(stage_extra::kBackground).texture;
        assert(IsTextureValid(atlas)&&atlas.width==1024&&atlas.height==1024);
        const auto originalCamera=game.camera_;
        const auto originalSize=Vector2{static_cast<float>(GetScreenWidth()),static_cast<float>(GetScreenHeight())};
        // 独立离屏只画内容场景，排除HUD闪烁；重复提交同一时刻必须逐像素相同，推进后应有实际纹理位移。
        const auto fingerprint=[&](float seconds) {
            const auto target=LoadRenderTexture(320,240);assert(IsRenderTextureValid(target));
            BeginDrawing();BeginTextureMode(target);ClearBackground(MAGENTA);BeginMode3D(originalCamera);
            game.stageContent_->DrawScenery(seconds,game.assets_);
            EndMode3D();EndTextureMode();EndDrawing();
            const auto image=LoadImageFromTexture(target.texture);assert(IsImageValid(image));
            const auto colors=LoadImageColors(image);assert(colors);
            std::uint64_t hash=14695981039346656037ULL;int visible=0;
            for(int pixel=0;pixel<320*240;++pixel) {
                const auto c=colors[pixel];visible+=c.r!=MAGENTA.r||c.g!=MAGENTA.g||c.b!=MAGENTA.b;
                for(const auto channel:std::array{c.r,c.g,c.b,c.a}) {hash^=channel;hash*=1099511628211ULL;}
            }
            UnloadImageColors(colors);UnloadImage(image);UnloadRenderTexture(target);assert(visible>320*240/2);
            return hash;
        };
        const auto start=fingerprint(0);assert(start==fingerprint(0)&&start!=fingerprint(3));
        game.Update(1.0F/60);const auto time=game.scenerySeconds_;const auto paused=fingerprint(time);
        game.TogglePause();for(int frame=0;frame<120;++frame)game.Update(1.0F/60);
        assert(game.scenerySeconds_==time&&fingerprint(time)==paused);game.TogglePause();
        game.Update(1.0F/60);assert(game.scenerySeconds_>time);
        for(int view=0;view<4;++view) {
            game.camera_=view==0?originalCamera:view==1?Camera3D{{8,8,-10},{0,-2,15},{0,1,0},72,CAMERA_PERSPECTIVE}:
                view==2?Camera3D{{-8,-8,-10},{0,0,15},{0,1,0},72,CAMERA_PERSPECTIVE}:
                Camera3D{{0,0,0},{0,0,-10},{0,1,0},72,CAMERA_PERSPECTIVE};
            game.scenerySeconds_=view*137.25;const auto before=game.scenerySeconds_;
            const auto label="extra-scenery-view"+std::to_string(view);Capture(game,prefix,label.c_str());
            assert(game.scenerySeconds_==before&&game.assets_.GetVisual(stage_extra::kBackground).texture.id==atlas.id);
        }
        game.camera_=originalCamera;SetWindowSize(390,844);Capture(game,prefix,"extra-scenery-portrait");
        SetWindowSize(originalSize.x,originalSize.y);game.EnterMainMenu();
        assert(!game.assets_.HasStage()&&game.assets_.GetVisual(stage_extra::kBackground).texture.id==0);
        stage_extra::StageExtraContent content;
        BeginDrawing();BeginMode3D(originalCamera);content.DrawScenery(0,game.assets_);EndMode3D();EndDrawing();
        std::cout<<"Extra scenery actual atlas, deterministic draw, scroll, pause, camera/portrait and unload passed\n";
    }

    /// @brief 经实际敌弹碰撞和道具拾取验证净Miss；保留总Miss用于符卡取得，且拾取只能消费一次。
    static void TestChallengeProgress(Game& game) {
        for(bool challenge:{false,true}) {
            game.StartPreview();game.stage_=StageDirector{};game.boss_.reset();game.actors_.Clear();game.drops_.Clear();
            game.player_.Reset();game.player_.ConfigureRun(3,2,challenge,ShotType::HomingAmulet);
            game.challengeMode_=challenge;game.missCount_=0;
#ifdef KOUMAKYOU_DEBUG_FEATURES
            game.debugInvincible_=false;
#endif
            const auto hit=[&] {
                game.player_.SetInvulnerability(0);
                assert(game.bullets_.Spawn(BulletOwner::Enemy,game.player_.Position(),{},0.1F,RED,stage1::kBlueBullet));
                game.ResolveCollisions();
            };
            const auto life=[&] {
                const auto count=game.player_.NetMisses();const auto misses=game.missCount_;
                game.drops_.SpawnNativeItem(game.player_.Position(),5,false);
                game.UpdatePickups(0);assert(PickupCount(game)==0);
                assert(game.player_.NetMisses()==count-(challenge?1:0)&&game.missCount_==misses);
                game.UpdatePickups(0);assert(game.player_.NetMisses()==count-(challenge?1:0));
            };
            life();assert(game.player_.NetMisses()==(challenge?-1:0));
            hit();assert(game.missCount_==1&&game.player_.NetMisses()==(challenge?0:1));
            game.ResolveCollisions();assert(game.missCount_==1);
            life();hit();hit();
            assert(game.missCount_==3&&game.player_.NetMisses()==(challenge?1:3));
            assert(game.player_.RemainingLives()==(challenge?4:3));
            const auto count=game.player_.NetMisses();
            game.TogglePause();game.Update(.5F);assert(game.player_.NetMisses()==count&&game.missCount_==3);game.TogglePause();
            game.player_.ResetPose();assert(game.player_.NetMisses()==count);
            game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
        }
        game.challengeMode_=false;
        std::cout<<"Standard/Challenge actual collisions, signed life credits, total Miss isolation and unload passed\n";
    }
    /// @brief 两角色通过正式Extra入口以自然对白和真实120Hz串接普通出生/道中/主战，开局rank18。
    /// 挑战模式在非符期把净Miss提升到6以隔离尾波残机抵扣，标准模式走追加三符；连续运行直至slot4命名保存。
    static void TestExtraTimeline(Game& game,const std::string& prefix) {
        for(int shot:{0,2}) {
            game.EnterMainMenu();game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.StartStage(StageId::Extra,false,native_combat::Difficulty::Normal,StageEntry::Extra);
            game.challengeMode_=shot==0;
            const double deadline=GetTime()+60;
            while(game.IsLoading()) {
                assert(GetTime()<deadline&&!game.HasLoadingFailed());game.Update(1.0F/60);WaitTime(.001);
            }
            assert(game.state_==RunState::Playing&&game.referenceRank_==18&&!game.previewMode_);
            game.player_.SetInvulnerability(5000);
            if(shot==2)game.player_.AdjustPower(128);
            bool midDialogue=false,mainDialogue=false,mainStarted=false,lifeDrop=false,paused=false,postmid=false;
            float midEnd=-1,mainDialogueStart=-1;std::size_t peak=0;int ticks=0;std::vector<int> spells,encoreSpells;
            for(;ticks<1000*120&&game.state_==RunState::Playing;++ticks) {
                game.Update(1.0F/120);
                assert(!game.actors_.DroppedSpawns()&&!game.bullets_.Pool().DroppedSpawns());
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.dialogueActive_) {
                    if(game.dialogueScene_==DialogueScene::Midboss) {
                        if(!midDialogue) {assert(game.bossEventId_==1&&game.boss_->attack==0);Capture(game,prefix,("extra-timeline-mid-shot"+std::to_string(shot)).c_str());}
                        midDialogue=true;
                    } else {
                        if(!mainDialogue)mainDialogueStart=game.stage_.Elapsed();
                        mainDialogue=true;
                        if(game.bossEventId_==4)assert(game.boss_->attack==0&&!game.boss_->CanTakeDamage());
                    }
                }
                if(!paused&&game.dialogueActive_&&game.dialogueTimer_>1) {
                    paused=true;const auto clock=game.dialogueTimer_;const auto stageClock=game.stage_.Elapsed();
                    game.TogglePause();game.Update(.5F);assert(game.dialogueTimer_==clock&&game.stage_.Elapsed()==stageClock);game.TogglePause();
                }
                if(midEnd>=0&&!postmid) {
                    bool ordinary=false;game.actors_.ForEachActive([&](const behavior::Actor& actor){if(actor.visual==stage_extra::kDarkFairy&&actor.maxHealth==400)ordinary=true;});
                    if(ordinary) {postmid=true;assert(std::abs(game.stage_.Elapsed()-midEnd-2)<5e-4F);}
                }
                behavior::Actor* lifeActor=nullptr;
                game.actors_.ForEachActive([&](behavior::Actor& actor){if(actor.sourceItem==5)lifeActor=&actor;});
                if(lifeActor) {
                    assert(!lifeDrop&&lifeActor->health==20&&lifeActor->CanBeTargeted());
                    const auto lives=[&] {int count=0;game.drops_.ForEachActive([&](const Pickup& item){if(item.kind==PickupKind::Life)++count;});return count;};
                    const int before=lives();lifeActor->health=0;game.ResolveCollisions();assert(lives()==before+1);
                    game.ResolveCollisions();assert(lives()==before+1);lifeDrop=true;
                }
                if(game.boss_&&game.boss_->active) {
                    const bool main=game.bossEventId_==4;
                    if(main&&game.boss_->attack>0) {
                        if(!mainStarted) {
                            mainStarted=true;assert(!game.dialogueActive_&&mainDialogue&&midDialogue&&postmid&&lifeDrop);
                            const auto end=stage_extra::ExtraDialogue(stage_extra::DialogueSection::MainBefore,static_cast<ShotType>(shot)).endFrame;
                            assert(std::abs(game.stage_.Elapsed()-mainDialogueStart-end/60.0F-1.0F/60)<5e-4F);
                            if(shot==0) {
                                assert(!game.boss_->SpellCardNumber());
                                std::cout<<"Extra nonspell gate before hits: netMisses="<<game.player_.NetMisses()<<std::endl;
                                while(game.player_.NetMisses()<6)game.HandlePlayerHit();
                                game.player_.SetInvulnerability(5000);
                            }
                            Capture(game,prefix,("extra-timeline-main-shot"+std::to_string(shot)).c_str());
                        }
                        const int card=game.boss_->SpellCardNumber();if(card&&(spells.empty()||spells.back()!=card))spells.push_back(card);
                    }
                    if(game.bossEventId_==11) {
                        const int card=game.boss_->SpellCardNumber();
                        if(card&&(encoreSpells.empty()||encoreSpells.back()!=card))encoreSpells.push_back(card);
                    }
                    if(game.boss_->CanTakeDamage()&&game.boss_->skillTimer>2) {
                        const auto* skill=static_cast<bosses::ConfiguredBoss&>(*game.boss_).CurrentSkill();assert(skill);
                        if(!skill->survival) {
                            game.boss_->health=skill->reserveHealth>0?skill->reserveHealth-1:0;game.ResolveCollisions();
                        }
                    }
                } else if(game.boss_) {
                    if(!mainStarted&&midDialogue&&midEnd<0) {midEnd=game.stage_.Elapsed();assert(!game.bossEventId_&&game.stageSpellCaptures_==0);}
                }
            }
            assert(ticks<1000*120&&midDialogue&&mainDialogue&&mainStarted&&lifeDrop&&paused&&postmid);
            assert(game.state_==RunState::StageClear&&!game.bossEventId_&&game.stageClearShown_&&!game.dialogueActive_&&spells.size()==10);
            std::cout<<"Extra full exit shot="<<shot<<" captures="<<game.stageSpellCaptures_<<" netMisses="<<game.player_.NetMisses()<<" encore="<<encoreSpells.size()<<std::endl;
            assert(encoreSpells.size()==(shot==0?0:3)&&game.stageSpellCaptures_==(shot==0?10:13));
            for(std::size_t i=0;i<encoreSpells.size();++i)assert(encoreSpells[i]==132+static_cast<int>(i));
            for(int i=0;i<10;++i)assert(spells[i]==122+i);
            const auto value=game.score_;game.ContinueStageClear();
            for(int n=0;n<120&&game.state_==RunState::Finishing;++n)game.Update(1.0F/120);
            assert(game.state_==RunState::ScoreSave&&game.scoreDifficulty_==4&&!game.pendingEnding_&&!game.stageContent_);
            game.scoreName_="FULL";game.SaveNamedScore();assert(game.state_==RunState::Score);
            const auto saved=score::Load(score::kFilename);
            assert(saved.Get(shot==0,shot,4)[0]&&saved.Get(shot==0,shot,4)[0]->value==value);
            game.EnterMainMenu();assert(!game.boss_&&!EnemyCount(game)&&game.assets_.Stage().visuals.empty());
            std::cout<<"Extra full timeline shot "<<shot<<" ordinary/midboss/dialogue/main/encore="<<(shot!=0)<<"/score-save passed; peak "<<peak<<" bullets; "<<ticks/120.0<<"s\n";
        }
    }
    /// @brief 正式Extra入口后定位战后七事件；四类门槛×四御札，保留追加战/MSG9/11与slot4命名保存。
    /// 只隔离已验收的前半关；快照主线记录和结局标记，检查两种模式均不污染本篇存档。
    static void TestExtraPostfightFlow(Game& game,const std::string& prefix) {
        struct Case {int captures;bool challenge;int misses;bool encore;};
        constexpr std::array cases{Case{5,false,0,false},Case{6,false,3,true},Case{6,true,2,true},Case{10,true,3,false}};
        const auto stageScores=game.stageHighScores_;const auto best=game.bestScore_;const auto endings=game.scoreProfile_.endingsSeen;
        for(int shot=0;shot<4;++shot)for(std::size_t profile=0;profile<cases.size();++profile) {
            const auto c=cases[profile];
            game.EnterMainMenu();game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.StartStage(StageId::Extra,false,native_combat::Difficulty::Normal,StageEntry::Extra);
            game.challengeMode_=c.challenge;
            const double deadline=GetTime()+60;
            while(game.IsLoading()) {
                assert(GetTime()<deadline&&!game.HasLoadingFailed());game.Update(1.0F/60);WaitTime(.001);
            }
            assert(game.state_==RunState::Playing&&game.referenceRank_==18&&!game.previewMode_&&game.ScoreDifficultyIndex()==4);
            game.player_.ConfigureRun(8,3,c.challenge,static_cast<ShotType>(shot));
            for(int miss=0;miss<c.misses;++miss)game.HandlePlayerHit();
            assert(game.player_.NetMisses()==c.misses&&game.missCount_==c.misses);
            game.player_.SetInvulnerability(5000);game.stageSpellCaptures_=c.captures;
            game.stagePointItems_=17;game.stageGraze_=123;
            const auto tail=stage_extra::ExtraMainTimeline().last(7);
            std::vector<StageEvent> events(tail.begin(),tail.end());events.front().afterEndOf.reset();
            game.stage_=StageDirector(events);game.assets_.PlayStageTrack(1);
            game.Update(1.0F/60);
            assert(game.dialogueActive_==!c.encore&&game.dialogueFlow_.skip==c.encore&&!game.stageClearShown_);
            if(c.encore)assert(!game.dialogueEventId_&&game.assets_.activeMusic_==AssetManager::MusicSlot::Boss);
            std::vector<int> spells;bool before=false,after=false,awarded=false,paused=false,locked=false;
            double beforeStarted=-1,awardedAt=-1;int ticks=0;long long awardedScore=0;std::size_t peak=0;
            for(;ticks<650*120&&game.state_==RunState::Playing;++ticks) {
                const auto priorScore=game.score_;
                game.Update(1.0F/120);
                assert(!game.actors_.DroppedSpawns()&&!game.bullets_.Pool().DroppedSpawns());
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.dialogueActive_&&game.dialogueScene_==DialogueScene::EncoreBoss&&!game.dialogueAfterBattle_) {
                    if(!before) {beforeStarted=game.stage_.Elapsed();assert(c.encore&&game.dialogueEventId_==9);}
                    before=true;
                    if(game.bossEventId_==11) {
                        assert(game.boss_->attack==0&&!game.boss_->CanTakeDamage());
                        const double relative=(game.stage_.Elapsed()-beforeStarted)*60;
                        assert(relative>=1000-0.002); // MSG3在999帧放行，下一源帧创建97。
                    }
                }
                if(game.boss_&&game.boss_->active&&game.boss_->attack>0) {
                    assert(c.encore&&!game.dialogueActive_&&game.bossEventId_==11&&before);
                    const int card=game.boss_->SpellCardNumber();
                    if(card&&(spells.empty()||spells.back()!=card))spells.push_back(card);
                    if(game.boss_->CanTakeDamage()&&game.boss_->skillTimer>8) {
                        const auto* skill=static_cast<bosses::ConfiguredBoss&>(*game.boss_).CurrentSkill();assert(skill);
                        game.boss_->health=skill->reserveHealth>0?skill->reserveHealth-1:0;game.ResolveCollisions();
                    }
                }
                if(game.dialogueActive_&&game.dialogueAfterBattle_) {
                    after=true;assert(game.dialogueScene_==(c.encore?DialogueScene::EncoreBoss:DialogueScene::FinalBoss));
                    if(c.encore)assert(game.boss_&&!game.boss_->active&&!game.bossEventId_);
                }
                if(game.stageClearShown_&&!awarded) {
                    awarded=true;awardedAt=game.dialogueTimer_;awardedScore=game.score_;
                    assert(game.state_==RunState::Playing&&game.dialogueActive_&&game.stageClearSummary_.stage==7);
                    assert(std::abs(awardedAt*60-game.dialogueFlow_.awardFrame)<.002);
                    assert(game.score_-priorScore==game.stageBonus_);
                    game.AwardStageClear();game.AwardStageClear();assert(game.score_==awardedScore);
                    const auto label="extra-postfight-shot"+std::to_string(shot)+"-case"+std::to_string(profile);
                    Capture(game,prefix,label.c_str());
                }
                if(awarded&&game.dialogueActive_&&!locked) {
                    locked=true;const auto clock=game.dialogueTimer_;
                    // 输入锁由原始cue控制；持续快进和确认均不能跨过MSG9之后的360帧。
                    game.UpdateBattleStep(1.0F/120,{},true,true);
                    assert(game.dialogueActive_&&std::abs(game.dialogueTimer_-clock-1.0/120)<1e-6);
                }
                if(awarded&&!paused) {
                    paused=true;const auto clock=game.dialogueTimer_;const auto stageClock=game.stage_.Elapsed();
                    game.TogglePause();game.Update(.5F);
                    assert(game.dialogueTimer_==clock&&game.stage_.Elapsed()==stageClock&&game.score_==awardedScore);
                    game.TogglePause();
                }
                if(awarded)assert(game.score_==awardedScore);
            }
            assert(ticks<650*120&&game.state_==RunState::StageClear&&after&&awarded&&locked&&paused&&!game.dialogueActive_);
            assert(std::abs((game.dialogueTimer_-awardedAt)*60-360)<.002);
            assert(before==c.encore&&spells.size()==(c.encore?3:0));
            assert((peak>0)==c.encore); // 实际发射/子演员经过池与碰撞更新，不能只在首发前跳过全部技能。
            for(std::size_t i=0;i<spells.size();++i)assert(spells[i]==132+static_cast<int>(i));
            if(game.stageSpellCaptures_!=c.captures+(c.encore?3:0))
                std::cerr<<"Extra capture progress mismatch: shot="<<shot<<" case="<<profile<<" captures="<<game.stageSpellCaptures_<<" misses="<<game.missCount_<<"\n";
            assert(game.stageSpellCaptures_==c.captures+(c.encore?3:0));
            game.stageClearSeconds_=2;game.BeginStageClear();assert(game.stageClearSeconds_==2&&game.score_==awardedScore);
            game.ContinueStageClear();for(int n=0;n<120&&game.state_==RunState::Finishing;++n)game.Update(1.0F/120);
            assert(game.state_==RunState::ScoreSave&&!game.boss_&&!EnemyCount(game)&&game.assets_.Stage().visuals.empty()&&!game.stageContent_);
            assert(game.scoreDifficulty_==4&&game.scoreShot_==shot&&game.scoreMode_==int(c.challenge)&&game.pendingScore_.value==awardedScore);
            assert(!game.pendingEnding_&&game.clearedMainlineStages_==0&&game.scoreProfile_.endingsSeen==endings&&game.bestScore_==best);
            const auto beforeSave=game.scoreProfile_;
            const auto position=beforeSave.Position(c.challenge,shot,4,awardedScore);assert(position);
            game.scoreName_="EXTRA";game.SaveNamedScore();assert(game.state_==RunState::Score);
            const auto saved=score::Load(score::kFilename);
            assert(saved.Get(c.challenge,shot,4)[*position]&&saved.Get(c.challenge,shot,4)[*position]->value==awardedScore);
            assert(saved.endingsSeen==endings);
            for(int mode=0;mode<2;++mode)for(int other=0;other<4;++other)for(int difficulty=0;difficulty<5;++difficulty)
                if(mode!=int(c.challenge)||other!=shot||difficulty!=4)for(int slot=0;slot<score::kSlots;++slot) {
                    const auto& a=beforeSave.Get(mode!=0,other,difficulty)[slot];const auto& b=saved.Get(mode!=0,other,difficulty)[slot];
                    assert(a.has_value()==b.has_value());if(a)assert(a->value==b->value&&a->name==b->name&&a->completedAt==b->completedAt);
                }
            assert(game.stageHighScores_.scores==stageScores.scores&&game.stageHighScores_.difficultyScores==stageScores.difficultyScores);
            std::cout<<"Extra postfight shot "<<shot<<" case "<<profile<<" encore="<<c.encore<<" award/360-frame lock/exit passed; peak "<<peak<<" bullets\n";
        }
        game.EnterMainMenu();
    }
    /// @brief 两种字重均须加载真实中英文字形和等宽数字，避免浮窗加粗静默回退或分数宽度跳动。
    static void TestUiFont(const Game& game) {
        const auto& common = game.assets_.Common();
        assert(common.uiFont.texture.id != common.hudFont.texture.id);
        for (const Font font : {common.uiFont, common.hudFont}) {
            assert(font.texture.id != GetFontDefault().texture.id);
            for (int codepoint : std::array<int, 16>{'0', '9', 'Z', 0x4e2d, 0x300c, 0x2605, 0x5e7b, 0x5377,0x5723,0x9489,0x83b1,0x74e6,0x6c40,0x4f0f,0x75d5,0x810f})
                assert(font.glyphs[GetGlyphIndex(font, codepoint)].value == codepoint);
            const float zeroWidth = ui::MeasureText(font, "000000000", 38).x;
            assert(std::abs(zeroWidth - ui::MeasureText(font, "111111111", 38).x) < 0.001F);
            assert(std::abs(zeroWidth - ui::MeasureText(font, "987654321", 38).x) < 0.001F);
        }
    }
    static std::size_t EnemyCount(const Game& game) {
        std::size_t count = 0;
        game.actors_.ForEachActive([&](const behavior::Actor&) { ++count; });
        return count;
    }
    static std::size_t PickupCount(const Game& game) {
        std::size_t count = 0;
        game.drops_.ForEachActive([&](const Pickup&) { ++count; });
        return count;
    }
    /// @brief 可选导出同一测试状态的图片；prefix 为空时只绘制并检查逻辑。
    static void Capture(const Game& game, const std::string& prefix, const char* name) {
        const RenderTexture2D target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
        assert(IsRenderTextureValid(target));
        BeginDrawing();
        BeginTextureMode(target);
        ClearBackground(BLACK);
        game.Draw();
        EndTextureMode();
        EndDrawing();
        if (!prefix.empty()) {
            Image image = LoadImageFromTexture(target.texture);
            assert(IsImageValid(image));
            ImageFlipVertical(&image);
            const std::string path = prefix + "-" + name + ".png";
            assert(ExportImage(image, path.c_str()));
            UnloadImage(image);
        }
        UnloadRenderTexture(target);
    }

    /// @brief 经过正式碰撞总控，检查本体/弹幕同帧只扣一次、无敌和对话门禁、敌方不受碰撞伤害。
    static void TestActorContacts(Game& game) {
        const auto MakeTarget=[](Vector3 p) {
            auto spawn=testing::SpawnAt(p,stage1::kFairy,100,0.5F);spawn.damageable=false;return spawn;
        };
        struct TargetBoss final:Boss {
            explicit TargetBoss(Vector3 p){active=true;position=p;health=maxHealth=100;visual=stage1::kRumia;}
            bool Update(float,Vector3,BulletSystem&) override {return false;}
            bool AdvanceSkill() override {return false;}
            const char* SpellName() const override {return nullptr;}
            const char* DisplayName() const override {return "Body contact target";}
            bool CanTakeDamage() const override {return false;}
        };
        game.StartPreview();game.stage_=StageDirector{};
        const auto reset=[&](int bombs=3,bool challenge=false) {
            game.ClearPlayerBomb();game.actors_.Clear();game.boss_.reset();game.bullets_.Reset();
            game.actorEffects_.Clear();game.drops_.Clear();game.dialogueActive_=false;game.state_=RunState::Playing;
            game.player_.Reset();game.player_.ConfigureRun(3,bombs,challenge,ShotType::HomingAmulet);
            game.player_.SetInvulnerability(0);game.player_.AdjustPower(72);game.player_.SyncCamera(game.camera_);
            game.missCount_=game.graze_=0;game.score_=0;
#ifdef KOUMAKYOU_DEBUG_FEATURES
            game.debugInvincible_=false;
#endif
        };
        reset();const auto position=game.player_.Position();
        assert(game.actors_.Spawn(MakeTarget(position)));
        assert(game.actors_.Spawn(MakeTarget(position)));
        game.boss_=std::make_unique<TargetBoss>(position);
        auto* bullet=game.bullets_.Spawn(BulletOwner::Enemy,position,{},0.1F,RED,stage1::kBlueBullet);assert(bullet);
        game.ResolveCollisions();
        assert(game.missCount_==1 && game.player_.RemainingLives()==3 && game.player_.Power()==64);
        assert(game.score_==0 && game.graze_==0 && PickupCount(game)==0 && game.actorEffects_.Pool().ActiveCount()==0);
        game.actors_.ForEachActive([](const behavior::Actor& enemy){assert(enemy.health==100);});
        assert(EnemyCount(game)==2 && game.boss_->health==100 && !bullet->active);
        game.ResolveCollisions();assert(game.missCount_==1); // 重叠不绕过复活无敌。
        game.player_.SetInvulnerability(0);game.ResolveCollisions();assert(game.missCount_==2);

        reset();game.boss_=std::make_unique<TargetBoss>(position);game.ResolveCollisions();
        assert(game.missCount_==1); // Boss 单独接触也必须生效。
        reset();assert(game.actors_.Spawn(MakeTarget(position)));
        game.dialogueActive_=true;game.ResolveCollisions();assert(game.missCount_==0);
        game.dialogueActive_=false;game.state_=RunState::Paused;game.ResolveCollisions();assert(game.missCount_==0);
#ifdef KOUMAKYOU_DEBUG_FEATURES
        reset();assert(game.actors_.Spawn(MakeTarget(position)));
        game.debugInvincible_=true;game.ResolveCollisions();assert(game.missCount_==0 && game.player_.Power()==80);
#endif
        reset();game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),3);game.boss_->position=position;
        assert(game.boss_->AdvanceSkill());game.ResolveCollisions();assert(game.missCount_==0);
        // 四面关闭本体接触的晶体仍吸收自机弹；隐藏/非交互发射体则同时退出接触、伤害和弹接触。
        for(int mode=0;mode<3;++mode) {
            reset();auto* actor=game.actors_.Spawn(MakeTarget(position));assert(actor);
            actor->contactEnabled=false;
            if(mode==1)actor->visible=false;
            if(mode==2)actor->interactable=false;
            auto* shot=game.bullets_.Spawn(BulletOwner::Player,position,{},0.1F,WHITE,stage1::kPlayerGoldCard);assert(shot);
            game.ResolveCollisions();assert(game.missCount_==0&&actor->health==100);
            assert(shot->active==(mode!=0));
            if(mode==1) {game.ClearActorsWithEffects();assert(game.actorEffects_.Pool().ActiveCount()==0);}
        }
        // 经过真实碰撞/Miss结算恢复本局Bomb设置，包括零库存及高于设置值的拾取库存。
        for(bool challenge:{false,true})for(int stock=0;stock<=3;++stock) {
            reset(stock,challenge);game.player_.CollectResources(0,8);
            auto* hit=game.bullets_.Spawn(BulletOwner::Enemy,game.player_.Position(),{},.1F,RED,stage1::kBlueBullet);assert(hit);
            game.ResolveCollisions();
            assert(game.missCount_==1&&game.player_.Bombs()==stock);
            assert(game.player_.RemainingLives()==(challenge?4:3));
        }
        game.Reset();std::cout<<"Player/enemy/Boss body contacts, configured respawn Bombs and protection gates passed\n";
    }

    /// @brief 独立绘制世界，检查四边没有留黑，左右边缘随镜头变化，防止旧侧栏或裁切恢复。
    static void TestFullWindow(Game& game, const std::string& prefix) {
        game.StartPreview();
        const int originalWidth=GetScreenWidth(), originalHeight=GetScreenHeight();
        for(const Vector2 size : {Vector2{1100,760},{1280,720},{960,720},{1680,720},{390,844}}) {
            SetWindowSize(static_cast<int>(size.x),static_cast<int>(size.y));
            const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
            const auto render=[&](float yaw) {
                game.SetPreviewView(yaw,0);
                BeginDrawing(); BeginTextureMode(target);
                ClearBackground(Color{255,0,255,255});
                game.DrawWorld();
                EndTextureMode(); EndDrawing();
                Image image=LoadImageFromTexture(target.texture);
                assert(IsImageValid(image));
                ImageFlipVertical(&image);
                return image;
            };
            Image first=render(0), turned=render(35);
            assert(game.camera_.fovy==72);
            for(const Vector2 point : {Vector2{1,1},{size.x-2,1},{1,size.y-2},{size.x-2,size.y-2}}) {
                const Color pixel=GetImageColor(first,static_cast<int>(point.x),static_cast<int>(point.y));
                assert(!(pixel.r==255 && pixel.g==0 && pixel.b==255));
            }
            for(const int x : {2,static_cast<int>(size.x)-3}) {
                int changed=0, samples=0;
                for(int y=2;y<static_cast<int>(size.y)-2;y+=7) {
                    const Color a=GetImageColor(first,x,y), b=GetImageColor(turned,x,y);
                    if(std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b)>6)++changed;
                    ++samples;
                }
                assert(changed>samples/10);
            }
            UnloadImage(first); UnloadImage(turned); UnloadRenderTexture(target);
            game.SetPreviewView(0,0);
            const std::string name="full-window-"+std::to_string(static_cast<int>(size.x))+"x"+std::to_string(static_cast<int>(size.y));
            Capture(game,prefix,name.c_str());
        }
        SetWindowSize(originalWidth,originalHeight);
        // 历史全窗口计时仅显式启用时执行，默认功能检查不运行。
        if(std::getenv("KOUMAKYOU_RUN_PERF_TESTS") && std::string(std::getenv("KOUMAKYOU_RUN_PERF_TESTS"))=="1"){
        // 实际提交全窗口世界与 HUD；只报告耗时，不用依赖机器速度的阈值卡测试。
        for(int frame=0;frame<10;++frame) { BeginDrawing(); game.Draw(); EndDrawing(); }
        const double start=GetTime();
        for(int frame=0;frame<60;++frame) { BeginDrawing(); game.Draw(); EndDrawing(); }
        std::cout<<"Full-window world and HUD: "<<(GetTime()-start)*1000/60<<" ms/frame\n";
        }
    }

    /// @brief 按正式更新顺序以玩家弹击破或等超时，验证 30～240 FPS 各脚本帧间隙均能保留第二组非符。
    static void TestBossTransitions(Game& game, const std::string& prefix) {
        game.StartPreview();
        game.stage_=StageDirector{};
        game.player_.SetInvulnerability(1000);
        for(int fps : {30,60,120,240}) for(bool timeout : {false,true}) {
            // 在不同显示帧间隙送入伤害，最多等待一个 120 Hz 战斗步；准备期仍须立即保护。
            for(int alignment=0;alignment<std::max(1,fps/60);++alignment) {
                const float dt=1.0F/fps;
                game.bullets_.Reset(); game.drops_.Clear();
                game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),1);
                const auto ready=[&](int phase) {
                    int frames=0;
                    while(!game.boss_->CanTakeDamage() && frames++<5*fps) {
                        game.Update(dt);
                        assert(game.boss_->attack==phase);
                    }
                    assert(game.boss_->attack==phase && game.boss_->CanTakeDamage());
                };
                const auto hit=[&] {
                    auto* shot=game.bullets_.Spawn(BulletOwner::Player,game.boss_->position,{},0.1F,WHITE,kInvalidVisualId);
                    assert(shot); shot->damage=100000;
                    const int phase=game.boss_->attack;
                    for(int frame=0;frame<std::max(1,(fps+119)/120)&&game.boss_->attack==phase;++frame)
                        game.Update(dt); // 经过实际固定步、弹幕移动与碰撞，不能直接推进阶段。
                };
                ready(1); hit();
                assert(game.boss_->attack==2);
                ready(2);
                for(int frame=0;frame<alignment;++frame)game.Update(dt);
                if(timeout) {
                    int frames=0;
                    while(game.boss_->attack==2 && frames++<30*fps)game.Update(dt);
                } else hit();
                assert(game.boss_->attack==3 && game.boss_->SpellName()==nullptr);
                assert(game.boss_->health==7500);
                assert(game.boss_->maxHealth==7500);
                const auto bar=game.boss_->HealthBar();
                assert(bar.remainingBars==0 && std::abs(bar.segments[1].remaining-6700.0F/7500)<0.00001F);
                for(int frame=0;frame<std::max(1,fps/60);++frame) {
                    game.Update(dt);
                    assert(game.boss_->attack==3 && game.boss_->SpellName()==nullptr);
                }
                ready(3);
                for(int frame=0;frame<static_cast<int>(1.1F*fps);++frame) {
                    game.Update(dt);
                    assert(game.boss_->attack==3);
                }
                int beams=0;
                game.bullets_.Pool().ForEachActive([&](const Bullet& bullet) { if(bullet.beamLength>0)++beams; });
                assert(beams==6 && game.boss_->TimeLimit()==30);
                if(!timeout && fps==240 && alignment==3)Capture(game,prefix,"second-nonspell-restored");
                hit();
                assert(game.boss_->attack==4 && game.boss_->SpellName()!=nullptr);
                assert(std::abs(game.boss_->HealthBar().segments[0].remaining-800.0F/7500)<0.00001F);
            }
        }
        std::cout<<"Boss damage/timeout transitions and second nonspell beams passed at 30/60/120/240 FPS\n";
    }

    /// @brief 通过正式 HUD 读取分段颜色与空白像素，覆盖四个攻击阶段、道中及多种窗口比例。
    static void TestBossHud(Game& game, const std::string& prefix) {
        game.StartPreview();
        const int width=GetScreenWidth(),height=GetScreenHeight();
        struct Scene { int phase; float sourceHealth; const char* name; bool configured=false; };
        const std::array<Scene,11> scenes{{
            {1,7000,"first-full"},{1,3950,"first-normal-half"},{1,900,"first-boundary"},
            {2,900,"first-spell-full"},{2,450,"first-spell-half"},{2,0,"first-spell-empty"},
            {3,7500,"second-full"},{3,4150,"second-normal-half"},
            {4,400,"second-spell-half"},{0,6000,"midboss-full"},{1,3950,"custom-ratios-colors",true}}};
        for(const Vector2 size : {Vector2{1280,720},{900,900},{390,844},{1600,400},{320,240}}) {
            SetWindowSize(static_cast<int>(size.x),static_cast<int>(size.y));
            const GameplayLayout layout(GetScreenWidth(),GetScreenHeight());
            const ui::BossHudLayout row(layout.BossPanel(),layout.scale);
            const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
            for(const auto& scene : scenes) {
                game.BeginStageBoss(testing::BattleAt(*game.stageContent_,static_cast<std::size_t>(scene.phase!=0)),1);
                if(scene.configured) {
                    auto config=stage1::RumiaFinalConfig();
                    config.phases[0].healthBar.segments={{0,0.3F,GREEN},{1,0.7F,BLUE}};
                    config.phases[0].healthBar.background=BLACK;
                    dynamic_cast<bosses::RumiaBoss&>(*game.boss_).StartBoss(config);
                }
                while(game.boss_->attack<scene.phase)assert(game.boss_->AdvanceSkill());
                game.UpdateBoss(1.0F/60);
                game.bullets_.Reset(); game.drops_.Clear(); // 隔离 Sub19 的散落，截图不能累积上一个展示场景的对象。
                game.boss_->skillTimer=5;
                game.boss_->health=scene.sourceHealth;
                const auto health=game.boss_->HealthBar();
                BeginDrawing(); BeginTextureMode(target); ClearBackground(Color{17,31,47,255});
                game.DrawHud();
                EndTextureMode(); EndDrawing();
                Image pixels=LoadImageFromTexture(target.texture); ImageFlipVertical(&pixels);
                const auto near=[](Color a,Color b) {
                    return std::abs(int(a.r)-b.r)<=2 && std::abs(int(a.g)-b.g)<=2 && std::abs(int(a.b)-b.b)<=2;
                };
                float offset=0;
                for(std::size_t index=0;index<health.count;++index) {
                    const auto segment=health.segments[index];
                    for(float fraction : {0.25F,0.75F}) {
                        const float sample=segment.capacity*fraction;
                        const int x=static_cast<int>(row.bar.x+(offset+sample)*row.bar.width);
                        const int y=static_cast<int>(row.bar.y+row.bar.height/2);
                        const auto color=GetImageColor(pixels,x,y);
                        if(sample<segment.remaining) {
                            const Color expected=segment.color;
                            assert(near(color,expected));
                        } else {
                            assert(!near(color,WHITE) && !near(color,Color{239,138,152,255}));
                        }
                    }
                    offset+=segment.capacity;
                }
                UnloadImage(pixels);
                const auto name="boss-hud-"+std::to_string(GetScreenWidth())+"-"+scene.name;
                Capture(game,prefix,name.c_str());
            }
            UnloadRenderTexture(target);
        }
        SetWindowSize(width,height);
        std::cout<<"Boss HUD segmented colors and same-row layout passed at five window sizes\n";
    }

    /// @brief 用纯色纹理读取边缘像素验证透明混合、暂停冻结，并用正式立绘导出切换过渡。
    static void TestDialogueTransitions(Game& game, const std::string& prefix) {
        game.StartPreview();
        game.BeginStageDialogue(1);
        const auto lines=game.CurrentDialogue();
        const auto at=[&](int index) {
            float seconds=1.1F;
            for(int line=0;line<index;++line)
                seconds+=lines[line].duration>0 ? lines[line].duration : game.stageContent_->DialogueLineDuration();
            return seconds;
        };
        // 用不透明纯色替代纹理，直接验证最终像素混合，防止把变暗 tint 误当作原生 Alpha 128。
        auto& visuals=const_cast<StageAssets&>(game.assets_.Stage()).visuals;
        std::vector<std::pair<VisualId,VisualResource>> originals;
        Image redImage=GenImageColor(32,32,RED), greenImage=GenImageColor(32,32,GREEN);
        const Texture2D red=LoadTextureFromImage(redImage), green=LoadTextureFromImage(greenImage);
        UnloadImage(redImage); UnloadImage(greenImage);
        for(int index=0;index<10;++index) {
            const auto id=index<6 ? game.stageContent_->HeroPortraitVisual(index)
                : game.stageContent_->BossPortraitVisual(index-6);
            originals.emplace_back(id,visuals[id]);
            visuals[id].texture=index<6 ? red : green;
            visuals[id].dialogueCrop={0,0,1,1};
        }
        const RenderTexture2D target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
        constexpr Color background{17,31,47,255};
        const auto pixels=[&](float leftAlpha,float rightAlpha) {
            BeginDrawing(); BeginTextureMode(target); ClearBackground(background);
            game.DrawDialogue();
            EndTextureMode(); EndDrawing();
            Image image=LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
            const auto check=[&](int x,Color ink,float alpha) {
                // raylib 将浮点 Alpha 量化为 8 位，允许最多两级的 GPU 舍入误差。
                const float a=static_cast<unsigned char>(alpha*255)/255.0F;
                const Color pixel=GetImageColor(image,x,image.height*3/5);
                assert(std::abs(int(pixel.r)-int(std::round(ink.r*a+background.r*(1-a))))<=2);
                assert(std::abs(int(pixel.g)-int(std::round(ink.g*a+background.g*(1-a))))<=2);
                assert(std::abs(int(pixel.b)-int(std::round(ink.b*a+background.b*(1-a))))<=2);
            };
            check(0,RED,leftAlpha); check(image.width-1,GREEN,rightAlpha);
            UnloadImage(image);
        };
        constexpr float inactive=128.0F/255;
        const float turn=2648.0F/60;
        game.dialogueTimer_=turn-1.0F/60; game.UpdateUiMotion(0);
        pixels(1,inactive);
        game.dialogueTimer_=turn; game.UpdateUiMotion(0);
        pixels(1,inactive);
        game.dialogueTimer_=turn+8.0F/60;game.UpdateUiMotion(0);
        pixels(191.0F/255,191.0F/255);
        const auto paused=game.dialogueMotion_.Sample(false);
        game.TogglePause(); game.Update(0.5F);
        assert(game.dialogueMotion_.Sample(false).opacity==paused.opacity);
        assert(game.dialogueMotion_.Sample(false).focus==paused.focus);
        game.TogglePause();game.dialogueTimer_=turn+16.0F/60;game.UpdateUiMotion(0);
        pixels(inactive,1);
        for(const auto& [id,original]:originals)visuals[id]=original;
        UnloadTexture(red); UnloadTexture(green); UnloadRenderTexture(target);

        // 正式差分逐帧审阅两个方向的过渡，保持其他世界状态不动；Draw 不推进动画。
        game.dialogueTimer_=at(8); game.UpdateUiMotion(0.3F);
        Capture(game,prefix,"dialogue-focus-left");
        for(int direction=0;direction<2;++direction) {
            const float boundary=(direction==0?2648.0F:2949.0F)/60;
            for(int frame=0;frame<8;++frame) {
                game.dialogueTimer_=boundary+frame*0.04F;game.UpdateUiMotion(0);
                const std::string name="dialogue-turn-"+std::to_string(direction)+"-"+std::to_string(frame);
                Capture(game,prefix,name.c_str());
            }
        }
        game.BeginStageDialogue(2,true);
        assert(game.dialogueMotion_.Sample(false).slide==1 && game.dialogueMotion_.Sample(true).opacity==0);
        game.dialogueTimer_=0.5F;game.UpdateUiMotion(0);
        assert(game.dialogueMotion_.Sample(false).opacity==1 && game.dialogueMotion_.Sample(true).opacity==0);
        std::cout<<"Dialogue transition opacity, pause and edge tests passed\n";
    }

    /// @brief 逐句核对两位自机战前/战后真实差分、音乐触发和末尾退场，截图覆盖横屏与竖屏。
    static void TestNativeDialogue(Game& game,const std::string& prefix) {
        const int width=GetScreenWidth(),height=GetScreenHeight();
        for(int character=0;character<2;++character) {
            game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            game.stage_=StageDirector{};game.actors_.Clear();game.boss_.reset();game.drops_.Clear();
            for(bool after:{false,true}) {
                game.BeginStageDialogue(1,after);
                const auto lines=game.CurrentDialogue();
                float start=0;
                for(std::size_t index=0;index<lines.size();++index) {
                    game.dialogueTimer_=start+std::max(1.1F,lines[index].textDelay+lines[index].line2Delay+0.1F);
                    game.UpdateUiMotion(0);
                    const auto expressions=DialogueExpressionsAt(lines,index,game.dialogueTimer_-start);
                    const auto hero=game.stageContent_->HeroPortraitVisual(expressions[0],game.player_.Shot());
                    assert(IsTextureValid(game.assets_.GetVisual(hero).texture));
                    assert(!lines[index].showBoss || game.dialogueMotion_.Sample(true).opacity>0);
                    for(const auto size:std::array<Vector2,2>{{{1100,760},{390,844}}}) {
                        SetWindowSize(size.x,size.y);
                        const auto name="native-dialogue-"+std::to_string(character)+(after?"-after-":"-before-")+
                            std::to_string(index)+"-"+std::to_string(static_cast<int>(size.x));
                        Capture(game,prefix,name.c_str());
                    }
                    start+=lines[index].duration;
                }
                if(!after) {
                    const float musicFrame=character==0?3852.0F:2467.0F;
                    game.BeginStageDialogue(1);game.dialogueTimer_=(musicFrame-1)/60;game.Update(1.0F/120);
                    assert(!game.dialogueBossMusic_);
                    game.dialogueTimer_=musicFrame/60;game.Update(1.0F/120);assert(game.dialogueBossMusic_);
                    game.dialogueTimer_=game.DialogueDuration()-1.0F/60;game.UpdateUiMotion(0);
                    assert(game.dialogueMotion_.Sample(false).slide>0.99F && game.dialogueMotion_.Sample(true).slide>0.99F);
                }
                game.dialogueActive_=false;
            }
        }
        SetWindowSize(width,height);game.selectedCharacter_=0;game.selectedShot_=0;game.StartPreview();
        std::cout<<"Both native Stage 1 dialogue branches, portraits, BGM and exits passed\n";
    }

    /// @brief 在真实伤害、清弹、死亡掉落和 Boss 保护入口验证 Bomb，避免仅凭画面通过。
    static void TestPlayerBombCombat(Game& game) {
        // 固定靶借用本关外观；不移动或发弹，允许单独控制无敌和原始生命。
        const auto MakeTarget=[](Vector3 p,float hp) {
            return testing::SpawnAt(p,stage1::kFairy,hp,0.4F);
        };
        struct SpellTarget final:Boss {
            bool damageable=true;
            SpellTarget() {active=true;position={0,0,4};health=maxHealth=1000;visual=stage1::kRumia;}
            bool Update(float,Vector3,BulletSystem&) override {return false;}
            bool AdvanceSkill() override {return false;}
            const char* SpellName() const override {return "Bomb test";}
            const char* DisplayName() const override {return "Bomb test";}
            bool CanTakeDamage() const override {return damageable;}
        };
        const auto reset=[&](ShotType shot) {
            game.ClearPlayerBomb();game.stage_=StageDirector{};game.actors_.Clear();game.boss_.reset();
            game.bullets_.Reset();game.drops_.Clear();game.actorEffects_.Clear();
            game.player_.Reset();game.player_.ConfigureRun(3,3,false,shot);game.player_.SyncCamera(game.camera_);
        };
        reset(ShotType::Needle);
        auto* untouched=game.actors_.Spawn(MakeTarget({9,4,4},1000));
        auto* touched=game.actors_.Spawn(MakeTarget({0,0,-6.2F},1000));assert(untouched&&touched);
        game.ApplyBombEffect();
        assert(game.screenFlashTimer_==0 && untouched->health==1000 && touched->health==1000);
        game.UpdatePlayerBomb(1);
        assert(game.playerBomb_.Frame()==60 && untouched->health==1000 && touched->health==1000);
        game.UpdatePlayerBomb(1.0F/60);
        assert(untouched->health==1000 && touched->health<1000); // 先接触施放中心附近，不能倒着从边界清怪。
        const float firstHit=touched->health;
        game.UpdatePlayerBomb(1.0F/60);assert(untouched->health==1000 && touched->health==firstHit);
        game.UpdatePlayerBomb(77.0F/60);assert(untouched->health<1000);

        reset(ShotType::IllusionLaser);
        auto* hit=game.actors_.Spawn(MakeTarget({0,0,4},24));
        auto* miss=game.actors_.Spawn(MakeTarget({0,8,-9},1000));
        auto* covered=game.actors_.Spawn(MakeTarget({9,9,-6.25F},1000));
        auto* safe=game.actors_.Spawn(MakeTarget({0,0,4},1000));
        assert(hit&&miss&&covered&&safe);safe->damageable=false;
        auto* enemy=game.bullets_.Spawn(BulletOwner::Enemy,{0,0,4},{},0.1F,RED,stage1::kBlueBullet);
        auto* back=game.bullets_.Spawn(BulletOwner::Enemy,{0,0,-9},{},0.1F,RED,stage1::kBlueBullet);
        auto* friendly=game.bullets_.Spawn(BulletOwner::Player,{0,0,4},{},0.1F,WHITE,stage1::kPlayerGoldCard);
        auto* beam=game.bullets_.SpawnBeam({0,-5,4},{0,1,0},10,0.1F,0,5,RED);
        assert(enemy && back && friendly && beam);friendly->damage=0;
        game.drops_.SpawnFixedPower({-8,8,2},1);game.ApplyBombEffect();
        assert(hit->health==24 && enemy->active && back->active && beam->active); // 释放时没有全场扣血/清弹。
        game.drops_.ForEachActive([](const Pickup& item) {assert(item.homing);});
        game.UpdatePlayerBomb(1.0F/60);
        assert(hit->health==12 && covered->health==988 && miss->health==1000 && safe->health==1000);
        assert(!enemy->active && !beam->active && back->active && friendly->active);
        game.ResolvePlayerBombFrame();assert(hit->health==12);
        game.TogglePause();game.Update(1);assert(hit->health==12 && game.playerBomb_.Frame()==1);game.TogglePause();
        game.UpdatePlayerBomb(1.0F/60);assert(hit->health==0);
        game.ResolveCollisions();assert(!hit->active && PickupCount(game)==2 && game.actorEffects_.Pool().ActiveCount()==1);
        game.ResolveCollisions();assert(PickupCount(game)==2);

        reset(ShotType::HomingAmulet);
        auto* orbTarget=game.actors_.Spawn(MakeTarget({0,0,4},3000));assert(orbTarget);orbTarget->hitRadius=20;
        game.ApplyBombEffect();
        game.UpdatePlayerBomb(93.0F/60);
        assert(game.playerBomb_.Frame()==93 && orbTarget->health==2826); // 13×8 接触，随后一帧 200 截到 70。
        assert(game.playerBomb_.Attacks().size()==1 && game.playerBomb_.Attacks()[0].damage==200);
        game.UpdatePlayerBomb(1.0F/60);assert(orbTarget->health==2826 && game.playerBomb_.Attacks().empty());

        reset(ShotType::IllusionLaser);
        auto boss=std::make_unique<SpellTarget>();auto* spell=boss.get();game.boss_=std::move(boss);
        game.spellStartBombs_=game.bombCount_;game.ApplyBombEffect();game.UpdatePlayerBomb(1.0F/60);
        assert(spell->health==996);
        spell->damageable=false;game.UpdatePlayerBomb(1.0F/60);assert(spell->health==996);
        spell->damageable=true;game.spellStartBombs_=game.bombCount_; // 模拟新符卡开始，旧 Bomb 不得带入伤害。
        game.UpdatePlayerBomb(1.0F/60);assert(spell->health==996);
        game.ApplyBombEffect();game.UpdatePlayerBomb(1.0F/60);assert(spell->health==992);

        // 每帧重新生成敌弹及两端在外的横穿激光，验证后续来弹也会清除；不能只在释放时清一次。
        // 使用独立帧条件核对数量，跳过伤害帧时不得沿用上一帧残留实体。
        for(int type=0;type<4;++type) {
            const auto shot=static_cast<ShotType>(type);reset(shot);game.ApplyBombEffect();
            for(int frame=0;frame<BombDefinition(shot).durationFrames;++frame) {
                auto next=game.playerBomb_;
                if(frame>0)next.Update(1.0F/60,game.camera_);
                const std::size_t expected=type==0 ? (frame==0 ? 1 : frame<80 || frame>270 ? 0 : std::min(7,(frame-80)/16+1)) :
                    type==1 ? (frame>60 ? 6 : 0) : type==2 ? (frame>0 && frame%3!=0 ? 64 : 0) :
                    (frame>0 && frame%4!=0 ? 1 : 0);
                assert(next.CancelVolumes().size()==expected);
                const auto center=expected ? next.CancelVolumes()[0].origin : game.camera_.position;
                game.bullets_.Reset();
                auto* incoming=game.bullets_.Spawn(BulletOwner::Enemy,center,{},0.1F,RED,stage1::kBlueBullet);
                auto* playerShot=game.bullets_.Spawn(BulletOwner::Player,center,{},0.1F,WHITE,stage1::kPlayerGoldCard);
                auto* crossing=game.bullets_.SpawnBeam({center.x-50,center.y,center.z},{1,0,0},100,0.1F,0,5,RED);
                assert(incoming && playerShot && crossing);
                game.UpdatePlayerBomb(frame==0 ? 0 : 1.0F/60);
                assert(incoming->active==(expected==0) && crossing->active==(expected==0) && playerShot->active);
            }
            game.UpdatePlayerBomb(1.0F/60);
            assert(!game.playerBomb_.Active() && game.playerBomb_.CancelVolumes().empty());
        }
        reset(ShotType::HomingAmulet);
        std::cout<<"Bomb spatial damage, channel union, cancellation, defeat drops, homing burst, spell protection and pause passed\n";
    }

    /// @brief 使用真实 Game 入口绑定四 Bomb；隔离特效像素，验证主/侧视角、暂停及资源卸载。
    static void TestPlayerBombs(Game& game,const std::string& prefix) {
        const int width=GetScreenWidth(),height=GetScreenHeight();
        game.StartPreview();
        TestPlayerBombCombat(game);
        for(int type=0;type<4;++type) {
            SetWindowSize(width,height);
            game.stage_=StageDirector{};game.actors_.Clear();game.boss_.reset();game.bullets_.Reset();
            game.player_.Reset();game.player_.ConfigureRun(3,3,false,static_cast<ShotType>(type));
            game.player_.SyncCamera(game.camera_);
            const bool consumed=game.player_.Update(0,{.bomb=true},game.bullets_,game.stageContent_->PlayerVisuals(game.player_.Shot()),true);
            assert(consumed && game.player_.Bombs()==2);game.ApplyBombEffect();
            assert(game.playerBomb_.Active() && game.playerBomb_.Shot()==game.player_.Shot() && game.playerBomb_.Frame()==0);
            const auto visuals=game.stageContent_->PlayerBombVisuals(game.player_.Shot());
            const auto& definition=BombDefinition(game.player_.Shot());
            for(int layer=0;layer<definition.layers;++layer) {
                const auto& visual=game.assets_.GetVisual(visuals[layer]);
                assert(visual.archive && IsTextureValid(visual.texture));
                assert(visual.animation.script==definition.firstScript+layer);
            }
            for(int frame=1;frame<=120;++frame) {
                game.Update(1.0F/60);
                if(!prefix.empty() && (type==1 || type==3) && (frame==30 || frame==61 || frame==90))
                    Capture(game,prefix,("bomb-"+std::to_string(type)+"-frame-"+std::to_string(frame)).c_str());
            }
            assert(game.playerBomb_.Frame()==120);
            const auto target=LoadRenderTexture(1100,760);
            BeginDrawing();BeginTextureMode(target);ClearBackground(WHITE);BeginMode3D(game.camera_);
            const auto beforeView=rlGetMatrixModelview(),beforeProjection=rlGetMatrixProjection();
            game.DrawPlayerBombBackdrop(1100,760);
            const auto afterView=rlGetMatrixModelview(),afterProjection=rlGetMatrixProjection();
            assert(std::memcmp(&beforeView,&afterView,sizeof(Matrix))==0 && std::memcmp(&beforeProjection,&afterProjection,sizeof(Matrix))==0);
            EndMode3D();EndTextureMode();EndDrawing();
            Image dimmed=LoadImageFromTexture(target.texture);
            for(const auto point:std::array<Vector2,3>{{{0,0},{550,380},{1099,759}}}) {
                const auto pixel=GetImageColor(dimmed,point.x,point.y);
                assert(std::abs(int(pixel.r)-(255-game.playerBomb_.BackdropAlpha()))<=2);
            }
            UnloadImage(dimmed);
            std::vector<BombVolumeSprite> volumes;
            for(const auto& sprite:game.playerBomb_.Sprites())volumes.push_back({&sprite,visuals[sprite.layer]});
            BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);BeginMode3D(game.camera_);
            assert(game.assets_.Projectiles());
            game.combatRenderer_.Prepare(game.assets_.Projectiles(),game.bullets_,game.assets_.Stage().visuals,game.camera_,game.player_.Position(),1100.F/760);
            game.spriteRenderer_.Draw(game.camera_,game.player_.Position(),game.bullets_,{},nullptr,{},game.assets_.Stage().visuals,
                                      game.assets_.Stage().shaders,game.actorEffects_,volumes,&game.externalEffects_,&game.combatRenderer_);
            EndMode3D();EndTextureMode();EndDrawing();
            Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
            const auto* pixels=static_cast<const Color*>(image.data);int ink=0,bright=0;
            for(int i=0;i<image.width*image.height;++i) {
                const int sum=pixels[i].r+pixels[i].g+pixels[i].b;
                if(sum>24)++ink;if(sum>500)++bright;
            }
            std::cout<<"Bomb "<<type<<": "<<game.playerBomb_.Sprites().size()<<" visual layers, "<<ink<<" isolated pixels\n";
            assert(ink>1000); // 不能让背景压暗或宣告文字冒充 Bomb 本体已经渲染。
            if(type==0)assert(bright<image.width*image.height/2); // 近眼盘旋不能把整屏染亮。
            if(type==3) {
                // 模型炮口位于视点前下方，允许束芯进入准星，但不能把整个瞄准区域曝成白色圆盘。
                int clippedWhite=0;
                for(int x=image.width/2-30;x<=image.width/2+30;++x)
                    for(int y=image.height/2-30;y<=image.height/2+30;++y) {
                        const auto center=GetImageColor(image,x,y);
                        if(center.r>235&&center.g>235&&center.b>235)++clippedWhite;
                    }
                assert(clippedWhite<61*61/2);
            }
            if(!prefix.empty())assert(ExportImage(image,(prefix+"-bomb-"+std::to_string(type)+"-isolated.png").c_str()));
            UnloadImage(image);UnloadRenderTexture(target);
            const auto originalCamera=game.camera_;
            game.camera_=type==3 ? Camera3D{{45,35,-25},{0,0,6},{0,1,0},50,CAMERA_PERSPECTIVE} :
                                 Camera3D{{18,12,-2},{0,0,5},{0,1,0},50,CAMERA_PERSPECTIVE};
            Capture(game,prefix,("bomb-"+std::to_string(type)+"-side").c_str());
            game.camera_=originalCamera;
            // 小于 1000 面模型只做功能检查；历史计时段须显式启用，普通验收不执行。
            if(type==3 && std::getenv("KOUMAKYOU_RUN_PERF_TESTS") && std::string(std::getenv("KOUMAKYOU_RUN_PERF_TESTS"))=="1") {
                const auto begin=std::chrono::steady_clock::now();
                for(int frame=0;frame<24;++frame) {BeginDrawing();game.Draw();EndDrawing();}
                const auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/24;
                std::cout<<"Master Spark full scene render: "<<milliseconds<<" ms/frame (24 frames, no image readback)\n";
                const auto saved=game.playerBomb_;game.playerBomb_.Clear();
                const auto baseline=std::chrono::steady_clock::now();
                for(int frame=0;frame<24;++frame) {BeginDrawing();game.Draw();EndDrawing();}
                std::cout<<"Baseline full scene render: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-baseline).count()/24<<" ms/frame\n";
                game.playerBomb_=saved;
            }
            for(const auto size:std::array<Vector2,2>{{{1100,760},{390,844}}}) {
                SetWindowSize(size.x,size.y);
                const auto name="bomb-"+std::to_string(type)+"-"+std::to_string(static_cast<int>(size.x));
                Capture(game,prefix,name.c_str());
            }
            const auto frozen=game.playerBomb_.Frame();
            game.TogglePause();game.Update(0.5F);assert(game.playerBomb_.Frame()==frozen);
            BeginDrawing();game.Draw();game.Draw();EndDrawing();assert(game.playerBomb_.Frame()==frozen);
            game.TogglePause();
            game.playerBomb_.Update((definition.durationFrames-frozen)/60.0F,game.camera_);
            assert(!game.playerBomb_.Active() && game.playerBomb_.Sprites().empty());
            game.ApplyBombEffect();assert(game.playerBomb_.Active());
            game.EndStage(RunState::Won);assert(!game.playerBomb_.Active());
            game.state_=RunState::Playing;
        }
        game.ApplyBombEffect();assert(game.playerBomb_.Active());
        game.EnterMainMenu();assert(!game.playerBomb_.Active() && game.playerBomb_.Sprites().empty());
        SetWindowSize(width,height);
        std::cout<<"Four native Bomb effects, isolated rendering, portrait/landscape, pause and cleanup passed\n";
    }

    /// @brief 静音加载真实 Master Spark 波形，检查暂停、恢复和退出不会遗留五秒长音效。
    static void TestPlayerBombAudio(Game& game) {
        assert(IsAudioDeviceReady());game.StartPreview();
        game.player_.ConfigureRun(3,3,false,ShotType::IllusionLaser);game.ApplyBombEffect();
        assert(game.playerBomb_.Active());
        game.TogglePause();assert(game.playerBombSoundPaused_[3]);
        assert(!game.assets_.PauseSoundEffect("bomb_master"));
        game.TogglePause();assert(!game.playerBombSoundPaused_[3]);
        assert(game.assets_.PauseSoundEffect("bomb_master"));game.assets_.ResumeSoundEffect("bomb_master");
        game.EnterMainMenu();
        assert(!game.playerBomb_.Active() && !game.assets_.PauseSoundEffect("bomb_master"));
        for(bool paused:game.playerBombSoundPaused_)assert(!paused);
        std::cout<<"Native Master Spark WAV pause, resume and exit cancellation passed (muted)\n";
    }

    /// @brief 静音加载新增短音效，经过真实拾取/战斗更新检查触发、暂停及非符/退场门禁。
    static void TestCombatAudio(Game& game) {
        struct AudioBoss final:Boss {
            bool spell=true,finish=false;float limit=20;
            AudioBoss(){active=true;attack=phase=skill=1;health=maxHealth=100;visual=stage1::kRumia;}
            bool Update(float seconds,Vector3,BulletSystem&) override {skillTimer+=seconds;return finish;}
            bool AdvanceSkill() override {active=false;return false;}
            const char* SpellName() const override {return spell?"Audio clock":nullptr;}
            int SpellCardNumber() const override {return spell?1:0;}
            const char* DisplayName() const override {return "Audio probe";}
            float TimeLimit() const override {return limit;}
        };
        assert(IsAudioDeviceReady());game.StartPreview();game.stage_=StageDirector{};game.actors_.Clear();game.drops_.Clear();
        game.player_.ConfigureRun(3,3,false,ShotType::HomingAmulet);
        const auto playing=[&](std::string_view name){const auto* sound=game.assets_.FindSoundEffect(name);assert(sound&&IsSoundValid(*sound));return IsSoundPlaying(*sound);};
        const auto stop=[&]{for(auto name:{"life_up","pickup","countdown","countdown_urgent"})game.assets_.StopSoundEffect(name);};
        stop();const int lives=game.player_.RemainingLives();
        game.drops_.SpawnNativeItem(game.player_.Position(),5,false);game.UpdatePickups(0);
        assert(game.player_.RemainingLives()==lives+1&&playing("life_up")&&!playing("pickup"));
        stop();game.drops_.SpawnFixedPower(game.player_.Position(),1);game.UpdatePickups(0);
        assert(playing("pickup")&&!playing("life_up"));
        game.boss_=std::make_unique<AudioBoss>();auto* clock=static_cast<AudioBoss*>(game.boss_.get());
        stop();clock->skillTimer=9.9F;game.UpdateBoss(.1F);assert(playing("countdown")&&!playing("countdown_urgent"));
        stop();game.UpdateBoss(0);assert(!playing("countdown"));
        clock->skillTimer=14.9F;game.UpdateBoss(.1F);assert(playing("countdown_urgent"));
        stop();clock->skillTimer=9.9F;const float frozen=clock->skillTimer;
        game.TogglePause();game.Update(2);assert(clock->skillTimer==frozen&&!playing("countdown")&&!playing("countdown_urgent"));game.TogglePause();
        clock->spell=false;clock->skillTimer=14.9F;game.UpdateBoss(.1F);assert(!playing("countdown_urgent"));
        clock->spell=true;clock->skillTimer=19.9F;game.UpdateBoss(.1F);assert(!playing("countdown_urgent"));
        clock->skillTimer=14.9F;clock->finish=true;game.UpdateBoss(.1F);assert(!playing("countdown_urgent")&&!clock->active);
        stop();game.EnterMainMenu();std::cout<<"1UP pickup, countdown/urgent WAV, pause and skill-end audio gates passed (muted)\n";
    }

    /// @brief 实际加载十个差分，渲染逐句切换及菜单/符卡，验证离开关卡不释放通用立绘。
    static void TestPortraits(Game& game, const std::string& prefix) {
        game.StartPreview();
        const auto menuTexture=game.assets_.Common().reimuPortrait;
        assert(IsTextureValid(menuTexture) && menuTexture.width==976 && menuTexture.height==1612);
        for(int index=0;index<6;++index)
            assert(IsTextureValid(game.assets_.GetVisual(game.stageContent_->HeroPortraitVisual(index)).texture));
        for(int index=0;index<4;++index) {
            const auto& visual=game.assets_.GetVisual(game.stageContent_->BossPortraitVisual(index));
            assert(IsTextureValid(visual.texture) && visual.texture.width==887 && visual.texture.height==1774);
            assert(visual.dialogueCrop.height<1);
        }
        const int width=GetScreenWidth(),height=GetScreenHeight();
        // 用正式 PNG（含近透明修复残留）独立绘制每个差分，防止纯色画布贴边测试漏掉内部留白。
        for (const Vector2 size : {Vector2{1100,760},{1280,720},{390,844}}) {
            const GameplayLayout layout(size.x,size.y);
            const auto target=LoadRenderTexture(static_cast<int>(size.x),static_cast<int>(size.y));
            constexpr Color background{255,0,255,255};
            for (int index=0;index<10;++index) {
                const bool right=index>=6;
                const auto id=right ? game.stageContent_->BossPortraitVisual(index-6)
                    : game.stageContent_->HeroPortraitVisual(index);
                const auto& visual=game.assets_.GetVisual(id);
                const auto crop=visual.dialogueCrop;
                const Rectangle source{crop.x*visual.texture.width,crop.y*visual.texture.height,
                    crop.width*visual.texture.width,crop.height*visual.texture.height};
                BeginDrawing(); BeginTextureMode(target); ClearBackground(background);
                DrawTexturePro(visual.texture,source,layout.DialoguePortrait(source.width/source.height,right),{},0,WHITE);
                EndTextureMode(); EndDrawing();
                Image image=LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
                int x0=image.width,x1=0,y1=0;
                for (int y=0;y<image.height;++y) for(int x=0;x<image.width;++x) {
                    const auto pixel=GetImageColor(image,x,y);
                    if (std::abs(int(pixel.r)-255)+pixel.g+std::abs(int(pixel.b)-255)<=6) continue;
                    x0=std::min(x0,x); x1=std::max(x1,x); y1=std::max(y1,y);
                }
                const int gap=right ? image.width-1-x1 : x0;
                std::cout<<"Portrait edge "<<index<<" at "<<image.width<<"x"<<image.height
                         <<": side="<<gap<<", bottom="<<image.height-1-y1<<" px\n";
                assert(gap<=2 && image.height-1-y1<=2);
                if (!prefix.empty()) {
                    const auto path=prefix+"-portrait-edge-"+std::to_string(index)+"-"+std::to_string(image.width)+".png";
                    assert(ExportImage(image,path.c_str()));
                }
                UnloadImage(image);
            }
            UnloadRenderTexture(target);
        }
        game.BeginStageDialogue(1);
        const auto lines=game.CurrentDialogue();
        float start=0;
        for(std::size_t index=0;index<lines.size();++index) {
            game.dialogueTimer_=start+std::max(1.1F,lines[index].textDelay+lines[index].line2Delay+0.1F);
            assert(game.DialogueIndex()==static_cast<int>(index));
            game.UpdateUiMotion(0.3F);
            const auto name="portrait-dialogue-"+std::to_string(index);
            Capture(game,prefix,name.c_str());
            start+=lines[index].duration;
        }
        // 重建计时器时应恢复双方表情，而不是只设置当前说话人的图。
        game.dialogueTimer_=31;
        game.UpdateUiMotion(0.3F);
        for(const auto size:std::array<Vector2,2>{{{1280,720},{390,844}}}) {
            SetWindowSize(size.x,size.y);
            const auto name="portrait-dialogue-"+std::to_string(static_cast<int>(size.x));
            Capture(game,prefix,name.c_str());
        }
        SetWindowSize(width,height);
        game.dialogueActive_=false;
        game.state_=RunState::StartSetup;
        game.UpdateUiMotion(1);
        Capture(game,prefix,"portrait-menu");
        game.state_=RunState::Playing;
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),2);
        game.boss_->AdvanceSkill();
        game.spellCutInTimer_=1.5F;
        Capture(game,prefix,"portrait-spell");
        game.EnterMainMenu();
        assert(game.assets_.Stage().visuals.empty());
        assert(IsTextureValid(game.assets_.Common().reimuPortrait));
        assert(game.assets_.Common().reimuPortrait.id==menuTexture.id);
    }

    /// @brief 道中与关底 Boss 入场均保留现存战斗对象，并继续推进其运动。
    static void TestBossEntryPreservesField(Game& game) {
        for (const bool final : {false, true}) {
            game.StartPreview();
            game.stage_ = StageDirector{};
            auto moving=testing::SpawnAt({4,0,5});moving.pattern.movement.speed=1;
            game.actors_.Spawn(moving);
            Bullet* enemyShot = game.bullets_.Spawn(
                BulletOwner::Enemy, {-4, 0, 0}, {1, 0, 0}, 0.1F, WHITE, kInvalidVisualId);
            Bullet* playerShot = game.bullets_.Spawn(
                BulletOwner::Player, {-4, 2, 0}, {1, 0, 0}, 0.1F, WHITE, kInvalidVisualId);
            assert(enemyShot && playerShot);
            game.drops_.SpawnFixedPower({4, 0, 0}, 1);
            const auto pickups = PickupCount(game);
            assert(pickups > 0);

            game.BeginStageBoss(testing::BattleAt(*game.stageContent_,static_cast<std::size_t>(final)), 1);
            assert(game.boss_ && game.boss_->active);
            assert(EnemyCount(game) == 1);
            assert(enemyShot->active && playerShot->active);
            assert(game.bullets_.Pool().ActiveCount() == 2);
            assert(PickupCount(game) == pickups);
            assert(game.actorEffects_.Pool().ActiveCount() == 0);

            game.Update(0.1F);
            assert(EnemyCount(game) == 1);
            game.actors_.ForEachActive([](const behavior::Actor& enemy) {
                assert(std::abs(enemy.age-0.1F)<1e-6F && enemy.position.z < 5.0F);
            });
            assert(enemyShot->active && enemyShot->position.x > -4.0F);
            assert(playerShot->active && playerShot->position.x > -4.0F);
            assert(PickupCount(game) == pickups);
        }
    }

    /// @brief 从真实 Game 更新比较各遭遇的弹池轨迹，确保显示帧率不改变补发、延迟转向或空间位置。
    static void TestSpatialFrameRates(Game& game,const std::string& prefix) {
        // 跨重开只保留数值，不复制借用关卡 ANM 的动画游标，避免期望快照延长素材借用期。
        struct BulletSample {
            Vector3 position,velocity,acceleration,rotationAxis,launchDirection;
            float age;
            int nativeType,nativeColor;
            bool turnApplied;
        };
        std::size_t peak=0;
        for(int phase:{0,1,2,3,4}) {
            std::vector<BulletSample> expected;Vector3 expectedBoss{};float expectedTimer=0;
            for(int fps:{120,30,60,144,240}) {
                game.StartPreview();game.stage_=StageDirector{};game.BeginStageBoss(testing::BattleAt(*game.stageContent_,static_cast<std::size_t>(phase!=0)),3);
                while(game.boss_->attack<phase)game.boss_->AdvanceSkill();
                // 以同频率的原编排给出实际位移预算；包含原边界与插值终点补偿，不能用任意速度上限代替。
                behavior::Actor original;
                original.Reset(phase==0 ? Vector3{0,0,11.4285717F} : Vector3{0,0,5.71428585F},game.boss_->health,phase==0 ? 1882 : 5093);
                stage1::ConfigureBehavior(original,false);
                constexpr std::array<behavior::Entry,5> entries{bosses::RumiaBoss::Behaviors::Midboss,bosses::RumiaBoss::Behaviors::FirstNonspell,
                    bosses::RumiaBoss::Behaviors::NightBird,bosses::RumiaBoss::Behaviors::SecondNonspell,bosses::RumiaBoss::Behaviors::Demarcation};
                original.Start(entries[phase](original));auto referenceBullets=std::make_unique<BulletSystem>();
                double referenceTicks=0;
                for(int frame=0;frame<6*fps;++frame) {
                    referenceTicks+=static_cast<double>(1.0F/fps)*120;
                    float budget=0;
                    while(referenceTicks+0.00001>=1) {
                        referenceTicks-=1;const auto before=original.Position();
                        original.Update(1.0F/120,game.player_.Position(),*referenceBullets);
                        const auto after=original.Position();budget+=BulletSystem::Length({after.x-before.x,after.y-before.y,after.z-before.z});
                    }
                    const Vector3 before=game.boss_->position;game.Update(1.0F/fps);
                    const Vector3 after=game.boss_->position;
                    assert(BulletSystem::Length({after.x-before.x,after.y-before.y,after.z-before.z})<=budget+0.0001F);
                }
                std::vector<BulletSample> actual;
                game.bullets_.Pool().ForEachActive([&](const Bullet& bullet){
                    actual.push_back({bullet.position,bullet.Velocity(),bullet.acceleration,bullet.rotationAxis,bullet.launchDirection,
                                      bullet.age,bullet.nativeType,bullet.nativeColor,bullet.turnApplied});
                });
                assert(game.bullets_.Pool().DroppedSpawns()==0);peak=std::max(peak,actual.size());
                const auto equal=[](Vector3 a,Vector3 b) {
                    assert(std::abs(a.x-b.x)<1e-5F&&std::abs(a.y-b.y)<1e-5F&&std::abs(a.z-b.z)<1e-5F);
                };
                if(fps==120) {expected=actual;expectedBoss=game.boss_->position;expectedTimer=game.boss_->skillTimer;}
                else {
                    assert(expected.size()==actual.size());equal(expectedBoss,game.boss_->position);
                    assert(expectedTimer==game.boss_->skillTimer);
                    for(std::size_t i=0;i<actual.size();++i) {
                        equal(expected[i].position,actual[i].position);equal(expected[i].velocity,actual[i].velocity);
                        equal(expected[i].acceleration,actual[i].acceleration);equal(expected[i].rotationAxis,actual[i].rotationAxis);
                        equal(expected[i].launchDirection,actual[i].launchDirection);
                        assert(expected[i].age==actual[i].age&&expected[i].turnApplied==actual[i].turnApplied);
                        assert(expected[i].nativeColor==actual[i].nativeColor&&expected[i].nativeType==actual[i].nativeType);
                    }
                }
                if(fps==120) {
                    const auto name="spatial-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());
                }
            }
        }
        // 高 FPS 尚未到战斗步时保留边沿；暂停清输入且不追赶暂停的墙钟时间。
        game.StartPreview();game.stage_=StageDirector{};
        game.pendingPlayerInput_.mouseDelta={3,4};game.pendingPlayerInput_.bomb=true;
        game.Update(1.0F/240);assert(game.pendingPlayerInput_.bomb&&game.bombCount_==0);
        game.Update(1.0F/240);assert(!game.pendingPlayerInput_.bomb&&game.bombCount_==1);
        game.Update(1.0F/30);assert(game.bombCount_==1);
        const auto paused=game.stage_.Elapsed();game.TogglePause();game.Update(3);assert(game.stage_.Elapsed()==paused);
        game.TogglePause();game.Update(1.0F/120);assert(std::abs(game.stage_.Elapsed()-paused-1.0F/120)<1e-5F);
        std::cout<<"Spatial Game trajectories identical at 30/60/120/144/240 FPS; sampled peak "<<peak<<" bullets\n";
    }

    /// @brief 不跳过事件，按 60 Hz 推进一面到结算并验证正式切关；随后独立回归一面符卡练习和光束。
    static void TestWholeStage(Game& game, const std::string& prefix) {
        game.StartPreview();
        game.previewMode_ = false; // 本用例在临时目录验证标准模式成绩保存。
        bool midbossSeen = false, dialogueSeen = false;
        std::array<bool, 4> phases{};
        int frame = 0;std::size_t peakBullets=0;
        for (; frame < 400 * 60 && game.state_ != RunState::StageClear; ++frame) {
            game.Update(1.0F / 60.0F);
            assert(game.bullets_.Pool().DroppedSpawns()==0);
            peakBullets=std::max(peakBullets,game.bullets_.Pool().ActiveCount());
            if (frame == 12 * 60) {
                const int width=GetScreenWidth(),height=GetScreenHeight();
                for(const auto size:std::array<Vector2,3>{{{1280,720},{960,720},{1680,720}}}) {
                    SetWindowSize(static_cast<int>(size.x),static_cast<int>(size.y));
                    const std::string name="opening-"+std::to_string(static_cast<int>(size.x));
                    Capture(game,prefix,name.c_str());
                }
                SetWindowSize(width,height);
                Capture(game, prefix, "stage-opening");
                // 真实默认 framebuffer 与离屏纹理分别验证；Retina 恢复逻辑视口会使右半屏空白。
                BeginDrawing(); ClearBackground(BLACK); game.Draw(); EndDrawing();
                BeginDrawing(); ClearBackground(BLACK); game.Draw(); rlDrawRenderBatchActive();
                Image windowImage=LoadImageFromScreen();
                const Color edge=GetImageColor(windowImage,windowImage.width*9/10,windowImage.height/2);
                assert(edge.r+edge.g+edge.b>0);
                if(!prefix.empty())assert(ExportImage(windowImage,(prefix+"-window-retina.png").c_str()));
                UnloadImage(windowImage); EndDrawing();
            }
            if (game.dialogueActive_ && !dialogueSeen && game.dialogueTimer_ > 1) {
                dialogueSeen = true;
                Capture(game, prefix, "dialogue");
            }
            if (game.boss_ && game.boss_->active) {
                if (game.bossEventId_==1) midbossSeen = true;
                else if (game.boss_->skillTimer > 5.1F && !phases[game.boss_->attack - 1]) {
                    phases[game.boss_->attack - 1] = true;
                    const auto name = "phase-" + std::to_string(game.boss_->attack);
                    Capture(game, prefix, name.c_str());
                }
            }
            assert(game.state_ != RunState::GameOver);
        }
        if (frame>=400*60) std::cerr << "Stage stalled: state " << static_cast<int>(game.state_)
            << " stage " << game.stage_.Elapsed() << " dialogue " << game.dialogueActive_
            << " after " << game.dialogueAfterBattle_ << " boss phase " << (game.boss_ ? game.boss_->attack : -1)
            << " hp " << (game.boss_ ? game.boss_->health : -1) << " timer " << (game.boss_ ? game.boss_->skillTimer : -1) << "\n";
        assert(frame < 400 * 60 && midbossSeen && dialogueSeen);
        for (bool seen : phases) assert(seen);
        assert(game.state_ == RunState::StageClear && game.bullets_.Pool().ActiveCount() == 0);
        assert(game.stageHighScores_.Best(false,1) >= 120000);
        Capture(game, prefix, "whole-stage-clear");
        std::cout << "Whole Stage 1 cleared at " << frame / 60.0F << " seconds; peak "<<peakBullets<<" / "<<BulletSystem::kCapacity<<" bullets\n";
        game.ContinueStageClear();
        assert(game.state_==RunState::Loading && game.stageId_==StageId::Stage2 && game.assets_.Stage().visuals.empty());
        // 单关回归在二面加载入口结束；连续六关另有 TestCampaignTransitions，符卡用例仍须显式回到一面。
        game.stageId_=StageId::Stage1;

        for (int phase : {2, 4}) {
            game.practiceMode_ = game.spellPractice_ = true;
            game.spellSelection_={spell::Page::Difficulty,0,phase==2?1:2,1,0};
            game.practiceAttack_ = phase;
            const int bestScore = game.bestScore_;
            game.StartRun(false, true);
            game.player_.SetInvulnerability(1000);
            assert(game.boss_->attack == phase && game.player_.Power() == Player::kMaxPower);
            for (int step = 0; step < 30 * 60 && game.state_ != RunState::Won; ++step)
                game.Update(1.0F / 60);
            assert(game.state_ == RunState::Won && game.bestScore_ == bestScore);
            // 超时练习只保留实际擦弹分数，不能获得旧版固定 25000 分捕获奖励。
            assert(game.score_ == game.graze_*500 && EnemyCount(game) == 0);
            game.StartRun(false, true); game.player_.SetInvulnerability(1000);
            for(int step=0;step<130;++step)game.Update(1.0F/60);
            assert(game.boss_->CanTakeDamage());
            game.boss_->health=0; game.ResolveCollisions();
            assert(game.score_>=200000 && game.spellBonus_>=200000);
            assert(game.bestScore_==bestScore); // 捕获练习不能写入整关最高分。
        }
        game.StartPreview();
        game.stage_ = StageDirector{};
        game.bullets_.SpawnBeam({-2, 2, 7}, {0, -0.1F, -1}, 24, 0.22F, 1, 0.65F, SKYBLUE);
        game.Update(0.5F);
        Capture(game, prefix, "beam-warning");
        game.TogglePause();
        game.Update(3);
        game.bullets_.Pool().ForEachActive([](const Bullet& bullet) { assert(std::abs(bullet.age-0.5F)<1e-5F); });
        Capture(game, prefix, "pause");
        game.TogglePause();
        game.Update(0.6F);
        Capture(game, prefix, "beam-active");
        game.ApplyBombEffect();
        game.UpdatePlayerBomb(0);
        assert(game.bullets_.Pool().ActiveCount() == 0);
        game.StartPreview(); // 恢复整关模式，不能把练习设置泄漏给后续调用方。
        assert(!game.spellPractice_ && !game.practiceMode_ && !game.challengeMode_);
    }

    /// @brief 经正式练习入口验证已开放难度整关；超时和逐技能实际伤害两路均包含对白、暂停、奖励及资源卸载。
    /// 每个难度分配两种御札，组合覆盖四机体；finalDamageOnly仅复现Lunatic击破路线，不改变原断言。
    static void TestStageDifficulties(Game& game,StageId id,const std::string& prefix,bool finalDamageOnly=false) {
        assert(id==StageId::Stage1||id==StageId::Stage2||id==StageId::Stage3||id==StageId::Stage4||id==StageId::Stage5||id==StageId::Stage6);
        for(int index=0;index<4;++index)for(bool damage:{false,true}) {
            if(finalDamageOnly&&(index!=3||!damage))continue;
            const auto difficulty=*native_combat::DifficultyFromIndex(index);
            if(!SupportsStage(id,difficulty,StageEntry::Practice))continue;
            const int shot=(index+(damage?2:0))%4;
            game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            const int oldBest=game.bestScore_;
            game.StartStage(id,false,difficulty,StageEntry::Practice);
            const double deadline=GetTime()+60;
            while(game.IsLoading()) {
                assert(GetTime()<deadline&&!game.HasLoadingFailed());game.Update(1.0F/60);
                BeginDrawing();ClearBackground(BLACK);game.Draw();EndDrawing();
            }
            assert(game.state_==RunState::Playing&&game.practiceMode_&&!game.previewMode_);
            assert(game.runDifficulty_==difficulty&&game.stageContent_->Difficulty()==difficulty);
            game.player_.SetInvulnerability(1000);
            std::array<bool,9> finalSeen{},captured{};std::array<bool,2> middleSeen{};
            bool before=false,after=false,paused=false;std::size_t peak=0;int frame=0;
            const int limit=id==StageId::Stage6?1300*60:id==StageId::Stage5?1100*60:id==StageId::Stage4?750*60:id==StageId::Stage3?550*60:400*60;
            for(;frame<limit&&game.state_!=RunState::StageClear;++frame) {
                game.Update(1.0F/60);
                if(game.state_==RunState::StageClear)break;
                assert(game.state_==RunState::Playing&&game.stageId_==id);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                game.actors_.ForEachActive([&](const behavior::Actor& actor){assert(actor.difficulty==difficulty);});
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.dialogueActive_) {if(game.dialogueAfterBattle_)after=true;else before=true;}
                if(!game.boss_||!game.boss_->active||game.boss_->attack<1)continue;
                const int skill=game.boss_->attack;
                if(id==StageId::Stage3&&index>=2&&!damage&&game.bossEventId_!=1&&
                   (skill==4||skill==6)&&game.boss_->skillTimer>6&&!captured.at(skill-1)) {
                    captured[skill-1]=true;
                    const auto label="stage3-difficulty-"+std::to_string(index)+"-skill-"+std::to_string(skill);
                    Capture(game,prefix,label.c_str());
                }
                if(game.boss_->skillTimer>=2.5F) {
                    if(game.bossEventId_==1)middleSeen.at(skill-1)=true;
                    else finalSeen.at(skill-1)=true;
                }
                if(!paused&&game.boss_->skillTimer>2.6F) {
                    paused=true;const auto time=game.stage_.Elapsed();const auto bossTime=game.boss_->skillTimer;
                    game.TogglePause();game.Update(0.5F);
                    assert(game.stage_.Elapsed()==time&&game.boss_->skillTimer==bossTime);game.TogglePause();
                }
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>3) {
                    game.boss_->health=0;game.ResolveCollisions();
                }
            }
            assert(frame<limit&&before&&after&&paused&&middleSeen[0]);
            const int finalSkills=id==StageId::Stage6?9:id==StageId::Stage5?6:id==StageId::Stage1?(index==0?3:4):id==StageId::Stage2?(index==0?4:5):id==StageId::Stage4?(index==0?4:index==1?6:7):(index==0?5:index==1?6:7);
            for(int skill=0;skill<int(finalSeen.size());++skill)assert(finalSeen[skill]==(skill<finalSkills));
            if(id==StageId::Stage1)assert(middleSeen[1]==(damage&&index>=2));
            if(id==StageId::Stage3)assert(middleSeen[1]==damage);
            if(id==StageId::Stage5||id==StageId::Stage6)assert(middleSeen[1]==damage);
            assert(game.state_==RunState::StageClear&&game.stageClearSummary_.difficulty==difficulty);
            assert(game.stageHighScores_.Best(false,StageNumber(id),index)>=game.score_);
            const std::string label="stage"+std::to_string(StageNumber(id))+"-difficulty-"+std::to_string(index)+(damage?"-damage":"-timeout");
            Capture(game,prefix,(label+"-clear").c_str());
            game.ContinueStageClear();
            for(int tick=0;tick<360&&game.state_==RunState::Finishing;++tick)game.Update(1.0F/60);
            assert(game.state_==RunState::StartSetup&&game.setupStep_==StartSetupStep::Stage);
            assert(game.bestScore_==oldBest&&!game.pendingEnding_&&game.assets_.Stage().visuals.empty());
            assert(!EnemyCount(game)&&!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" complete at "<<frame/60.0F<<" seconds; peak "<<peak<<" projectiles\n";
        }
    }

    /// @brief firstStage/lastStage 为目录关卡索引半开区间，默认覆盖全部已接入符卡的击破和超时。
    /// 从生产加载、120 Hz 战斗到返回选卡检查记录与资源。
    static void TestDifficultySpellPractice(Game& game,const std::string& prefix,int firstStage=0,int lastStage=6) {
        for(const auto& card:spell::Cards())if(card.stage>=firstStage&&card.stage<lastStage&&spell::Playable(&card,0))for(bool damage:{false,true}) {
            const int shot=(card.number+(damage?2:0))%4;
            game.practiceMode_=game.spellPractice_=true;game.challengeMode_=false;
            game.spellSelection_={spell::Page::Difficulty,card.stage,card.group,card.difficulty,shot};
            game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            const auto before=game.spellRecords_.Get(shot,card.number);const int oldBest=game.bestScore_;
            const auto oldStageBest=game.stageHighScores_.Best(false,card.stage+1,card.difficulty);
            game.StartRun(false,true);game.player_.SetInvulnerability(1000);
            assert(game.state_==RunState::Playing&&game.boss_->attack==card.phase);
            assert(StageNumber(game.stageId_)==card.stage+1&&native_combat::DifficultyIndex(game.runDifficulty_)==card.difficulty);
            assert(static_cast<int>(game.player_.Shot())==shot&&game.player_.Power()==Player::kMaxPower);
            bool seen=false,paused=false;int frame=0;const int limit=(card.stage==5?170:70)*60;
            for(;frame<limit&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                if(game.state_!=RunState::Playing)continue;
                assert(game.boss_&&game.boss_->attack==card.phase&&!game.dialogueActive_&&!EnemyCount(game));
                if(game.boss_->SpellName()) {seen=true;assert(game.boss_->SpellCardNumber()==card.number);}
                if(card.stage==5&&game.boss_->CanTakeDamage())assert(game.boss_->CanContactPlayer());
                if(!paused&&game.boss_->skillTimer>3) {
                    paused=true;const float time=game.boss_->skillTimer;
                    game.TogglePause();game.Update(0.5F);assert(game.boss_->skillTimer==time);game.TogglePause();
                    if(!damage&&(card.number==1||card.number==13||card.number==17||(card.stage==3&&
                       (card.number==38||card.number==44||card.number==48||card.number==55||card.number==62||card.number==65||
                        card.number==69||card.number==73||card.number==77||card.number==80||card.number==84))))
                        Capture(game,prefix,("spell-"+std::to_string(card.number)).c_str());
                }
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>6) {
                    game.boss_->health=0;game.ResolveCollisions();
                }
            }
            assert(frame<limit&&seen&&paused&&game.state_==RunState::Won&&game.spellCaptured_==damage);
            const auto after=game.spellRecords_.Get(shot,card.number);
            assert(after.attempts==before.attempts+1&&after.captured==before.captured+int(damage));
            assert(after.mainAttempts==before.mainAttempts&&after.mainCaptured==before.mainCaptured);
            assert(game.bestScore_==oldBest&&game.stageHighScores_.Best(false,card.stage+1,card.difficulty)==oldStageBest);
            game.ReturnToSpellPractice();
            assert(game.state_==RunState::StartSetup&&game.spellSelection_.stage==card.stage&&game.spellSelection_.group==card.group);
            assert(game.spellSelection_.difficulty==card.difficulty&&game.spellSelection_.character==shot);
            assert(game.assets_.Stage().visuals.empty()&&!game.bullets_.Pool().ActiveCount()&&!EnemyCount(game));
            std::cout<<"Spell "<<card.number<<(damage?" damage":" timeout")<<" lifecycle passed\n";
        }
    }

    /// @brief 正式一/二面素材的两场遭遇：四难度×五显示FPS检查轨道、闪光、爆散、暂停和资源释放。
    static void TestStage12ExternalEffects(Game& game,const std::string& prefix) {
        for(int stage:{1,2})for(int d=0;d<4;++d) {
            std::array<std::vector<Vector3>,2> expected;
            for(int fps:{120,30,60,144,240}) {
                game.EnterMainMenu();game.stageId_=stage==1?StageId::Stage1:StageId::Stage2;game.selectedCharacter_=game.selectedShot_=0;
                game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;
                game.runDifficulty_=*native_combat::DifficultyFromIndex(d);
                if(stage==1)game.stageContent_=std::make_unique<stage1::Stage1Content>(game.runDifficulty_);
                else game.stageContent_=std::make_unique<stage2::Stage2Content>(game.runDifficulty_);
                game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.stage_=StageDirector{};
                game.player_.SetInvulnerability(1000);
                const auto clearParticles=[&] {
                    const auto* archive=game.externalEffects_.Source();assert(archive);
                    game.externalEffects_.Clear();game.externalEffects_.Bind(*archive,stage_assets::kExternalParticle);
                };
                std::weak_ptr<const effects::AttachmentAnchor> anchor;
                for(int encounter=0;encounter<2;++encounter) {
                    clearParticles();
                    if(stage==1) {
                        auto config=encounter==0?stage1::RumiaMidbossConfig(ShotType::HomingAmulet,game.runDifficulty_):stage1::RumiaFinalConfig(ShotType::HomingAmulet,game.runDifficulty_);
                        config.intro={};config.phases={{{encounter==0?bosses::RumiaBoss::Behavior::Midboss:bosses::RumiaBoss::Behavior::FirstNonspell},{{{0,1,WHITE}}}}};
                        if(encounter)config.position={0,0,5};
                        auto boss=std::make_unique<bosses::RumiaBoss>();boss->StartBoss(config);game.boss_=std::move(boss);
                    } else if(encounter==0) {
                        auto config=stage2::DaiyouseiMidbossConfig();config.difficulty=game.runDifficulty_;
                        auto boss=std::make_unique<bosses::DaiyouseiBoss>();boss->StartBoss(config);game.boss_=std::move(boss);
                    } else {
                        auto config=stage2::CirnoFinalConfig(ShotType::HomingAmulet,game.runDifficulty_);
                        config.phases={{{bosses::CirnoBoss::Behavior::FirstNonspell},{{{0,1,WHITE}}}}};
                        auto boss=std::make_unique<bosses::CirnoBoss>();boss->StartBoss(config);game.boss_=std::move(boss);
                    }
                    for(int n=0;n<fps*2;++n)game.Update(1.0F/fps);
                    std::vector<Vector3> actual;double age=0;
                    game.externalEffects_.ForEachActive([&](const auto& p) {
                        assert(p.kind==effects::Kind::Attached&&p.animation.ScriptId()==18&&!p.released);
                        assert(p.visual==stage_assets::kExternalParticle&&p.animation.Source()==game.externalEffects_.Source());
                        actual.push_back(p.position);anchor=p.attachment;age+=p.frame;
                    });
                    assert(actual.size()==(stage==1?3U:4U)&&!anchor.expired()&&!game.externalEffects_.DroppedSpawns());
                    if(fps==120)expected[encounter]=actual;
                    else for(std::size_t n=0;n<actual.size();++n) {
                        const auto e=expected[encounter][n],a=actual[n];
                        assert(std::abs(e.x-a.x)<=5e-4F&&std::abs(e.y-a.y)<=5e-4F&&std::abs(e.z-a.z)<=5e-4F);
                    }
                    game.TogglePause();game.Update(.5F);double paused=0;game.externalEffects_.ForEachActive([&](const auto& p){paused+=p.frame;});
                    assert(paused==age);game.TogglePause();
                    if(d==1&&fps==120) {
                        if(encounter==1) {
                            const auto render=[&] {
                                const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());assert(IsRenderTextureValid(target));
                                BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.Draw();EndTextureMode();EndDrawing();
                                Image image=LoadImageFromTexture(target.texture);auto* colors=LoadImageColors(image);
                                std::vector<Color> pixels(colors,colors+image.width*image.height);UnloadImageColors(colors);UnloadImage(image);UnloadRenderTexture(target);return pixels;
                            };
                            const auto with=render();auto saved=std::make_unique<effects::ExternalEffectSystem>(game.externalEffects_);
                            game.externalEffects_.Clear();const auto without=render();game.externalEffects_=*saved;
                            int changed=0;for(std::size_t n=0;n<with.size();++n)changed+=std::memcmp(&with[n],&without[n],sizeof(Color))!=0;assert(changed>100);
                        }
                        Capture(game,prefix,("stage"+std::to_string(stage)+"-attached-"+std::to_string(encounter)).c_str());
                    }
                    if(encounter==0) {game.boss_.reset();assert(anchor.expired());continue;}
                    assert(game.boss_->CanTakeDamage());game.boss_->health=0;game.ResolveCollisions();assert(anchor.expired());bool burst=false;
                    for(int n=0;n<240;++n) {
                        game.Update(1.0F/120);
                        game.externalEffects_.ForEachActive([&](const auto& p) {if(p.kind==effects::Kind::Burst) {burst=true;assert(p.animation.ScriptId()==6);}});
                    }
                    assert(burst&&!game.boss_->active&&!game.externalEffects_.ActiveCount()&&!game.externalEffects_.DroppedSpawns());
                }
                if(stage==2&&d>0) {
                    auto config=stage2::CirnoFinalConfig(ShotType::HomingAmulet,game.runDifficulty_);
                    config.phases={{{bosses::CirnoBoss::Behavior::DiamondBlizzard},{{{0,1,WHITE}}}}};
                    auto boss=std::make_unique<bosses::CirnoBoss>();boss->StartBoss(config);game.boss_=std::move(boss);clearParticles();
                    game.Update(1.0F/120);int flashes=0;
                    game.externalEffects_.ForEachActive([&](const auto& p){assert(p.kind==effects::Kind::Flash&&p.animation.ScriptId()==17);++flashes;});
                    assert(flashes==1);if(d==1&&fps==120)Capture(game,prefix,"stage2-diamond-flash");
                } else {
                    if(stage==1) {
                        auto config=stage1::RumiaFinalConfig(ShotType::HomingAmulet,game.runDifficulty_);
                        config.intro={};config.position={0,0,5};config.phases={{{bosses::RumiaBoss::Behavior::FirstNonspell},{{{0,1,WHITE}}}}};
                        auto boss=std::make_unique<bosses::RumiaBoss>();boss->StartBoss(config);game.boss_=std::move(boss);
                    } else {
                        auto cirno=stage2::CirnoFinalConfig(ShotType::HomingAmulet,game.runDifficulty_);
                        cirno.phases={{{bosses::CirnoBoss::Behavior::FirstNonspell},{{{0,1,WHITE}}}}};
                        auto boss=std::make_unique<bosses::CirnoBoss>();boss->StartBoss(cirno);game.boss_=std::move(boss);
                    }
                    for(int n=0;n<fps;++n)game.Update(1.0F/fps);
                }
                // 在粒子仍活动时结束；Game必须等粒子清完再释放档案借用。
                game.EndStage(RunState::GameOver);bool retained=false;int n=0;
                for(;n<300&&game.state_==RunState::Finishing;++n) {
                    if(game.externalEffects_.ActiveCount()) {retained=true;assert(game.externalEffects_.Source()&&!game.assets_.Stage().visuals.empty());}
                    game.Update(1.0F/120);
                }
                assert(n<300&&retained&&game.state_==RunState::GameOver&&!game.externalEffects_.Source()&&game.assets_.Stage().visuals.empty());
                std::cout<<"Stage"<<stage<<" effects d"<<d<<" at "<<fps<<" FPS: both encounters, orbit, flash/burst, pause and release passed\n";
            }
        }
        game.EnterMainMenu();
    }

    /// @brief 正式四面素材下核验五轨道、组合闪光、六轮爆散及暂停/卸载；四难度各覆盖五显示FPS。
    static void TestStage4ExternalEffects(Game& game,const std::string& prefix) {
        using P=bosses::PatchouliBoss;std::vector<Vector3> expected;
        for(int d=0;d<4;++d)for(int fps:{120,30,60,144,240}) {
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=game.selectedShot_=0;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;
            game.runDifficulty_=*native_combat::DifficultyFromIndex(d);
            game.stageContent_=std::make_unique<stage4::Stage4Content>(game.runDifficulty_);
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.stage_=StageDirector{};
            game.player_.SetInvulnerability(1000);
            const auto original=stage4::PatchouliFinalConfig(ShotType::HomingAmulet,game.runDifficulty_);
            auto config=original;config.intro={};config.position={0,0,5};config.phases={original.phases.front()};
            auto boss=std::make_unique<P>();boss->StartBoss(config);game.boss_=std::move(boss);
            for(int n=0;n<fps;++n)game.Update(1.0F/fps);
            std::vector<Vector3> actual;std::weak_ptr<const effects::AttachmentAnchor> anchor;double age=0;
            game.externalEffects_.ForEachActive([&](const auto& p) {
                assert(p.kind==effects::Kind::Attached&&p.animation.ScriptId()==18&&!p.released);
                assert(p.visual==stage_assets::kExternalParticle&&p.animation.Source()==game.externalEffects_.Source());
                actual.push_back(p.position);anchor=p.attachment;age+=p.frame;
            });
            assert(actual.size()==5&&!anchor.expired()&&!game.externalEffects_.DroppedSpawns());
            if(expected.empty())expected=actual;
            else for(std::size_t n=0;n<actual.size();++n)assert(BulletSystem::Length({actual[n].x-expected[n].x,actual[n].y-expected[n].y,actual[n].z-expected[n].z})<=5e-4F);
            game.TogglePause();game.Update(.5F);double paused=0;game.externalEffects_.ForEachActive([&](const auto& p){paused+=p.frame;});
            assert(paused==age);game.TogglePause();
            if(d==0&&fps==120) {
                const auto render=[&] {
                    const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());assert(IsRenderTextureValid(target));
                    BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.Draw();EndTextureMode();EndDrawing();
                    Image image=LoadImageFromTexture(target.texture);auto* colors=LoadImageColors(image);
                    std::vector<Color> pixels(colors,colors+image.width*image.height);UnloadImageColors(colors);UnloadImage(image);UnloadRenderTexture(target);return pixels;
                };
                const auto with=render();auto saved=std::make_unique<effects::ExternalEffectSystem>(game.externalEffects_);
                game.externalEffects_.Clear();const auto without=render();game.externalEffects_=*saved;
                int changed=0;for(std::size_t n=0;n<with.size();++n)changed+=std::memcmp(&with[n],&without[n],sizeof(Color))!=0;assert(changed>100);
                Capture(game,prefix,"stage4-attached");
            }
            const auto clearParticles=[&] {
                const auto* archive=game.externalEffects_.Source();assert(archive);
                game.externalEffects_.Clear();game.externalEffects_.Bind(*archive,stage_assets::kExternalParticle);
            };
            // 用正式角色解析每个组合位置；只保留待测技能，生产技能仍决定HP和中/末段闪光。
            for(std::size_t segment=0;segment<original.phases[2].skills.size();++segment) {
                config.phases={{{original.phases[2].skills[segment]},{{{0,1,WHITE}}}}};
                boss=std::make_unique<P>();boss->StartBoss(config);game.boss_=std::move(boss);assert(anchor.expired());clearParticles();
                game.Update(1.0F/120);int flashes=0;
                game.externalEffects_.ForEachActive([&](const auto& p) {assert(p.kind==effects::Kind::Flash&&p.animation.ScriptId()==17);++flashes;});
                assert(flashes==(segment?1:0));
                if(segment&&fps==120)Capture(game,prefix,("stage4-flash-d"+std::to_string(d)).c_str());
            }
            for(int n=0;n<5*fps;++n)game.Update(1.0F/fps);
            assert(game.boss_->CanTakeDamage());game.boss_->health=0;game.ResolveCollisions();bool burst=false;
            for(int n=0;n<240;++n) {
                game.Update(1.0F/120);
                game.externalEffects_.ForEachActive([&](const auto& p) {if(p.kind==effects::Kind::Burst) {burst=true;assert(p.animation.ScriptId()==6);}});
            }
            assert(burst&&!game.boss_->active&&!game.externalEffects_.ActiveCount()&&!game.externalEffects_.DroppedSpawns());
            config.phases={original.phases.front()};boss=std::make_unique<P>();boss->StartBoss(config);game.boss_=std::move(boss);
            for(int n=0;n<fps;++n)game.Update(1.0F/fps);
            game.EndStage(RunState::GameOver);bool retained=false;int n=0;
            for(;n<300&&game.state_==RunState::Finishing;++n) {
                if(game.externalEffects_.ActiveCount()) {retained=true;assert(game.externalEffects_.Source()&&!game.assets_.Stage().visuals.empty());}
                game.Update(1.0F/120);
            }
            assert(n<300&&retained&&game.state_==RunState::GameOver&&!game.externalEffects_.Source()&&game.assets_.Stage().visuals.empty());
            std::cout<<"Stage4 effects d"<<d<<" at "<<fps<<" FPS: five orbits, combination flash, burst, pause and release passed\n";
        }
        game.EnterMainMenu();
    }

    /// @brief 三面正式素材覆盖两场附着、漂散/闪光/爆散、暂停、像素及结束时资源保留；四难度各五FPS。
    static void TestStage3ExternalEffects(Game& game,const std::string& prefix) {
        using M=bosses::MeilingBoss;
        struct Sample {Vector3 position;double frame;effects::Kind kind;};
        for(int d=0;d<4;++d) {
            std::array<std::vector<Sample>,3> expected;
            for(int fps:{120,30,60,144,240}) {
                game.EnterMainMenu();game.stageId_=StageId::Stage3;game.selectedCharacter_=game.selectedShot_=0;
                game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;
                game.runDifficulty_=*native_combat::DifficultyFromIndex(d);
                game.stageContent_=std::make_unique<stage3::Stage3Content>(game.runDifficulty_);
                game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.stage_=StageDirector{};
                game.player_.SetInvulnerability(1000);game.referenceRank_=32;
                const auto clearParticles=[&] {
                    const auto* archive=game.externalEffects_.Source();assert(archive);
                    game.externalEffects_.Clear();game.externalEffects_.Bind(*archive,stage_assets::kExternalParticle);
                };
                const auto render=[&] {
                    const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());assert(IsRenderTextureValid(target));
                    BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.Draw();EndTextureMode();EndDrawing();
                    Image image=LoadImageFromTexture(target.texture);auto* colors=LoadImageColors(image);
                    std::vector<Color> pixels(colors,colors+image.width*image.height);UnloadImageColors(colors);UnloadImage(image);UnloadRenderTexture(target);return pixels;
                };
                std::weak_ptr<const effects::AttachmentAnchor> anchor;std::size_t peak=0;
                for(int route=0;route<3;++route) {
                    auto config=route==0?stage3::MeilingMidbossConfig(ShotType::HomingAmulet,game.runDifficulty_):stage3::MeilingFinalConfig(ShotType::HomingAmulet,game.runDifficulty_);
                    config.intro={};
                    const auto skill=route==0?M::Behavior::Midboss:route==1?M::Behavior::FirstNonspell:
                        d>=2?M::Behavior::FlowerDream:M::Behavior::RainbowWindChime;
                    config.phases={{{skill},{{{0,1,WHITE}}}}};
                    if(route==1)config.position={0,0,5};
                    auto boss=std::make_unique<M>();boss->StartBoss(config);game.boss_=std::move(boss);clearParticles();
                    const int seconds=route==1?4:route==2&&d>=2?6:3;bool drift=false,auxiliarySeen=false;
                    for(int n=0;n<fps*seconds;++n) {
                        game.Update(1.0F/fps);peak=std::max(peak,game.externalEffects_.ActiveCount());
                        game.externalEffects_.ForEachActive([&](const auto& p){
                            if(p.kind==effects::Kind::Drift) {
                                drift=true;assert(p.animation.ScriptId()==21);
                                const auto sample=p.animation.Sample();assert(sample.sprite>=7&&sample.sprite<=10&&sample.additive&&sample.disableZWrite);
                                assert(sample.color.r==0&&sample.color.g==0&&sample.color.b==1);
                            }
                            if(p.kind==effects::Kind::Attached) {assert(p.animation.ScriptId()==18&&!p.released);anchor=p.attachment;}
                            assert(p.animation.Source()==game.externalEffects_.Source()&&p.visual==stage_assets::kExternalParticle);
                        });
                        const auto animations=game.boss_->AuxiliaryAnimations();const auto& auxiliary=animations[4];
                        for(std::size_t slot=0;slot<animations.size();++slot)if(slot!=4)assert(animations[slot].script<0);
                        if(auxiliary.script>=0) {
                            assert(auxiliary.script==66&&auxiliary.cursor.selection.atlas==battle::Atlas::Stage3Enemies);
                            assert(auxiliary.cursor.player.Source()&&auxiliary.cursor.player.Source()!=game.externalEffects_.Source());
                            const auto& sample=auxiliary.cursor.player.Sample();
                            if(!auxiliary.cursor.player.Finished()) {
                                auxiliarySeen=true;assert(sample.sprite==94&&sample.disableZWrite&&sample.autoRotate==1);
                                if(d==1&&fps==120&&route==0&&n==260) {
                                    const auto with=render();auto saved=animations[4];game.boss_->AuxiliaryAnimations()[4]={};
                                    const auto without=render();game.boss_->AuxiliaryAnimations()[4]=saved;
                                    int changed=0;for(std::size_t pixel=0;pixel<with.size();++pixel)changed+=std::memcmp(&with[pixel],&without[pixel],sizeof(Color))!=0;
                                    assert(changed>0);Capture(game,prefix,"stage3-auxiliary");
                                    game.TogglePause();game.Update(.5F);assert(game.boss_->AuxiliaryAnimations()[4].age==saved.age);game.TogglePause();
                                }
                            } else assert(!sample.visible);
                        }
                    }
                    assert(auxiliarySeen);
                    if(route==2&&d>=2)assert(game.boss_->AuxiliaryAnimations()[4].revision>=2);
                    if(route!=1)assert(drift);
                    std::vector<Sample> actual;double age=0;int attached=0;
                    game.externalEffects_.ForEachActive([&](const auto& p){actual.push_back({p.position,p.frame,p.kind});age+=p.frame;attached+=p.kind==effects::Kind::Attached;});
                    if(route<2)assert(attached==4&&!anchor.expired());
                    if(fps==120)expected[route]=actual;
                    else {
                        assert(actual.size()==expected[route].size());
                        for(std::size_t n=0;n<actual.size();++n) {
                            const auto a=actual[n],e=expected[route][n];assert(a.frame==e.frame&&a.kind==e.kind);
                            assert(BulletSystem::Length({a.position.x-e.position.x,a.position.y-e.position.y,a.position.z-e.position.z})<=5e-4F);
                        }
                    }
                    game.TogglePause();game.Update(.5F);double paused=0;game.externalEffects_.ForEachActive([&](const auto& p){paused+=p.frame;});
                    assert(paused==age);game.TogglePause();
                    if(d==1&&fps==120) {
                        Capture(game,prefix,("stage3-effects-"+std::to_string(route)).c_str());
                        if(route==2) {
                            const auto with=render();auto saved=std::make_unique<effects::ExternalEffectSystem>(game.externalEffects_);
                            game.externalEffects_.Clear();const auto without=render();game.externalEffects_=*saved;
                            int changed=0;for(std::size_t n=0;n<with.size();++n)changed+=std::memcmp(&with[n],&without[n],sizeof(Color))!=0;assert(changed>0);
                        }
                    }
                }
                if(d>0) {
                    auto config=stage3::MeilingFinalConfig(ShotType::HomingAmulet,game.runDifficulty_);config.intro={};config.position={0,0,5};
                    config.phases={{{M::Behavior::DazzlingColorTyphoon},{{{0,1,WHITE}}}}};
                    auto boss=std::make_unique<M>();boss->StartBoss(config);game.boss_=std::move(boss);clearParticles();
                    game.Update(1.0F/120);bool flash=false;game.externalEffects_.ForEachActive([&](const auto& p){flash|=p.kind==effects::Kind::Flash;assert(p.animation.ScriptId()==17);});assert(flash);
                    if(d==1&&fps==120)Capture(game,prefix,"stage3-flash");
                }
                for(int n=0;n<600&&!game.boss_->CanTakeDamage();++n)game.Update(1.0F/120);
                assert(game.boss_->CanTakeDamage());game.boss_->health=0;game.ResolveCollisions();bool burst=false;
                for(int n=0;n<400;++n) {
                    game.Update(1.0F/120);game.externalEffects_.ForEachActive([&](const auto& p){if(p.kind==effects::Kind::Burst){burst=true;assert(p.animation.ScriptId()==6);}});
                }
                assert(burst&&!game.boss_->active&&!game.externalEffects_.ActiveCount()&&!game.externalEffects_.DroppedSpawns());
                auto config=stage3::MeilingFinalConfig(ShotType::HomingAmulet,game.runDifficulty_);config.intro={};config.position={0,0,5};
                config.phases={{{M::Behavior::FirstNonspell},{{{0,1,WHITE}}}}};
                auto boss=std::make_unique<M>();boss->StartBoss(config);game.boss_=std::move(boss);for(int n=0;n<fps;++n)game.Update(1.0F/fps);
                game.EndStage(RunState::GameOver);bool retained=false;int n=0;
                for(;n<400&&game.state_==RunState::Finishing;++n) {
                    if(game.externalEffects_.ActiveCount()){retained=true;assert(game.externalEffects_.Source()&&!game.assets_.Stage().visuals.empty());}
                    game.Update(1.0F/120);
                }
                assert(n<400&&retained&&game.state_==RunState::GameOver&&!game.externalEffects_.Source()&&game.assets_.Stage().visuals.empty());
                std::cout<<"Stage3 effects d"<<d<<" at "<<fps<<" FPS: two orbits, drift, flash, burst, pause and release passed; peak "<<peak<<"/512\n";
            }
        }
        game.EnterMainMenu();
    }

    /// @brief 正式五面素材下核验附着、三组符卡蓄力及完整退场；四难度各覆盖五显示FPS和时停。
    static void TestStage5ExternalEffects(Game& game,const std::string& prefix) {
        using S=bosses::SakuyaBoss;
        struct Sample {Vector3 position;double frame;};
        for(int d=0;d<4;++d) {
            std::vector<Vector3> expectedOrbits;std::array<std::vector<Sample>,3> expectedCharges;
            for(int fps:{120,30,60,144,240}) {
                game.EnterMainMenu();game.stageId_=StageId::Stage5;game.selectedCharacter_=game.selectedShot_=0;
                game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;
                game.runDifficulty_=*native_combat::DifficultyFromIndex(d);
                game.stageContent_=std::make_unique<stage5::Stage5Content>(game.runDifficulty_);
                game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.stage_=StageDirector{};
                game.player_.SetInvulnerability(1000);game.referenceRank_=32;
                auto config=stage5::SakuyaFinalConfig(ShotType::HomingAmulet,game.runDifficulty_);
                config.intro={};config.position={0,0,5};config.phases={{{S::Behavior::FirstNonspell},{{{0,1,WHITE}}}}};
                auto boss=std::make_unique<S>();boss->StartBoss(config);game.boss_=std::move(boss);
                for(int n=0;n<fps;++n)game.Update(1.0F/fps);
                std::vector<Vector3> actual;std::weak_ptr<const effects::AttachmentAnchor> anchor;double age=0;
                game.externalEffects_.ForEachActive([&](const auto& p) {
                    assert(p.kind==effects::Kind::Attached&&p.animation.ScriptId()==18&&!p.released);
                    assert(p.visual==stage_assets::kExternalParticle&&p.animation.Source()==game.externalEffects_.Source());
                    actual.push_back(p.position);anchor=p.attachment;age+=p.frame;
                });
                assert(actual.size()==5&&!anchor.expired());
                if(fps==120)expectedOrbits=actual;
                else for(std::size_t n=0;n<actual.size();++n)
                    assert(BulletSystem::Length({actual[n].x-expectedOrbits[n].x,actual[n].y-expectedOrbits[n].y,actual[n].z-expectedOrbits[n].z})<=5e-4F);
                game.TogglePause();game.Update(.5F);double paused=0;game.externalEffects_.ForEachActive([&](const auto& p){paused+=p.frame;});
                assert(paused==age);game.TogglePause();
                if(d==1&&fps==120)Capture(game,prefix,"stage5-attached");
                const auto clearParticles=[&] {
                    const auto* archive=game.externalEffects_.Source();assert(archive);
                    game.externalEffects_.Clear();game.externalEffects_.Bind(*archive,stage_assets::kExternalParticle);
                };
                std::size_t peak=5;
                for(int group=0;group<3;++group) {
                    game.boss_=stage5::SakuyaSpellBattle(89+d+group*4,game.runDifficulty_).create(ShotType::HomingAmulet,game.runDifficulty_);
                    assert(anchor.expired());clearParticles();bool charge=false,stopped=false,chargedWhileStopped=false;
                    // Clock Corpse的第二组蓄力在第428帧开始；窗口须越过7秒才能实际观察时停内创建。
                    for(int n=0;n<fps*10;++n) {
                        game.Update(1.0F/fps);peak=std::max(peak,game.externalEffects_.ActiveCount());
                        const bool frozen=game.boss_->WorldTimeStopped();stopped|=frozen;
                        game.externalEffects_.ForEachActive([&](const auto& p) {
                            assert(p.kind==effects::Kind::Charge&&p.animation.ScriptId()==7&&p.visual==stage_assets::kExternalParticle);
                            assert(p.animation.Source()==game.externalEffects_.Source());charge=true;chargedWhileStopped|=frozen;
                        });
                        if(n+1==fps*(group==2?4:3)) {
                            std::vector<Sample> samples;game.externalEffects_.ForEachActive([&](const auto& p){samples.push_back({p.position,p.frame});});
                            assert(!samples.empty());
                            if(fps==120)expectedCharges[group]=samples;
                            else {
                                assert(samples.size()==expectedCharges[group].size());
                                for(std::size_t i=0;i<samples.size();++i) {
                                    const auto a=samples[i],e=expectedCharges[group][i];assert(a.frame==e.frame);
                                    assert(BulletSystem::Length({a.position.x-e.position.x,a.position.y-e.position.y,a.position.z-e.position.z})<=5e-4F);
                                }
                            }
                            if(d==1&&fps==120)Capture(game,prefix,("stage5-charge-"+std::to_string(group)).c_str());
                        }
                    }
                    if(!charge||!stopped||!chargedWhileStopped||game.externalEffects_.DroppedSpawns())
                        std::cerr<<"Stage5 effect window d"<<d<<" fps"<<fps<<" group"<<group<<" charge/stop/frozen-charge "<<charge<<'/'<<stopped<<'/'<<chargedWhileStopped<<'\n';
                    assert(charge&&stopped&&chargedWhileStopped&&!game.externalEffects_.DroppedSpawns());
                    if(d==1&&fps==120&&group==0) {
                        const auto render=[&] {
                            const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());assert(IsRenderTextureValid(target));
                            BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.Draw();EndTextureMode();EndDrawing();
                            Image image=LoadImageFromTexture(target.texture);auto* colors=LoadImageColors(image);
                            std::vector<Color> pixels(colors,colors+image.width*image.height);UnloadImageColors(colors);UnloadImage(image);UnloadRenderTexture(target);return pixels;
                        };
                        // 显示步末可能处在无粒子空窗，另等真实蓄力再核对GPU像素。
                        for(int n=0;n<480&&!game.externalEffects_.ActiveCount();++n)game.Update(1.0F/120);
                        assert(game.externalEffects_.ActiveCount());const auto with=render();
                        auto saved=std::make_unique<effects::ExternalEffectSystem>(game.externalEffects_);game.externalEffects_.Clear();const auto without=render();game.externalEffects_=*saved;
                        int changed=0;for(std::size_t n=0;n<with.size();++n)changed+=std::memcmp(&with[n],&without[n],sizeof(Color))!=0;assert(changed>100);
                    }
                }
                for(int n=0;n<600&&!game.boss_->CanTakeDamage();++n)game.Update(1.0F/120);
                assert(game.boss_->CanTakeDamage());game.boss_->health=0;game.ResolveCollisions();bool burst=false;
                for(int n=0;n<240;++n) {
                    game.Update(1.0F/120);game.externalEffects_.ForEachActive([&](const auto& p){if(p.kind==effects::Kind::Burst) {burst=true;assert(p.animation.ScriptId()==6);}});
                }
                assert(burst&&!game.boss_->active&&!game.externalEffects_.ActiveCount()&&!game.externalEffects_.DroppedSpawns());
                boss=std::make_unique<S>();boss->StartBoss(config);game.boss_=std::move(boss);
                for(int n=0;n<fps;++n)game.Update(1.0F/fps);
                game.EndStage(RunState::GameOver);bool retained=false;int n=0;
                for(;n<300&&game.state_==RunState::Finishing;++n) {
                    if(game.externalEffects_.ActiveCount()) {retained=true;assert(game.externalEffects_.Source()&&!game.assets_.Stage().visuals.empty());}
                    game.Update(1.0F/120);
                }
                assert(n<300&&retained&&game.state_==RunState::GameOver&&!game.externalEffects_.Source()&&game.assets_.Stage().visuals.empty());
                std::cout<<"Stage5 effects d"<<d<<" at "<<fps<<" FPS: five orbits, three charges/time stop, burst, pause and release passed; peak "<<peak<<"/512\n";
            }
        }
        game.EnterMainMenu();
    }

    /// @brief 正式六面素材下覆盖环境、附着、蓄力/慢收束、闪光与爆散的双对白及结束生命周期。
    static void TestStage6AmbientEffects(Game& game,const std::string& prefix) {
        using R=bosses::RemiliaBoss;
        struct Sample {Vector3 position,velocity;double frame;};
        std::vector<Sample> expected;
        std::vector<Vector3> expectedOrbits;
        for(int d=1;d<4;++d)for(int fps:{120,30,60,144,240}) {
            game.EnterMainMenu();game.stageId_=StageId::Stage6;game.selectedCharacter_=game.selectedShot_=0;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;
            game.runDifficulty_=*native_combat::DifficultyFromIndex(d);
            game.stageContent_=std::make_unique<stage6::Stage6Content>(game.runDifficulty_);
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.stage_=StageDirector{};
            game.player_.SetInvulnerability(1000);game.referenceRank_=32;
            auto config=stage6::RemiliaFinalConfig(ShotType::HomingAmulet,game.runDifficulty_);
            auto boss=std::make_unique<R>();boss->StartBoss(config);game.boss_=std::move(boss);
            for(int n=0;n<3*fps;++n)game.Update(1.0F/fps);
            assert(!game.externalEffects_.ActiveCount()&&game.boss_->attack==0);
            game.boss_->ResumeCombat();
            std::size_t peak=0;
            for(int n=0;n<fps*5/2;++n) {game.Update(1.0F/fps);peak=std::max(peak,game.externalEffects_.ActiveCount());}
            std::vector<Sample> actual;double age=0;
            game.externalEffects_.ForEachActive([&](const auto& p) {
                assert(p.kind==effects::Kind::Ambient&&p.animation.ScriptId()==19&&p.visual==stage_assets::kExternalParticle);
                const auto sample=p.animation.Sample();assert(sample.sprite>=7&&sample.sprite<=10&&sample.additive&&sample.disableZWrite);
                assert(p.animation.Source()==game.externalEffects_.Source());
                actual.push_back({p.position,p.velocity,p.frame});age+=p.frame;
            });
            assert(!actual.empty()&&game.boss_->attack==0&&!game.externalEffects_.DroppedSpawns());
            if(expected.empty())expected=actual;
            else {
                assert(actual.size()==expected.size());
                for(std::size_t n=0;n<actual.size();++n) {
                    const auto a=actual[n],e=expected[n];assert(a.frame==e.frame);
                    assert(std::abs(a.position.x-e.position.x)<=5e-4F&&std::abs(a.position.y-e.position.y)<=5e-4F&&std::abs(a.position.z-e.position.z)<=5e-4F);
                    assert(std::abs(a.velocity.x-e.velocity.x)<=1e-4F&&std::abs(a.velocity.y-e.velocity.y)<=1e-4F&&std::abs(a.velocity.z-e.velocity.z)<=1e-4F);
                }
            }
            game.TogglePause();game.Update(.5F);double pausedAge=0;
            game.externalEffects_.ForEachActive([&](const auto& p){pausedAge+=p.frame;});assert(age==pausedAge);game.TogglePause();
            if(fps==120&&d==1) {
                const auto render=[&] {
                    const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());assert(IsRenderTextureValid(target));
                    BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.Draw();EndTextureMode();EndDrawing();
                    Image image=LoadImageFromTexture(target.texture);auto* colors=LoadImageColors(image);
                    std::vector<Color> pixels(colors,colors+image.width*image.height);UnloadImageColors(colors);UnloadImage(image);UnloadRenderTexture(target);return pixels;
                };
                const auto with=render();auto saved=std::make_unique<effects::ExternalEffectSystem>(game.externalEffects_);
                game.externalEffects_.Clear();const auto without=render();game.externalEffects_=*saved;saved.reset();
                int changed=0;for(std::size_t n=0;n<with.size();++n)changed+=std::memcmp(&with[n],&without[n],sizeof(Color))!=0;assert(changed>100);
                Capture(game,prefix,"stage6-ambient-prelude");
            }
            game.boss_->ResumeCombat();
            for(int n=0;n<fps;++n) {game.Update(1.0F/fps);peak=std::max(peak,game.externalEffects_.ActiveCount());}
            assert(game.boss_->attack==1&&!game.externalEffects_.DroppedSpawns());
            std::vector<Vector3> orbits;std::weak_ptr<const effects::AttachmentAnchor> anchor;
            game.externalEffects_.ForEachActive([&](const auto& p) {
                if(p.kind!=effects::Kind::Attached)return;
                assert(p.animation.ScriptId()==18&&!p.released);orbits.push_back(p.position);anchor=p.attachment;
            });
            assert(orbits.size()==5&&!anchor.expired());
            if(expectedOrbits.empty())expectedOrbits=orbits;
            else for(std::size_t n=0;n<orbits.size();++n) {
                assert(std::abs(orbits[n].x-expectedOrbits[n].x)<=5e-4F&&std::abs(orbits[n].y-expectedOrbits[n].y)<=5e-4F&&std::abs(orbits[n].z-expectedOrbits[n].z)<=5e-4F);
            }
            const auto clearParticles=[&] {
                const auto* archive=game.externalEffects_.Source();assert(archive);
                game.externalEffects_.Clear();game.externalEffects_.Bind(*archive,stage_assets::kExternalParticle);
            };
            if(fps==120) {
                config.intro={};config.resumeSteps.clear();
                config.phases={{{d==1?R::Behavior::StarOfVanities:R::Behavior::YoungDemonLord},{{{0,1,WHITE}}}}};
                boss=std::make_unique<R>();boss->StartBoss(config);game.boss_=std::move(boss);assert(anchor.expired());clearParticles();
                for(int n=0;n<300;++n) {game.Update(1.0F/120);peak=std::max(peak,game.externalEffects_.ActiveCount());}
                int charges=0;game.externalEffects_.ForEachActive([&](const auto& p) {
                    if(p.kind==effects::Kind::Charge) {++charges;assert(p.animation.ScriptId()==7);}
                });assert(charges>0);
                Capture(game,prefix,("stage6-charge-d"+std::to_string(d)).c_str());
                config.phases={{{d==1?R::Behavior::ScarletShoot:R::Behavior::ScarletMeister},{{{0,1,WHITE}}}}};
                boss=std::make_unique<R>();boss->StartBoss(config);game.boss_=std::move(boss);clearParticles();game.Update(1.0F/120);
                int flashes=0;game.externalEffects_.ForEachActive([&](const auto& p) {
                    if(p.kind==effects::Kind::Flash) {++flashes;assert(p.animation.ScriptId()==17);}
                });assert(flashes==1);
                game.actors_.Clear();clearParticles();
                game.actors_.Spawn({.position={0,0,5},.pattern=stage6::kAmbientPositions,.sourceHealth=1,.seed=7,.visual=stage_assets::kOrb,.retireOutside=false});
                for(int n=0;n<82;++n)game.Update(1.0F/120);
                int slow=0;game.externalEffects_.ForEachActive([&](const auto& p) {
                    if(p.kind==effects::Kind::SlowCharge) {++slow;assert(p.animation.ScriptId()==8);}
                });assert(slow==2);game.actors_.Clear();
            }
            // 同一正式素材下单独进入最终符，核对Living停发与完整60帧退场；整场技能链由既有六面流程另验。
            config.intro={};config.resumeSteps.clear();
            config.phases={{{d==1?R::Behavior::RedMagic:R::Behavior::ScarletGensokyo},{{{0,1,WHITE}}}}};
            boss=std::make_unique<R>();boss->StartBoss(config);game.boss_=std::move(boss);
            clearParticles();
            // 180内容帧准备的末端在下一固定步才执行；采样在第240帧，明确位于可受伤窗口内。
            for(int n=0;n<4*fps;++n) {game.Update(1.0F/fps);peak=std::max(peak,game.externalEffects_.ActiveCount());}
            assert(game.externalEffects_.ActiveCount()&&game.boss_->CanTakeDamage());
            game.boss_->health=0;game.ResolveCollisions();
            bool burst=false;
            for(int n=0;n<2*120;++n) {
                game.Update(1.0F/120);
                for(const auto& request:game.boss_->EffectEvents())assert(request.kind!=effects::Kind::Ambient);
                game.externalEffects_.ForEachActive([&](const auto& p) {
                    if(p.kind==effects::Kind::Burst) {burst=true;assert(p.animation.ScriptId()==6);}
                });
            }
            assert(burst&&!game.boss_->active&&!game.externalEffects_.ActiveCount()&&!game.externalEffects_.DroppedSpawns());
            // 活跃粒子下结束游戏，素材须等最后一粒子删除后释放。
            boss=std::make_unique<R>();config.phases={{{R::Behavior::FirstNonspell},{{{0,1,WHITE}}}}};boss->StartBoss(config);game.boss_=std::move(boss);
            for(int n=0;n<3*fps;++n)game.Update(1.0F/fps);
            game.EndStage(RunState::GameOver);bool retained=false;int n=0;
            for(;n<300&&game.state_==RunState::Finishing;++n) {
                if(game.externalEffects_.ActiveCount()) {retained=true;assert(game.externalEffects_.Source()&&!game.assets_.Stage().visuals.empty());}
                game.Update(1.0F/120);
            }
            assert(n<300&&retained&&game.state_==RunState::GameOver&&!game.externalEffects_.Source()&&game.assets_.Stage().visuals.empty());
            std::cout<<"Stage6 effects d"<<d<<" at "<<fps<<" FPS: five orbits, dialogue, final burst, pause and resource release; peak "<<peak<<" passed\n";
        }
        game.EnterMainMenu();
    }

    /// @brief 使用正式Extra素材与完整Boss配置，验证附着轨道在五显示FPS、暂停、阈值交接和结果收尾的生命周期。
    static void TestAttachedEffects(Game& game,const std::string& prefix) {
        using F=bosses::FlandreBoss;
        std::vector<Vector3> expected;
        for(int fps:{120,30,60,144,240}) {
            game.EnterMainMenu();game.stageId_=StageId::Extra;game.selectedCharacter_=game.selectedShot_=0;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;
            game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<stage_extra::StageExtraContent>();
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.stage_=StageDirector{};
            game.player_.SetInvulnerability(1000);game.referenceRank_=18;
            auto config=stage_extra::FlandreMainConfig();config.intro={};
            auto boss=std::make_unique<F>();boss->StartBoss(config);game.boss_=std::move(boss);
            std::size_t peak=0;for(int n=0;n<4*fps;++n) {game.Update(1.0F/fps);peak=std::max(peak,game.externalEffects_.ActiveCount());}
            std::vector<Vector3> actual;std::weak_ptr<const effects::AttachmentAnchor> anchor;double age=0;
            game.externalEffects_.ForEachActive([&](const auto& p) {
                age+=p.frame;if(p.kind!=effects::Kind::Attached)return;
                actual.push_back(p.position);anchor=p.attachment;
                assert(p.animation.ScriptId()==18&&p.animation.Sample().sprite==24&&p.animation.Sample().additive&&p.animation.Sample().disableZWrite);
                assert(p.visual==stage_assets::kExternalParticle&&p.animation.Source()==game.externalEffects_.Source()&&!p.released);
            });
            assert(actual.size()==5&&!anchor.expired()&&!game.externalEffects_.DroppedSpawns());
            if(expected.empty())expected=actual;
            else for(std::size_t n=0;n<actual.size();++n) {
                assert(std::abs(actual[n].x-expected[n].x)<=5e-4F&&std::abs(actual[n].y-expected[n].y)<=5e-4F&&std::abs(actual[n].z-expected[n].z)<=5e-4F);
            }
            game.TogglePause();game.Update(.5F);double pausedAge=0;
            game.externalEffects_.ForEachActive([&](const auto& p){pausedAge+=p.frame;});assert(age==pausedAge);game.TogglePause();
            if(fps==120) {
                const auto render=[&] {
                    const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());assert(IsRenderTextureValid(target));
                    BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.Draw();EndTextureMode();EndDrawing();
                    Image image=LoadImageFromTexture(target.texture);auto* colors=LoadImageColors(image);
                    std::vector<Color> pixels(colors,colors+image.width*image.height);UnloadImageColors(colors);UnloadImage(image);UnloadRenderTexture(target);return pixels;
                };
                const auto with=render();auto saved=std::make_unique<effects::ExternalEffectSystem>(game.externalEffects_);
                game.externalEffects_.Clear();const auto without=render();game.externalEffects_=*saved;saved.reset();
                int changed=0;for(std::size_t n=0;n<with.size();++n)changed+=std::memcmp(&with[n],&without[n],sizeof(Color))!=0;assert(changed>100);
                Capture(game,prefix,"attached-first-nonspell");
                // 当帧碰撞越过正生命阈值，保留附着；随后符卡生命归零须立即释放，不能等下一次Actor::Tick。
                game.boss_->health=1799;game.ResolveCollisions();assert(!anchor.expired());
                for(int n=0;n<260;++n)game.Update(1.0F/120);assert(game.boss_->CanTakeDamage());
                game.boss_->health=0;game.ResolveCollisions();assert(anchor.expired());
                for(int n=0;n<32;++n)game.Update(1.0F/120);
                game.externalEffects_.ForEachActive([](const auto& p){assert(p.kind!=effects::Kind::Attached);});
            }
            game.EndStage(RunState::GameOver);assert(game.state_==RunState::Finishing&&anchor.expired());
            bool retained=false;int n=0;
            for(;n<300&&game.state_==RunState::Finishing;++n) {
                if(game.externalEffects_.ActiveCount()) {retained=true;assert(game.externalEffects_.Source()&&!game.assets_.Stage().visuals.empty());}
                game.Update(1.0F/120);
            }
            assert(n<300&&retained&&game.state_==RunState::GameOver&&!game.externalEffects_.Source()&&game.assets_.Stage().visuals.empty());
            std::cout<<"Attached effects "<<fps<<" FPS: five orbits, pause, retirement and resource release; peak "<<peak<<" passed\n";
        }
        game.EnterMainMenu();
    }

    /// @brief 正式单符入口验证独立粒子的像素、暂停、最后一组退场后的资源保留以及直接退出/重开。
    static void TestExternalEffects(Game& game,const std::string& prefix,int firstRoute=0) {
        for(int route=firstRoute;route<2;++route)for(int number:{124,128,130,131}) {
            const auto& card=spell::Cards()[number-1];const int shot=number==124?0:number==128?1:number==130?2:3;
            game.practiceMode_=game.spellPractice_=true;game.challengeMode_=false;
            game.spellSelection_={spell::Page::Difficulty,6,card.group,4,shot};
            game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartRun(false,true);game.player_.SetInvulnerability(1000);
            assert(game.externalEffects_.Source()!=nullptr&&game.externalEffects_.ActiveCount()==0);
            const int sample=number==130?270:330;std::size_t peak=0;
            for(int n=0;n<sample;++n) {game.Update(1.0F/120);peak=std::max(peak,game.externalEffects_.ActiveCount());}
            assert(game.externalEffects_.ActiveCount()>0&&peak<=512&&!game.externalEffects_.DroppedSpawns());
            bool ambient=false;
            game.externalEffects_.ForEachActive([&](const effects::Particle& p) {
                assert(p.animation.Source()==game.externalEffects_.Source()&&p.visual==stage_assets::kExternalParticle);
                ambient|=p.kind==effects::Kind::Ambient;
                assert(p.animation.ScriptId()==(p.kind==effects::Kind::Ambient?19:number==130?17:7)&&p.animation.Sample().additive&&p.animation.Sample().disableZWrite);
            });
            assert(ambient==(number!=130)); // 生存符在120帧隐藏本体；环境时钟冻结，闪光仍独立执行。
            const auto count=game.externalEffects_.ActiveCount();double age=0;
            game.externalEffects_.ForEachActive([&](const auto& p){age+=p.frame;});
            game.TogglePause();game.Update(.5F);double pausedAge=0;
            game.externalEffects_.ForEachActive([&](const auto& p){pausedAge+=p.frame;});
            assert(count==game.externalEffects_.ActiveCount()&&age==pausedAge);game.TogglePause();
            // 同一逻辑时刻只移除独立粒子重绘，确认像素来自新池，不重跑行为或推进动画。
            game.spellCutInTimer_=0;
            const auto render=[&] {
                const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());assert(IsRenderTextureValid(target));
                BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.Draw();EndTextureMode();EndDrawing();
                Image image=LoadImageFromTexture(target.texture);auto* colors=LoadImageColors(image);
                std::vector<Color> pixels(colors,colors+image.width*image.height);UnloadImageColors(colors);UnloadImage(image);UnloadRenderTexture(target);return pixels;
            };
            const auto with=render();auto saved=std::make_unique<effects::ExternalEffectSystem>(game.externalEffects_);
            game.externalEffects_.Clear();const auto without=render();game.externalEffects_=*saved;saved.reset();
            int changed=0;for(std::size_t i=0;i<with.size();++i)changed+=std::memcmp(&with[i],&without[i],sizeof(Color))!=0;
            assert(changed>100);
            Capture(game,prefix,("external-"+std::to_string(number)+"-route"+std::to_string(route)).c_str());
            if(route==0) {
                game.EnterMainMenu();assert(game.externalEffects_.Source()==nullptr&&game.externalEffects_.ActiveCount()==0&&game.assets_.Stage().visuals.empty());
            } else {
                if(number==130)game.EndStage(RunState::GameOver);
                else {assert(game.boss_->CanTakeDamage());game.boss_->health=0;game.ResolveCollisions();}
                bool retained=false,sawBurst=false;int n=0;
                for(;n<600&&game.state_!=RunState::Won&&game.state_!=RunState::GameOver;++n) {
                    game.Update(1.0F/120);peak=std::max(peak,game.externalEffects_.ActiveCount());
                    game.externalEffects_.ForEachActive([&](const auto& p){sawBurst|=p.kind==effects::Kind::Burst;});
                    if(game.state_==RunState::Finishing&&game.externalEffects_.ActiveCount()) {
                        retained=true;assert(!game.assets_.Stage().visuals.empty()&&game.externalEffects_.Source());
                    }
                    assert(!game.externalEffects_.DroppedSpawns()&&!game.bullets_.Pool().DroppedSpawns());
                }
                // 项目单符练习在技能完成即进入结果收尾，不执行整场FinalExit；只等已经出生的粒子。
                assert(n<600&&retained&&!sawBurst);
                assert(!game.externalEffects_.Source()&&!game.externalEffects_.ActiveCount()&&game.assets_.Stage().visuals.empty());
                game.ReturnToSpellPractice();
            }
            std::cout<<"External effects "<<number<<" route "<<route<<" pixels "<<changed<<" peak "<<peak<<" passed\n";
        }
    }

    /// @brief 正式Extra单符入口覆盖13卡×四御札×击破/自然超时，另测生存符Miss/Bomb失败。
    /// 使用生产120Hz与正式素材；记录、主线成绩和命名档隔离，返回前完成练习结束效果及借用资源释放。
    static void TestExtraSpellPractice(Game& game,const std::string& prefix) {
        const auto profile=game.scoreProfile_;const auto stageScores=game.stageHighScores_;const int best=game.bestScore_;
        for(int number=119;number<=131;++number)for(int shot=0;shot<4;++shot)for(int route=0;route<2;++route) {
            const auto& card=spell::Cards()[number-1];
            game.practiceMode_=game.spellPractice_=true;game.challengeMode_=false;
            game.spellSelection_={spell::Page::Difficulty,6,card.group,4,shot};
            game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            const auto before=game.spellRecords_.Get(shot,number);
            game.StartRun(false,true);game.player_.SetInvulnerability(1000);
            assert(game.state_==RunState::Playing&&game.stageId_==StageId::Extra&&game.runDifficulty_==native_combat::Difficulty::Normal);
            assert(game.referenceRank_==18&&game.ScoreDifficultyIndex()==4&&game.boss_->attack==1);
            assert(static_cast<int>(game.player_.Shot())==shot&&game.player_.Power()==Player::kMaxPower);
            assert(game.boss_->visual==(number<=121?stage_extra::kPatchouli:stage_extra::kFlandre));
            bool seen=false,paused=false,interrupted=false,sampled=false;std::size_t peak=0,children=0;int tick=0;
            const bool capture=number==130?route==0:route==1;
            for(;tick<155*120&&game.state_!=RunState::Won;++tick) {
                game.Update(1.0F/120);
                assert(game.state_!=RunState::ScoreSave&&game.state_!=RunState::ScoreName&&game.state_!=RunState::GameOver);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());children=std::max(children,EnemyCount(game));
                if(game.state_!=RunState::Playing)continue;
                assert(game.boss_&&game.boss_->attack==1&&!game.dialogueActive_);
                if(game.boss_->SpellName()) {seen=true;assert(game.boss_->SpellCardNumber()==number);}
                if(!paused&&game.boss_->skillTimer>3) {
                    paused=true;const float clock=game.boss_->skillTimer;const auto count=game.bullets_.Pool().ActiveCount();
                    game.TogglePause();game.Update(.5F);assert(game.boss_->skillTimer==clock&&game.bullets_.Pool().ActiveCount()==count);game.TogglePause();
                }
                if(!sampled&&game.boss_->skillTimer>6) {
                    sampled=true;assert(peak>0);
                    if(number>=122) {
                        const auto auxiliary=game.boss_->AuxiliaryAnimations();
                        assert(auxiliary[0].visual==stage_assets::kOrb&&auxiliary[0].cursor.selection.atlas==battle::Atlas::ExtraEnemies);
                    }
                    if(shot==0&&route==0)Capture(game,prefix,("extra-spell-"+std::to_string(number)).c_str());
                }
                if(number==130&&route==1&&!interrupted&&game.boss_->skillTimer>3.5F) {
                    interrupted=true;
                    if(shot%2)game.ApplyBombEffect();else {game.HandlePlayerHit();game.player_.SetInvulnerability(1000);}
                }
                if(number!=130&&route==1&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>6) {
                    game.boss_->health=0;game.ResolveCollisions();
                }
            }
            if(tick>=155*120)std::cerr<<"Extra spell stalled: card="<<number<<" shot="<<shot<<" route="<<route<<" state="<<int(game.state_)<<'\n';
            assert(tick<155*120&&seen&&paused&&sampled&&game.state_==RunState::Won&&game.spellCaptured_==capture);
            const auto after=game.spellRecords_.Get(shot,number);
            assert(after.attempts==before.attempts+1&&after.captured==before.captured+int(capture));
            assert(after.mainAttempts==before.mainAttempts&&after.mainCaptured==before.mainCaptured);
            assert(!game.pendingEnding_&&game.bestScore_==best&&game.scoreProfile_.endingsSeen==profile.endingsSeen);
            assert(game.stageHighScores_.scores==stageScores.scores&&game.stageHighScores_.difficultyScores==stageScores.difficultyScores);
            for(int mode=0;mode<2;++mode)for(int s=0;s<4;++s)for(int d=0;d<5;++d)
                for(int slot=0;slot<score::kSlots;++slot) {
                    const auto& a=game.scoreProfile_.Get(mode,s,d)[slot];const auto& b=profile.Get(mode,s,d)[slot];
                    assert(a.has_value()==b.has_value());if(a)assert(a->value==b->value&&a->completedAt==b->completedAt&&a->name==b->name);
                }
            assert(game.assets_.Stage().visuals.empty()&&!game.boss_&&!EnemyCount(game)&&!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            game.ReturnToSpellPractice();
            assert(game.state_==RunState::StartSetup&&game.spellSelection_.stage==6&&game.spellSelection_.group==card.group);
            assert(game.spellSelection_.difficulty==4&&game.spellSelection_.character==shot);
            const auto persisted=LoadOrCreateSettings(kSettingsFilename).spellRecords.Get(shot,number);
            assert(persisted.attempts==after.attempts&&persisted.captured==after.captured&&persisted.mainAttempts==after.mainAttempts);
            std::cout<<"Extra spell "<<number<<" shot "<<shot<<" route "<<route<<" capture="<<capture<<" passed; peak "<<peak<<" bullets / "<<children<<" actors\n";
        }
    }

    static void TestStage2(Game& game,const std::string& prefix) {
        for(int character=0;character<2;++character) {
            game.stageId_=StageId::Stage2;game.selectedCharacter_=character;game.selectedShot_=0;
            game.StartPreview();
            bool midboss=false,before=false,after=false,pauseChecked=false,opening=false,nameplate=false;
            std::array<bool,5> phases{};std::size_t peak=0;int frame=0;
            for(;frame<420*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                assert(game.bullets_.Pool().DroppedSpawns()==0&&game.actors_.DroppedSpawns()==0);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(!opening&&frame>=12*60) {opening=true;Capture(game,prefix,"stage2-opening");}
                if(game.dialogueActive_&&game.dialogueTimer_>1) {
                    bool& seen=game.dialogueAfterBattle_ ? after : before;
                    if(!seen) {
                        seen=true;const auto name="stage2-dialogue-"+std::to_string(character)+(game.dialogueAfterBattle_?"-after":"-before");
                        Capture(game,prefix,name.c_str());
                    }
                    float lineStart=0;const int line=game.DialogueIndex(&lineStart);
                    if(!game.dialogueAfterBattle_&&!nameplate&&game.CurrentDialogue()[line].introduce&&
                       game.dialogueTimer_-lineStart>0.75F&&GetScreenWidth()>500) {
                        nameplate=true;
                        Capture(game,prefix,"stage2-nameplate");
                        const int width=GetScreenWidth(),height=GetScreenHeight();SetWindowSize(390,844);
                        Capture(game,prefix,"stage2-dialogue-portrait");SetWindowSize(width,height);
                    }
                }
                if(game.boss_&&game.boss_->active) {
                    if(game.bossEventId_==1)midboss=true;
                    else if(game.boss_->attack>=1&&game.boss_->attack<=5&&game.boss_->skillTimer>5.1F) {
                        const int phase=game.boss_->attack;
                        if(!phases[phase-1]) {
                            phases[phase-1]=true;const auto name="stage2-phase-"+std::to_string(phase);
                            Capture(game,prefix,name.c_str());
                        }
                        if(phase==4&&!pauseChecked) {
                            pauseChecked=true;const auto time=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                            game.TogglePause();game.Update(0.5F);
                            assert(game.stage_.Elapsed()==time&&game.boss_->skillTimer==timer);
                            game.TogglePause();
                        }
                    }
                }
            }
            if(frame>=420*60)std::cerr<<"Stage 2 stalled at "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)<<'\n';
            assert(frame<420*60&&midboss&&before&&after&&pauseChecked);
            for(bool seen:phases)assert(seen);
            assert(game.state_==RunState::Won&&game.assets_.Stage().visuals.empty());
            assert(game.bullets_.Pool().ActiveCount()==0&&EnemyCount(game)==0&&game.actorEffects_.Pool().ActiveCount()==0);
            std::cout<<"Whole Stage 2 character "<<character<<" cleared; peak "<<peak<<" projectiles\n";
        }
        // 击破路线独立于超时：真实总控必须只生成一枚 Bomb，并将正 HP 符卡阈值视为取得。
        game.stageId_=StageId::Stage2;game.selectedCharacter_=game.selectedShot_=0;
        game.StartPreview();game.previewMode_=false;game.stage_=StageDirector{};
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),100);
        for(int tick=0;tick<130;++tick)game.Update(1.0F/120);
        assert(game.boss_->CanTakeDamage());game.boss_->health=0;game.ResolveCollisions();
        for(int tick=0;tick<5&&game.boss_->active;++tick)game.Update(1.0F/120);
        assert(!game.boss_->active&&game.boss_->Defeated());
        int bombItems=0;game.drops_.ForEachActive([&](const Pickup& pickup){bombItems+=pickup.kind==PickupKind::Bomb;});
        assert(bombItems==1);Capture(game,prefix,"stage2-bomb-drop");
        game.drops_.MagnetAllItems();
        for(int tick=0;tick<240;++tick)game.UpdatePickups(1.0F/120);
        assert(game.player_.Bombs()==4);
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),101);
        for(int phase=1;phase<=5;++phase) {
            int ticks=0;
            while(!game.boss_->CanTakeDamage()&&ticks++<600)game.Update(1.0F/120);
            assert(ticks<600&&game.boss_->attack==phase);
            const auto scoreBefore=game.score_;
            game.boss_->health=phase==4 ? 1399 : 0;game.ResolveCollisions();
            if(phase==4)assert(game.score_>scoreBefore&&game.spellBonus_>0&&game.boss_->attack==5);
        }
        for(int tick=0;tick<180&&game.boss_->active;++tick)game.Update(1.0F/120);
        assert(!game.boss_->active&&game.boss_->Defeated());
        for(int number:{11,15,18})assert(game.spellRecords_.Seen(0,number));
        // 跨关切换必须清掉所有借用二面档案的对象，再重新加载一面。
        game.stageId_=StageId::Stage1;game.selectedCharacter_=game.selectedShot_=0;
        game.StartPreview();game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
    }

    /// @brief 三面真实整关覆盖道中等待/对白、入场后开战、六阶段和子演员；击破路线另覆盖道中符卡。
    static void TestStage3(Game& game,const std::string& prefix) {
        for(int character=0;character<2;++character) {
            game.stageId_=StageId::Stage3;game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            bool mid=false,midDialogue=false,before=false,after=false,nameplate=false,children=false,paused=false;
            std::array<bool,6> phases{};std::size_t peak=0;int frame=0;
            for(;frame<550*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                assert(game.bullets_.Pool().DroppedSpawns()==0&&game.actors_.DroppedSpawns()==0);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.dialogueActive_&&game.dialogueTimer_>1) {
                    const bool middle=game.dialogueScene_==DialogueScene::Midboss;
                    bool& seen=middle ? midDialogue : game.dialogueAfterBattle_ ? after : before;
                    if(!seen) {
                        seen=true;const auto name="stage3-dialogue-"+std::to_string(character)+(middle ? "-mid" : game.dialogueAfterBattle_ ? "-after" : "-before");
                        Capture(game,prefix,name.c_str());
                    }
                    if(middle)assert(!game.boss_->active&&game.state_==RunState::Playing);
                    else if(!game.dialogueAfterBattle_)assert(game.boss_->attack==0&&!game.boss_->CanTakeDamage());
                    float start=0;const int line=game.DialogueIndex(&start);
                    if(!middle&&!game.dialogueAfterBattle_&&!nameplate&&game.CurrentDialogue()[line].introduce&&game.dialogueTimer_-start>0.75F) {
                        nameplate=true;Capture(game,prefix,"stage3-nameplate");
                        const int w=GetScreenWidth(),h=GetScreenHeight();SetWindowSize(390,844);Capture(game,prefix,"stage3-portrait");SetWindowSize(w,h);
                    }
                }
                if(game.boss_&&game.boss_->active) {
                    if(game.bossEventId_==1)mid=true;
                    else if(game.boss_->attack>0&&game.boss_->attack<=6&&game.boss_->skillTimer>5.1F) {
                        const int phase=game.boss_->attack;
                        if(!phases[phase-1]) {phases[phase-1]=true;const auto name="stage3-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());}
                        if(phase==3&&EnemyCount(game)>0)children=true;
                        if(phase==5&&!paused) {
                            paused=true;const auto time=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                            game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==time&&game.boss_->skillTimer==timer);game.TogglePause();
                        }
                    }
                }
            }
            if(frame>=550*60)std::cerr<<"Stage 3 stalled at "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)<<'\n';
            assert(frame<550*60&&mid&&midDialogue&&before&&after&&nameplate&&children&&paused);for(bool seen:phases)assert(seen);
            assert(game.state_==RunState::Won&&game.assets_.Stage().visuals.empty()&&EnemyCount(game)==0&&game.bullets_.Pool().ActiveCount()==0);
            std::cout<<"Whole Stage 3 character "<<character<<" cleared; peak "<<peak<<" projectiles\n";
        }
        game.stageId_=StageId::Stage3;game.selectedCharacter_=game.selectedShot_=0;game.StartPreview();
        game.previewMode_=false;game.stage_=StageDirector{};game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),100);
        for(int tick=0;tick<210;++tick)game.Update(1.0F/120);assert(game.boss_->CanTakeDamage());
        game.boss_->health=1299;game.ResolveCollisions();assert(game.boss_->attack==2&&!game.boss_->CanTakeDamage());
        for(int tick=0;tick<245;++tick)game.Update(1.0F/120);assert(game.boss_->CanTakeDamage());
        Capture(game,prefix,"stage3-midboss-spell");game.boss_->health=0;game.ResolveCollisions();
        for(int tick=0;tick<370&&game.boss_->active;++tick)game.Update(1.0F/120);
        assert(!game.boss_->active&&game.boss_->Defeated()&&game.spellRecords_.Seen(0,22));
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),101);game.ResumeStageBoss();
        for(int phase=1;phase<=6;++phase) {
            int ticks=0;while(!game.boss_->CanTakeDamage()&&ticks++<700)game.Update(1.0F/120);
            assert(ticks<700&&game.boss_->attack==phase);
            if(phase==3) {
                for(int tick=0;tick<450;++tick)game.Update(1.0F/120);
                assert(EnemyCount(game)>=6);Capture(game,prefix,"stage3-summons");
            }
            const auto score=game.score_;game.boss_->health=phase==5 ? 1999 : 0;game.ResolveCollisions();
            if(phase==5)assert(game.score_>score&&game.spellBonus_>0&&game.boss_->attack==6);
        }
        for(int tick=0;tick<180&&game.boss_->active;++tick)game.Update(1.0F/120);
        assert(!game.boss_->active&&game.boss_->Defeated());for(int card:{26,32,35})assert(game.spellRecords_.Seen(0,card));
        game.stageId_=StageId::Stage2;game.StartPreview();game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
    }

    /// @brief 新关卡可保留对白开始前的场上对象；默认一面策略仍清除演员/敌弹并保留自机弹。
    static void TestDialogueClearPolicy(Game& game) {
        game.stageId_=StageId::Stage1;game.StartPreview();game.stage_=StageDirector{};
        auto spawn=testing::SpawnAt({1,2,5},stage1::kFairy,100,0.4F);auto* actor=game.actors_.Spawn(spawn);
        auto* enemy=game.bullets_.Spawn(BulletOwner::Enemy,{1,2,3},{},0.1F,WHITE,stage1::kBlueBullet);
        auto* player=game.bullets_.Spawn(BulletOwner::Player,{1,2,3},{},0.1F,WHITE,stage1::kPlayerGoldCard);
        assert(actor&&enemy&&player);game.BeginStageDialogue(1,false,DialogueScene::FinalBoss,DialogueClearPolicy::PreserveWorld);
        assert(actor->active&&enemy->active&&player->active);
        game.BeginStageDialogue(2);assert(!actor->active&&!enemy->active&&player->active);game.EnterMainMenu();
    }

    /// @brief 时停冻结玩家、弹幕、道具和背景，敌人脚本/移动继续；既有持续伤害和 Bomb 区域仍可接触敌人。
    static void TestWorldTimeClock(Game& game,const std::string& prefix) {
        struct ClockBoss final:Boss {
            bool stopped=true;
            ClockBoss() {active=true;phase=1;health=maxHealth=1000;visual=stage1::kRumia;position={0,0,4};}
            bool Update(float seconds,Vector3,BulletSystem&) override {skillTimer+=seconds;return false;}
            bool AdvanceSkill() override {return true;}
            const char* SpellName() const override {return nullptr;}
            const char* DisplayName() const override {return "Clock probe";}
            bool ControlsWorldTime() const override {return true;}
            bool WorldTimeStopped() const override {return stopped;}
            bool CanContactPlayer() const override {return false;}
        };
        game.stageId_=StageId::Stage1;game.selectedCharacter_=game.selectedShot_=0;game.StartPreview();game.stage_=StageDirector{};
        game.boss_=std::make_unique<ClockBoss>();auto* clock=static_cast<ClockBoss*>(game.boss_.get());
        game.player_.SetInvulnerability(0);
#ifdef KOUMAKYOU_DEBUG_FEATURES
        game.debugInvincible_=false; // Release 不含调试无敌字段，正常受伤状态由玩家保护期控制。
#endif
        const auto playerPosition=game.player_.Position();const auto visualAge=game.player_.VisualAge();
        auto* contact=game.bullets_.Spawn(BulletOwner::Enemy,playerPosition,{},0.1F,WHITE,stage1::kBlueBullet);
        auto* enemy=game.bullets_.Spawn(BulletOwner::Enemy,{1,2,3},{1,0,0},0.1F,WHITE,stage1::kBlueBullet);
        auto* laser=game.bullets_.Spawn(BulletOwner::Player,clock->position,{},0.1F,WHITE,stage1::kPlayerGoldCard);
        laser->playerShotKind=PlayerShotKind::Laser;laser->damage=0;laser->damagePerSecond=60;laser->age=0.3F;
        auto spawn=testing::SpawnAt({3,1,5},stage1::kFairy,1000,0.4F);spawn.pattern.movement={.angle=0,.speed=1};
        auto* actor=game.actors_.Spawn(spawn);assert(actor);game.drops_.SpawnNativeItem({2,3,5},0,false);
        PlayerInput input;input.moveRight=true;input.fire=true;input.mouseDelta={4,3};
        for(int tick=0;tick<60;++tick)game.UpdateBattleStep(1.0F/120,input,false,false);
        assert(game.player_.Position().x==playerPosition.x&&game.player_.VisualAge()==visualAge&&game.missCount_==0&&game.graze_==0);
        assert(enemy->position.x==1&&enemy->age==0&&laser->age==0.3F&&contact->active&&!contact->grazed);
        assert(actor->position.x>3.4F&&actor->phaseFrames==0&&actor->animationAge>0.49);
        game.drops_.ForEachActive([](const Pickup& item){assert(item.age==0);});
        if(std::abs(clock->health-976)>=0.01F||game.scenerySeconds_!=0||game.stage_.Elapsed()<=0.49F)
            std::cerr<<"World clock witness health="<<clock->health<<" scenery="<<game.scenerySeconds_<<" timeline="<<game.stage_.Elapsed()<<'\n';
        assert(std::abs(clock->health-976)<0.01F&&game.scenerySeconds_==0&&game.stage_.Elapsed()>0.49F);
        Capture(game,prefix,"world-time-stopped");
        contact->active=false;clock->stopped=false;game.UpdateBattleStep(1.0F/120,input,false,false);
        assert(game.player_.Position().x!=playerPosition.x&&enemy->position.x>1&&game.scenerySeconds_>0&&actor->phaseFrames>0);
        game.bullets_.Reset();game.actors_.Clear();game.drops_.Clear();clock->health=clock->maxHealth=1000000;
        game.player_.ConfigureRun(3,3,false,ShotType::Needle);game.ApplyBombEffect();
        for(int tick=0;tick<300&&game.playerBomb_.Attacks().empty();++tick)game.UpdateBattleStep(1.0F/120,{},false,false);
        assert(!game.playerBomb_.Attacks().empty());clock->position=game.playerBomb_.Attacks().front().volume.origin;
        int contactDamage=0;for(int damage:game.playerBomb_.ContactDamage(clock->position,clock->hitRadius))contactDamage+=damage;assert(contactDamage>0);
        clock->stopped=true;const int bombFrame=game.playerBomb_.Frame();const float health=clock->health;
        auto* frozen=game.bullets_.Spawn(BulletOwner::Enemy,clock->position,{},0.1F,WHITE,stage1::kBlueBullet);
        for(int tick=0;tick<60;++tick)game.UpdateBattleStep(1.0F/120,{},false,false);
        assert(game.playerBomb_.Frame()==bombFrame&&clock->health<health&&frozen->active&&frozen->age==0);
        const auto elapsed=game.stage_.Elapsed();game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==elapsed);game.TogglePause();
        frozen->active=false;clock->stopped=false;for(int tick=0;tick<2;++tick)game.UpdateBattleStep(1.0F/120,{},false,false);
        assert(game.playerBomb_.Frame()>bombFrame);game.EnterMainMenu();assert(!game.WorldTimeStopped()&&game.assets_.Stage().visuals.empty());
    }

    /// @brief 四面两角色整关超时路线及四机体击破路线；覆盖延迟符卡宣告、召唤、激光、暂停与跨关释放。
    static void TestStage4(Game& game,const std::string& prefix) {
        for(int character=0;character<2;++character) {
            game.stageId_=StageId::Stage4;game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            bool mid=false,before=false,after=false,nameplate=false,children=false,paused=false,beams=false;
            std::array<bool,6> phases{};std::size_t peak=0;int frame=0;
            for(;frame<700*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                assert(game.bullets_.Pool().DroppedSpawns()==0&&game.actors_.DroppedSpawns()==0);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                game.bullets_.Pool().ForEachActive([&](const Bullet& bullet){beams|=bullet.owner==BulletOwner::Enemy&&bullet.IsBeam();});
                if(game.dialogueActive_&&game.dialogueTimer_>1) {
                    bool& seen=game.dialogueAfterBattle_ ? after : before;
                    if(!seen) {seen=true;const auto name="stage4-dialogue-"+std::to_string(character)+(game.dialogueAfterBattle_ ? "-after" : "-before");Capture(game,prefix,name.c_str());}
                    if(!game.dialogueAfterBattle_)assert(game.boss_->attack==0&&!game.boss_->CanTakeDamage());
                    float start=0;const int line=game.DialogueIndex(&start);
                    if(!game.dialogueAfterBattle_&&!nameplate&&game.CurrentDialogue()[line].introduce&&game.dialogueTimer_-start>0.75F) {
                        nameplate=true;Capture(game,prefix,"stage4-nameplate");const int w=GetScreenWidth(),h=GetScreenHeight();
                        SetWindowSize(390,844);Capture(game,prefix,"stage4-portrait");SetWindowSize(w,h);
                    }
                }
                if(game.boss_&&game.boss_->active) {
                    if(game.bossEventId_==1) {mid=true;children|=EnemyCount(game)>0;}
                    else if(game.boss_->attack>0&&game.boss_->attack<=6&&game.boss_->skillTimer>5.1F) {
                        const int phase=game.boss_->attack;
                        if(!phases[phase-1]) {phases[phase-1]=true;const auto name="stage4-character-"+std::to_string(character)+"-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());}
                        if(phase==5&&!paused) {
                            paused=true;const auto time=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                            game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==time&&game.boss_->skillTimer==timer);game.TogglePause();
                        }
                    }
                }
            }
            if(frame>=700*60)std::cerr<<"Stage 4 stalled at "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)<<'\n';
            assert(frame<700*60&&mid&&before&&after&&nameplate&&children&&paused&&beams);for(bool seen:phases)assert(seen);
            assert(game.state_==RunState::Won&&game.assets_.Stage().visuals.empty()&&EnemyCount(game)==0&&game.bullets_.Pool().ActiveCount()==0);
            std::cout<<"Whole Stage 4 character "<<character<<" cleared; peak "<<peak<<" projectiles\n";
        }
        constexpr std::array<std::array<int,4>,4> cards{{{39,58,67,78},{46,51,75,78},{50,40,71,82},{57,63,82,75}}};
        for(int shot=0;shot<4;++shot) {
            game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();
            game.previewMode_=false;game.stage_=StageDirector{};game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),100);
            for(int tick=0;tick<65;++tick)game.Update(1.0F/120);assert(game.boss_->CanTakeDamage());
            game.boss_->health=0;game.ResolveCollisions();
            for(int tick=0;tick<10&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());
            game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),101);game.ResumeStageBoss();
            for(int phase=1;phase<=6;++phase) {
                int ticks=0;bool announced=false;
                while(!game.boss_->CanTakeDamage()&&ticks++<700) {
                    game.Update(1.0F/120);
                    if(phase>=5&&!announced&&game.boss_->SpellCardNumber()>0) {announced=true;assert(game.spellAttemptOpen_&&game.spellCutInTimer_>1.9F);}
                }
                assert(ticks<700&&game.boss_->attack==phase);
                for(int tick=0;tick<260;++tick)game.Update(1.0F/120);
                const auto name="stage4-shot-"+std::to_string(shot)+"-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());
                if(phase==5||phase==6)assert(game.spellAttemptOpen_&&announced);
                const auto score=game.score_;game.boss_->health=phase==5 ? 1699 : 0;game.ResolveCollisions();
                if(phase==5)assert(game.score_>score&&game.boss_->attack==6&&!game.boss_->CanTakeDamage());
            }
            for(int tick=0;tick<150&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());for(int card:cards[shot])assert(game.spellRecords_.Seen(shot,card));
        }
        game.stageId_=StageId::Stage3;game.StartPreview();game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
    }


    /// @brief 以四面素材隔离运行各难度技能，与完整整关和正式练习记录路径互补。
    /// firstVariant/lastVariant 为测试技能表的半开区间，支持分组重跑；验证真实 120 Hz 弹体积分、召唤、暂停及卸载，不替代完整组合验收。
    static void TestStage4DifficultyCombat(Game& game,const std::string& prefix,int firstVariant=0,int lastVariant=21) {
        using P=bosses::PatchouliBoss;using B=P::Behavior;
        constexpr std::array<B,20> skills{B::FirstNonspell,B::SecondNonspell,B::AgniShine,B::AgniShineAdvanced,B::AgniRadiance,
            B::SylphyHorn,B::SylphyHornAdvanced,B::GreenStorm,B::LazyTrilithon,B::LazyTrilithonAdvanced,B::TrilithonShake,B::MetalFatigue,B::SilverDragon,B::PrincessUndine,B::BuryInLake,B::LavaCromlech,B::ForestBlaze,B::WaterElf,B::MercuryPoison,B::EmeraldMegalith};
        for(int d=0;d<4;++d)for(int variant=firstVariant;variant<lastVariant;++variant)for(bool damage:{false,true}) {
            if(((variant==3||variant==6||variant==9||variant==14)&&d>=2)||((variant==4||variant==7||variant==10)&&d==0)||((variant==5||variant==8||variant==11||variant==13||variant==15)&&d<2)||(variant==12&&d!=1)||(variant==19&&d==0))continue;
            const auto difficulty=*native_combat::DifficultyFromIndex(d);const int shot=(d+variant)%4;
            game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();
            game.stage_=StageDirector{};game.actors_.Clear();game.bullets_.Reset();game.bossEventId_.reset();
            game.runDifficulty_=difficulty;
            // 素材仍走已经开放的 Normal 加载路径；仅替换被测角色配置，不能据此宣布全关已可玩。
            if(variant==0) {
                auto boss=std::make_unique<bosses::KoakumaBoss>();
                boss->StartBoss(stage4::KoakumaMidbossConfig(static_cast<ShotType>(shot),difficulty));game.boss_=std::move(boss);
            } else {
                auto config=stage4::PatchouliFinalConfig(static_cast<ShotType>(shot));config.difficulty=difficulty;
                config.position={0,0,4.28571415F};config.intro={};config.phases={{{skills[variant-1]},{{{0,1,WHITE}}}}};
                auto boss=std::make_unique<P>();boss->StartBoss(config);game.boss_=std::move(boss);
            }
            // Game每步同步本局rank；只设置Boss会在下一步被覆盖。
            game.referenceRank_=32;
            game.boss_->SetChallengeRank(32);std::size_t peak=0;bool paused=false,sampled=false;int ticks=0;
            const auto label="stage4-combat-d"+std::to_string(d)+"-skill"+std::to_string(variant)+(damage?"-damage":"-timeout");
            for(;ticks<45*120&&game.boss_->active;++ticks) {
                game.Update(1.0F/120);assert(game.state_==RunState::Playing);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(!paused&&game.boss_->skillTimer>3) {
                    paused=true;const auto time=game.boss_->skillTimer;const auto count=game.bullets_.Pool().ActiveCount();
                    game.TogglePause();game.Update(0.5F);assert(game.boss_->skillTimer==time&&game.bullets_.Pool().ActiveCount()==count);game.TogglePause();
                }
                if(!sampled&&game.boss_->skillTimer>6) {
                    sampled=true;assert(peak>0);if(!damage)Capture(game,prefix,label.c_str());
                }
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>10) {game.boss_->health=0;game.ResolveCollisions();}
            }
            assert(ticks<45*120&&paused&&sampled&&!game.boss_->active&&game.boss_->Defeated()==damage);
            game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty()&&!game.boss_&&!EnemyCount(game));
            assert(!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" complete; peak "<<peak<<" projectiles\n";
        }
    }

    /// @brief 通过 Normal 预览借用同一素材，再切换内容身份预验整关；正式记录由 TestStageDifficulties 验收。
    /// firstDifficulty/lastDifficulty 为 0…4 的测试难度半开区间，支持只复现失败难度。
    static void TestStage4PendingDifficulties(Game& game,const std::string& prefix,int firstDifficulty=0,int lastDifficulty=4) {
        for(int d=firstDifficulty;d<lastDifficulty;++d)for(bool damage:{false,true}) {
            const int shot=(d+(damage?2:0))%4;const auto difficulty=*native_combat::DifficultyFromIndex(d);
            game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();
            game.stageContent_=std::make_unique<stage4::Stage4Content>(difficulty);game.runDifficulty_=difficulty;
            game.stage_=StageDirector(game.stageContent_->Events());
            const int count=d==0?4:d==1?6:7;std::array<bool,7> seen{},sampled{};
            bool mid=false,before=false,after=false,paused=false;std::size_t peak=0;int frame=0;
            const auto label="stage4-pending-d"+std::to_string(d)+(damage?"-damage":"-timeout");
            for(;frame<850*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                if(game.bullets_.Pool().DroppedSpawns()||game.actors_.DroppedSpawns())
                    std::cerr<<label<<" overflow frame "<<frame<<" stage "<<game.stage_.Elapsed()<<" attack "<<(game.boss_?game.boss_->attack:0)
                        <<" timer "<<(game.boss_?game.boss_->skillTimer:0)<<" bullets "<<game.bullets_.Pool().ActiveCount()
                        <<" actors "<<EnemyCount(game)<<" bullet drops "<<game.bullets_.Pool().DroppedSpawns()<<" actor drops "<<game.actors_.DroppedSpawns()<<'\n';
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                game.actors_.ForEachActive([&](const behavior::Actor& a){assert(a.difficulty==difficulty);});
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.dialogueActive_) {if(game.dialogueAfterBattle_)after=true;else before=true;}
                if(!game.boss_||!game.boss_->active||game.boss_->attack<1)continue;
                const int skill=game.boss_->attack;
                if(game.boss_->skillTimer>5.1F) {
                    if(game.bossEventId_==1)mid=true;
                    else {
                        assert(skill<=count);seen.at(skill-1)=true;
                        if(skill>=count-(d==0?0:d==1?1:2)&&!sampled.at(skill-1)) {
                            sampled.at(skill-1)=true;const auto name=label+"-skill"+std::to_string(skill);Capture(game,prefix,name.c_str());
                        }
                    }
                }
                if(!paused&&game.boss_->skillTimer>6) {
                    paused=true;const auto elapsed=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                    game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==elapsed&&game.boss_->skillTimer==timer);game.TogglePause();
                }
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>8) {game.boss_->health=0;game.ResolveCollisions();}
            }
            assert(frame<850*60&&mid&&before&&after&&paused&&game.state_==RunState::Won);
            for(int n=0;n<7;++n)assert(seen[n]==(n<count));
            assert(game.assets_.Stage().visuals.empty()&&!EnemyCount(game)&&!game.bullets_.Pool().ActiveCount());
            std::cout<<label<<" complete at "<<frame/60.0F<<" seconds; peak "<<peak<<" projectiles\n";
            game.EnterMainMenu();assert(!game.boss_&&!game.actorEffects_.Pool().ActiveCount());
        }
    }

    /// @brief 四难度道中在本局rank32运行三种结束路线，验证实际弹体、暂停和卸载；奖励另由角色生命周期核对。
    static void TestStage5DifficultyMidboss(Game& game,const std::string& prefix) {
        for(int d=0;d<4;++d)for(int route=0;route<3;++route) {
            const int shot=(d+route)%4;const auto difficulty=*native_combat::DifficultyFromIndex(d);
            game.stageId_=StageId::Stage5;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();
            game.stage_=StageDirector{};game.actors_.Clear();game.bullets_.Reset();game.bossEventId_.reset();game.runDifficulty_=difficulty;
            game.referenceRank_=32;
            auto boss=std::make_unique<bosses::SakuyaBoss>();boss->StartBoss(stage5::SakuyaMidbossConfig(static_cast<ShotType>(shot),difficulty));game.boss_=std::move(boss);
            for(int tick=0;tick<140;++tick)game.Update(1.0F/120);
            game.boss_->ResumeCombat();bool nonspell=false,spell=false,paused=false,sampled=false;std::size_t peak=0;int ticks=0;
            const auto label="stage5-mid-d"+std::to_string(d)+"-route"+std::to_string(route);
            for(;ticks<80*120&&game.boss_->active;++ticks) {
                game.Update(1.0F/120);assert(game.state_==RunState::Playing);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns()&&game.referenceRank_==32);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.boss_->skillTimer>3) {
                    if(game.boss_->attack==1)nonspell=true;
                    else if(game.boss_->SpellName()) {spell=true;assert(game.boss_->SpellCardNumber()==85+d);}
                }
                if(!paused&&game.boss_->skillTimer>4) {
                    paused=true;const auto time=game.boss_->skillTimer;game.TogglePause();game.Update(0.5F);assert(game.boss_->skillTimer==time);game.TogglePause();
                }
                if(!sampled&&game.boss_->skillTimer>7&&(route==0||game.boss_->attack==2)) {sampled=true;Capture(game,prefix,label.c_str());}
                if(game.boss_->CanTakeDamage()&&((route>0&&game.boss_->attack==1&&game.boss_->skillTimer>5)||
                   (route==1&&game.boss_->attack==2&&game.boss_->skillTimer>9))) {game.boss_->health=game.boss_->attack==1?709:0;game.ResolveCollisions();}
            }
            assert(ticks<80*120&&nonspell&&spell==(route>0)&&paused&&sampled&&!game.boss_->active&&game.boss_->Defeated()==(route==1));
            game.EnterMainMenu();assert(!game.boss_&&game.assets_.Stage().visuals.empty()&&!EnemyCount(game));
            assert(!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" complete; peak "<<peak<<" projectiles\n";
        }
    }

    /// @brief N/H/L道中在本局rank32运行三种结束路线，验证实际弹体、暂停和卸载；奖励另由角色生命周期核对。
    static void TestStage6DifficultyMidboss(Game& game,const std::string& prefix) {
        for(int d=1;d<4;++d)for(int route=0;route<3;++route) {
            const int shot=(d+route)%4;const auto difficulty=*native_combat::DifficultyFromIndex(d);
            game.stageId_=StageId::Stage6;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();
            game.stage_=StageDirector{};game.actors_.Clear();game.bullets_.Reset();game.bossEventId_.reset();game.runDifficulty_=difficulty;
            game.referenceRank_=32;
            auto boss=std::make_unique<bosses::SakuyaBoss>();boss->StartBoss(stage6::SakuyaMidbossConfig(static_cast<ShotType>(shot),difficulty));game.boss_=std::move(boss);
            for(int tick=0;tick<140;++tick)game.Update(1.0F/120);
            game.boss_->ResumeCombat();bool nonspell=false,spell=false,paused=false,sampled=false;std::size_t peak=0;int ticks=0;
            const auto label="stage6-mid-d"+std::to_string(d)+"-route"+std::to_string(route);
            for(;ticks<80*120&&game.boss_->active;++ticks) {
                game.Update(1.0F/120);assert(game.state_==RunState::Playing);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns()&&game.referenceRank_==32);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.boss_->skillTimer>3) {
                    if(game.boss_->attack==1)nonspell=true;
                    else if(game.boss_->SpellName()) {spell=true;assert(game.boss_->SpellCardNumber()==100+d);}
                }
                if(!paused&&game.boss_->skillTimer>4) {
                    paused=true;const auto time=game.boss_->skillTimer;game.TogglePause();game.Update(0.5F);assert(game.boss_->skillTimer==time);game.TogglePause();
                }
                if(!sampled&&game.boss_->skillTimer>7&&(route==0||game.boss_->attack==2)) {sampled=true;Capture(game,prefix,label.c_str());}
                if(game.boss_->CanTakeDamage()&&((route>0&&game.boss_->attack==1&&game.boss_->skillTimer>5)||
                   (route==1&&game.boss_->attack==2&&game.boss_->skillTimer>9))) {game.boss_->health=game.boss_->attack==1?std::array{749,999,1099,1099}.at(shot):0;game.ResolveCollisions();}
            }
            assert(ticks<80*120&&nonspell&&spell==(route>0)&&paused&&sampled&&!game.boss_->active&&game.boss_->Defeated()==(route==1));
            game.EnterMainMenu();assert(!game.boss_&&game.assets_.Stage().visuals.empty()&&!EnemyCount(game));
            assert(!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" complete; peak "<<peak<<" projectiles\n";
        }
    }

    /// @brief 五面三段非符或符卡在四难度分别走击破/超时，使用真实内容身份传递召唤难度；不替代正式整关入口。
    static void TestStage5DifficultyCombat(Game& game,const std::string& prefix,bool spells=false) {
        using S=bosses::SakuyaBoss;using B=S::Behavior;
        constexpr std::array skills{B::FirstNonspell,B::SecondNonspell,B::ThirdNonspell};constexpr std::array thresholds{1400,1400,1700};
        for(int d=0;d<4;++d)for(int phase=0;phase<3;++phase)for(bool damage:{false,true}) {
            const int shot=(d+phase)%4;const auto difficulty=*native_combat::DifficultyFromIndex(d);
            game.stageId_=StageId::Stage5;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();
            game.stage_=StageDirector{};game.actors_.Clear();game.bullets_.Reset();game.bossEventId_.reset();game.runDifficulty_=difficulty;
            game.stageContent_=std::make_unique<stage5::Stage5Content>(difficulty);
            game.referenceRank_=32;
            auto config=stage5::SakuyaFinalConfig(static_cast<ShotType>(shot),difficulty);
            const auto id=spells?config.phases[phase].skills[1].id:bosses::BehaviorId{skills[phase]};
            config.phases={{{id},{{{0,1,WHITE}}}}};auto boss=std::make_unique<S>();boss->StartBoss(config);game.boss_=std::move(boss);
            for(int tick=0;tick<140;++tick)game.Update(1.0F/120);game.boss_->ResumeCombat();
            const auto label=std::string(spells?"stage5-spell-d":"stage5-nonspell-d")+std::to_string(d)+"-phase"+std::to_string(phase)+(damage?"-damage":"-timeout");
            std::size_t peak=0;int ticks=0;bool paused=false,sampled=false,children=false,stopped=false;
            for(;ticks<65*120&&game.boss_->active;++ticks) {
                game.Update(1.0F/120);assert(game.state_==RunState::Playing&&game.referenceRank_==32);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());children|=EnemyCount(game)>0;
                if(game.WorldTimeStopped()) {stopped=true;assert(!game.boss_->CanTakeDamage()&&!game.boss_->CanBeTargeted());}
                if(spells&&game.boss_->SpellName())assert(game.boss_->SpellCardNumber()==89+phase*4+d);
                game.actors_.ForEachActive([&](const behavior::Actor& a){assert(a.difficulty==difficulty);});
                if(!paused&&game.boss_->skillTimer>4) {
                    paused=true;const auto time=game.boss_->skillTimer;game.TogglePause();game.Update(0.5F);assert(game.boss_->skillTimer==time);game.TogglePause();
                }
                if(!sampled&&game.boss_->skillTimer>9) {sampled=true;assert(peak>0);if(!damage)Capture(game,prefix,label.c_str());}
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>12) {game.boss_->health=spells?0:thresholds[phase]-1;game.ResolveCollisions();}
            }
            assert(ticks<65*120&&paused&&sampled&&!game.boss_->active&&game.boss_->Defeated()==damage&&(spells?stopped:phase==2||children));
            game.EnterMainMenu();assert(!game.boss_&&game.assets_.Stage().visuals.empty()&&!EnemyCount(game));
            assert(!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" complete; peak "<<peak<<" projectiles\n";
        }
    }

    /// @brief 已接入符卡的隔离Game预验，覆盖四机体、击破/超时、实际Bomb、子演员/激光与真实后继保护。
    /// 结束后停止在各自后继非符，不将占位退场或缺失的其余符卡计入完整Extra验收。
    static void TestExtraFlandreSpell(Game& game,const std::string& prefix,bosses::FlandreBoss::Behavior skill,bool bombOnly=false) {
        using F=bosses::FlandreBoss;
        TestUiFont(game);
        const bool cranberry=skill==F::Behavior::CranberryTrap;
        const bool four=skill==F::Behavior::FourOfAKind;
        const bool kagome=skill==F::Behavior::KagomeKagome;
        const bool maze=skill==F::Behavior::MazeOfLove;
        const bool starbow=skill==F::Behavior::StarbowBreak;
        const bool catadioptric=skill==F::Behavior::Catadioptric;
        const bool clock=skill==F::Behavior::ClockOfThePast;
        assert(cranberry||four||kagome||maze||starbow||catadioptric||clock||skill==F::Behavior::Laevatein);
        for(int shot=0;shot<4;++shot)for(bool damage:{false,true})for(bool bomb:{false,true}) {
            if(bombOnly&&(shot!=0||damage||!bomb))continue;
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160});
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.player_.SetInvulnerability(1000);
            game.referenceRank_=32;
            // 普通演员的UpdateActor也必须遵循独立辅助外观，不能只修Boss的动画同步路径。
            auto* probe=game.actors_.Spawn({.position={0,0,5},.pattern={.movement={.speed=0},.initialAnimation=160},
                .visual=stage_assets::kBoss,.retireOutside=false});assert(probe);
            probe->AnimateAuxiliary(0,18,stage_assets::kOrb);game.UpdateActor(*probe,1.0F/120);
            assert(probe->auxiliaryAnimations[0].cursor.selection.atlas==battle::Atlas::ExtraEnemies);game.actors_.Clear();
            bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.defeatBase=stage_assets::kNativeDefeat,
                .configure=+[](behavior::Actor& actor,bool spatial) {
                    F::Behaviors::ConfigurePractice(actor,spatial);
                    // 单符夹具恢复源37入场遗留的光圈；正式关底由Entrance启动，符卡本身不重启它。
                    actor.AnimateAuxiliary(0,18,stage_assets::kOrb);
                },.shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
                .phases={{{skill,cranberry?F::Behavior::SecondNonspell:four?F::Behavior::FourthNonspell:kagome?F::Behavior::FifthNonspell:maze?F::Behavior::SixthNonspell:starbow?F::Behavior::SeventhNonspell:catadioptric?F::Behavior::EighthNonspell:clock?F::Behavior::SurvivalPrelude:F::Behavior::ThirdNonspell},{{{0,0.5F,PINK},{1,0.5F,WHITE}}}}}};
            auto boss=std::make_unique<F>();boss->StartBoss(config);game.boss_=std::move(boss);
            const auto label=std::string(cranberry?"extra-cranberry-shot":four?"extra-four-kind-shot":kagome?"extra-kagome-shot":maze?"extra-maze-shot":starbow?"extra-starbow-shot":catadioptric?"extra-catadioptric-shot":clock?"extra-clock-shot":"extra-laevatein-shot")+std::to_string(shot)+(damage?"-damage":"-timeout")+(bomb?"-bomb":"-plain");
            bool paused=false,sampled=false,startedBomb=false,sawBat=false,recovered=false,sawChild=false,sawSword=false,swordCaptured=false,killedClone=false,sawReleased=false,sawMazeWall=false;
            bool sawClusterLarge=false,sawClusterSmall=false;
            std::size_t peak=0,childPeak=0,beamPeak=0;int ticks=0;
            for(;ticks<((starbow||catadioptric||clock)?72:(four||kagome||maze)?67:57)*120&&game.boss_->attack==1;++ticks) {
                game.Update(1.0F/120);assert(game.state_==RunState::Playing&&game.referenceRank_==32);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                if(game.boss_->attack!=1)break;
                assert(game.boss_->SpellCardNumber()==(cranberry?122:four?124:kagome?125:maze?126:starbow?127:catadioptric?128:clock?129:123));
                const auto& auxiliary=game.boss_->AuxiliaryAnimations()[0];
                assert(auxiliary.script==18&&auxiliary.visual==stage_assets::kOrb&&auxiliary.cursor.selection.atlas==battle::Atlas::ExtraEnemies);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                childPeak=std::max(childPeak,EnemyCount(game));
                if(!cranberry&&!four&&!kagome&&!maze&&!starbow&&!catadioptric&&!clock) {
                    assert(!EnemyCount(game));std::size_t beams=0;
                    game.bullets_.Pool().ForEachActive([&](const Bullet& b){if(b.IsBeam()) {
                        ++beams;sawSword=true;assert(b.beamAnimationScript==1&&b.animation.selection.atlas==battle::Atlas::BeamBullets);
                        assert(std::isfinite(b.Direction().y)&&b.radius>0);
                    }});
                    beamPeak=std::max(beamPeak,beams);
                    if(!swordCaptured&&game.boss_->skillTimer>2.8F) {
                        swordCaptured=true;assert(beams>=2);if(shot==0&&!damage)Capture(game,prefix,(label+"-sword").c_str());
                    }
                }
                game.actors_.ForEachActive([&](const behavior::Actor& child){
                    sawChild=true;
                    if(four) {
                        assert(child.health>0&&child.health<=1200&&child.contactEnabled&&child.visual==stage_assets::kBoss);
                        assert(child.animation.selection.atlas==battle::Atlas::Flandre&&child.deathEffect==2&&child.sourceItem==2&&child.scoreValue==10);
                        assert(child.damageable==(child.phaseFrames>60));
                    } else if(kagome||starbow) {
                        assert(!child.damageable&&!child.contactEnabled&&!child.interactable&&!child.visible&&(starbow?child.health>=102&&child.health<=114:child.health==1200)&&child.visual==stage_assets::kBoss);
                        assert(child.animation.selection.atlas==battle::Atlas::Flandre&&child.animationScript==167&&child.scoreValue==10);
                        assert(child.animation.player.Sample().visible&&child.animation.player.Sample().LogicalSize().x==64);
                        if(starbow)assert(child.sourceItem==-2);
                    } else {
                        assert(!child.damageable&&!child.contactEnabled&&child.health>0&&child.visual==stage_assets::kOrb);
                        assert(child.animation.selection.atlas==battle::Atlas::ExtraEnemies&&child.animationScript==16);
                        if(clock)assert(child.visible&&!child.interactable&&child.health==10&&child.sourceItem==-2&&child.hitRadius==1.25F);
                    }
                });
                if(kagome)game.bullets_.Pool().ForEachActive([&](const Bullet& b){if(b.nativeType==1&&b.nativeColor==10) {
                    sawReleased=true;assert(BulletSystem::Length(b.acceleration)>1.6F);
                    assert(b.animation.selection.atlas==battle::Atlas::Bullets);
                    assert(b.animation.selection.spriteOffset==(b.age<b.spawnVisualSeconds?3:10));
                }});
                if(maze) {
                    assert(!EnemyCount(game));
                    game.bullets_.Pool().ForEachActive([&](const Bullet& b){
                        if(b.owner!=BulletOwner::Enemy)return;
                        assert(!b.IsBeam()&&(b.nativeType==2||b.nativeType==3));
                        if(b.nativeType==3) {sawMazeWall=true;assert(b.nativeColor==6||b.nativeColor==2);}
                    });
                }
                if(starbow)game.bullets_.Pool().ForEachActive([&](const Bullet& b){
                    if(b.owner!=BulletOwner::Enemy)return;
                    assert(!b.IsBeam()&&b.nativeType==3&&b.sampleMotionAtStart);
                    assert(b.nativeColor==2||b.nativeColor==4||b.nativeColor==6||b.nativeColor==8||b.nativeColor==11||b.nativeColor==13||b.nativeColor==14);
                    assert(std::isfinite(b.position.y)&&std::isfinite(b.Velocity().z)&&b.acceleration.z<0&&b.acceleration.y==0);
                    assert(b.animation.selection.atlas==battle::Atlas::Bullets);
                });
                if(catadioptric) {
                    assert(!EnemyCount(game));
                    game.bullets_.Pool().ForEachActive([&](const Bullet& b){
                        if(b.owner!=BulletOwner::Enemy)return;
                        assert(!b.IsBeam()&&b.sampleMotionAtStart&&!b.reflectLowerZ&&b.bouncesRemaining>=0&&b.bouncesRemaining<=2);
                        assert(std::isfinite(b.position.y)&&std::isfinite(b.Velocity().z));
                        if(b.nativeType==9) {sawClusterLarge=true;assert(b.nativeColor==1&&b.animation.selection.atlas==battle::Atlas::BulletEffects);}
                        else {sawClusterSmall=true;assert((b.nativeType==6&&b.nativeColor==3||b.nativeType==1&&b.nativeColor==6)&&b.animation.selection.atlas==battle::Atlas::Bullets);}
                    });
                }
                if(clock) {
                    std::size_t beams=0;game.bullets_.Pool().ForEachActive([&](const Bullet& b) {
                        if(b.owner!=BulletOwner::Enemy)return;
                        if(b.IsBeam()) {
                            ++beams;sawSword=true;assert(b.beamAnimationScript==1&&b.animation.selection.atlas==battle::Atlas::BeamBullets);
                            assert(std::abs(BulletSystem::Length(b.Direction())-1)<1e-5F&&b.radius>0&&std::isfinite(b.position.y));
                        } else assert(b.nativeType==3&&b.nativeColor==2&&b.animation.selection.atlas==battle::Atlas::Bullets);
                    });beamPeak=std::max(beamPeak,beams);
                    if(!swordCaptured&&beams==8&&game.boss_->skillTimer>2.8F) {
                        swordCaptured=true;if(shot==0&&!damage)Capture(game,prefix,(label+"-cross").c_str());
                    }
                }
                if(!paused&&game.boss_->skillTimer>2.5F) {
                    paused=true;const auto clock=game.boss_->skillTimer;const auto count=game.bullets_.Pool().ActiveCount();
                    game.TogglePause();game.Update(0.5F);assert(game.boss_->skillTimer==clock&&game.bullets_.Pool().ActiveCount()==count);game.TogglePause();
                }
                if(bomb&&!startedBomb&&game.boss_->skillTimer>3) {
                    startedBomb=true;game.ApplyBombEffect();game.referenceRank_=32;
                }
                if(bomb&&game.playerBomb_.Active()&&game.boss_->skillTimer>3.1F) {
                    if(!sawBat&&shot==0&&!damage)Capture(game,prefix,(label+"-bat").c_str());
                    sawBat=true;assert(!game.boss_->CanBeTargeted()&&game.boss_->animationScript==165);
                }
                if(sawBat&&!game.playerBomb_.Active()&&game.boss_->CanTakeDamage())recovered=true;
                if(!sampled&&game.boss_->skillTimer>12) {
                    sampled=true;assert(peak>0&&(clock?childPeak>=2&&beamPeak==8:catadioptric?sawClusterLarge&&sawClusterSmall:maze?sawMazeWall:cranberry||kagome||starbow?childPeak>=2:four?childPeak==3:beamPeak>=2));if(shot==0&&!damage)Capture(game,prefix,label.c_str());
                }
                if(four&&damage&&!bomb&&!killedClone&&game.boss_->skillTimer>14) {
                    const auto score=game.score_;const auto pickups=PickupCount(game);const auto count=EnemyCount(game);
                    bool selected=false;game.actors_.ForEachActive([&](behavior::Actor& child){if(!selected) {selected=true;assert(child.defeatVisual==stage_assets::kNativeDefeat+2);child.health=0;}});
                    assert(selected&&count==3);game.ResolveCollisions();killedClone=true;
                    assert(EnemyCount(game)==2&&game.score_==score+10&&PickupCount(game)==pickups+1);
                }
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>18) {game.boss_->health=0;game.ResolveCollisions();}
            }
            assert(ticks<((starbow||catadioptric||clock)?72:(four||kagome||maze)?67:57)*120&&paused&&sampled&&(clock?sawChild&&sawSword&&swordCaptured:catadioptric?sawClusterLarge&&sawClusterSmall:maze?sawMazeWall:(cranberry||four||kagome||starbow)?sawChild:sawSword&&swordCaptured)&&(!bomb||(startedBomb&&sawBat&&recovered)));
            if(kagome)assert(sawReleased);
            if(four&&damage&&!bomb)assert(killedClone);
            assert(game.boss_->attack==2&&game.boss_->health==(clock?1000:(kagome||maze||starbow||catadioptric)?10000:8000)&&!game.boss_->CanTakeDamage());
            game.Update(1.0F/120);assert(!EnemyCount(game)&&!game.boss_->CanTakeDamage());
            game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty()&&!game.boss_&&!EnemyCount(game));
            assert(!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" verified; peak "<<peak<<" projectiles / "<<childPeak<<" emitters / "<<beamPeak<<" beams\n";
        }
    }

    /// @brief 第九符在四机体上运行无失误/Bomb/Miss三条真实Game路径；生存成功、隐藏绘制和后继清场分别断言。
    /// 使用隔离Extra素材，停止在真实91前奏；不将其视为正式Extra关底已验收。
    static void TestExtraSurvival(Game& game,const std::string& prefix) {
        using F=bosses::FlandreBoss;
        for(int shot=0;shot<4;++shot)for(int failure=0;failure<3;++failure) {
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160});
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.player_.SetInvulnerability(1000);
            game.referenceRank_=32;
            const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.defeatBase=stage_assets::kNativeDefeat,
                .configure=+[](behavior::Actor& actor,bool spatial) {F::Behaviors::ConfigurePractice(actor,spatial);actor.AnimateAuxiliary(0,18,stage_assets::kOrb);},
                .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
                .phases={{{F::Behavior::AndThenWillThereBeNone,F::Behavior::FinalPrelude},{{{0,.5F,PINK},{1,.5F,WHITE}}}}}};
            auto boss=std::make_unique<F>();boss->StartBoss(config);game.boss_=std::move(boss);
            const auto previous=game.spellRecords_.Get(shot,130);
            // 只为测试打开一次真实记录，后续更新保留预览以隔离场景；成绩写入main已创建的临时目录。
            game.previewMode_=false;game.BeginSpellRecord();game.previewMode_=true;
            assert(game.spellAttemptOpen_&&game.spellAttemptNumber_==130);
            const auto label="extra-survival-shot"+std::to_string(shot)+(failure==0?"-plain":failure==1?"-bomb":"-miss");
            bool paused=false,sampled=false,interrupted=false,bat=false,recovered=false,borders=false,hiddenPixels=false;
            std::size_t peak=0,childPeak=0;Bullet* marker=nullptr;std::uint64_t generation=0;int tick=0;
            for(;tick<=86*120+2&&game.boss_->attack==1;++tick) {
                if(!marker&&game.boss_->skillTimer>85.9F) {
                    marker=game.bullets_.Spawn(BulletOwner::Enemy,{-4,4,4},{},.1F,WHITE,stage_assets::kNativeBulletBase+3*16+2);
                    assert(marker);marker->nativeType=3;marker->nativeColor=2;generation=marker->generation;
                }
                game.Update(1.0F/120);assert(game.state_==RunState::Playing);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                if(game.boss_->attack!=1)break;
                assert(game.boss_->SpellCardNumber()==130&&game.boss_->health==1000&&!game.boss_->CanTakeDamage());
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());childPeak=std::max(childPeak,EnemyCount(game));
                if(game.boss_->skillTimer>2)assert(!game.boss_->BodyVisible()&&!game.boss_->CanBeTargeted()&&!game.boss_->CanContactPlayer());
                game.actors_.ForEachActive([](const behavior::Actor& actor) {
                    assert(!actor.visible&&!actor.interactable&&!actor.damageable&&!actor.contactEnabled&&actor.health>=2&&actor.health<=4);
                    assert(actor.animation.selection.atlas==battle::Atlas::ExtraEnemies&&actor.animationScript==16&&actor.stageClearResponse==behavior::StageClearResponse::Remove);
                });
                game.bullets_.Pool().ForEachActive([&](const Bullet& b) {
                    if(b.owner!=BulletOwner::Enemy)return;
                    assert(!b.IsBeam()&&b.nativeType==3&&b.animation.selection.atlas==battle::Atlas::Bullets);
                    if(b.polarMotion) {borders=true;assert(b.nativeColor==13||b.nativeColor==14);}
                });
                if(!paused&&game.boss_->skillTimer>2.5F) {
                    paused=true;const float clock=game.boss_->skillTimer;const auto count=game.bullets_.Pool().ActiveCount();
                    game.TogglePause();game.Update(.5F);assert(game.boss_->skillTimer==clock&&game.bullets_.Pool().ActiveCount()==count);game.TogglePause();
                }
                if(failure&&!interrupted&&game.boss_->skillTimer>3) {
                    interrupted=true;
                    if(failure==1)game.ApplyBombEffect();else {game.HandlePlayerHit();game.player_.SetInvulnerability(1000);}
                    game.referenceRank_=32;
                }
                if(failure==1&&game.playerBomb_.Active()&&game.boss_->skillTimer>3.1F) {bat=true;assert(game.boss_->animationScript==165&&!game.boss_->BodyVisible());}
                if(bat&&!game.playerBomb_.Active()&&game.boss_->animationScript!=165) {recovered=true;assert(!game.boss_->CanBeTargeted());}
                if(!sampled&&game.boss_->skillTimer>12) {
                    sampled=true;assert(peak>0&&childPeak>0);if(shot==0)Capture(game,prefix,label.c_str());
                    if(shot==0&&failure==0) {
                        const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
                        const auto render=[&] {
                            BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.DrawWorld();EndTextureMode();EndDrawing();
                            Image result=LoadImageFromTexture(target.texture);ImageFlipVertical(&result);return result;
                        };
                        Image hidden=render();auto saved=std::move(game.boss_);Image absent=render();game.boss_=std::move(saved);
                        for(int y=0;y<hidden.height;++y)for(int x=0;x<hidden.width;++x) {
                            const auto a=GetImageColor(hidden,x,y),b=GetImageColor(absent,x,y);assert(a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a);
                        }
                        hiddenPixels=true;UnloadImage(hidden);UnloadImage(absent);UnloadRenderTexture(target);
                    }
                }
                if(game.boss_->skillTimer>50&&game.boss_->skillTimer<50.01F&&shot==0)Capture(game,prefix,(label+"-borders").c_str());
            }
            assert(tick<=86*120+2&&game.boss_->attack==2&&paused&&sampled&&borders&&childPeak>=3);
            assert(failure!=1||interrupted&&bat&&recovered);assert(shot!=0||failure!=0||hiddenPixels);
            assert(marker&&marker->active&&marker->generation==generation&&game.boss_->health==6000&&!game.boss_->CanTakeDamage());
            assert(game.spellBonus_==(failure==0?200000:0)&&!game.spellAttemptOpen_);
            const auto& record=game.spellRecords_.Get(shot,130);
            assert(record.mainAttempts==previous.mainAttempts+1&&record.mainCaptured==previous.mainCaptured+(failure==0?1:0));
            game.Update(1.0F/120);assert(!game.bullets_.Pool().ActiveCount()&&!EnemyCount(game)&&game.boss_->BodyVisible()&&!game.boss_->CanTakeDamage());
            for(int step=0;step<175;++step) {game.Update(1.0F/120);assert(game.boss_->attack==2&&!game.boss_->CanTakeDamage());}
            game.EnterMainMenu();assert(!game.boss_&&game.assets_.Stage().visuals.empty()&&!EnemyCount(game)&&!game.bullets_.Pool().ActiveCount());
            std::cout<<label<<" verified; peak "<<peak<<" projectiles / "<<childPeak<<" hidden pursuers; capture "<<(failure==0)<<'\n';
        }
    }

    /// @brief 最终符在四机体上覆盖击破/超时及实际Bomb；真实记录与击破/超时退场分支分开结算。
    /// 仍借用隔离Extra素材，正式关卡的MSG/时间轴和整场结束另行验收。
    static void TestExtraRipples(Game& game,const std::string& prefix,bool effectsOnly=false) {
        using F=bosses::FlandreBoss;
        for(int shot=0;shot<4;++shot)for(bool damage:{false,true})for(bool bomb:{false,true}) {
            if(effectsOnly&&(!damage||bomb))continue;
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160});
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.player_.SetInvulnerability(1000);
            game.referenceRank_=32;
            const bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.defeatBase=stage_assets::kNativeDefeat,
                .configure=+[](behavior::Actor& actor,bool spatial) {F::Behaviors::ConfigurePractice(actor,spatial);actor.AnimateAuxiliary(0,18,stage_assets::kOrb);},
                .shot=static_cast<ShotType>(shot),.defeated=F::Behavior::FinalExit,.retreat=F::Behavior::FinalExit,
                .phases={{{F::Behavior::RipplesOf495Years},{{{0,1,PINK}}}}},.defeatScore=10000,.retreatScore=100};
            auto boss=std::make_unique<F>();boss->StartBoss(config);game.boss_=std::move(boss);
            // 用真实Director挂起一个异步事件，验证攻击结算不会提前报告整场结束。
            const std::array<StageEvent,1> events{{{.action=ShowMessage{},.id=0xe131}}};
            game.stage_=StageDirector(events);game.stage_.Update(0,[](const StageEvent&){return false;});game.bossEventId_=0xe131;
            const auto previous=game.spellRecords_.Get(shot,131);
            game.previewMode_=false;game.BeginSpellRecord();game.previewMode_=true;
            assert(game.spellAttemptOpen_&&game.spellAttemptNumber_==131);
            const auto label="extra-ripples-shot"+std::to_string(shot)+(damage?"-damage":"-timeout")+(bomb?"-bomb":"-plain");
            bool paused=false,sampled=false,startedBomb=false,bat=false,recovered=false,lowHealth=false,late=false;
            std::size_t peak=0;int tick=0;
            for(;tick<=150*120+2&&static_cast<F&>(*game.boss_).CurrentSkill();++tick) {
                game.Update(1.0F/120);assert(game.state_==RunState::Playing);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns()&&!EnemyCount(game));
                assert(game.bossEventId_==0xe131);
                if(!static_cast<F&>(*game.boss_).CurrentSkill())break;
                assert(game.boss_->SpellCardNumber()==131&&game.boss_->BodyVisible()&&game.boss_->health>0);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                game.bullets_.Pool().ForEachActive([&](const Bullet& b) {
                    if(b.owner!=BulletOwner::Enemy)return;
                    assert(!b.IsBeam()&&b.nativeType==2&&b.nativeColor==6&&b.animation.selection.atlas==battle::Atlas::Bullets);
                    assert(b.bouncesRemaining>=0&&b.bouncesRemaining<=1&&!b.reflectLowerZ&&b.sampleMotionAtStart&&b.spawnVisualSeconds==0);
                    assert(std::isfinite(b.position.y)&&std::isfinite(b.Velocity().z));
                    if(game.boss_->skillTimer>121)late=true;
                });
                if(!paused&&game.boss_->skillTimer>2.5F) {
                    paused=true;const auto clock=game.boss_->skillTimer;const auto count=game.bullets_.Pool().ActiveCount();
                    game.TogglePause();game.Update(.5F);assert(game.boss_->skillTimer==clock&&game.bullets_.Pool().ActiveCount()==count);game.TogglePause();
                }
                if(bomb&&!startedBomb&&game.boss_->skillTimer>3.5F) {startedBomb=true;game.ApplyBombEffect();game.referenceRank_=32;}
                if(bomb&&game.playerBomb_.Active()&&game.boss_->skillTimer>3.6F) {bat=true;assert(!game.boss_->CanBeTargeted()&&game.boss_->animationScript==165);}
                if(bat&&!game.playerBomb_.Active()&&game.boss_->CanTakeDamage())recovered=true;
                if(damage&&!lowHealth&&game.boss_->skillTimer>16) {lowHealth=true;game.boss_->health=1500;}
                if(!sampled&&game.boss_->skillTimer>24) {sampled=true;assert(peak>0);if(shot==0&&!bomb)Capture(game,prefix,label.c_str());}
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>25) {game.boss_->health=0;game.ResolveCollisions();}
            }
            assert(tick<=150*120+2&&paused&&sampled&&peak>0&&(damage?lowHealth:late)&&(!bomb||startedBomb&&bat&&recovered));
            assert(game.boss_->active&&game.boss_->Defeated()==damage&&!game.boss_->CanTakeDamage()&&!game.boss_->CanContactPlayer());
            assert(!game.spellAttemptOpen_&&(game.spellBonus_>0)==(damage&&!bomb));
            const auto record=game.spellRecords_.Get(shot,131);
            assert(record.mainAttempts==previous.mainAttempts+1&&record.mainCaptured==previous.mainCaptured+(damage&&!bomb?1:0));
            const auto score=game.score_;bool exitPause=false,sawBurst=false;int exitTicks=0;
            for(;exitTicks<=123&&game.boss_->active;++exitTicks) {
                game.Update(1.0F/120);assert(!game.boss_->CanTakeDamage()&&!game.boss_->CanContactPlayer());
                game.externalEffects_.ForEachActive([&](const auto& p){sawBurst|=p.kind==effects::Kind::Burst;});
                assert(!game.externalEffects_.DroppedSpawns());
                assert(game.bossEventId_.has_value()==game.boss_->active);
                if(!exitPause&&exitTicks==30) {
                    exitPause=true;const auto clock=game.boss_->skillTimer;game.TogglePause();game.Update(.5F);assert(game.boss_->skillTimer==clock);game.TogglePause();
                    if(shot==0&&!bomb)Capture(game,prefix,(label+"-exit").c_str());
                }
            }
            assert(exitTicks==(damage?121:122)&&exitPause&&!game.boss_->active&&game.boss_->Defeated()==damage);
            assert(!game.bossEventId_&&!game.stage_.EndEvent(0xe131));
            assert(game.score_==score+(damage?10000:100)&&!game.bullets_.Pool().ActiveCount()&&!EnemyCount(game));
            assert(game.spellRecords_.Get(shot,131).mainCaptured==record.mainCaptured);
            if(effectsOnly) {
                assert(sawBurst&&game.externalEffects_.ActiveCount()>0);
                game.EndStage(RunState::Won);assert(game.state_==RunState::Finishing&&!game.assets_.Stage().visuals.empty());
                for(int step=0;step<240&&game.state_==RunState::Finishing;++step)game.Update(1.0F/120);
                assert(game.state_==RunState::Won&&!game.externalEffects_.Source()&&!game.externalEffects_.ActiveCount()&&game.assets_.Stage().visuals.empty());
            }
            game.EnterMainMenu();assert(!game.boss_&&game.assets_.Stage().visuals.empty()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" verified; peak "<<peak<<" projectiles; capture "<<(damage&&!bomb)<<"; exit "<<exitTicks<<" half steps\n";
        }
    }

    /// @brief 真实Game播放两角色六段静态对白；完整等待与异步放行、全部汉字、立绘及三音乐精确时刻分别检查。
    /// 使用独立Midboss事件防止通用战后结算掩盖MSG9/11间隔；正式Extra流程仍须消费对应元数据。
    static void TestExtraDialogue(Game& game,const std::string& prefix,int onlySection=-1) {
        using Section=stage_extra::DialogueSection;
        constexpr std::array sections{Section::MainBefore,Section::MainAfter,Section::Midboss,Section::EncoreBefore,Section::EncoreAfter,Section::EncoreInterlude};
        for(int shot:{0,2})for(std::size_t section=0;section<sections.size();++section) {
            if(onlySection>=0&&static_cast<int>(section)!=onlySection)continue;
            game.EnterMainMenu();game.previewMode_=true;game.captureCursor_=false;game.stageClearShown_=false;
            game.player_.Reset();game.player_.ConfigureRun(3,3,false,static_cast<ShotType>(shot));
            game.stageContent_=std::make_unique<ExtraDialogueFixtureContent>(sections[section]);
            game.assets_.LoadStage(game.stageContent_->Assets());game.assets_.PlayStageTrack(0);
            game.state_=RunState::Playing;game.battleTicks_=0;game.dialogueActive_=false;
            const bool after=sections[section]==Section::MainAfter||sections[section]==Section::EncoreAfter;
            const std::array<StageEvent,2> events{{
                {.action=StartDialogue{after,DialogueScene::Midboss,DialogueClearPolicy::PreserveWorld},.id=1},
                {.action=ShowMessage{"Extra dialogue released",1},.afterEndOf=1}
            }};
            game.stage_=StageDirector(events);game.Update(1.0F/120);assert(game.dialogueActive_&&game.dialogueTimer_==0);
            const auto& script=stage_extra::ExtraDialogue(sections[section],static_cast<ShotType>(shot));
            const auto StepTo=[&](double frame) {while(game.dialogueTimer_*60+1e-6<frame&&game.dialogueActive_)game.Update(1.0F/120);};
            const auto CheckText=[&](const char* text) {
                for(const char* cursor=text;*cursor;) {
                    int bytes=0;const int codepoint=GetCodepointNext(cursor,&bytes);assert(bytes>0);cursor+=bytes;
                    for(const auto font:{game.assets_.Common().uiFont,game.assets_.Common().hudFont})
                        assert(font.glyphs[GetGlyphIndex(font,codepoint)].value==codepoint);
                }
            };
            float start=0;
            for(std::size_t line=0;line<script.lines.size();++line) {
                const auto& text=script.lines[line];CheckText(text.speaker);CheckText(text.text);CheckText(text.secondLine);
                // 源战后存在只显示1内容帧的真实插话；采样不能假设每句至少持续两帧。
                const double sample=std::min(double(text.textDelay+text.line2Delay)*60+2,double(text.duration)*60-.5);
                StepTo(double(start)*60+sample);game.UpdateUiMotion(0);
                float actualStart=0;
                if(game.DialogueIndex(&actualStart)!=static_cast<int>(line))std::cerr<<"Dialogue index mismatch: shot "<<shot<<" section "<<section<<" line "<<line<<" at "<<game.dialogueTimer_*60<<" frames, actual "<<game.DialogueIndex()<<" active "<<game.dialogueActive_<<"\n";
                assert(game.DialogueIndex(&actualStart)==static_cast<int>(line));
                const auto expressions=DialogueCueExpressionsAt(script.cues,game.dialogueTimer_,DialogueExpressionsAt(script.lines,line));
                assert(IsTextureValid(game.assets_.GetVisual(game.stageContent_->BossPortraitVisual(expressions[1])).texture));
                if(text.introduce)assert(game.dialogueMotion_.Sample(true).opacity>.5F&&game.dialogueMotion_.Sample(true).slide==0);
                if(line==0||text.introduce||(!*text.text&&!*text.secondLine)||line+1==script.lines.size()) {
                    const auto name="extra-dialogue-shot"+std::to_string(shot)+"-section"+std::to_string(section)+"-line"+std::to_string(line);
                    Capture(game,prefix,name.c_str());
                }
                start+=text.duration;
            }
            if(script.clearFrame>=0&&game.dialogueTimer_*60<script.clearFrame) {
                StepTo(script.clearFrame);assert(game.dialogueActive_&&game.state_==RunState::Playing&&!game.stageClearShown_);
            }
            StepTo(script.endFrame-0.5);assert(game.dialogueActive_);
            game.TogglePause();const auto paused=game.dialogueTimer_;game.Update(.5F);assert(game.dialogueTimer_==paused);game.TogglePause();
            StepTo(script.endFrame+1);assert(!game.dialogueActive_&&!game.dialogueEventId_);
            game.Update(1.0F/120);assert(game.message_=="Extra dialogue released");
            int lastTrack=0;
            for(const auto& cue:script.cues)if(cue.kind==DialogueCueKind::StageMusic)lastTrack=cue.value;
            assert(game.assets_.activeMusic_==(lastTrack==0?AssetManager::MusicSlot::Stage:lastTrack==1?AssetManager::MusicSlot::Boss:lastTrack==2?AssetManager::MusicSlot::Event:AssetManager::MusicSlot::None));
            std::cout<<"Extra dialogue shot "<<shot<<" section "<<section<<" completed at "<<script.endFrame<<" content frames\n";
        }
        game.EnterMainMenu();std::cout<<"Extra actual dialogue playback, fonts, portraits, BGM, pause and async release passed\n";
    }

    /// @brief 在真实音频流验证对白帧0、半帧边界、跳时、暂停、战后音乐及同帧停止/重播；兼容旧BossMusic。
    static void TestDialogueMusic(Game& game) {
        constexpr std::array<DialogueCue,11> cues{{
            {0,DialogueCueKind::StageMusic,0,2}, {120,DialogueCueKind::StageMusic,0,2},
            {180,DialogueCueKind::StageMusic,0,-1}, {180,DialogueCueKind::StageMusic,0,2},
            {240,DialogueCueKind::StageMusic,0,0}, {300,DialogueCueKind::BossMusic,0,1},
            {360,DialogueCueKind::StageMusic,0,-1}, {420,DialogueCueKind::StageMusic,0,1},
            {480,DialogueCueKind::StageMusic,0,2}, {600,DialogueCueKind::StageMusic,0,-1},
            {660,DialogueCueKind::InputEnabled,0,0}
        }};
        game.EnterMainMenu();game.stage_=StageDirector{};game.previewMode_=true;game.captureCursor_=false;
        game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160},true,cues);
        game.assets_.LoadStage(game.stageContent_->Assets());game.state_=RunState::Playing;
        auto& assets=game.assets_;
        const auto StepTo=[&](double frame) {while(game.dialogueTimer_*60+1e-6<frame)game.Update(1.0F/120);};
        const auto AdvanceAudio=[&] {for(int n=0;n<12;++n) {assets.UpdateAudio();WaitTime(.01);}};
        for(bool after:{false,true}) {
            game.BeginStageDialogue(1,after,DialogueScene::FinalBoss,DialogueClearPolicy::PreserveWorld);
            assert(assets.activeMusic_==AssetManager::MusicSlot::Event&&game.dialogueMusicCursor_==1&&!game.dialogueBossMusic_);
            AdvanceAudio();const float initial=GetMusicTimePlayed(assets.eventMusic_);assert(initial>.02F);
            StepTo(119.5);assert(game.dialogueMusicCursor_==1);
            game.TogglePause();const auto timer=game.dialogueTimer_;game.Update(1);
            assert(game.dialogueTimer_==timer&&game.dialogueMusicCursor_==1&&!IsMusicStreamPlaying(assets.eventMusic_));
            game.TogglePause();StepTo(120);assert(game.dialogueMusicCursor_==2&&IsMusicStreamPlaying(assets.eventMusic_));
            assert(GetMusicTimePlayed(assets.eventMusic_)>=initial-.02F); // 同槽选择不重启。
            StepTo(179.5);assert(game.dialogueMusicCursor_==2);
            StepTo(180);assert(game.dialogueMusicCursor_==4&&IsMusicStreamPlaying(assets.eventMusic_));
            assert(GetMusicTimePlayed(assets.eventMusic_)<.025F); // 停止与重播不能合并为最终槽号。
            AdvanceAudio();const auto restarted=GetMusicTimePlayed(assets.eventMusic_);game.Update(1.0F/120);
            assert(GetMusicTimePlayed(assets.eventMusic_)>=restarted-.02F&&game.dialogueMusicCursor_==4);
            StepTo(240);assert(assets.activeMusic_==AssetManager::MusicSlot::Stage&&!game.dialogueBossMusic_);
            StepTo(300);assert(assets.activeMusic_==AssetManager::MusicSlot::Boss&&game.dialogueBossMusic_);
            // 模拟确认跳句跨过停止/两次选择；按表序消费全部音乐，不能受历史BossMusic布尔值屏蔽。
            game.dialogueTimer_=480.0/60;game.Update(1.0F/120);
            assert(game.dialogueMusicCursor_==9&&assets.activeMusic_==AssetManager::MusicSlot::Event&&!game.dialogueBossMusic_);
            StepTo(600);assert(game.dialogueMusicCursor_==10&&assets.activeMusic_==AssetManager::MusicSlot::None);
        }
        game.EnterMainMenu();assert(!IsMusicValid(assets.eventMusic_));
        std::cout<<"Dialogue three-slot BGM, ordered restart, pause, skip and legacy cues passed\n";
    }

    /// @brief 验证正式Extra清单的同步/异步加载、三音乐流、双Boss图集、真实立绘取景、取消与释放；不开放关卡入口。
    static void TestExtraAssets(Game& game,const std::string& prefix) {
        assert(IsAudioDeviceReady());const auto& manifest=stage_extra::StageExtraManifest();
        assert(manifest.visuals.size()==stage_extra::kBackground+1&&!manifest.eventBgm.empty());
        for(std::size_t i=0;i<manifest.visuals.size();++i)assert(manifest.visuals[i].id==i);
        for(bool async:{false,true}) {
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.previewMode_=true;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160},true);
            auto& assets=game.assets_;
            if(async) {
                assets.BeginStageLoad(manifest);int polls=0;
                while(!assets.AdvanceStageLoad()) {assert(++polls<60000);WaitTime(.001);}
            } else assets.LoadStage(manifest);
            assert(assets.HasStage()&&assets.Stage().visuals.size()==manifest.visuals.size());
            const auto& flandre=assets.GetVisual(stage_extra::kFlandre);const auto& patchouli=assets.GetVisual(stage_extra::kPatchouli);
            assert(flandre.archive&&patchouli.archive&&flandre.texture.id!=patchouli.texture.id);
            assert(flandre.animation.atlas==battle::Atlas::Flandre&&flandre.archive->FindScript(176));
            assert(patchouli.animation.atlas==battle::Atlas::ExtraEnemies&&patchouli.archive->FindScript(65));
            const auto& bg=assets.GetVisual(stage_extra::kBackground);assert(IsTextureValid(bg.texture)&&bg.texture.width==1024&&bg.texture.height==1024);
            for(int pose:{0,2,3}) {
                const auto& v=assets.GetVisual(stage_extra::kFlandrePortraits[pose]);
                // 现代重绘保留全身透明构图；取景仍按 Alpha 求上部，不沿用原图图集留白尺寸。
                assert(IsTextureValid(v.texture)&&v.texture.width==1024&&v.texture.height==1536&&v.dialogueCrop.height<=.60F&&v.dialogueCrop.width<1);
            }
            assert(assets.GetVisual(stage_extra::kFlandrePortraits[0]).texture.id==assets.GetVisual(stage_extra::kFlandrePortraits[1]).texture.id);
            assert(stage_extra::PatchouliMidbossConfig().visual==stage_extra::kPatchouli);
            for(const auto item:std::array<std::pair<Music*,float>,3>{{{&assets.stageMusic_,166.8575F},{&assets.bossMusic_,174.7375F},{&assets.eventMusic_,117.0575F}}}) {
                assert(IsMusicValid(*item.first)&&std::abs(GetMusicTimeLength(*item.first)-item.second)<.15F);
            }
            assert(assets.stageMusic_.ctxData!=assets.bossMusic_.ctxData&&assets.bossMusic_.ctxData!=assets.eventMusic_.ctxData);
            for(int track=0;track<3;++track) {
                assets.PlayStageTrack(track);assert(assets.activeMusic_==(track==0?AssetManager::MusicSlot::Stage:track==1?AssetManager::MusicSlot::Boss:AssetManager::MusicSlot::Event));
                auto* current=assets.MusicFor(assets.activeMusic_);assert(current&&IsMusicStreamPlaying(*current));
                for(const auto* other:{&assets.stageMusic_,&assets.bossMusic_,&assets.eventMusic_})if(other!=current)assert(!IsMusicStreamPlaying(*other));
                for(int n=0;n<20;++n) {assets.UpdateAudio();WaitTime(.01);}
                const float before=GetMusicTimePlayed(*current);assert(before>.02F);
                assets.PlayStageTrack(track);assert(GetMusicTimePlayed(*current)>=before-.02F);
                assets.PauseMusic();assert(!IsMusicStreamPlaying(*current));const auto paused=GetMusicTimePlayed(*current);
                WaitTime(.03);assets.UpdateAudio();assert(std::abs(GetMusicTimePlayed(*current)-paused)<.02F);
                assets.ResumeMusic();assert(IsMusicStreamPlaying(*current));
            }
            for(int invalid:{-2,3}) {bool rejected=false;try {assets.PlayStageTrack(invalid);}catch(const std::invalid_argument&) {rejected=true;}assert(rejected&&assets.activeMusic_==AssetManager::MusicSlot::Event);}
            assets.SetMusicGain(.5F);assets.SetMusicVolumePercent(25);assets.UpdateAudio();
            assets.PlayStageTrack(-1);assert(assets.activeMusic_==AssetManager::MusicSlot::None&&!IsMusicStreamPlaying(assets.eventMusic_));
            // 独立画真实立绘，检查近透明留白不会把窗口侧边和底边的锚点推离可见人物。
            if(!async)for(Vector2 size:{Vector2{1100,760},Vector2{1280,720},Vector2{390,844}}) {
                GameplayLayout layout(size.x,size.y);auto target=LoadRenderTexture(size.x,size.y);assert(IsRenderTextureValid(target));
                for(int pose:{0,2,3}) {
                    const auto& v=assets.GetVisual(stage_extra::kFlandrePortraits[pose]);const auto c=v.dialogueCrop;
                    Rectangle source{c.x*v.texture.width,c.y*v.texture.height,c.width*v.texture.width,c.height*v.texture.height};
                    BeginDrawing();BeginTextureMode(target);ClearBackground(MAGENTA);
                    DrawTexturePro(v.texture,source,layout.DialoguePortrait(source.width/source.height,true),{},0,WHITE);EndTextureMode();EndDrawing();
                    Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);int right=0,bottom=0;
                    for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x) {auto px=GetImageColor(image,x,y);if(std::abs(int(px.r)-255)+px.g+std::abs(int(px.b)-255)>6) {right=std::max(right,x);bottom=std::max(bottom,y);}}
                    assert(image.width-1-right<=2&&image.height-1-bottom<=2);
                    if(!prefix.empty())assert(ExportImage(image,(prefix+"-flandre-"+std::to_string(pose)+"-"+std::to_string(static_cast<int>(size.x))+".png").c_str()));
                    UnloadImage(image);
                }
                UnloadRenderTexture(target);
            }
            std::weak_ptr<const anm::Archive> borrowed=flandre.archive;assets.PlayStageTrack(2);assets.UnloadStage();
            assert(!assets.HasStage()&&assets.Stage().visuals.empty()&&borrowed.expired()&&IsMusicValid(assets.menuMusic_));
            assert(!IsMusicValid(assets.stageMusic_)&&!IsMusicValid(assets.bossMusic_)&&!IsMusicValid(assets.eventMusic_)&&assets.activeMusic_==AssetManager::MusicSlot::None);
            assets.BeginStageLoad(manifest);assert(!assets.AdvanceStageLoad());assets.UnloadStage();assert(!assets.HasStage());
        }
        // 无第三槽的旧关卡不能沿用Extra音乐；缺失槽停止旧曲，公共菜单音乐仍属于全局生命周期。
        game.assets_.LoadStage(stage4::Stage4Content{}.Assets());game.assets_.PlayStageTrack(0);game.assets_.PlayStageTrack(2);
        assert(!IsMusicValid(game.assets_.eventMusic_)&&game.assets_.activeMusic_==AssetManager::MusicSlot::None);
        game.EnterMainMenu();assert(!game.assets_.HasStage());game.assets_.SetMusicGain(1);
        std::cout<<"Extra formal assets sync/async, three BGM streams, portraits, native background and ownership verified\n";
    }

    /// @brief 使用生产完整配方连续执行主战/后继；profile=0阈值与击破，1自然超时，2非符归零跳过符卡。
    /// 隔离素材不冒充正式MSG/时间轴；入场外部放行，所有技能及使魔经真实Game固定步与演员池运行。
    static void TestExtraFullBattles(Game& game,const std::string& prefix,int profile,int firstShot=0,int lastShot=4,bool formalAssets=false) {
        using F=bosses::FlandreBoss;
        for(int shot=firstShot;shot<lastShot;++shot)for(bool encore:{false,true}) {
            if(profile==2&&encore)continue;
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160},formalAssets);
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.player_.SetInvulnerability(5000);
            game.referenceRank_=32;
            auto config=encore?stage_extra::FlandreEncoreConfig(static_cast<ShotType>(shot)):stage_extra::FlandreMainConfig(static_cast<ShotType>(shot));
            game.boss_=(encore?stage_extra::kFlandreEncore:stage_extra::kFlandreMain).create(static_cast<ShotType>(shot),native_combat::Difficulty::Normal);
            auto* identity=game.boss_.get();auto& boss=static_cast<F&>(*game.boss_);
            const std::array<StageEvent,1> events{{{.action=ShowMessage{},.id=0xef01}}};
            game.stage_=StageDirector(events);game.stage_.Update(0,[](const StageEvent&){return false;});game.bossEventId_=0xef01;
            for(int tick=0;tick<360;++tick) {game.Update(1.0F/120);assert(game.boss_.get()==identity&&!boss.CurrentSkill()&&game.bossEventId_==0xef01);}
            assert(!boss.CanTakeDamage()&&boss.BodyVisible());boss.ResumeCombat();
            const auto label=std::string(encore?"extra-full-encore-":"extra-full-main-")+std::to_string(profile)+"-shot"+std::to_string(shot);
            std::vector<int> attacks,spells;int lastAttack=0;bool paused=false,bomb=false,recovered=false;int bombCard=0;
            std::size_t peak=0,childPeak=0;int steps=0;
            for(;steps<1300*120&&boss.CurrentSkill();++steps) {
                game.Update(1.0F/120);assert(game.boss_.get()==identity&&game.state_==RunState::Playing&&game.bossEventId_==0xef01);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());childPeak=std::max(childPeak,EnemyCount(game));
                if(!boss.CurrentSkill())break;
                const auto* definition=boss.CurrentSkill();const int phase=boss.phase,skill=boss.skill;
                const auto& slot=config.phases[phase-1].skills[skill-1];
                if(lastAttack!=boss.attack) {lastAttack=boss.attack;attacks.push_back(lastAttack);}
                const int card=boss.SpellCardNumber();if(card&&(spells.empty()||spells.back()!=card))spells.push_back(card);
                const auto bar=boss.HealthBar();assert(bar.remainingBars==static_cast<int>(config.phases.size())-phase);
                assert(bar.count==(encore||phase>=9?1:2));
                if(slot.omitHealthBarSegment)assert(bar.segments[0].capacity==1&&bar.segments[0].remaining==1);
                // 每个新技能的第0步都有明确清敌请求；此时尚未召唤新演员，旧控制器和分身须已完成清场。
                if(boss.skillTimer>0&&boss.skillTimer*60<=1)assert(!EnemyCount(game));
                if(!paused&&boss.skillTimer>.25F) {
                    paused=true;const auto clock=boss.skillTimer;const auto count=game.bullets_.Pool().ActiveCount();
                    game.TogglePause();game.Update(.5F);assert(boss.skillTimer==clock&&game.bullets_.Pool().ActiveCount()==count);game.TogglePause();
                }
                if(profile==0&&!bomb&&card&&boss.skillTimer>3.5F) {bomb=true;bombCard=card;game.ApplyBombEffect();game.referenceRank_=32;}
                if(bomb&&card==bombCard&&!game.playerBomb_.Active()&&boss.CanTakeDamage()&&boss.skillTimer>7)recovered=true;
                if(shot==firstShot&&boss.skillTimer>6&&boss.skillTimer<6.01F&&card)Capture(game,prefix,(label+"-card"+std::to_string(card)).c_str());
                if(profile!=1&&boss.CanTakeDamage()) {
                    const bool zero=profile==2&&definition->reserveHealth>0&&boss.skillTimer>.6F;
                    const bool defeat=!slot.omitHealthBarSegment&&!definition->survival&&boss.skillTimer>10;
                    if(zero||defeat) {
                        boss.health=zero||definition->reserveHealth<=0?0:definition->reserveHealth-1;
                        game.ResolveCollisions();
                    }
                }
            }
            assert(steps<1300*120&&!boss.CurrentSkill()&&game.boss_.get()==identity&&game.bossEventId_==0xef01&&paused);
            const std::size_t expectedAttacks=encore?5:profile==2?11:20;
            assert(attacks.size()==expectedAttacks&&spells.size()==(encore?3:profile==2?1:10));
            if(profile!=2)for(std::size_t i=0;i<attacks.size();++i)assert(attacks[i]==static_cast<int>(i+1));
            for(std::size_t i=0;i<spells.size();++i)assert(spells[i]==(encore?132:profile==2?131:122)+static_cast<int>(i));
            assert(profile!=0||bomb&&recovered);assert(boss.Defeated()==(profile!=1)&&boss.RetirementFromTimeout()==(profile==1));
            int exit=0;for(;exit<125&&boss.active;++exit) {game.Update(1.0F/120);assert(game.bossEventId_.has_value()==boss.active);}
            assert(exit==(profile==1?122:121)&&!boss.active&&!game.bossEventId_&&!game.stage_.EndEvent(0xef01));
            assert(boss.CompletionScore()==200000&&!game.bullets_.Pool().ActiveCount()&&!EnemyCount(game));
            const auto score=game.score_;game.ResolveCollisions();assert(game.score_==score);
            game.EnterMainMenu();assert(!game.boss_&&game.assets_.Stage().visuals.empty()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" verified; "<<attacks.size()<<" skills / "<<spells.size()<<" spells; peak "<<peak<<" bullets / "<<childPeak<<" actors; "<<steps/120.0<<"s\n";
        }
    }

    /// @brief 隔离95/119不同的清敌边界：95保留演员，119爆破清使魔，超时下一步才放行事件。
    static void TestExtraFinalExitCleanup(Game& game) {
        using F=bosses::FlandreBoss;
        for(bool encore:{false,true})for(bool damage:{false,true}) {
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.previewMode_=true;
            game.practiceMode_=game.spellPractice_=game.challengeMode_=false;game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160});
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.player_.SetInvulnerability(1000);
            bosses::StartConfig config{.position={0,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,.defeatBase=stage_assets::kNativeDefeat,
                .configure=F::Behaviors::ConfigurePractice,.defeated=encore?F::Behavior::EncoreFinalExit:F::Behavior::FinalExit,
                .retreat=encore?F::Behavior::EncoreFinalExit:F::Behavior::FinalExit,
                .phases={{{encore?F::Behavior::DanmakuHeart:F::Behavior::RipplesOf495Years},{{{0,1,PINK}}}}}};
            auto boss=std::make_unique<F>();boss->StartBoss(config);game.boss_=std::move(boss);
            if(damage)game.boss_->health=0;
            else {
                // 只加速攻击期；这里不交付子演员/积分弹幕，随后只验真实Game退场，不冒充完整攻击验收。
                assert(!game.boss_->Update(150,{},game.bullets_));
                assert(game.boss_->Update(1.0F/120,{},game.bullets_));
            }
            assert(game.boss_->AdvanceSkill()&&static_cast<F&>(*game.boss_).RetirementFromTimeout()==!damage);
            game.bullets_.Reset();const std::array<StageEvent,1> events{{{.action=ShowMessage{},.id=0xef00}}};
            game.stage_=StageDirector(events);game.stage_.Update(0,[](const StageEvent&){return false;});game.bossEventId_=0xef00;
            behavior::Spawn spawn{.position={0,0,7},.pattern={.movement={.speed=0},.initialAnimation=167},.sourceHealth=1200,.seed=77,
                .sourceItem=-2,.visual=stage_assets::kBoss,.retireOutside=false,.damageable=false,.visible=false,
                .stageClearResponse=behavior::StageClearResponse::Remove};
            auto* sentinel=game.actors_.Spawn(spawn);assert(sentinel);bool clearedBeforeEnd=false;
            for(int tick=0;tick<124&&game.boss_->active;++tick) {
                game.Update(1.0F/120);
                if(game.boss_->skillTimer*60<=60)assert(sentinel->active);
                if(game.boss_->active&&game.boss_->skillTimer*60>60) {clearedBeforeEnd=encore;assert(sentinel->active==!encore&&game.bossEventId_==0xef00);}
            }
            assert(!game.boss_->active&&sentinel->active==!encore&&!game.bossEventId_&&clearedBeforeEnd==(encore&&!damage));
            game.EnterMainMenu();assert(!game.boss_&&game.assets_.Stage().visuals.empty());
        }
        std::cout<<"Extra final exits preserve actors for 95 and clear actors for 119 before timeout event completion\n";
    }

    /// @brief 132隔离Game验收：四机体实际入场/Resume、击破/超时、Bomb及隐藏使魔与素材生命周期。
    /// 停在本符完成边界，测试收尾使用等待入场；不替代尚未接入的106后继或正式Extra流程。
    static void TestExtraFragileWing(Game& game,const std::string& prefix) {
        using F=bosses::FlandreBoss;
        for(int shot=0;shot<4;++shot)for(bool damage:{false,true})for(bool bomb:{false,true}) {
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160});
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.player_.SetInvulnerability(1000);
            game.referenceRank_=32;
            const bosses::StartConfig config{.position={},.seed=7,.visual=stage_assets::kBoss,.defeatBase=stage_assets::kNativeDefeat,
                .configure=F::Behaviors::ConfigurePractice,.shot=static_cast<ShotType>(shot),.intro=F::Behavior::EncoreEntrance,
                .defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,.phases={{{F::Behavior::FragileWing},{{{0,1,PINK}}}}}};
            auto boss=std::make_unique<F>();boss->StartBoss(config);game.boss_=std::move(boss);
            for(int tick=0;tick<320;++tick)game.Update(1.0F/120);
            assert(game.boss_->health==15000&&game.boss_->BodyVisible()&&!game.boss_->CanTakeDamage());game.boss_->ResumeCombat();
            const auto label="extra-fragile-shot"+std::to_string(shot)+(damage?"-damage":"-timeout")+(bomb?"-bomb":"-plain");
            bool paused=false,sampled=false,bat=false,recovered=false,startedBomb=false,sawChild=false,sawNegativeRadius=false;
            std::size_t peak=0,childPeak=0;int tick=0;
            for(;tick<=120*120+2&&static_cast<F&>(*game.boss_).CurrentSkill();++tick) {
                game.Update(1.0F/120);assert(game.state_==RunState::Playing);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                if(!static_cast<F&>(*game.boss_).CurrentSkill())break;
                assert(game.boss_->SpellCardNumber()==132&&game.boss_->BodyVisible()&&game.boss_->health>0);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());childPeak=std::max(childPeak,EnemyCount(game));
                game.actors_.ForEachActive([&](const behavior::Actor& child) {
                    sawChild=true;assert(!child.visible&&!child.interactable&&!child.damageable&&!child.contactEnabled&&child.health==1200);
                    assert(child.animation.selection.atlas==battle::Atlas::Flandre&&child.animationScript==167&&child.polarSweep);
                    assert(child.animation.player.Source()&&child.stageClearResponse==behavior::StageClearResponse::Remove);
                    if(child.phaseFrames>112)sawNegativeRadius=true;
                });
                game.bullets_.Pool().ForEachActive([&](const Bullet& b) {
                    if(b.owner!=BulletOwner::Enemy)return;
                    assert(!b.IsBeam()&&b.nativeType==5&&b.nativeColor%2==1&&b.nativeColor<14&&b.animation.selection.atlas==battle::Atlas::Bullets);
                    assert(b.sampleMotionAtStart&&b.spawnVisualSeconds==10.0F/60&&b.launchBoostSeconds==0);
                    assert(std::isfinite(b.position.y)&&std::isfinite(b.Velocity().z));
                });
                if(!paused&&game.boss_->skillTimer>4) {
                    paused=true;const auto clock=game.boss_->skillTimer;const auto count=game.bullets_.Pool().ActiveCount();const auto children=EnemyCount(game);
                    game.TogglePause();game.Update(.5F);assert(game.boss_->skillTimer==clock&&game.bullets_.Pool().ActiveCount()==count&&EnemyCount(game)==children);game.TogglePause();
                }
                if(bomb&&!startedBomb&&game.boss_->skillTimer>4.5F) {startedBomb=true;game.ApplyBombEffect();game.referenceRank_=32;}
                if(bomb&&game.playerBomb_.Active()&&game.boss_->skillTimer>4.6F) {bat=true;assert(!game.boss_->CanBeTargeted()&&game.boss_->animationScript==165);}
                if(bat&&!game.playerBomb_.Active()&&game.boss_->CanTakeDamage())recovered=true;
                if(!sampled&&game.boss_->skillTimer>26) {sampled=true;assert(peak>0&&childPeak>0);if(shot==0&&!bomb)Capture(game,prefix,label.c_str());}
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>27) {game.boss_->health=0;game.ResolveCollisions();}
            }
            assert(tick<=120*120+2&&paused&&sampled&&sawChild&&sawNegativeRadius&&peak>0&&childPeak>0&&(!bomb||startedBomb&&bat&&recovered));
            assert(!static_cast<F&>(*game.boss_).CurrentSkill()&&game.boss_->Defeated()==damage&&!game.boss_->CanTakeDamage());
            game.EnterMainMenu();assert(!game.boss_&&game.assets_.Stage().visuals.empty()&&!EnemyCount(game)&&!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" verified; peak "<<peak<<" projectiles / "<<childPeak<<" hidden emitters\n";
        }
    }

    /// @brief 四机体以同一实例走完132、真实106过渡和133；覆盖两符击破/自然超时、133准备期Bomb与卸载。
    /// 133完成后停止，收尾等待行为不表示112或正式Extra完整流程已验收。
    static void TestExtraBloodstain(Game& game,const std::string& prefix,bool reviewOnly=false) {
        using F=bosses::FlandreBoss;TestUiFont(game);
        for(int shot=0;shot<4;++shot)for(bool damage:{false,true})for(bool bomb:{false,true}) {
            // 默认仍运行全部16条路径；reviewOnly只复核补字后的同场景截图，不替代完整战斗验收。
            if(reviewOnly&&(shot!=0||!damage||bomb))continue;
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160});
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.player_.SetInvulnerability(1000);
            game.referenceRank_=32;
            const bosses::StartConfig config{.position={},.seed=7,.visual=stage_assets::kBoss,.defeatBase=stage_assets::kNativeDefeat,
                .configure=F::Behaviors::ConfigurePractice,.shot=static_cast<ShotType>(shot),.intro=F::Behavior::EncoreEntrance,
                .defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
                .phases={{{F::Behavior::FragileWing,F::Behavior::CreepingBloodstainPrelude,F::Behavior::CreepingBloodstain},
                    {{{0,.4F,PINK},{1,.2F,WHITE},{2,.4F,PINK}}}}}};
            auto boss=std::make_unique<F>();boss->StartBoss(config);game.boss_=std::move(boss);auto* identity=game.boss_.get();
            for(int tick=0;tick<320;++tick)game.Update(1.0F/120);game.boss_->ResumeCombat();
            const auto label="extra-bloodstain-shot"+std::to_string(shot)+(damage?"-damage":"-timeout")+(bomb?"-bomb":"-plain");
            bool paused=false,preludePause=false,sampled=false,startedBomb=false,bat=false,recovered=false,sawChild=false,sawLarge=false,sawSmall=false;
            std::size_t peak=0,childPeak=0;int drops=0,items=0,tick=0;bool sawBombPickup=false;
            for(;tick<=250*120&&static_cast<F&>(*game.boss_).CurrentSkill();++tick) {
                game.Update(1.0F/120);assert(game.boss_.get()==identity&&game.state_==RunState::Playing);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                if(!static_cast<F&>(*game.boss_).CurrentSkill())break;
                const int attack=game.boss_->attack;
                if(attack==1) {
                    if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>6) {game.boss_->health=0;game.ResolveCollisions();}
                    continue;
                }
                if(attack==2) {
                    assert(game.boss_->health==4000&&!game.boss_->CanTakeDamage()&&game.boss_->SpellCardNumber()==0);
                    for(int count:game.boss_->DropEvents()) {assert(count==40);++drops;}
                    for(int item:game.boss_->ItemEvents()) {assert(item==3);++items;game.drops_.ForEachActive([&](const Pickup& p){sawBombPickup|=p.kind==PickupKind::Bomb;});}
                    if(!preludePause&&game.boss_->skillTimer>.2F) {preludePause=true;const auto clock=game.boss_->skillTimer;game.TogglePause();game.Update(.5F);assert(game.boss_->skillTimer==clock);game.TogglePause();}
                    continue;
                }
                assert(attack==3&&game.boss_->SpellCardNumber()==133&&game.boss_->BodyVisible());
                if(bomb&&!startedBomb) {startedBomb=true;assert(game.boss_->skillTimer<.02F);game.ApplyBombEffect();game.referenceRank_=32;}
                if(bomb&&game.playerBomb_.Active()&&game.boss_->skillTimer>.05F) {
                    bat=true;assert(game.boss_->animationScript==165&&!game.boss_->CanBeTargeted());
                }
                if(bat&&!game.playerBomb_.Active()&&game.boss_->CanTakeDamage())recovered=true;
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());childPeak=std::max(childPeak,EnemyCount(game));
                game.actors_.ForEachActive([&](const behavior::Actor& child) {
                    sawChild=true;assert(!child.visible&&!child.interactable&&!child.damageable&&!child.contactEnabled&&child.health==1200);
                    assert(child.animation.selection.atlas==battle::Atlas::Flandre&&child.animationScript==167&&child.animation.player.Source());
                    assert(child.stageClearResponse==behavior::StageClearResponse::Remove&&!child.polarSweep);
                });
                game.bullets_.Pool().ForEachActive([&](const Bullet& b) {
                    if(b.owner!=BulletOwner::Enemy)return;
                    assert(!b.IsBeam()&&b.animation.selection.atlas==(b.nativeType==9?battle::Atlas::BulletEffects:battle::Atlas::Bullets)&&b.sampleMotionAtStart&&b.spawnVisualSeconds==10.0F/60);
                    assert(b.nativeType==3||b.nativeType==9);sawLarge|=b.nativeType==9;sawSmall|=b.nativeType==3;
                    if(b.nativeType==3)assert(b.nativeColor==1&&b.accelerationSeconds>=0);
                });
                if(!paused&&game.boss_->skillTimer>4) {paused=true;const auto clock=game.boss_->skillTimer;const auto count=game.bullets_.Pool().ActiveCount();const auto children=EnemyCount(game);game.TogglePause();game.Update(.5F);assert(game.boss_->skillTimer==clock&&game.bullets_.Pool().ActiveCount()==count&&EnemyCount(game)==children);game.TogglePause();}
                if(!sampled&&game.boss_->skillTimer>10) {sampled=true;assert(peak>0&&childPeak>0);if(shot==0&&!bomb)Capture(game,prefix,label.c_str());}
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>18) {game.boss_->health=0;game.ResolveCollisions();}
            }
            assert(tick<=250*120&&paused&&preludePause&&sampled&&sawChild&&sawLarge&&sawSmall&&drops==1&&items==1&&sawBombPickup);
            assert(!bomb||startedBomb&&bat&&recovered);assert(!static_cast<F&>(*game.boss_).CurrentSkill()&&game.boss_->Defeated()==damage);
            game.EnterMainMenu();assert(!game.boss_&&game.assets_.Stage().visuals.empty()&&!EnemyCount(game)&&!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" verified; peak "<<peak<<" projectiles / "<<childPeak<<" hidden emitters; real132/106/133 handoffs\n";
        }
    }

    /// @brief 133→112→134→119使用同一实例与真实异步事件，覆盖四机体自然超时/击破、Bomb与击破/超时各自删除时刻。
    /// 仍为隔离素材夹具；不把本次验证计入正式Extra完整对白/时间轴验收。
    static void TestExtraHeart(Game& game,const std::string& prefix) {
        using F=bosses::FlandreBoss;TestUiFont(game);
        for(int shot=0;shot<4;++shot)for(bool damage:{false,true})for(bool bomb:{false,true}) {
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160});
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.player_.SetInvulnerability(1000);
            game.referenceRank_=32;
            const bosses::StartConfig config{.position={0,0,1.42857146F},.seed=7,.visual=stage_assets::kBoss,.defeatBase=stage_assets::kNativeDefeat,
                .configure=+[](behavior::Actor& actor,bool spatial) {
                    F::Behaviors::ConfigurePractice(actor,spatial);(actor.liftedMotion?actor.liftedMotion->planar:actor.motion).bounds={{-7.14285714F,1.42857146F},{7.14285714F,7.85714293F}};
                    actor.AnimateAuxiliary(0,18,stage_assets::kOrb);
                },.shot=static_cast<ShotType>(shot),.defeated=F::Behavior::EncoreFinalExit,.retreat=F::Behavior::EncoreFinalExit,
                .phases={{{F::Behavior::CreepingBloodstain,F::Behavior::DanmakuHeartPrelude,F::Behavior::DanmakuHeart},
                    {{{0,.4F,PINK},{1,.2F,WHITE},{2,.4F,PINK}}}}},.defeatScore=10000,.retreatScore=100};
            auto boss=std::make_unique<F>();boss->StartBoss(config);game.boss_=std::move(boss);auto* identity=game.boss_.get();
            const std::array<StageEvent,1> events{{{.action=ShowMessage{},.id=0xe134}}};
            game.stage_=StageDirector(events);game.stage_.Update(0,[](const StageEvent&){return false;});game.bossEventId_=0xe134;
            const auto label="extra-heart-shot"+std::to_string(shot)+(damage?"-damage":"-timeout")+(bomb?"-bomb":"-plain");
            bool paused=false,preludePause=false,sampled=false,startedBomb=false,bat=false,recovered=false,sawScan=false,sawOrb=false,saw176=false;
            std::size_t peak=0,childPeak=0;int drops=0,tick=0;
            for(;tick<=280*120&&static_cast<F&>(*game.boss_).CurrentSkill();++tick) {
                game.Update(1.0F/120);assert(game.boss_.get()==identity&&game.state_==RunState::Playing&&game.bossEventId_==0xe134);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                if(!static_cast<F&>(*game.boss_).CurrentSkill())break;
                const int attack=game.boss_->attack;
                if(attack==1) {if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>6) {game.boss_->health=0;game.ResolveCollisions();}continue;}
                if(attack==2) {
                    assert(game.boss_->health==6000&&!game.boss_->CanTakeDamage()&&game.boss_->SpellCardNumber()==0&&game.boss_->ItemEvents().empty());
                    for(int count:game.boss_->DropEvents()) {assert(count==40);++drops;}
                    if(!preludePause&&game.boss_->skillTimer>.2F) {preludePause=true;const auto clock=game.boss_->skillTimer;game.TogglePause();game.Update(.5F);assert(game.boss_->skillTimer==clock);game.TogglePause();}continue;
                }
                assert(attack==3&&game.boss_->SpellCardNumber()==134&&game.boss_->BodyVisible());
                if(game.boss_->animationScript==176)saw176=true;
                if(bomb&&!startedBomb&&game.boss_->skillTimer>2.5F) {startedBomb=true;game.ApplyBombEffect();game.referenceRank_=32;}
                if(bomb&&game.playerBomb_.Active()&&game.boss_->skillTimer>2.6F) {bat=true;assert(game.boss_->animationScript==165&&!game.boss_->CanBeTargeted());}
                if(bat&&!game.playerBomb_.Active()&&game.boss_->CanTakeDamage()) {recovered=true;assert(game.boss_->animationScript==176);}
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());childPeak=std::max(childPeak,EnemyCount(game));
                game.actors_.ForEachActive([&](const behavior::Actor& child) {
                    assert(!child.visible&&!child.interactable&&!child.damageable&&!child.contactEnabled&&child.health==1200);
                    assert(child.animation.selection.atlas==battle::Atlas::Flandre&&child.animationScript==167&&child.animation.player.Source()&&child.spiralTrail);
                    assert(child.stageClearResponse==behavior::StageClearResponse::Remove);
                });
                game.bullets_.Pool().ForEachActive([&](const Bullet& b) {
                    if(b.owner!=BulletOwner::Enemy)return;
                    assert(!b.IsBeam()&&b.animation.selection.atlas==battle::Atlas::Bullets&&b.sampleMotionAtStart&&b.spawnVisualSeconds==10.0F/60);
                    assert(b.nativeType==5||b.nativeType==6);sawScan|=b.nativeType==5;sawOrb|=b.nativeType==6;
                    if(b.nativeType==5)assert((b.nativeColor==2||b.nativeColor==3)&&b.accelerationSeconds>=0);
                });
                if(!paused&&game.boss_->skillTimer>4) {paused=true;const auto clock=game.boss_->skillTimer;const auto count=game.bullets_.Pool().ActiveCount();const auto children=EnemyCount(game);game.TogglePause();game.Update(.5F);assert(game.boss_->skillTimer==clock&&game.bullets_.Pool().ActiveCount()==count&&EnemyCount(game)==children);game.TogglePause();}
                if(!sampled&&game.boss_->skillTimer>24) {sampled=true;assert(peak>0&&childPeak>0);if(shot==0&&!bomb)Capture(game,prefix,label.c_str());}
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>25) {game.boss_->health=0;game.ResolveCollisions();}
            }
            assert(tick<=280*120&&paused&&preludePause&&sampled&&sawScan&&sawOrb&&saw176&&drops==1&&(!bomb||startedBomb&&bat&&recovered));
            assert(!static_cast<F&>(*game.boss_).CurrentSkill()&&game.boss_->Defeated()==damage&&game.bossEventId_==0xe134);
            const auto score=game.score_;int exit=0;bool hpOne=false;
            for(;exit<126&&game.boss_->active;++exit) {
                game.Update(1.0F/120);assert(!game.boss_->CanTakeDamage()&&!game.boss_->CanContactPlayer());
                assert(game.bossEventId_.has_value()==game.boss_->active);
                if(game.boss_->skillTimer*60>60&&game.boss_->active) {hpOne=true;assert(game.boss_->health==1);}
            }
            assert(exit==(damage?121:122)&&hpOne==!damage&&!game.boss_->active&&!game.bossEventId_&&!game.stage_.EndEvent(0xe134));
            assert(game.score_==score+(damage?10000:100)&&!game.bullets_.Pool().ActiveCount()&&!EnemyCount(game));
            game.EnterMainMenu();assert(!game.boss_&&game.assets_.Stage().visuals.empty()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" verified; peak "<<peak<<" projectiles / "<<childPeak<<" hidden actors; exit "<<exit<<" half steps\n";
        }
    }

    /// @brief 条件后继入场在四机体验证隐藏绘制、11对象、暂停及中途/等待期卸载；不启动未移植额外符卡。
    static void TestExtraEncoreEntrance(Game& game,const std::string& prefix) {
        using F=bosses::FlandreBoss;
        for(int shot=0;shot<4;++shot)for(bool early:{true,false}) {
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;
            game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160});
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.player_.SetInvulnerability(1000);
            // 第一非符仅为配置解析提供已实现技能；本专项始终不Resume，不能当作额外第一符占位验收。
            const bosses::StartConfig config{.position={},.seed=7,.visual=stage_assets::kBoss,.defeatBase=stage_assets::kNativeDefeat,
                .configure=F::Behaviors::ConfigurePractice,.shot=static_cast<ShotType>(shot),.intro=F::Behavior::EncoreEntrance,
                .defeated=F::Behavior::FinalExit,.retreat=F::Behavior::FinalExit,
                .phases={{{F::Behavior::FirstNonspell},{{{0,1,WHITE}}}}}};
            auto boss=std::make_unique<F>();boss->StartBoss(config);game.boss_=std::move(boss);
            std::size_t peak=0;bool paused=false,hiddenPixels=false,converging=false,stationary=false;
            const int ticks=early?120:2400;
            for(int tick=0;tick<ticks;++tick) {
                game.Update(1.0F/120);const auto clock=game.boss_->skillTimer*60;
                assert(game.state_==RunState::Playing&&!static_cast<F&>(*game.boss_).CurrentSkill()&&game.boss_->attack==0);
                assert(!game.boss_->CanTakeDamage()&&!game.boss_->CanContactPlayer()&&game.boss_->BodyVisible()==(clock>100));
                assert(!game.bullets_.Pool().ActiveCount()&&!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                peak=std::max(peak,EnemyCount(game));
                game.actors_.ForEachActive([&](const behavior::Actor& actor) {
                    assert(actor.animation.selection.atlas==battle::Atlas::Flandre&&!actor.contactEnabled&&actor.health==15000);
                    assert(actor.animationScript==160||actor.animationScript==168||actor.animationScript==169);
                    assert(actor.animation.player.Source());
                    converging|=actor.animationScript==168;stationary|=actor.animationScript==169;
                    if(actor.animationScript==160)assert(!actor.visible&&actor.stageClearResponse==behavior::StageClearResponse::Remove);
                });
                if(tick==90) {
                    paused=true;const auto count=EnemyCount(game);game.TogglePause();game.Update(.5F);
                    assert(game.boss_->skillTimer*60==clock&&EnemyCount(game)==count);game.TogglePause();
                }
                if(tick==133&&!early&&shot==0) {
                    Capture(game,prefix,"extra-encore-convergence");
                    const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
                    const auto render=[&] {
                        BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.DrawWorld();EndTextureMode();EndDrawing();
                        Image result=LoadImageFromTexture(target.texture);ImageFlipVertical(&result);return result;
                    };
                    Image hidden=render();auto saved=std::move(game.boss_);Image absent=render();game.boss_=std::move(saved);
                    for(int y=0;y<hidden.height;++y)for(int x=0;x<hidden.width;++x) {
                        const auto a=GetImageColor(hidden,x,y),b=GetImageColor(absent,x,y);assert(a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a);
                    }
                    hiddenPixels=true;UnloadImage(hidden);UnloadImage(absent);UnloadRenderTexture(target);
                }
                if(tick==240&&!early&&shot==0)Capture(game,prefix,"extra-encore-visible");
                if(clock>153)assert(EnemyCount(game)==0);
            }
            assert(paused&&converging&&stationary&&(early||peak==11));
            assert(early||shot!=0||hiddenPixels);
            game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty()&&!game.boss_&&!EnemyCount(game));
            assert(!game.bullets_.Pool().ActiveCount());
            std::cout<<"Extra encore entrance shot "<<shot<<(early?" early unload":" full waiting")<<" verified; peak "<<peak<<" actors\n";
        }
    }

    /// @brief 芙兰朵露八段非符的Game隔离验证；不运行未移植后继，不表示完整遭遇已验收。
    /// 检查本体Flandre与辅助ExtraEnemies同时加载、暂停、跨段及卸载前解除两种CPU/GPU借用。
    static void TestExtraFlandre(Game& game,const std::string& prefix) {
        using F=bosses::FlandreBoss;
        for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;
            game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::Flandre,160});
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();
            game.player_.SetInvulnerability(1000);game.referenceRank_=32;
            bosses::StartConfig config{.position={-10,0,5.71428585F},.seed=7,.visual=stage_assets::kBoss,
                .defeatBase=stage_assets::kNativeDefeat,.configure=F::Behaviors::ConfigurePractice,.shot=static_cast<ShotType>(shot),
                .intro=F::Behavior::Entrance,.defeated=F::Behavior::Entrance,.retreat=F::Behavior::Entrance,
                .phases={{{F::Behavior::FirstNonspell,F::Behavior::SecondNonspell,F::Behavior::ThirdNonspell,F::Behavior::FourthNonspell,
                    F::Behavior::FifthNonspell,F::Behavior::SixthNonspell,F::Behavior::SeventhNonspell,F::Behavior::EighthNonspell},
                    {{{0,0.125F,WHITE},{1,0.125F,WHITE},{2,0.125F,WHITE},{3,0.125F,WHITE},{4,0.125F,WHITE},{5,0.125F,WHITE},{6,0.125F,WHITE},{7,0.125F,WHITE}}}}}};
            auto boss=std::make_unique<F>();boss->StartBoss(config);game.boss_=std::move(boss);
            for(int tick=0;tick<300;++tick) {game.Update(1.0F/120);assert(!game.boss_->CanTakeDamage());}
            game.boss_->ResumeCombat();
            const auto label="extra-flandre-shot"+std::to_string(shot)+(damage?"-damage":"-timeout");
            std::array<bool,8> seen{},sampled{};bool paused=false;std::size_t peak=0;int ticks=0;
            for(;ticks<245*120&&dynamic_cast<F&>(*game.boss_).CurrentSkill();++ticks) {
                game.Update(1.0F/120);assert(game.state_==RunState::Playing&&game.referenceRank_==32);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                if(!dynamic_cast<F&>(*game.boss_).CurrentSkill())break;
                const int attack=game.boss_->attack;assert(attack>=1&&attack<=8);seen[attack-1]=true;
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                const auto& auxiliary=game.boss_->AuxiliaryAnimations()[0];
                assert(game.boss_->animation.selection.atlas==battle::Atlas::Flandre&&auxiliary.visual==stage_assets::kOrb);
                assert(auxiliary.cursor.selection.atlas==battle::Atlas::ExtraEnemies&&auxiliary.cursor.player.Source());
                assert(game.boss_->animation.player.Source()!=auxiliary.cursor.player.Source());
                if(!paused&&game.boss_->skillTimer>3) {
                    paused=true;const auto age=auxiliary.age;const auto time=game.boss_->skillTimer;
                    game.TogglePause();game.Update(0.5F);assert(game.boss_->skillTimer==time&&auxiliary.age==age);game.TogglePause();
                }
                if(!sampled[attack-1]&&game.boss_->skillTimer>7) {
                    sampled[attack-1]=true;assert(peak>0);if(shot==0&&!damage)Capture(game,prefix,(label+"-attack"+std::to_string(attack)).c_str());
                }
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>10) {
                    constexpr int reserve[]{1800,1200,1900,2400,1400,2200,1520,1800};game.boss_->health=reserve[attack-1]-1;game.ResolveCollisions();
                }
            }
            assert(ticks<245*120&&paused&&std::ranges::all_of(seen,[](bool v){return v;})&&std::ranges::all_of(sampled,[](bool v){return v;}));
            // 夹具在已实现八段之后立即卸载，占位退场不会被执行或计入完整关底验收。
            game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty()&&!game.boss_&&!EnemyCount(game));
            assert(!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" eight nonspells verified; peak "<<peak<<" projectiles\n";
        }
    }

    /// @brief Extra三符的真实Game隔离预验；使用Extra本体ANM，覆盖四御札、击破/超时、暂停和卸载。
    /// 该夹具无完整时间轴/对白，不开放Extra正式入口；本体和辅助槽使用各自独立游标。
    static void TestExtraPatchouli(Game& game,const std::string& prefix) {
        using P=bosses::PatchouliBoss;
        for(auto id:{P::Behavior::SilentSelene,P::Behavior::RoyalFlare,P::Behavior::PhilosophersStone})for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;
            game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>();
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();
            game.player_.SetInvulnerability(1000);game.referenceRank_=32;
            bosses::StartConfig config{.position={0,0,4.64285707F},.seed=7,.visual=stage_assets::kBoss,
                .defeatBase=stage_assets::kNativeDefeat,.configure=P::Behaviors::ConfigureExtraPractice,
                .shot=static_cast<ShotType>(shot),.defeated=P::Behavior::ExtraRetreat,.retreat=P::Behavior::ExtraRetreat,
                .phases={{{id},{{{0,1,WHITE}}}}}};
            auto boss=std::make_unique<P>();boss->StartBoss(config);game.boss_=std::move(boss);
            const auto label=std::string(id==P::Behavior::SilentSelene?"extra-selene":id==P::Behavior::RoyalFlare?"extra-royal":"extra-stone")+"-shot"+std::to_string(shot)+(damage?"-damage":"-timeout");
            int ticks=0;std::size_t peak=0;bool paused=false,sampled=false;
            for(;ticks<60*120&&game.boss_->active;++ticks) {
                game.Update(1.0F/120);assert(game.state_==RunState::Playing&&game.referenceRank_==32);
                if(game.boss_->skillTimer>0.1F) {
                    const auto animations=game.boss_->AuxiliaryAnimations();assert(animations.size()==8&&animations[0].script==17);
                    assert(animations[0].cursor.player.Source()&&animations[0].cursor.selection.atlas==battle::Atlas::ExtraEnemies);
                }
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.boss_->SpellName())assert(game.boss_->SpellCardNumber()==(id==P::Behavior::SilentSelene?119:id==P::Behavior::RoyalFlare?120:121));
                if(!paused&&game.boss_->skillTimer>3) {
                    paused=true;const auto time=game.boss_->skillTimer;const auto count=game.bullets_.Pool().ActiveCount();
                    game.TogglePause();game.Update(0.5F);assert(game.boss_->skillTimer==time&&game.bullets_.Pool().ActiveCount()==count);game.TogglePause();
                }
                if(!sampled&&game.boss_->skillTimer>7) {sampled=true;assert(peak>0);if(shot==0&&!damage)Capture(game,prefix,label.c_str());}
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>10) {game.boss_->health=0;game.ResolveCollisions();}
            }
            assert(ticks<60*120&&paused&&sampled&&!game.boss_->active&&game.boss_->Defeated()==damage);
            game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty()&&!game.boss_&&!EnemyCount(game));
            assert(!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" complete; peak "<<peak<<" projectiles\n";
        }
    }

    /// @brief 完整Extra道中三符使用生产配置，验证对白放行接口、全击破/全超时、五使魔回收和退出资源。
    /// 仍为隔离Game夹具，不含Extra其余时间轴及实际对白。
    static void TestExtraPatchouliMidboss(Game& game,const std::string& prefix,bool formalAssets=false) {
        for(int shot=0;shot<4;++shot)for(bool damage:{false,true}) {
            game.EnterMainMenu();game.stageId_=StageId::Stage4;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.previewMode_=true;game.practiceMode_=game.spellPractice_=game.challengeMode_=false;
            game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<ExtraBattleFixtureContent>(battle::Selection{battle::Atlas::ExtraEnemies,65},formalAssets);
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();
            game.player_.SetInvulnerability(1000);game.referenceRank_=32;
            const auto config=stage_extra::PatchouliMidbossConfig(static_cast<ShotType>(shot));
            auto boss=std::make_unique<bosses::PatchouliBoss>();boss->StartBoss(config);game.boss_=std::move(boss);
            assert(game.boss_->health==6000);
            for(int tick=0;tick<240;++tick) {game.Update(1.0F/120);assert(!game.boss_->CanTakeDamage()&&!game.boss_->CanContactPlayer());}
            game.boss_->ResumeCombat();assert(game.boss_->health==1800);
            const auto label="extra-midboss-shot"+std::to_string(shot)+(damage?"-damage":"-timeout");
            std::array<bool,3> seen{},sampled{};std::size_t peak=0;int ticks=0;bool paused=false,children=false;
            for(;ticks<140*120&&game.boss_->active;++ticks) {
                game.Update(1.0F/120);assert(game.state_==RunState::Playing&&game.referenceRank_==32);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.boss_->SpellName()) {
                    const int card=game.boss_->SpellCardNumber();assert(card>=119&&card<=121);seen[card-119]=true;
                    if(card==121&&game.boss_->skillTimer>2.1F) {
                        assert(EnemyCount(game)==5);children=true;
                        game.actors_.ForEachActive([](const behavior::Actor& actor){
                            assert(actor.health==1&&!actor.interactable&&actor.scoreValue==10&&actor.sourceItem==-2);
                            const auto& auxiliary=actor.auxiliaryAnimations[0];assert(auxiliary.script==16&&auxiliary.cursor.player.Source());
                            assert(auxiliary.cursor.selection.atlas==battle::Atlas::ExtraEnemies);
                        });
                    }
                    if(!sampled[card-119]&&game.boss_->skillTimer>7) {
                        sampled[card-119]=true;if(shot==0&&!damage)Capture(game,prefix,(label+"-card"+std::to_string(card)).c_str());
                    }
                }
                if(!paused&&game.boss_->attack==2&&game.boss_->skillTimer>4) {
                    paused=true;const auto time=game.boss_->skillTimer;game.TogglePause();game.Update(0.5F);assert(game.boss_->skillTimer==time);game.TogglePause();
                }
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>10) {game.boss_->health=0;game.ResolveCollisions();}
            }
            assert(ticks<140*120&&!game.boss_->active&&game.boss_->Defeated()==damage&&paused&&children&&peak>0);
            assert(std::all_of(seen.begin(),seen.end(),[](bool value){return value;})&&std::all_of(sampled.begin(),sampled.end(),[](bool value){return value;}));
            for(int tick=0;tick<80;++tick)game.Update(1.0F/120);
            assert(!EnemyCount(game));game.EnterMainMenu();
            assert(game.assets_.Stage().visuals.empty()&&!game.boss_&&!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" complete; peak "<<peak<<" projectiles\n";
        }
    }

    /// @brief 六面四段非符或五组符卡在N/H/L分别走击破/超时，核对实际弹池、召唤身份、暂停和卸载。
    /// spellGroups为0时测试非符，为1或5时测试相应组数的符卡；batsOnly仅筛选化蝠非符，仍覆盖N/H/L及击破/超时。
    static void TestStage6DifficultyNonspells(Game& game,const std::string& prefix,int spellGroups=0,bool batsOnly=false) {
        using S=bosses::RemiliaBoss;using B=S::Behavior;
        constexpr std::array skills{B::FirstNonspell,B::SecondNonspell,B::ThirdNonspell,B::BatNonspell};constexpr std::array thresholds{1200,1600,1600,1300};
        constexpr std::array normal{B::StarOfVanities,B::ScarletNetherworld,B::VladTepesCurse,B::ScarletShoot,B::RedMagic};
        constexpr std::array high{B::YoungDemonLord,B::ThousandNeedles,B::VampireIllusion,B::ScarletMeister,B::ScarletGensokyo};
        for(int d=1;d<4;++d)for(int phase=0;phase<(spellGroups?spellGroups:4);++phase)for(bool damage:{false,true}) {
            if(batsOnly&&(spellGroups||phase!=3))continue;
            const int shot=(d+phase)%4;const auto difficulty=*native_combat::DifficultyFromIndex(d);
            game.stageId_=StageId::Stage6;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();
            game.stage_=StageDirector{};game.actors_.Clear();game.bullets_.Reset();game.bossEventId_.reset();game.runDifficulty_=difficulty;
            game.stageContent_=std::make_unique<stage6::Stage6Content>(difficulty);
            game.referenceRank_=32;
            auto config=stage6::RemiliaFinalConfig(static_cast<ShotType>(shot),difficulty);config.resumeSteps.clear();
            const auto id=bosses::BehaviorId{spellGroups?(d==1?normal[phase]:high[phase]):skills[phase]};
            config.phases={{{id},{{{0,1,WHITE}}}}};auto boss=std::make_unique<S>();boss->StartBoss(config);game.boss_=std::move(boss);
            for(int tick=0;tick<260;++tick)game.Update(1.0F/120);game.boss_->ResumeCombat();
            const auto label=std::string(spellGroups?"stage6-spell-d":"stage6-nonspell-d")+std::to_string(d)+"-phase"+std::to_string(phase)+(damage?"-damage":"-timeout");
            std::size_t peak=0;int ticks=0;bool paused=false,sampled=false,batSpread=false;
            for(;ticks<170*120&&game.boss_->active;++ticks) {
                game.Update(1.0F/120);assert(game.state_==RunState::Playing&&game.referenceRank_==32);
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(spellGroups&&game.boss_->SpellName())assert(game.boss_->SpellCardNumber()==103+phase*3+d);
                game.actors_.ForEachActive([&](const behavior::Actor& a){assert(a.difficulty==difficulty);});
                if(!spellGroups&&phase==3) {
                    bool left=false,right=false,up=false,down=false;
                    game.actors_.ForEachActive([&](const behavior::Actor& a){
                        assert(a.motion.velocity.z<0&&!a.interactable&&a.animationScript==165);
                        left|=a.motion.velocity.x<0;right|=a.motion.velocity.x>0;up|=a.motion.velocity.y>0;down|=a.motion.velocity.y<0;
                    });
                    if(!batSpread&&left&&right&&up&&down&&EnemyCount(game)>=20){
                        batSpread=true;if(!damage)Capture(game,prefix,(label+"-bat-spread").c_str());
                    }
                }
                if(!paused&&game.boss_->skillTimer>4) {
                    paused=true;const auto time=game.boss_->skillTimer;game.TogglePause();game.Update(0.5F);assert(game.boss_->skillTimer==time);game.TogglePause();
                }
                if(!sampled&&game.boss_->skillTimer>9) {sampled=true;assert(peak>0);if(!damage)Capture(game,prefix,label.c_str());}
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>12) {game.boss_->health=spellGroups?0:thresholds[phase]-1;game.ResolveCollisions();}
            }
            assert(ticks<170*120&&paused&&sampled&&!game.boss_->active&&game.boss_->Defeated()==damage);
            if(!spellGroups&&phase==3)assert(batSpread);
            game.EnterMainMenu();assert(!game.boss_&&game.assets_.Stage().visuals.empty()&&!EnemyCount(game));
            assert(!game.bullets_.Pool().ActiveCount()&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" complete; peak "<<peak<<" projectiles\n";
        }
    }

    /// @brief 正式目录开放前，借用Normal素材并替换四难度内容进行整关预验；结算记录和选关返回由正式入口另验。
    static void TestStage5PendingDifficulties(Game& game,const std::string& prefix) {
        for(int d=0;d<4;++d)for(bool damage:{false,true}) {
            const int shot=(d+(damage?2:0))%4;const auto difficulty=*native_combat::DifficultyFromIndex(d);
            game.stageId_=StageId::Stage5;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();
            game.stageContent_=std::make_unique<stage5::Stage5Content>(difficulty);game.runDifficulty_=difficulty;
            game.stage_=StageDirector(game.stageContent_->Events());
            std::array<bool,6> seen{};std::array<bool,3> stopped{};
            bool mid=false,midDialogue=false,before=false,after=false,paused=false;std::size_t peak=0;int frame=0;
            const auto label="stage5-pending-d"+std::to_string(d)+(damage?"-damage":"-timeout");
            for(;frame<1100*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                if(game.bullets_.Pool().DroppedSpawns()||game.actors_.DroppedSpawns())
                    std::cerr<<label<<" overflow frame "<<frame<<" stage "<<game.stage_.Elapsed()<<" attack "<<(game.boss_?game.boss_->attack:0)
                        <<" bullets "<<game.bullets_.Pool().ActiveCount()<<" actors "<<EnemyCount(game)<<'\n';
                assert(!game.bullets_.Pool().DroppedSpawns()&&!game.actors_.DroppedSpawns());
                game.actors_.ForEachActive([&](const behavior::Actor& a){assert(a.difficulty==difficulty);});peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.dialogueActive_) {
                    if(game.dialogueScene_==DialogueScene::Midboss)midDialogue=true;else if(game.dialogueAfterBattle_)after=true;else before=true;
                }
                if(!game.boss_||!game.boss_->active||game.boss_->attack<1)continue;
                const int attack=game.boss_->attack;
                if(game.boss_->skillTimer>5.1F) {
                    if(game.bossEventId_==1)mid=true;
                    else if(!seen.at(attack-1)) {seen.at(attack-1)=true;const auto name=label+"-skill"+std::to_string(attack);Capture(game,prefix,name.c_str());}
                }
                if(game.bossEventId_!=1&&game.WorldTimeStopped()) {assert(attack%2==0);stopped.at(attack/2-1)=true;}
                if(!paused&&game.boss_->skillTimer>6) {
                    paused=true;const auto elapsed=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                    game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==elapsed&&game.boss_->skillTimer==timer);game.TogglePause();
                }
                if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>8) {game.boss_->health=0;game.ResolveCollisions();}
            }
            assert(frame<1100*60&&mid&&midDialogue&&before&&after&&paused&&game.state_==RunState::Won);
            for(bool value:seen)assert(value);for(bool value:stopped)assert(value);
            assert(game.assets_.Stage().visuals.empty()&&!EnemyCount(game)&&!game.bullets_.Pool().ActiveCount());
            game.EnterMainMenu();assert(!game.boss_&&!game.actorEffects_.Pool().ActiveCount());
            std::cout<<label<<" complete at "<<frame/60.0F<<" seconds; peak "<<peak<<" projectiles\n";
        }
    }

    /// @brief 五面两角色整关超时与四机体击破，覆盖道中对白、时停冻结/恢复、召唤体和完整资源释放。
    static void TestStage5(Game& game,const std::string& prefix) {
        for(int character=0;character<2;++character) {
            game.stageId_=StageId::Stage5;game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            bool mid=false,midDialogue=false,before=false,after=false,children=false,paused=false;
            std::array<bool,6> phases{};std::array<bool,3> stops{};std::size_t peak=0;int frame=0;
            for(;frame<1100*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                assert(game.bullets_.Pool().DroppedSpawns()==0&&game.actors_.DroppedSpawns()==0);
                peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                if(game.dialogueActive_&&game.dialogueTimer_>1) {
                    bool& seen=game.dialogueScene_==DialogueScene::Midboss ? midDialogue : game.dialogueAfterBattle_ ? after : before;
                    if(!seen) {seen=true;const auto name="stage5-dialogue-"+std::to_string(character)+(game.dialogueScene_==DialogueScene::Midboss ? "-mid" : game.dialogueAfterBattle_ ? "-after" : "-before");Capture(game,prefix,name.c_str());}
                    if(!game.dialogueAfterBattle_)assert(game.boss_->attack==0&&!game.boss_->CanTakeDamage());
                }
                if(game.boss_&&game.boss_->active) {
                    if(game.bossEventId_==1)mid=true;
                    else if(game.boss_->attack>0&&game.boss_->attack<=6) {
                        const int phase=game.boss_->attack;children|=EnemyCount(game)>0;
                        if(game.boss_->skillTimer>5.1F&&!phases[phase-1]) {
                            phases[phase-1]=true;const auto name="stage5-character-"+std::to_string(character)+"-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());
                        }
                        if(game.WorldTimeStopped()) {
                            assert(!game.boss_->CanBeTargeted()&&!game.boss_->CanTakeDamage());
                            const int group=phase/2-1;assert(group>=0&&group<3);
                            if(!stops[group]) {stops[group]=true;const auto name="stage5-time-stop-"+std::to_string(group);Capture(game,prefix,name.c_str());}
                            if(!paused) {
                                paused=true;const auto time=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                                game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==time&&game.boss_->skillTimer==timer);game.TogglePause();
                            }
                        }
                    }
                }
            }
            if(frame>=1100*60)std::cerr<<"Stage 5 stalled at "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)<<'\n';
            assert(frame<1100*60&&mid&&midDialogue&&before&&after&&children&&paused);
            for(bool seen:phases)assert(seen);for(bool stopped:stops)assert(stopped);
            assert(game.state_==RunState::Won&&game.assets_.Stage().visuals.empty()&&EnemyCount(game)==0&&game.bullets_.Pool().ActiveCount()==0);
            std::cout<<"Whole Stage 5 character "<<character<<" cleared; peak "<<peak<<" projectiles\n";
        }
        for(int shot=0;shot<4;++shot) {
            game.stageId_=StageId::Stage5;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();
            game.previewMode_=false;game.stage_=StageDirector{};game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),100);game.ResumeStageBoss();
            for(int phase=1;phase<=2;++phase) {
                int ticks=0;while(!game.boss_->CanTakeDamage()&&ticks++<700)game.Update(1.0F/120);
                assert(ticks<700&&game.boss_->attack==phase);game.boss_->health=0;game.ResolveCollisions();
            }
            for(int tick=0;tick<400&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),101);game.ResumeStageBoss();
            for(int phase=1;phase<=6;++phase) {
                int ticks=0;while(!game.boss_->CanTakeDamage()&&ticks++<700)game.Update(1.0F/120);
                assert(ticks<700&&game.boss_->attack==phase);game.boss_->health=0;game.ResolveCollisions();
            }
            for(int tick=0;tick<150&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());for(int card:{86,90,94,98})assert(game.spellRecords_.Seen(shot,card));
        }
        game.stageId_=StageId::Stage4;game.StartPreview();game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
    }


    /// @brief 通过正式六面对话路径验证两自机的立绘创建、跳句重建与暂停；横竖屏实际像素必须包含两张蕾米莉亚差分。
    /// 临时隐藏当前纹理仅用于同帧像素对照，读取后立即恢复资源，退出时检查关卡资源释放。
    static void TestStage6Portraits(Game& game,const std::string& prefix) {
        const int width=GetScreenWidth(),height=GetScreenHeight();
        for(int character=0;character<2;++character) {
            game.stageId_=StageId::Stage6;game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            game.stage_=StageDirector{};
            game.BeginStageDialogue(3,false,DialogueScene::FinalBoss);
            const auto at=[&](int frame) {game.dialogueTimer_=frame/60.0F;game.UpdateUiMotion(0);};
            at(920);assert(game.dialogueMotion_.Sample(true).opacity==0);
            at(936);assert(std::abs(game.dialogueMotion_.Sample(true).slide-.25F)<1e-5F);
            at(951);assert(game.dialogueMotion_.Sample(true).slide==0&&game.dialogueMotion_.Sample(true).opacity==1);
            game.TogglePause();game.Update(.5F);assert(game.dialogueTimer_==951.0F/60);
            assert(game.dialogueMotion_.Sample(true).opacity==1);game.TogglePause();
            for(const auto size:std::array<Vector2,2>{{{1100,760},{390,844}}}) {
                SetWindowSize(size.x,size.y);
                const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());assert(IsRenderTextureValid(target));
                const auto draw=[&]() {
                    BeginDrawing();BeginTextureMode(target);ClearBackground(MAGENTA);game.DrawDialogue();EndTextureMode();EndDrawing();
                    Image image=LoadImageFromTexture(target.texture);assert(IsImageValid(image));ImageFlipVertical(&image);return image;
                };
                // 两角色换句相差一帧；取共同完成 16 帧 Alpha 过渡后的时刻，不能把过渡中途当稳定状态。
                for(const int frame:{936,1042,1460,1860}) {
                    at(frame);assert(game.dialogueMotion_.Sample(true).opacity==(frame==1460?128.0F/255:1));
                    const int expression=frame==1860?3:2;
                    const auto cues=game.stageContent_->SceneDialogueCues(DialogueScene::FinalBoss,false,game.player_.Shot());
                    assert(DialogueCueExpressionsAt(cues,game.dialogueTimer_,{0,0})[1]==expression);
                    const auto id=game.stageContent_->SceneBossPortraitVisual(DialogueScene::FinalBoss,expression);
                    auto& visual=const_cast<StageAssets&>(game.assets_.Stage()).visuals.at(id);
                    const auto texture=visual.texture;assert(IsTextureValid(texture));
                    Image shown=draw();visual.texture={};Image hidden=draw();visual.texture=texture;
                    int changed=0;
                    for(int y=0;y<shown.height;++y)for(int x=shown.width/2;x<shown.width;++x) {
                        const auto a=GetImageColor(shown,x,y),b=GetImageColor(hidden,x,y);
                        changed+=std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b)>6;
                    }
                    assert(changed>1000);UnloadImage(shown);UnloadImage(hidden);
                    const auto label="stage6-portrait-"+std::to_string(character)+"-frame-"+std::to_string(frame)+"-"+std::to_string(GetScreenWidth());
                    Capture(game,prefix,label.c_str());
                }
                UnloadRenderTexture(target);
            }
            at(920);assert(game.dialogueMotion_.Sample(true).opacity==0);
            at(1042);assert(game.dialogueMotion_.Sample(true).opacity==1);
            game.BeginStageDialogue(2,false,DialogueScene::Midboss);at(90);
            assert(game.dialogueMotion_.Sample(true).opacity==1);
            const auto maid=game.stageContent_->SceneBossPortraitVisual(DialogueScene::Midboss,1);
            assert(IsTextureValid(game.assets_.GetVisual(maid).texture));
            game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
        }
        SetWindowSize(width,height);
        std::cout<<"Stage 6 portrait entry, expressions, pause, seeking, sixteen rendered views and unload passed\n";
    }

    /// @brief 六面两角色整关按给定N/H/L走超时或击破，再验四机体遭遇/Bomb及卸载；预验时显式注入未开放内容。
    static void TestStage6(Game& game,const std::string& prefix,int difficultyIndex=1,bool damage=false) {
        const auto difficulty=*native_combat::DifficultyFromIndex(difficultyIndex);
        for(int character=0;character<2;++character) {
            game.stageId_=StageId::Stage6;game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            game.stageContent_=std::make_unique<stage6::Stage6Content>(difficulty);game.runDifficulty_=difficulty;
            game.stage_=StageDirector(game.stageContent_->Events());game.referenceRank_=32;
            bool mid=false,midDialogue=false,before=false,after=false,signalBirth=false,prelude=false,children=false,beams=false,paused=false;
            std::array<bool,9> phases{};std::size_t peak=0;int frame=0;
            for(;frame<1300*60&&game.state_!=RunState::Won;++frame) {
                game.Update(1.0F/60);assert(game.state_!=RunState::GameOver);
                game.actors_.ForEachActive([&](const behavior::Actor& actor){assert(actor.difficulty==difficulty);});
                if(game.bullets_.Pool().DroppedSpawns()||game.actors_.DroppedSpawns())
                    std::cerr<<"Stage 6 dropped at "<<frame<<" stage "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)
                        <<" timer "<<(game.boss_?game.boss_->skillTimer:-1)<<" actors "<<EnemyCount(game)<<" bullets "<<game.bullets_.Pool().ActiveCount()
                        <<" actor drops "<<game.actors_.DroppedSpawns()<<" bullet drops "<<game.bullets_.Pool().DroppedSpawns()<<'\n';
                assert(game.bullets_.Pool().DroppedSpawns()==0&&game.actors_.DroppedSpawns()==0);peak=std::max(peak,game.bullets_.Pool().ActiveCount());
                game.bullets_.Pool().ForEachActive([&](const Bullet& b){beams|=b.owner==BulletOwner::Enemy&&b.IsBeam();});
                if(game.dialogueActive_) {
                    if(game.dialogueScene_==DialogueScene::Midboss)midDialogue=true;
                    else if(game.dialogueAfterBattle_) {
                        assert(game.CurrentDialogue().empty());assert(std::abs(game.DialogueDuration()-1)<1e-5F);if(!after)Capture(game,prefix,"stage6-silent-ending");after=true;
                    } else {
                        if(!before&&game.dialogueTimer_>1) {before=true;const auto name="stage6-dialogue-"+std::to_string(character);Capture(game,prefix,name.c_str());}
                        if(game.boss_&&game.boss_->active&&!(game.bossEventId_==1)) {
                            signalBirth=true;assert(game.dialogueSignalsSeen_>=1&&game.boss_->attack==0&&!game.boss_->CanTakeDamage());
                            if(game.dialogueSignalsSeen_>=2)prelude=true;
                        }
                    }
                }
                if(game.boss_&&game.boss_->active) {
                    if(game.bossEventId_==1)mid=true;
                    else if(game.boss_->attack>0&&game.boss_->attack<=9) {
                        const int phase=game.boss_->attack;children|=EnemyCount(game)>0;
                        if(game.boss_->skillTimer>5.1F&&!phases[phase-1]) {
                            phases[phase-1]=true;const auto name="stage6-character-"+std::to_string(character)+"-phase-"+std::to_string(phase);Capture(game,prefix,name.c_str());
                        }
                        if(phase==9&&!paused) {
                            paused=true;const auto time=game.stage_.Elapsed(),timer=game.boss_->skillTimer;
                            game.TogglePause();game.Update(0.5F);assert(game.stage_.Elapsed()==time&&game.boss_->skillTimer==timer);game.TogglePause();
                        }
                    }
                    if(damage&&game.boss_->CanTakeDamage()&&game.boss_->skillTimer>8) {game.boss_->health=0;game.ResolveCollisions();}
                }
            }
            if(frame>=1300*60)std::cerr<<"Stage 6 stalled at "<<game.stage_.Elapsed()<<" phase "<<(game.boss_?game.boss_->attack:-1)<<'\n';
            assert(frame<1300*60&&mid&&midDialogue&&before&&after&&signalBirth&&prelude&&children&&beams&&paused);for(bool seen:phases)assert(seen);
            assert(game.state_==RunState::Won&&game.assets_.Stage().visuals.empty()&&EnemyCount(game)==0&&game.bullets_.Pool().ActiveCount()==0);
            std::cout<<"Whole Stage 6 d "<<difficultyIndex<<" character "<<character<<(damage?" damage":" timeout")<<" cleared at "<<frame/60.0<<"s; peak "<<peak<<" projectiles\n";
        }
        for(int shot=0;shot<4;++shot) {
            game.stageId_=StageId::Stage6;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.StartPreview();game.previewMode_=false;
            game.stageContent_=std::make_unique<stage6::Stage6Content>(difficulty);game.runDifficulty_=difficulty;
            game.stage_=StageDirector{};game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0),100);game.ResumeStageBoss();
            for(int phase=1;phase<=2;++phase) {
                int ticks=0;while(!game.boss_->CanTakeDamage()&&ticks++<700)game.Update(1.0F/120);
                assert(ticks<700&&game.boss_->attack==phase);game.boss_->health=0;game.ResolveCollisions();
            }
            for(int tick=0;tick<10&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),101);game.ResumeStageBoss();game.Update(1.0F/60);game.ResumeStageBoss();
            for(int phase=1;phase<=9;++phase) {
                int ticks=0;while(!game.boss_->CanTakeDamage()&&ticks++<700)game.Update(1.0F/120);
                assert(ticks<700&&game.boss_->attack==phase);
                if(phase==9) {
                    const float health=game.boss_->health;game.UpdateBattleStep(1.0F/120,PlayerInput{.bomb=true},false,false);
                    assert(game.playerBomb_.Active()&&!game.boss_->CanBeTargeted()&&!game.boss_->CanTakeDamage()&&game.boss_->health==health);
                    const auto name="stage6-bomb-bat-"+std::to_string(shot);Capture(game,prefix,name.c_str());
                    int recovery=0;while(!game.boss_->CanTakeDamage()&&recovery++<2400)game.Update(1.0F/120);
                    assert(recovery<2400&&!game.playerBomb_.Active()&&game.boss_->health==health);
                }
                game.boss_->health=0;game.ResolveCollisions();
            }
            for(int tick=0;tick<150&&game.boss_->active;++tick)game.Update(1.0F/120);
            assert(!game.boss_->active&&game.boss_->Defeated());for(int card:{100+difficultyIndex,103+difficultyIndex,106+difficultyIndex,109+difficultyIndex,112+difficultyIndex,115+difficultyIndex})assert(game.spellRecords_.Seen(shot,card));
        }
        game.stageId_=StageId::Stage5;game.StartPreview();game.EnterMainMenu();assert(game.assets_.Stage().visuals.empty());
    }


    /// @brief 通过真实追加战宣告/碰撞交接验证132…134的四御札记录，Miss/Bomb失败、预览隔离及中途退出。
    /// 不等待整关时间轴；单技能配置沿用角色数值及行为，测试不能直接调用记录写入接口制造取得。
    static void TestExtraEncounterRecords(Game& game) {
        using F=bosses::FlandreBoss;
        constexpr std::array skills{F::Behavior::FragileWing,F::Behavior::CreepingBloodstain,F::Behavior::DanmakuHeart};
        const auto stageScores=game.stageHighScores_;
        for(int shot=0;shot<4;++shot) {
            game.EnterMainMenu();game.stageId_=StageId::Extra;
            game.practiceMode_=game.spellPractice_=game.previewMode_=game.challengeMode_=false;
            game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;game.runDifficulty_=native_combat::Difficulty::Normal;
            game.stageContent_=std::make_unique<stage_extra::StageExtraContent>();
            game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();game.stage_=StageDirector{};
            for(int index=0;index<3;++index)for(int failure=0;failure<4;++failure) {
                const int card=132+index;const auto before=game.spellRecords_.Get(shot,card);
                game.ClearPlayerBomb();
                game.actorEffects_.Clear();game.actors_.Clear();game.bullets_.Reset();game.drops_.Clear();game.boss_.reset();
                game.previewMode_=failure==3;game.missCount_=game.bombCount_=0;
                game.player_.ConfigureRun(8,3,false,static_cast<ShotType>(shot));game.player_.SetInvulnerability(1000);
                const bosses::StartConfig config{.position={0,0,4},.seed=7,.visual=stage_extra::kBoss,
                    .defeatBase=stage_extra::kNativeDefeat,.configure=F::Behaviors::ConfigurePractice,.shot=static_cast<ShotType>(shot),
                    .defeated=F::Behavior::EncoreFinalExit,.retreat=F::Behavior::EncoreFinalExit,
                    .phases={{{skills[index]},{{{0,1,PINK}}}}}};
                auto boss=std::make_unique<F>();boss->StartBoss(config);game.boss_=std::move(boss);
                for(int tick=0;tick<1200&&(!game.boss_->CanTakeDamage()||game.boss_->SpellCardNumber()==0);++tick)game.Update(1.0F/120);
                assert(game.boss_->CanTakeDamage()&&game.boss_->SpellCardNumber()==card);
                game.Update(1.0F/120);assert(game.spellAttemptOpen_==(failure!=3));
                if(failure==1)game.HandlePlayerHit();
                if(failure==2)game.ApplyBombEffect();
                game.boss_->health=0;game.ResolveCollisions();
                for(int tick=0;tick<10&&static_cast<F&>(*game.boss_).CurrentSkill();++tick)game.Update(1.0F/120);
                assert(!static_cast<F&>(*game.boss_).CurrentSkill()&&!game.spellAttemptOpen_);
                const auto after=game.spellRecords_.Get(shot,card);
                assert(after.mainAttempts==before.mainAttempts+(failure!=3));
                assert(after.mainCaptured==before.mainCaptured+(failure==0));
                assert(after.attempts==0&&after.captured==0&&after.bestScore==0);
                assert(!game.spellRecords_.Seen((shot+1)%4,card)||(shot==3));
                const auto saved=LoadOrCreateSettings(kSettingsFilename).spellRecords.Get(shot,card);
                assert(saved.mainAttempts==after.mainAttempts&&saved.mainCaptured==after.mainCaptured);
                game.ResolveCollisions();game.Update(1.0F/120);
                assert(game.spellRecords_.Get(shot,card).mainCaptured==after.mainCaptured);
            }
            game.ClearPlayerBomb();game.boss_.reset();game.previewMode_=false;
            const auto beforeAbort=game.spellRecords_.Get(shot,134);
            const bosses::StartConfig abortConfig{.position={0,0,4},.seed=7,.visual=stage_extra::kBoss,
                .configure=F::Behaviors::ConfigurePractice,.shot=static_cast<ShotType>(shot),
                .defeated=F::Behavior::EncoreFinalExit,.retreat=F::Behavior::EncoreFinalExit,
                .phases={{{F::Behavior::DanmakuHeart},{{{0,1,PINK}}}}}};
            auto boss=std::make_unique<F>();boss->StartBoss(abortConfig);game.boss_=std::move(boss);
            game.Update(1.0F/120);assert(game.spellAttemptOpen_&&game.spellAttemptNumber_==134);
            game.EnterMainMenu();assert(!game.spellAttemptOpen_);
            const auto aborted=LoadOrCreateSettings(kSettingsFilename).spellRecords.Get(shot,134);
            assert(aborted.mainAttempts==beforeAbort.mainAttempts+1&&aborted.mainCaptured==beforeAbort.mainCaptured);
        }
        assert(game.stageHighScores_.scores==stageScores.scores&&game.stageHighScores_.difficultyScores==stageScores.difficultyScores);
        game.EnterMainMenu();assert(!game.boss_&&!EnemyCount(game)&&game.assets_.Stage().visuals.empty());
        std::cout<<"Extra encounter records: four shots / three encore cards / capture, Miss, Bomb, preview, abort and persistence passed\n";
    }

    /// @brief 正式一面/Extra素材验证擦弹创建点、碰撞后首步、半帧、暂停、GPU、池满奖励及收尾释放。
    /// 本专项隔离单次效果；光束周期另由TestBeamRepeatGraze覆盖，源统计门控仍待审阅。
    static void TestGrazeEffects(Game& game,const std::string& prefix) {
        for(bool extra:{false,true})for(bool beam:{false,true})for(int fps:{120,30,60,144,240}) {
            game.EnterMainMenu();game.stageId_=extra?StageId::Extra:StageId::Stage1;
            game.selectedDifficulty_=0;game.selectedCharacter_=game.selectedShot_=0;
            game.practiceMode_=game.spellPractice_=game.previewMode_=game.challengeMode_=false;
            game.StartRun(false,true);game.stage_=StageDirector{};game.boss_.reset();game.actors_.Clear();
            game.bullets_.Reset();game.drops_.Clear();game.player_.SetInvulnerability(1000);
            assert(game.externalEffects_.Source()&&!game.externalEffects_.ActiveCount());
            const auto player=game.player_.Position();
            const Vector3 point{player.x+.5F,player.y+.25F,player.z+.1F};
            auto spawn=[&] {
                auto* bullet=beam?game.bullets_.SpawnBeam({point.x,point.y,point.z-1},{0,0,1},2,.1F,0,10,WHITE)
                    :game.bullets_.Spawn(BulletOwner::Enemy,point,{},.1F,WHITE,stage1::kBlueBullet);
                assert(bullet&&bullet->Dangerous());return bullet;
            };
            auto* bullet=spawn();const auto score=game.score_;const int graze=game.graze_;
            game.ResolveCollisions();assert(bullet->grazed&&game.score_==score+500&&game.graze_==graze+1);
            assert(game.externalEffects_.ActiveCount()==1);
            const Vector3 expected=beam?player:Vector3{(player.x+point.x)/2,(player.y+point.y)/2,(player.z+point.z)/2};
            game.externalEffects_.ForEachActive([&](const auto& p){
                assert(p.kind==effects::Kind::Graze&&p.animation.ScriptId()==13&&p.frame==0&&!p.initialized);
                assert(p.position.x==expected.x&&p.position.y==expected.y&&p.position.z==expected.z);
                assert(p.animation.Source()==game.externalEffects_.Source()&&p.animation.Sample().color.a==255);
            });
            game.ResolveCollisions();assert(game.externalEffects_.ActiveCount()==1&&game.score_==score+500);
            game.TogglePause();game.Update(.5F);game.TogglePause();
            game.externalEffects_.ForEachActive([](const auto& p){assert(p.frame==0&&!p.initialized);});
            game.Update(1.0F/120);
            game.externalEffects_.ForEachActive([&](const auto& p){
                assert(p.frame==.5&&p.initialized&&p.animation.Sample().color.a==255&&p.position.y==expected.y);
                assert(p.position.x!=expected.x||p.position.z!=expected.z);
            });
            int additionalGrazes=0;
            if(fps==120) {
                // 光束请求位于相机原点，单粒可能飞向后方；用真实重复事件覆盖随机方向，不能挪动生产出生点。
                for(int n=0;n<31;++n) {
                    game.bullets_.Reset();auto* next=spawn();game.ResolveCollisions();assert(next->grazed);
                    ++additionalGrazes;assert(game.externalEffects_.ActiveCount()==static_cast<std::size_t>(additionalGrazes+1));
                }
                assert(game.score_==score+500LL*(additionalGrazes+1)&&game.graze_==graze+additionalGrazes+1);
                game.Update(2.0F/60);
                const auto render=[&] {
                    const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());assert(IsRenderTextureValid(target));
                    BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.Draw();EndTextureMode();EndDrawing();
                    Image image=LoadImageFromTexture(target.texture);auto* colors=LoadImageColors(image);
                    std::vector<Color> pixels(colors,colors+image.width*image.height);UnloadImageColors(colors);UnloadImage(image);UnloadRenderTexture(target);return pixels;
                };
                const auto with=render();auto saved=std::make_unique<effects::ExternalEffectSystem>(game.externalEffects_);
                game.externalEffects_.Clear();const auto without=render();game.externalEffects_=*saved;
                int changed=0;for(std::size_t n=0;n<with.size();++n)changed+=std::memcmp(&with[n],&without[n],sizeof(Color))!=0;
                if(!changed)std::cerr<<"Graze pixels extra/beam "<<extra<<'/'<<beam<<'\n';
                assert(changed>0);Capture(game,prefix,(std::string(extra?"extra":"stage1")+(beam?"-beam-graze":"-bullet-graze")).c_str());
            }
            game.bullets_.Reset(); // 隔离已生成效果的寿命，持续光束的后续周期由独立专项验证。
            for(int n=0;n<fps/2;++n)game.Update(1.0F/fps);
            assert(!game.externalEffects_.ActiveCount()&&!game.externalEffects_.DroppedSpawns()&&game.score_==score+500LL*(additionalGrazes+1));
            // 512槽仅限制视觉；满池的新擦弹仍结算一次分数和计数，也不覆盖活动粒子。
            assert(game.externalEffects_.Spawn({effects::Kind::Graze,player,512,WHITE})==512);
            game.bullets_.Reset();bullet=spawn();game.ResolveCollisions();
            assert(bullet->grazed&&game.score_==score+500LL*(additionalGrazes+2)&&game.graze_==graze+additionalGrazes+2);
            assert(game.externalEffects_.ActiveCount()==512&&game.externalEffects_.DroppedSpawns()==1);
            game.ResolveCollisions();assert(game.externalEffects_.DroppedSpawns()==1&&game.score_==score+500LL*(additionalGrazes+2));
            game.bullets_.Reset();game.EndStage(RunState::GameOver);
            assert(game.state_==RunState::Finishing&&game.externalEffects_.Source()&&!game.assets_.Stage().visuals.empty());
            int steps=0;while(game.state_==RunState::Finishing&&steps++<121)game.Update(1.0F/120);
            assert(steps<=60&&game.state_==RunState::GameOver&&!game.externalEffects_.Source()&&game.assets_.Stage().visuals.empty());
        }
        game.EnterMainMenu();
        std::cout<<"Graze effects: Stage1/Extra, ordinary/beam, five FPS, half step, pause, GPU, saturation and release passed\n";
    }

    /// @brief 正式素材与Game固定步核对持续光束的独立源节拍、500分、粒子数及暂停/停时恢复。
    /// 期望以背景独立时钟换算120Hz步数，再按源展开7帧、周期12帧计算，不读取弹体的擦弹记录。
    static void TestBeamRepeatGraze(Game& game) {
        struct ClockBoss final:Boss {
            bool stopped=false;
            ClockBoss() {active=true;health=maxHealth=100000;position={0,0,4};}
            bool Update(float,Vector3,BulletSystem&) override {return false;}
            bool AdvanceSkill() override {return true;}
            const char* SpellName() const override {return nullptr;}
            const char* DisplayName() const override {return "Beam clock probe";}
            bool ControlsWorldTime() const override {return true;}
            bool WorldTimeStopped() const override {return stopped;}
            bool CanContactPlayer() const override {return false;}
        };
        for(bool extra:{false,true})for(int fps:{30,60,120,144,240}) {
            game.EnterMainMenu();game.stageId_=extra?StageId::Extra:StageId::Stage1;
            game.selectedDifficulty_=game.selectedCharacter_=game.selectedShot_=0;
            game.practiceMode_=game.spellPractice_=game.previewMode_=game.challengeMode_=false;
            game.StartRun(false,true);game.stage_=StageDirector{};game.actors_.Clear();game.drops_.Clear();
            game.bullets_.Reset();game.boss_=std::make_unique<ClockBoss>();
            auto* clock=static_cast<ClockBoss*>(game.boss_.get());game.player_.SetInvulnerability(1000);
            const auto p=game.player_.Position();
            auto* beam=game.bullets_.SpawnBeam({p.x-2,p.y+.6F,p.z},{1,0,0},4,.1F,7.0F/60,2,WHITE);assert(beam);
            beam->warningSeconds=3.0F/60;beam->dangerEndSeconds=1;beam->maxAge=1.4F;
            auto* ordinary=game.bullets_.Spawn(BulletOwner::Enemy,{p.x,p.y+.6F,p.z},{},.1F,WHITE,stage1::kBlueBullet);assert(ordinary);
            const auto score=game.score_;const int graze=game.graze_,stageGraze=game.stageGraze_,rank=game.referenceRank_;
            const auto start=game.scenerySeconds_;
            for(int frame=0;frame<fps*3/2;++frame) {
                if(frame==fps/3||frame==fps/2) {
                    const auto age=beam->age;const auto previousScore=game.score_;const auto count=game.externalEffects_.ActiveCount();
                    if(frame==fps/3) {game.TogglePause();game.Update(.25F);game.TogglePause();}
                    else {clock->stopped=true;game.Update(.25F);clock->stopped=false;}
                    assert(beam->age==age&&game.score_==previousScore);
                    // 效果池在世界停时仍推进；暂停则连效果动画也冻结。
                    if(frame==fps/3)assert(game.externalEffects_.ActiveCount()==count);
                }
                game.Update(1.0F/fps);
                const int ticks=static_cast<int>(std::lround((game.scenerySeconds_-start)*120));
                int beamEvents=0;
                for(int sourceFrame:{7,19,31,43,55})beamEvents+=ticks>sourceFrame*2;
                const int events=beamEvents+(ticks>0);
                assert(game.score_==score+500LL*events&&game.graze_==graze+events&&game.stageGraze_==stageGraze+events);
                assert(game.referenceRank_==rank&&game.missCount_==0&&game.bombCount_==0);
                // 240FPS首个显示帧可能尚无逻辑步；不能在夹具中额外触发首次普通弹接触。
                if(ticks>0) {const auto previous=game.score_;game.ResolveCollisions();assert(game.score_==previous);}
                assert(!game.externalEffects_.DroppedSpawns()&&!game.bullets_.Pool().DroppedSpawns());
            }
            assert(game.graze_==graze+6&&!beam->active&&ordinary->active&&!game.externalEffects_.ActiveCount());
            game.EnterMainMenu();assert(!game.externalEffects_.Source()&&game.assets_.Stage().visuals.empty());
        }
        std::cout<<"Beam repeat graze: Stage1/Extra, five FPS, sustain 0/12 cadence, warning, fade, pause, time stop and score passed\n";
    }

    /// @brief 实际碰撞覆盖球形弹/光束与重复接触；源事件固定500分、不写rank，计数饱和也继续加分。
    /// 四机体/四难度/双模式/五FPS分别覆盖非上限rank，避免用32掩盖错误的事件调整。
    static void TestGrazeRankAndScore(Game& game) {
        for(int difficulty=0;difficulty<4;++difficulty)for(int shot=0;shot<4;++shot) {
            game.EnterMainMenu();game.stageId_=StageId::Stage1;game.selectedDifficulty_=difficulty;
            game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.practiceMode_=game.spellPractice_=game.previewMode_=game.challengeMode_=false;
            game.StartRun(false,true);game.stage_=StageDirector{};game.boss_.reset();game.actors_.Clear();game.drops_.Clear();
            for(bool challenge:{false,true})for(int fps:{30,60,120,144,240})for(int rank:{10,18,31,32})for(bool saturated:{false,true}) {
                game.challengeMode_=challenge;game.player_.ConfigureRun(3,2,challenge,static_cast<ShotType>(shot));
                game.player_.SetInvulnerability(1000);game.bullets_.Reset();
                game.referenceRank_=rank;
                game.score_=12345;game.graze_=saturated?999998:0;game.stageGraze_=saturated?99998:0;
                const auto player=game.player_.Position();
                for(int event=0;event<32;++event) {
                    game.bullets_.Reset();
                    // 距离0.6在命中球之外、擦弹球之内；零速度排除跨显示帧移动造成的额外接触。
                    const Vector3 origin{player.x+.6F,player.y,player.z};
                    auto* bullet=event%2 ? game.bullets_.SpawnBeam({origin.x,origin.y,origin.z-1},{0,0,1},2,.1F,0,10,WHITE)
                        : game.bullets_.Spawn(BulletOwner::Enemy,origin,{},.1F,WHITE,stage1::kBlueBullet);
                    assert(bullet&&bullet->Dangerous());
                    // 144/240FPS的首个显示更新可能尚未积满120Hz逻辑步；保留同一颗弹直到实际结算。
                    int updates=0;while(!bullet->grazed&&updates++<4)game.Update(1.0F/fps);
                    if(!bullet->grazed||!bullet->active)std::cerr<<"Graze collision d"<<difficulty<<" shot "<<shot<<" fps "<<fps<<" event "<<event
                        <<" active "<<bullet->active<<" grazed "<<bullet->grazed<<" state "<<static_cast<int>(game.state_)<<'\n';
                    assert(bullet->grazed&&bullet->active);
                    const auto score=game.score_;game.Update(1.0F/fps);assert(game.score_==score);
                    assert(game.score_==12345+500LL*(event+1));
                    assert(game.graze_==(saturated?999999:event+1)&&game.stageGraze_==(saturated?99999:event+1));
                    assert(game.referenceRank_==rank);
                }
                assert(game.missCount_==0&&game.bombCount_==0&&!game.bullets_.Pool().DroppedSpawns());
            }
        }
        game.EnterMainMenu();
        std::cout<<"Graze: four difficulties/shots, two modes, five FPS, rank boundaries, score and counter saturation passed\n";
    }

    /// @brief 真实拾取/输入/碰撞链保持局部事件不写rank，同时验证奖励、消耗、Miss与保护期仍结算。
    /// 注入10/18/31/32覆盖旧兼容上下界及内部值；本专项不推导原生rank的合法范围。
    static void TestRankEventSettlement(Game& game) {
#ifdef KOUMAKYOU_DEBUG_FEATURES
        game.debugInvincible_=false; // 使用真实Miss碰撞，不能让调试开局的默认无敌吞掉待验证的结算。
#endif
        for(int difficulty=0;difficulty<4;++difficulty)for(int shot=0;shot<4;++shot) {
            game.EnterMainMenu();game.stageId_=StageId::Stage1;game.selectedDifficulty_=difficulty;
            game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
            game.practiceMode_=game.spellPractice_=game.previewMode_=game.challengeMode_=false;
            game.StartRun(false,true);game.stage_=StageDirector{};game.boss_.reset();game.actors_.Clear();
            for(bool challenge:{false,true})for(int fps:{30,60,120,144,240})for(int rank:{10,18,31,32}) {
                game.challengeMode_=challenge;game.ClearPlayerBomb();game.player_.Reset();
                game.player_.ConfigureRun(3,2,challenge,static_cast<ShotType>(shot));game.player_.SetInvulnerability(0);
                game.bullets_.Reset();game.drops_.Clear();game.missCount_=game.bombCount_=0;
                game.referenceRank_=rank;
                const auto checkRank=[&] {assert(game.referenceRank_==rank);};
                // 原始0…6全部经过实际DropSystem和Game；每种P在非满/满火力下均须保持rank。
                for(bool full:{false,true})for(int item=0;item<7;++item) {
                    game.drops_.Clear();game.player_.AdjustPower((full?128:8)-game.player_.Power());
                    const int power=game.player_.Power(),bombs=game.player_.Bombs(),lives=game.player_.RemainingLives();
                    const auto misses=game.player_.NetMisses();const auto score=game.score_;const auto points=game.pointItems_;
                    game.drops_.SpawnNativeItem(game.player_.Position(),item,full);assert(PickupCount(game)==1);
                    game.UpdatePickups(0);assert(PickupCount(game)==0);checkRank();
                    if(item==0||item==2)assert(full?game.score_>score:game.player_.Power()>power);
                    if(item==1)assert(game.pointItems_==points+1&&game.score_>score);
                    if(item==3)assert(game.player_.Bombs()==std::min(8,bombs+1));
                    if(item==4)assert(game.player_.Power()==128);
                    if(item==5)assert(challenge?game.player_.NetMisses()==misses-1:game.player_.RemainingLives()==std::min(9,lives+1));
                    if(item==6)assert(game.score_>score&&game.pointItems_==points);
                }
                game.ClearPlayerBomb();game.player_.Reset();game.player_.ConfigureRun(3,2,challenge,static_cast<ShotType>(shot));
                game.UpdatePlayer(1.0F/120,{.bomb=true});
                assert(game.playerBomb_.Active()&&game.player_.Bombs()==1&&game.bombCount_==1);checkRank();
                game.UpdatePlayer(1.0F/120,{.bomb=true});assert(game.bombCount_==1);checkRank();
                game.ClearPlayerBomb();game.player_.SetInvulnerability(0);game.player_.AdjustPower(32-game.player_.Power());
                const int lives=game.player_.RemainingLives();
                assert(game.bullets_.Spawn(BulletOwner::Enemy,game.player_.Position(),{},.1F,WHITE,stage1::kBlueBullet));
                game.ResolveCollisions();checkRank();
                assert(game.missCount_==1&&game.bombCount_==1&&game.player_.NetMisses()==1&&game.player_.Power()==16);
                assert(game.player_.RemainingLives()==lives-(challenge?0:1)&&game.state_==RunState::Playing);
                assert(!game.bullets_.Pool().ActiveCount());
                assert(game.bullets_.Spawn(BulletOwner::Enemy,game.player_.Position(),{},.1F,WHITE,stage1::kBlueBullet));
                game.ResolveCollisions();assert(game.missCount_==1);checkRank();
                // 高FPS仍跨过至少一个120Hz逻辑步，确认事件后进入正式总控不会补发隐含rank调整。
                game.Update(2.0F/fps);assert(game.referenceRank_==rank);
                assert(!game.bullets_.Pool().DroppedSpawns());
            }
        }
        game.EnterMainMenu();
        std::cout<<"Rank events: four difficulties/shots, two modes, five FPS, seven pickups, Bomb, Miss and protection passed\n";
    }

    /// @brief 从Easy/Extra正式内容入口跨过旧兼容周期；五显示FPS覆盖最大库存，另测低库存长周期。
    /// 只验证移除无源依据的计时增长，不据此断言原生其他回调不会写rank。
    static void TestRankElapsedTime(Game& game) {
        int profiles=0;
        for(bool extra:{false,true})for(bool challenge:{false,true})for(int fps:{30,60,120,144,240}) {
            game.EnterMainMenu();game.stageId_=extra?StageId::Extra:StageId::Stage1;
            game.selectedDifficulty_=0;game.runDifficulty_=native_combat::Difficulty::Normal;
            game.practiceMode_=game.spellPractice_=game.previewMode_=false;game.challengeMode_=challenge;
            game.StartRun(false,true);game.stage_=StageDirector{};game.boss_.reset();game.actors_.Clear();
            game.bullets_.Reset();game.drops_.Clear();game.ClearPlayerBomb();
            const int expected=extra?18:20;assert(game.referenceRank_==expected);
            const auto advance=[&](int seconds) {
                const auto scenery=game.scenerySeconds_,age=game.player_.VisualAge();
                const float stageTime=game.stage_.Elapsed();
                for(int frame=0;frame<seconds*fps;++frame)game.Update(1.0F/fps);
                assert(game.state_==RunState::Playing&&game.referenceRank_==expected);
                assert(std::abs(game.scenerySeconds_-scenery-seconds)<.01);
                assert(std::abs(game.player_.VisualAge()-age-seconds)<.01);
                assert(std::abs(game.stage_.Elapsed()-stageTime-seconds)<.02F);
            };
            game.player_.Reset();game.player_.ConfigureRun(5,2,challenge,ShotType::HomingAmulet);
            game.player_.CollectResources(3,0);game.player_.SetInvulnerability(1000);
            // Challenge残机奖励抵扣净Miss，不增加库存；两模式分别跨过旧8/20秒周期。
            assert(game.player_.RemainingLives()==(challenge?6:9));advance(24);
            if(fps==120) {
                game.player_.Reset();game.player_.ConfigureRun(1,2,challenge,ShotType::HomingAmulet);
                game.player_.SetInvulnerability(1000);assert(game.player_.RemainingLives()==2);advance(40);
            }
            ++profiles;
        }
        game.EnterMainMenu();
        std::cout<<"Rank elapsed time: "<<profiles<<" Easy/Extra, mode/FPS profiles and low-inventory periods passed\n";
    }

    /// @brief 隔离战斗验证新开局/练习/重开取原生初值，切关保留当前值；Extra通过正式内容独立取18。
    static void TestRankInitialization(Game& game) {
        constexpr std::array<int,4> expected{20,32,32,32};
        for(int index=0;index<4;++index)for(bool practice:{false,true}) {
            game.EnterMainMenu();game.stageId_=StageId::Stage1;
            game.selectedDifficulty_=index;game.practiceMode_=practice;
            game.spellPractice_=game.challengeMode_=false;
            game.StartRun(false,true);
            assert(game.referenceRank_==expected[index]);
            game.referenceRank_=13;
            game.StartRun(false,true); // 实际重开入口，必须丢弃上一局注入值。
            assert(game.referenceRank_==expected[index]);
            game.referenceRank_=23;
            game.stageId_=StageId::Stage2;game.StartRun(false,true,true);
            assert(game.referenceRank_==23);
        }
        for(bool challenge:{false,true}) {
            game.EnterMainMenu();game.stageId_=StageId::Extra;
            game.practiceMode_=game.spellPractice_=false;game.challengeMode_=challenge;
            game.runDifficulty_=native_combat::Difficulty::Normal;
            game.StartRun(false,true);
            assert(game.referenceRank_==18);
            assert(game.stageContent_->CombatData()->stage==7);
        }
        game.stageId_=StageId::Stage4;game.StartPreview();
        assert(game.stageId_==StageId::Stage4&&game.previewMode_&&game.stageContent_->CombatData()->stage==4);
        game.EnterMainMenu();
        std::cout<<"Initial rank: four difficulties, practice/restart/continuation and both Extra modes passed\n";
    }

    /// @brief 通过正式开局、结算和异步加载验证难度终点及库存/身份继承；隔离战斗，只验连续流程。
    /// difficultyIndex为E/N/H/L索引，shot为四御札索引；Easy必须止于五面，其余止于六面。
    static void TestCampaignTransitions(Game& game,const std::string& prefix,int difficultyIndex=1,int shot=3) {
        const auto difficulty=*native_combat::DifficultyFromIndex(difficultyIndex);const auto type=static_cast<ShotType>(shot);
        const int initialLives=game.initialLives_;game.initialLives_=4;
        game.stageId_=StageId::Stage1;game.selectedCharacter_=shot/2;game.selectedShot_=shot%2;
        game.practiceMode_=game.spellPractice_=game.previewMode_=game.challengeMode_=false;game.selectedDifficulty_=difficultyIndex;game.StartRun(false,true);
        assert(game.runDifficulty_==difficulty&&game.stageContent_->Difficulty()==difficulty);
        assert(game.referenceRank_==(difficultyIndex==0?20:32));
        // 故意改变下次开局选项，连续切关必须使用本局快照，不能从菜单重新读取难度。
        game.selectedDifficulty_=(difficultyIndex+1)%4;
        game.player_.ConfigureRun(4,2,false,type);game.player_.AdjustPower(97-game.player_.Power());
        game.score_=12345;game.graze_=75;game.pointItems_=25;game.missCount_=2;game.bombCount_=1;game.referenceRank_=23;
        const auto lives=game.player_.RemainingLives(),bombs=game.player_.Bombs(),power=game.player_.Power();
        int visited=0;
        for(const auto stage:kPracticeStages) {
            if(!stage)break;++visited;assert(game.stageId_==*stage&&game.state_==RunState::Playing);
            game.stage_=StageDirector{};game.stageGraze_=11;game.stagePointItems_=7;
            auto* actor=game.actors_.Spawn(testing::SpawnAt({0,0,4},stage1::kFairy,100,0.4F));assert(actor);
            game.UpdateActor(*actor,1.0F/120);
            assert(game.actorEffects_.Spawn(actor->visual,actor->position,actor->hitRadius,actor->presentation,ActorExitReason::Retreat,&actor->animation.player.Sample()));
            game.player_.SetView(0.8F,0.3F);const auto before=game.score_;game.BeginStageClear();
            const auto bonus=game.stageClearSummary_.Total();assert(game.score_==before+bonus&&game.stageClearSummary_.graze==11&&game.stageClearSummary_.pointItems==7);
            const auto next=CampaignEndsAfter(*stage,difficulty)?std::nullopt:NextStage(*stage);game.ContinueStageClear();
            if(!next) {
                assert(game.state_==RunState::Finishing);for(int tick=0;tick<200&&game.state_==RunState::Finishing;++tick)game.Update(1.0F/60);
                const auto expected=difficultyIndex==0?(shot<2?ending::Id::ReimuBad:ending::Id::MarisaBad):static_cast<ending::Id>(shot);
                assert(game.state_==RunState::Loading&&game.pendingEnding_==expected&&game.assets_.Stage().visuals.empty());
                assert(game.clearedMainlineStages_==(difficultyIndex==0?0x1f:0x3f));break;
            }
            assert(game.state_==RunState::Loading&&game.stageId_==*next&&game.assets_.Stage().visuals.empty());
            assert(EnemyCount(game)==0&&game.actorEffects_.Pool().ActiveCount()==0&&game.bullets_.Pool().ActiveCount()==0&&PickupCount(game)==0);
            const double deadline=GetTime()+60;
            while(game.state_==RunState::Loading) {
                assert(GetTime()<deadline&&!game.loadingFailed_);game.Update(1.0F/60);
                BeginDrawing();ClearBackground(BLACK);game.Draw();EndDrawing();
            }
            assert(game.state_==RunState::Playing&&game.stage_.Elapsed()==0);
            assert(game.runDifficulty_==difficulty&&game.stageContent_->Difficulty()==difficulty);
            assert(game.score_==before+bonus&&game.graze_==75&&game.pointItems_==25&&game.missCount_==2&&game.bombCount_==1);
            assert(game.stageGraze_==0&&game.stagePointItems_==0&&game.referenceRank_==23);
            assert(game.player_.RemainingLives()==lives&&game.player_.Bombs()==bombs&&game.player_.Power()==power&&game.player_.Shot()==type);
            assert(game.player_.Position().x==0&&game.player_.Position().y==0&&game.player_.Position().z==-6.5F);
            const auto name="campaign-d"+std::to_string(difficultyIndex)+"-shot"+std::to_string(shot)+"-stage-"+std::to_string(StageNumber(*next));Capture(game,prefix,name.c_str());
        }
        assert(visited==(difficultyIndex==0?5:6));game.initialLives_=initialLives;
        std::cout<<"Campaign d "<<difficultyIndex<<" shot "<<shot<<" completed "<<visited<<" stage transitions\n";
        game.stageId_=StageId::Stage1;game.StartPreview();game.BeginStageClear();game.ContinueStageClear();
        assert(game.state_==RunState::Finishing&&game.stageId_==StageId::Stage1);
        game.StartPreview();game.previewMode_=false;game.practiceMode_=true;game.BeginStageClear();game.ContinueStageClear();
        assert(game.state_==RunState::Finishing&&game.stageId_==StageId::Stage1);game.EnterMainMenu();
    }

    /// @brief 通过正式开局绑定检查两种御札，并渲染真实追踪弹和侧针，防止生成正常而素材/动画不可见。
    static void TestPlayerShots(Game& game, const std::string& prefix) {
        const int previousShot=game.selectedShot_;
        for(int shot : {0,1})for(int power : {Player::kMinPower,16,128}) {
            game.selectedShot_=shot;
            game.StartPreview();
            game.stage_=StageDirector{};
            game.player_.AdjustPower(power - game.player_.Power());
            const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
            const auto render=[&] {
                BeginDrawing(); BeginTextureMode(target); ClearBackground(BLACK);
                game.DrawWorld(); EndTextureMode(); EndDrawing();
                Image image=LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
                return image;
            };
            Image background=render();
            game.player_.Update(0,{.fire=true},game.bullets_,game.stageContent_->PlayerBulletVisual(),
                                shot==0 ? game.stageContent_->PlayerHomingVisual() : game.stageContent_->PlayerNeedleVisual(),true);
            int homing=0,needles=0;
            float directionBefore=0;
            game.bullets_.Pool().ForEachActive([&](const Bullet& bullet) {
                if(bullet.homingTurnRate>0) { ++homing; directionBefore+=bullet.Velocity().x; }
                if(bullet.visual==stage1::kPlayerNeedle) { ++needles; directionBefore+=bullet.Velocity().x; }
            });
            assert(homing==(shot==0 ? 2 : 0));
            assert(needles==(shot==1 ? (power==128 ? 6 : 2) : 0));
            // 偏右目标迫使成对弹的横向合速度改变；不能只检查 homingTurnRate 字段存在。
            const std::array<Vector3,1> targets{{{4,0,4}}};
            game.bullets_.Update(0.2F,targets,game.player_.Position());
            game.UpdateBattleAnimations();
            if(homing>0 || needles>0) {
                float directionAfter=0;
                game.bullets_.Pool().ForEachActive([&](Bullet& bullet) {
                    if(bullet.homingTurnRate>0 || bullet.visual==stage1::kPlayerNeedle) {
                        directionAfter+=bullet.Velocity().x;
                        const auto& frame=bullet.animation.player.Sample();
                        assert(frame.visible && frame.sprite>=0 && frame.color.a>0);
                    } else bullet.active=false; // 隔离主弹，像素断言只能由当前机体副弹满足。
                });
                if(homing>0)assert(directionAfter>directionBefore+1);
                else assert(std::abs(directionAfter-directionBefore)<0.0001F);
                Image image=render();
                int ink=0,peak=0;
                for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x) {
                    const Color a=GetImageColor(image,x,y),b=GetImageColor(background,x,y);
                    const int difference=std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b);
                    peak=std::max(peak,difference);
                    if(difference>3)++ink;
                }
                assert(ink>20 && peak>6);
                if(!prefix.empty()) {
                    const std::string path=prefix+(shot==0 ? "-homing-power-" : "-needle-power-")+std::to_string(power)+".png";
                    assert(ExportImage(image,path.c_str()));
                }
                UnloadImage(image);
                std::cout<<(shot==0 ? "Homing Power " : "Needle Power ")<<power<<": "<<(homing+needles)<<" bullets, "<<ink<<" visible pixels, peak "<<peak<<"\n";
            }
            UnloadImage(background); UnloadRenderTexture(target);
        }
        game.selectedShot_=previousShot;
        game.StartPreview();
    }

    /// @brief 对照混合显示步长下的真实输入轨迹与回放，检查单关录制边界、多关卡入口恢复和跨关录制生命周期。
    /// 短轨迹只验证输入、种子和120Hz缓存一致性，不代替完整通关录像的终局验收。
    static void TestReplayAdaptation(Game& game) {
        game.EnterMainMenu();game.stageId_=StageId::Stage1;game.selectedDifficulty_=1;
        game.practiceMode_=game.spellPractice_=false;game.selectedCharacter_=0;game.selectedShot_=0;
#ifdef KOUMAKYOU_DEBUG_FEATURES
        game.debugInvincible_=false;
#endif
        game.StartRun(false,true);
        auto data=game.replayData_;
        constexpr std::array<float,4> steps{1.0F/30,1.0F/60,1.0F/144,1.0F/240};
        for(int frame=0;frame<360;++frame) {
            replay::Frame input{.dt=steps[frame%4],.player={.mouseDelta={.15F,-.04F},
                .forward=frame<60,.moveLeft=frame>=60&&frame<120,.focus=frame%80<40,
                .fire=true,.aimAssistEnabled=frame%90<45}};
            game.runInput_=input;game.StepRun(input.dt);data.frames.push_back(input);
        }
        const auto position=game.player_.Position(),target=game.camera_.target;
        const auto score=game.score_;
        const auto count=game.bullets_.Pool().ActiveCount();
        std::vector<Vector3> bullets;
        game.bullets_.Pool().ForEachActive([&](const Bullet& b){bullets.push_back(b.position);});
        replay::FinishSession(data,game.ReplayResult());
        std::string error;assert(replay::CanPlay(data,error));
        game.stageId_=StageId::Stage6;game.selectedDifficulty_=3;
        game.spellSelection_={.stage=6,.group=2,.difficulty=4,.character=1};
        game.PlayReplay(data);game.StartRun(false,true);
        for(int guard=0;game.replayCursor_<data.frames.size();++guard) {
            assert(guard<1000);game.Update(1.0F/60,1.0F/60);
        }
        assert(BulletSystem::Length(game_math::Subtract(game.player_.Position(),position))<1e-5F);
        assert(BulletSystem::Length(game_math::Subtract(game.camera_.target,target))<1e-5F);
        assert(game.score_==score&&game.bullets_.Pool().ActiveCount()==count);
        std::size_t index=0;
        game.bullets_.Pool().ForEachActive([&](const Bullet& b){
            assert(BulletSystem::Length(game_math::Subtract(b.position,bullets[index++]))<1e-5F);
        });
        game.EnterMainMenu();assert(game.MainMenuItemAvailable(1)); // Extra不应被Replay菜单可用性禁用。
        assert(game.stageId_==StageId::Stage6&&game.selectedDifficulty_==3&&game.spellSelection_.stage==6);
        game.stageId_=StageId::Stage1;game.selectedDifficulty_=1;game.StartRun(false,true);
        game.stage_=StageDirector{};game.actors_.Clear();game.boss_.reset();
        auto* targetActor=game.actors_.Spawn(testing::SpawnAt({.8F,0,0}));assert(targetActor);
        targetActor->damageable=false;
        game.UpdatePlayer(0,{.aimAssistEnabled=true});assert(!game.player_.Aim().assisted);
        targetActor->damageable=true;
        game.UpdatePlayer(0,{.aimAssistEnabled=true});assert(game.player_.Aim().assisted);
        targetActor->visible=false;
        game.UpdatePlayer(0,{.aimAssistEnabled=true});assert(!game.player_.Aim().assisted);
        targetActor->visible=true;targetActor->health=0;
        game.UpdatePlayer(0,{.aimAssistEnabled=true});assert(!game.player_.Aim().assisted);
        // 失败保留结果菜单；返回菜单先登记成绩，拒绝分数后仍须提供已封存录像的保存询问。
        game.replayData_=data;game.replayMode_=replay::Mode::Idle;
        game.replaySaveState_=Game::ReplaySaveState::Ready;
        game.EndStage(RunState::GameOver);game.actorEffects_.Clear();game.externalEffects_.Clear();
        game.UpdateState(0);assert(game.state_==RunState::GameOver&&!game.ReplaySaveModalOpen());
        game.BeginScoreSave();game.selectedScoreSave_=1;
        PlayAutomationEvent({0,2,{KEY_ENTER,0,0,0}});game.UpdateScoreSave();
        PlayAutomationEvent({0,1,{KEY_ENTER,0,0,0}});assert(game.ReplaySaveModalOpen());
        game.EnterMainMenu();
        // 实际资源入口覆盖主线四难度、后续关卡、Extra，以及共享phase的不同符卡。
        const auto checkEntry=[&](StageId stage,int difficulty,bool practice,const spell::Card* card=nullptr) {
            std::cout<<"Replay entry: stage "<<StageNumber(stage)<<", difficulty "<<difficulty<<", spell "<<(card?card->number:0)<<"\n";
            game.EnterMainMenu();game.stageId_=stage;game.selectedDifficulty_=difficulty;
            game.practiceMode_=practice;game.spellPractice_=card!=nullptr;game.challengeMode_=false;
            if(card)game.spellSelection_={.stage=card->stage,.group=card->group,.difficulty=card->difficulty,.character=0};
            game.StartRun(false,true);game.captureCursor_=true;game.PrepareReplay();
            assert(game.replayMode_==replay::Mode::Recording);
            const int expectedPower=card?(card->number<=2?32:card->number<=9?64:128):
                practice?(stage==StageId::Stage1?8:stage==StageId::Stage2?64:128):8;
            assert(game.player_.Power()==expectedPower);
            assert(game.player_.RemainingLives()==(card?1:game.initialLives_+1));
            assert(game.player_.Bombs()==(card?0:game.initialBombs_));
            const auto& snapshot=game.replayData_.segments.front();
            assert(snapshot.power==static_cast<unsigned>(expectedPower));
            assert(snapshot.lives==static_cast<unsigned>(card?0:game.initialLives_));
            assert(snapshot.bombs==static_cast<unsigned>(card?0:game.initialBombs_));
            for(int frame=0;frame<48;++frame)game.Update(steps[frame%4]);
            const auto position=game.player_.Position();const auto power=game.player_.Power();
            auto recorded=game.replayData_;replay::FinishSession(recorded,game.ReplayResult());
            std::string error;assert(replay::CanPlay(recorded,error));
            if(stage==StageId::Extra && !card) {
                // Extra胜利先登记成绩，退场期间封存的录像不能被后续成绩入口清空。
                game.BeginStageClear();game.stageClearSeconds_=10;game.Update(1.0F/120);
                assert(!game.ReplaySaveModalOpen() && game.CanSaveReplay() && game.replayData_.result.won);
                game.EnterMainMenu();
            }
            if(stage==StageId::Stage2 && difficulty==0 && !card) {
                game.EndStage(RunState::GameOver);replay::FinishSession(game.replayData_,game.ReplayResult());
                game.replayMode_=replay::Mode::Idle;game.replaySaveState_=Game::ReplaySaveState::Ready;game.state_=RunState::GameOver;
                PlayAutomationEvent({0,2,{KEY_ESCAPE,0,0,0}});game.UpdateState(0);
                PlayAutomationEvent({0,1,{KEY_ESCAPE,0,0,0}});
                assert(game.ReplaySaveModalOpen() && game.replayAfterSave_==Game::ReplayAction::StageSelection);
                game.EnterMainMenu();
            }
            game.PlayReplay(recorded);game.StartRun(false,true);
            assert(game.stageId_==stage && game.selectedDifficulty_==difficulty);
            if(card)assert(spell::Find(game.spellSelection_.stage,game.spellSelection_.group,game.spellSelection_.difficulty)->number==card->number);
            for(int guard=0;game.replayCursor_<recorded.frames.size();++guard) {
                assert(guard<1000);game.Update(1.0F/60,1.0F/60);
            }
            assert(BulletSystem::Length(game_math::Subtract(game.player_.Position(),position))<1e-5F);
            assert(game.player_.Power()==power);
        };
        for(int difficulty=0;difficulty<4;++difficulty) {
            checkEntry(StageId::Stage1,difficulty,false);
            checkEntry(StageId::Stage2,difficulty,true);
        }
        for(auto stage:{StageId::Stage3,StageId::Stage4,StageId::Stage5,StageId::Stage6})checkEntry(stage,3,true);
        checkEntry(StageId::Extra,1,false);
        checkEntry(StageId::Stage1,2,true,spell::Find(0,0,2));
        checkEntry(StageId::Stage1,2,true,spell::Find(0,1,2));
        checkEntry(StageId::Stage5,1,true,spell::Find(4,1,1));
        checkEntry(StageId::Extra,1,true,spell::Find(6,12,4));
        // 模拟真实结算出口，确认结算等待输入属于旧关、加载不录制、次关保留累计库存和输入。
        game.EnterMainMenu();game.stageId_=StageId::Stage1;game.selectedDifficulty_=1;
        game.practiceMode_=game.spellPractice_=false;game.StartRun(false,true);game.captureCursor_=true;game.PrepareReplay();
        game.Update(1.0F/120);game.BeginStageClear();assert(!game.replayEnded_);
        game.stageClearSeconds_=10;game.Update(1.0F/120);
        assert(game.state_==RunState::Loading && game.replayMode_==replay::Mode::Recording);
        assert(game.replayData_.segments.size()==1 && game.replayData_.segments[0].end==game.replayData_.frames.size());
        const auto boundary=game.replayData_.frames.size();const auto exit=game.replayData_.segments[0].result;
        game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();
        assert(game.stageId_==StageId::Stage2 && game.replayData_.segments.size()==2);
        const auto& second=game.replayData_.segments[1];
        assert(second.begin==boundary && second.score==exit.score && second.lives+1==exit.lives && second.power==exit.power);
        game.Update(1.0F/120);assert(game.replayData_.frames.size()==boundary+1);
        // 后退定位必须恢复首关，而不是从当前二面重新开始。
        auto multiple=game.replayData_;replay::FinishSession(multiple,game.ReplayResult());
        game.PlayReplay(multiple);game.StartRun(false,true);
        // 隔离长战斗，将播放推进到同一个真实结算出口；仍由Update消费录制的切关步骤。
        game.BeginStageClear();game.stageClearSeconds_=10;game.replayCursor_=boundary-1;
        game.Update(1.0F/120,1.0F/120);
        assert(game.state_==RunState::Loading && game.replaySegment_==1 && !game.replayFinished_);
        assert(game.replayCursor_==boundary);
        game.assets_.LoadStage(game.stageContent_->Assets());game.CompleteStartRun();
        assert(game.stageId_==StageId::Stage2 && !game.replayFinished_);
        game.SeekReplay(0);assert(game.stageId_==StageId::Stage1 && game.replayCursor_==0 && game.replaySegment_==0);
        game.EnterMainMenu();game.stageId_=StageId::Stage1;game.selectedDifficulty_=1;
        std::cout<<"Replay fixed-step inputs, all entry identities and campaign boundaries passed\n";
    }

    /// @brief 以正式玩家更新、原 ANM 和 Game::Draw 录制两款副机；8 秒包含 Focus 按下和松开。
    static void CaptureOptionAnimations(Game& game,const std::string& prefix) {
        for(int character=0;character<2;character++) {
            game.selectedCharacter_=character;game.selectedShot_=0;game.StartPreview();
            game.stage_=StageDirector{};game.actors_.Clear();game.boss_.reset();game.bullets_.Reset();game.drops_.Clear();game.actorEffects_.Clear();
            const auto visuals=game.stageContent_->PlayerVisuals(game.player_.Shot());
            const auto folder=std::filesystem::path(prefix)/(character?"marisa":"reimu");std::filesystem::create_directories(folder);
            const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
            float normalSeparation=0,focusedSeparation=0;float firstAngle=0,lastAngle=0;
            for(int frame=0;frame<200;frame++) {
                const bool focus=frame>=50&&frame<125;
                game.player_.Update(.04F,{.focus=focus},game.bullets_,visuals,false);
                game.player_.SyncCamera(game.camera_);game.UpdateBattleAnimations();
                auto positions=game.player_.OptionPositions();float separation=Vector3Distance(positions[0],positions[1]);
                if(frame==49)normalSeparation=separation;if(frame==100)focusedSeparation=separation;
                auto angle=game.playerOptionAnimations_[0].player.Sample().rotation.z;if(frame==0)firstAngle=angle;if(frame==199)lastAngle=angle;
                BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.Draw();
                DrawText(character?"MARISA":"REIMU",430,95,24,WHITE);
                DrawText(focus?"FOCUS held":"NORMAL / Focus released",370,125,20,WHITE);
                EndTextureMode();EndDrawing();
                Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
                assert(ExportImage(image,(folder/(std::to_string(frame)+".png")).string().c_str()));UnloadImage(image);
            }
            assert(normalSeparation>focusedSeparation*2.9F);assert(firstAngle!=lastAngle);
            auto released=game.player_.OptionPositions();assert(std::abs(Vector3Distance(released[0],released[1])-normalSeparation)<.001F);
            UnloadRenderTexture(target);
        }
    }

    /// @brief 通过正式 DropSystem、动画和 Game::Draw 渲染六秒场内掉落，固定 25 fps 供 GIF 验收。
    /// 前段为实际抛落与旋转，4.4 秒吸取现有道具并将新敌弹转换为清弹星点。
    static void CaptureDrops(Game& game,const std::string& prefix) {
        game.StartPreview();game.stage_=StageDirector{};game.actors_.Clear();game.boss_.reset();
        game.bullets_.Reset();game.drops_.Clear();game.actorEffects_.Clear();
        game.drops_.SpawnFixedPower({-2.4F,.8F,3.5F},1);
        game.drops_.SpawnReferenceDrop({0,.8F,3.5F},1,false);
        game.drops_.SpawnFixedPower({2.4F,.8F,3.5F},8);
        const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
        std::filesystem::create_directories(prefix);
        for(int frame=0;frame<150;frame++) {
            if(frame==110){
                game.drops_.MagnetAllItems();
                for(int i=0;i<12;i++)game.bullets_.Spawn(BulletOwner::Enemy,{(i%4-1.5F)*1.2F,(i/4-1)*.8F,1},{},.1F,WHITE,0);
                game.drops_.ConvertEnemyBulletsToStars(game.bullets_);
            }
            if(frame>0)game.UpdatePickups(.04F);
            game.UpdateBattleAnimations();
            BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.Draw();EndTextureMode();EndDrawing();
            Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
            assert(ExportImage(image,(std::filesystem::path(prefix)/(std::to_string(frame)+".png")).string().c_str()));UnloadImage(image);
        }
        assert(PickupCount(game)==0);UnloadRenderTexture(target);
    }

    /// @brief 隔离真实太极圈资源，检查双圈像素、Focus 收拢、暂停冻结及卸载时动画借用的释放。
    static void TestPlayerOptions(Game& game, const std::string& prefix) {
        game.StartPreview();
        game.stage_=StageDirector{};
        game.actors_.Clear();game.boss_.reset();game.bullets_.Reset();game.drops_.Clear();
        const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
        const auto render=[&](bool hideOptions=false) {
            BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);
            // 场上已清空其他关卡对象，临时移出内容接口只隔离太极圈，HUD 绘制前恢复。
            auto content=hideOptions ? std::move(game.stageContent_) : nullptr;
            game.DrawWorld();
            if(content)game.stageContent_=std::move(content);
            game.DrawHud();EndTextureMode();EndDrawing();
            Image result=LoadImageFromTexture(target.texture);ImageFlipVertical(&result);return result;
        };
        assert(game.player_.Power()==Player::kMinPower && game.player_.OptionsVisible());
        Image background=render(true);
        game.UpdateBattleAnimations();
        const auto outward=game.player_.OptionPositions();
        for(int focused=0;focused<2;++focused) {
            if(focused) {
                game.player_.Update(8.0F/60,{.focus=true},game.bullets_,0,1,true);
                game.UpdateBattleAnimations();
            }
            const auto positions=game.player_.OptionPositions();
            Image result=render();
            for(int side=0;side<2;++side) {
                const auto& cursor=game.playerOptionAnimations_[side];
                assert(cursor.player.Source()!=nullptr && cursor.player.Sample().sprite==128);
                assert(cursor.selection.script==128+side && cursor.player.Sample().visible);
                const auto center=GetWorldToScreen(positions[side],game.camera_);
                int ink=0;
                for(int y=std::max(0,int(center.y)-105);y<std::min(result.height,int(center.y)+105);++y)
                    for(int x=std::max(0,int(center.x)-105);x<std::min(result.width,int(center.x)+105);++x) {
                        const auto a=GetImageColor(result,x,y),b=GetImageColor(background,x,y);
                        if(std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b)>10)++ink;
                    }
                assert(ink>100);
            }
            if(focused) {
                const auto distance=[](const std::array<Vector3,2>& points) {
                    return BulletSystem::Length({points[1].x-points[0].x,points[1].y-points[0].y,points[1].z-points[0].z});
                };
                assert(std::abs(distance(positions)*3-distance(outward))<0.0001F);
            }
            if(!prefix.empty()) {
                const auto path=prefix+(focused ? "-options-focus.png" : "-options-normal.png");
                assert(ExportImage(result,path.c_str()));
            }
            UnloadImage(result);
        }
        const double age=game.player_.VisualAge();
        const auto frame=game.playerOptionAnimations_[0].player.Sample();
        game.TogglePause();game.Update(0.5F);
        assert(game.player_.VisualAge()==age && game.playerOptionAnimations_[0].player.Sample().rotation.z==frame.rotation.z);
        game.TogglePause();
        game.EnterMainMenu();
        assert(game.playerOptionAnimations_[0].player.Source()==nullptr && game.playerOptionAnimations_[1].player.Source()==nullptr);
        UnloadImage(background);UnloadRenderTexture(target);
        game.StartPreview();
    }

    /// @brief 验证魔理沙独立图集、两种副机状态、真实导弹/激光像素及持续激光穿透与帧率无关伤害。
    static void TestMarisa(Game& game, const std::string& prefix) {
        TestUiFont(game);
        const int previousCharacter=game.selectedCharacter_,previousShot=game.selectedShot_;
        game.selectedCharacter_=1;game.selectedShot_=0;game.StartPreview();
        game.stage_=StageDirector{};game.actors_.Clear();game.boss_.reset();game.drops_.Clear();
        const auto target=LoadRenderTexture(GetScreenWidth(),GetScreenHeight());
        for(auto shot:{ShotType::MagicMissile,ShotType::IllusionLaser})for(bool focus:{false,true}) {
            game.player_.Reset();game.player_.ConfigureRun(3,3,false,shot);game.player_.AdjustPower(128);
            game.bullets_.Reset();game.playerOptionAnimations_={};
            const auto visuals=game.stageContent_->PlayerVisuals(shot);
            game.player_.Update(0,{.focus=focus,.fire=true},game.bullets_,visuals,true);
            for(int frame=0;frame<20;++frame) {
                game.player_.Update(1.0F/60,{.focus=focus,.fire=true},game.bullets_,visuals,true);
                game.bullets_.Update(1.0F/60);
            }
            game.player_.SyncCamera(game.camera_);game.UpdateBattleAnimations();
            for(int side=0;side<2;++side) {
                const auto& cursor=game.playerOptionAnimations_[side];
                assert(cursor.selection.atlas==battle::Atlas::Marisa && cursor.selection.script==128+side);
                assert(cursor.player.Sample().sprite==128 && cursor.player.Sample().rotation.z>0);
                assert(cursor.player.Source()!=game.assets_.GetVisual(stage1::kPlayerOptionLeft).archive.get());
            }
            const auto positions=game.player_.OptionPositions();
            const float separation=BulletSystem::Length({positions[1].x-positions[0].x,positions[1].y-positions[0].y,positions[1].z-positions[0].z});
            assert(std::abs(separation-(focus?0.52F:1.56F))<0.0001F);
            int secondary=0;
            game.bullets_.Pool().ForEachActive([&](const Bullet& bullet) {
                assert(bullet.animation.selection.atlas==battle::Atlas::Marisa);
                if(bullet.playerShotKind==PlayerShotKind::Missile || bullet.playerShotKind==PlayerShotKind::Laser) {
                    ++secondary;assert(bullet.animation.player.Sample().visible && bullet.animation.player.Sample().color.a>0);
                }
            });
            assert(secondary>0);
            BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);
            game.DrawWorld();game.DrawHud();EndTextureMode();EndDrawing();
            if(!prefix.empty()) {
                Image screenshot=LoadImageFromTexture(target.texture);ImageFlipVertical(&screenshot);
                const auto path=prefix+(shot==ShotType::MagicMissile?"-missile":"-laser")+(focus?"-focus.png":"-normal.png");
                assert(ExportImage(screenshot,path.c_str()));UnloadImage(screenshot);
            }
            // 隔离主弹与副机背景，以像素差验证副弹本身可见；不能让主弹替不可见激光满足截图检查。
            game.bullets_.Pool().ForEachActive([](Bullet& bullet){if(bullet.playerShotKind==PlayerShotKind::Projectile)bullet.active=false;});
            const auto renderSecondary=[&] {
                BeginDrawing();BeginTextureMode(target);ClearBackground(BLACK);game.DrawWorld();EndTextureMode();EndDrawing();
                Image result=LoadImageFromTexture(target.texture);ImageFlipVertical(&result);return result;
            };
            Image secondaryImage=renderSecondary();
            std::vector<Bullet*> sideBullets;
            game.bullets_.Pool().ForEachActive([&](Bullet& bullet){sideBullets.push_back(&bullet);bullet.active=false;});
            Image background=renderSecondary();
            for(auto* bullet:sideBullets)bullet->active=true;
            int ink=0;
            for(int y=0;y<secondaryImage.height;++y)for(int x=0;x<secondaryImage.width;++x) {
                const auto a=GetImageColor(secondaryImage,x,y),b=GetImageColor(background,x,y);
                if(std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b)>5)++ink;
            }
            if(!prefix.empty()) {
                const auto path=prefix+(shot==ShotType::MagicMissile?"-missile":"-laser")+(focus?"-focus-secondary.png":"-normal-secondary.png");
                assert(ExportImage(secondaryImage,path.c_str()));
            }
            UnloadImage(secondaryImage);UnloadImage(background);
            std::cout<<"Marisa "<<(shot==ShotType::MagicMissile?"missile":"laser")<<" focus "<<focus<<": "<<ink<<" secondary pixels\n";
            assert(ink>100);
            // 满火力两种副机布局都应留下清晰视野，不能让包住镜头的整屏光壳通过可见性检查。
            if(shot==ShotType::IllusionLaser)assert(ink<target.texture.width*target.texture.height/2);
            if(shot==ShotType::MagicMissile) {
                bool impact=false;
                game.bullets_.Pool().ForEachActive([&](Bullet& bullet){
                    if(!impact && bullet.playerShotKind==PlayerShotKind::Missile) {
                        game.bullets_.ExplodePlayerMissile(bullet);impact=true;
                    }
                });
                assert(impact);game.UpdateBattleAnimations();
                game.bullets_.Pool().ForEachActive([](const Bullet& bullet){
                    if(bullet.playerShotKind==PlayerShotKind::Explosion)
                        assert(bullet.animation.selection.atlas==battle::Atlas::Marisa && bullet.animation.player.Sample().additive);
                });
            } else {
                // 一次跨过淡出边界，原生中断必须只执行一次，年龄相同的重复同步不能重启动画。
                game.bullets_.Pool().ForEachActive([](Bullet& bullet){
                    if(bullet.playerShotKind==PlayerShotKind::Laser)bullet.age=bullet.laserFadeAt+15.0F/60;
                });
                game.UpdateBattleAnimations();game.UpdateBattleAnimations();
                game.bullets_.Pool().ForEachActive([](const Bullet& bullet){
                    if(bullet.playerShotKind==PlayerShotKind::Laser) {
                        assert(bullet.laserFadeStarted && bullet.animation.player.Sample().color.a>0 && bullet.animation.player.Sample().color.a<160);
                    }
                });
            }
        }

        for(int fps:{30,60,144}) {
            game.bullets_.Reset();game.actors_.Clear();
            game.player_.Reset();game.player_.ConfigureRun(3,3,false,ShotType::IllusionLaser);
            game.player_.Update(0,{.fire=true},game.bullets_,game.stageContent_->PlayerVisuals(game.player_.Shot()),true);
            Vector3 origin{},direction{};
            game.bullets_.Pool().ForEachActive([&](Bullet& bullet){
                if(bullet.playerShotKind!=PlayerShotKind::Laser)bullet.active=false;
                else if(bullet.playerSlot==0){origin=bullet.position;direction=bullet.Direction();}
            });
            for(float depth:{4.0F,7.0F}) {
                auto* enemy=game.actors_.Spawn(testing::SpawnAt({origin.x+direction.x*depth,origin.y+direction.y*depth,origin.z+direction.z*depth},stage1::kFairy,1000,0.1F));assert(enemy);
            }
            game.boss_=game.stageContent_->CreateBoss(testing::BattleAt(*game.stageContent_,1));game.boss_->hitRadius=0.1F;
            game.boss_->position={origin.x+direction.x*7,origin.y+direction.y*7,origin.z+direction.z*7};
            game.score_=0;
            for(int frame=0;frame<fps;++frame) {game.bullets_.Update(1.0F/fps);game.ResolveCollisions();game.ResolveCollisions();}
            int enemies=0;
            game.actors_.ForEachActive([&](const behavior::Actor& enemy){++enemies;assert(std::abs(enemy.health-820)<0.002F);});
            assert(enemies==2); // 一束激光同时穿透两个目标，伤害不随 FPS 或重复结算次数增加。
            if(game.score_!=600)std::cerr<<"Marisa laser score at "<<fps<<" FPS: "<<game.score_<<"\n";
            assert(game.score_==600); // 同一束持续命中的原始 60 个逻辑帧，不能按显示帧重复加分。
            game.boss_.reset();
            game.bullets_.Reset();game.actors_.Clear();
            auto* enemy=game.actors_.Spawn(testing::SpawnAt({},stage1::kFairy,100,0.1F));assert(enemy);
            auto* burst=game.bullets_.Spawn(BulletOwner::Player,{}, {1,0,0},0.5F,WHITE,stage1::kMarisaExplosion);
            assert(burst);burst->playerShotKind=PlayerShotKind::Explosion;burst->damage=0;burst->damagePerSecond=20;
            for(int frame=0;frame<fps/6;++frame){game.bullets_.Update(1.0F/fps);game.ResolveCollisions();}
            assert(std::abs(burst->Velocity().x-1.0F/1024)<0.00001F); // 1/6 秒恰为十个原始减速帧。
            game.actors_.ForEachActive([](const behavior::Actor& enemy){assert(std::abs(enemy.health-(100-20.0F/6))<0.002F);});
        }

        // 以实际遭遇入口记录魔/恋组合，不能误写到灵梦的两栏。
        game.previewMode_=false;game.spellPractice_=false;game.practiceMode_=true;
        for(int combination:{2,3}) {
            game.FinishSpellRecord(false);game.selectedCharacter_=1;game.selectedShot_=combination%2;
            game.player_.ConfigureRun(3,3,false,static_cast<ShotType>(combination));
            const int before=game.spellRecords_.Get(combination,3).mainAttempts;
            game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1),3);game.AdvanceBossSkill();
            assert(game.player_.IsMarisa() && game.spellRecords_.Get(combination,3).mainAttempts==before+1);
            game.FinishSpellRecord(false);
        }
        UnloadRenderTexture(target);
        game.selectedCharacter_=previousCharacter;game.selectedShot_=previousShot;game.StartPreview();
        std::cout<<"Marisa native atlases, Focus, missiles, lasers, piercing damage and combination records passed\n";
    }

    static void Run(Game& game, const std::string& prefix) {
        TestUiFont(game);
        TestPlayerOptions(game,prefix);
        TestPlayerShots(game,prefix);
        TestMarisa(game,prefix);
        TestBossTransitions(game,prefix);
        TestBossHud(game,prefix);
        TestFullWindow(game,prefix);
        TestDialogueTransitions(game,prefix);
        TestPortraits(game,prefix);
        TestNativeDialogue(game,prefix);
        TestSpatialFrameRates(game,prefix);
        TestWholeStage(game, prefix);
        TestBossEntryPreservesField(game);
        TestActorLifecycle(game,prefix);
    }

    /// @brief 单独覆盖演员池复用、完整退场、暂停冻结及资源卸载，便于定位整关结束后的生命周期问题。
    static void TestActorLifecycle(Game& game,const std::string& prefix) {
        // 经过真实 Game 更新验证高帧率下直接行为演员的入场与独立视觉时钟。
        game.StartPreview();game.stage_=StageDirector{};
        game.actors_.Spawn(testing::SpawnAt({0,-0.5F,8.8F}));
        game.actors_.Spawn(testing::SpawnAt({9,3.15F,7.8F}));
        for (int frame = 0; frame < 90; ++frame) game.Update(1.0F / 240.0F);
        assert(EnemyCount(game) == 2);
        game.actors_.ForEachActive([](const behavior::Actor& enemy) {
            assert(enemy.presentation.Sample().alpha == 1.0F);
        });
        Capture(game, prefix, "fairy-visible");

        game.StartPreview();
        game.stage_ = StageDirector{};
        game.actors_.Spawn(testing::SpawnAt({0,0,4}));
        game.UpdateActors(0.125F);
        Capture(game, prefix, "enemy-entry");
        game.TogglePause();
        const Vector3 pausedPosition = [&] {
            Vector3 position{};
            game.actors_.ForEachActive([&](const behavior::Actor& enemy) { position = enemy.position; });
            return position;
        }();
        game.Update(2.0F);
        game.actors_.ForEachActive([&](const behavior::Actor& enemy) {
            assert(enemy.position.z == pausedPosition.z);
            assert(enemy.age == 0.125F);
        });
        game.TogglePause();
        assert(game.state_ == RunState::Playing);
        Bullet* shot = game.bullets_.Spawn(BulletOwner::Player, pausedPosition, {},
                                          0.1F, WHITE, kInvalidVisualId);
        assert(shot);
        shot->damage = 2.0F;
        game.ResolveCollisions();
        assert(!shot->active);
        assert(EnemyCount(game) == 0);
        assert(game.score_ == 500);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        const auto pickups = PickupCount(game);
        game.UpdateBattleAnimations();
        game.actorEffects_.Pool().ForEachActive([&](const ActorExitEffect& effect) {
            assert(effect.nativeDefeat);
            const auto& visual=game.assets_.GetVisual(effect.visual);
            assert(visual.animation.atlas==battle::Atlas::BulletEffects);
            assert(visual.animation.script==3 && effect.animation.visible);
            assert(effect.animation.additive && effect.animation.sprite==4);
        });
        Capture(game, prefix, "enemy-defeat");
        game.ResolveCollisions();
        assert(game.score_ == 500 && PickupCount(game) == pickups);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);

        // 槽位可立即复用；新敌人活动，旧快照仍停在旧位置，且不能再被玩家弹命中。
        game.actors_.Spawn(testing::SpawnAt({4,0,5},stage1::kOrb));
        assert(EnemyCount(game) == 1);
        game.actorEffects_.Pool().ForEachActive([&](const ActorExitEffect& effect) {
            assert(effect.position.x == pausedPosition.x && effect.position.z == pausedPosition.z);
            assert(effect.reason == ActorExitReason::Defeated);
        });
        game.BeginStageDialogue(1);
        assert(EnemyCount(game) == 0);
        assert(game.score_ == 500 && PickupCount(game) == pickups);
        assert(game.actorEffects_.Pool().ActiveCount() == 2);
        game.dialogueActive_ = false;
        game.actorEffects_.Update(1.0F);

        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1), 2);
        game.UpdateBoss(0.3F);
        assert(game.boss_ && game.boss_->active);
        Capture(game, prefix, "boss-entry");
        const float alpha = game.boss_->presentation.Sample().alpha;
        game.boss_->health = 0.0F;
        game.AdvanceBossSkill();
        assert(game.boss_->attack == 2);
        assert(game.boss_->presentation.Sample().alpha == alpha);
        assert(game.actorEffects_.Pool().ActiveCount() == 0);
        for (int phase = 2; phase <= 4; ++phase) {
            game.boss_->health = 0.0F;
            game.AdvanceBossSkill();
        }
        // 末符击破后先完成两句战后独白，再展示关卡奖励；关闭后完成剩余死亡演出与资源释放。
        for(int frame=0;frame<62;++frame)game.UpdateBoss(1.0F/60);
        assert(!game.boss_->active);
        game.BeginStageDialogue(4,true);
        assert(game.state_==RunState::Playing && game.dialogueActive_ && game.dialogueTimer_==0);
        assert(!game.stageClearShown_);
        game.dialogueTimer_=game.DialogueDuration();
        game.Update(1.0F/120);
        assert(game.state_==RunState::StageClear && !game.dialogueActive_);
        game.ContinueStageClear();
        assert(game.state_ == RunState::Finishing);
        assert(!game.boss_);
        assert(!game.assets_.Stage().visuals.empty());
        assert(game.bullets_.Pool().ActiveCount() == 0 && PickupCount(game) == 0);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        const auto finalScore = game.score_;
        game.AdvanceBossSkill();
        assert(game.score_ == finalScore);
        game.Update(0.2F);
        Capture(game, prefix, "boss-defeat");
        float pausedEffectAge=0;
        game.actorEffects_.Pool().ForEachActive([&](const ActorExitEffect& effect) {
            assert(effect.age>=0.2F&&effect.age<0.22F);pausedEffectAge=effect.age;
        });
        game.TogglePause();
        game.Update(2.0F);
        assert(game.state_ == RunState::Paused);
        assert(!game.assets_.Stage().visuals.empty());
        game.actorEffects_.Pool().ForEachActive([&](const ActorExitEffect& effect) {
            assert(effect.age==pausedEffectAge);
        });
        game.TogglePause();
        assert(game.state_ == RunState::Finishing);
        game.Update(0.2F);
        assert(game.state_ == RunState::Finishing);
        assert(!game.assets_.Stage().visuals.empty());
        game.Update(0.5F);
        assert(game.state_ == RunState::Won);
        assert(game.assets_.Stage().visuals.empty());
        assert(game.actorEffects_.Pool().ActiveCount() == 0);
        Capture(game, prefix, "stage-clear");

        game.StartPreview();
        game.stage_ = StageDirector{};
        auto retreat=testing::SpawnAt(Vector3{0,0,-10.5357141F});
        retreat.pattern=stage1::kLateTurningFairy;retreat.retireOutside=true;retreat.sourceScore=0;game.actors_.Spawn(retreat);
        game.UpdateActors(0.1F);
        assert(EnemyCount(game) == 0 && game.score_ == 0 && PickupCount(game) == 0);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        game.UpdateActors(0.1F);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        game.actorEffects_.Clear();

        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,0), 3);
        for(int frame=0;frame<1503;++frame)game.UpdateBoss(1.0F/60);
        assert(game.state_ == RunState::Playing);
        assert(!game.boss_->active);
        game.actorEffects_.Pool().ForEachActive([](const ActorExitEffect& effect) {
            assert(effect.reason == ActorExitReason::Retreat);
            assert(effect.duration == 0.5F);
        });
        game.actorEffects_.Update(0.2F);
        Capture(game, prefix, "boss-retreat");

        game.StartPreview();
        assert(game.actorEffects_.Pool().ActiveCount() == 0);
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1), 4);
        game.EndStage(RunState::GameOver);
        assert(game.state_ == RunState::Finishing);
        assert(!game.assets_.Stage().visuals.empty());
        game.Update(0.6F);
        assert(game.state_ == RunState::GameOver && game.assets_.Stage().visuals.empty());

        game.StartPreview();
        game.BeginStageBoss(testing::BattleAt(*game.stageContent_,1), 5);
        game.EndStage(RunState::Won);
        assert(game.actorEffects_.Pool().ActiveCount() == 1);
        game.Reset();
        assert(game.state_ == RunState::MainMenu);
        assert(game.actorEffects_.Pool().ActiveCount() == 0);
        assert(game.assets_.Stage().visuals.empty());
    }
};

}  // namespace koumakyou

/// @brief 隐藏窗口集成测试；argv[1]为截图前缀，argv[2]选择主线或Extra专项，完整选项见下方显式分派；缺省运行通用回归。
/// --bomb-audio-only/--combat-audio-only/--extra-assets-only初始化音频且强制静音，设置与成绩只写临时目录。
int main(int argc, char** argv) {
    const auto originalDirectory = std::filesystem::current_path();
    const std::string prefix = argc >= 2 && argv[1][0] != '\0' ? std::filesystem::absolute(argv[1]).string() : "";
    const auto scratch = std::filesystem::temp_directory_path() /
        ("koumakyou_stage_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(scratch);
    std::filesystem::current_path(scratch); // 配置和成绩只写入临时目录，不能改用户的实际记录。
    std::cout.setf(std::ios::unitbuf); // 长时回归逐条交付结果，重定向日志时也能定位最后完成的难度/路线。
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(1100, 760, "Actor lifecycle tests");
    SetExitKey(KEY_NULL);
    const bool extraAssets=argc>=3&&std::string(argv[2])=="--extra-assets-only";
    const bool dialogueMusic=argc>=3&&std::string(argv[2])=="--dialogue-music-only";
    const bool extraDialogue=argc>=3&&std::string(argv[2])=="--extra-dialogue-only";
    const bool extraTimeline=argc>=3&&std::string(argv[2])=="--extra-timeline-only";
    const bool extraPostfight=argc>=3&&std::string(argv[2])=="--extra-postfight-only";
    const bool combatAudio=argc>=3&&std::string(argv[2])=="--combat-audio-only";
    const bool audioTest=combatAudio||extraAssets||dialogueMusic||extraDialogue||extraTimeline||extraPostfight||(argc>=3 && std::string(argv[2])=="--bomb-audio-only");
    if(audioTest) {InitAudioDevice();SetMasterVolume(0);}
    {
        auto game = std::make_unique<koumakyou::Game>();
        SetTargetFPS(0);
        if(argc>=3&&std::string(argv[2])=="--replay-only") {
            koumakyou::GameLifecycleProbe::TestReplayAdaptation(*game);
        } else if(extraAssets) {
            koumakyou::GameLifecycleProbe::TestExtraAssets(*game,prefix);
        } else if(argc>=3&&std::string(argv[2])=="--extra-scenery-only") {
            koumakyou::GameLifecycleProbe::TestExtraScenery(*game,prefix);
        } else if(dialogueMusic) {
            koumakyou::GameLifecycleProbe::TestDialogueMusic(*game);
        } else if(extraDialogue) {
            koumakyou::GameLifecycleProbe::TestExtraDialogue(*game,prefix,argc>=4?std::stoi(argv[3]):-1);
        } else if(extraTimeline) {
            koumakyou::GameLifecycleProbe::TestExtraTimeline(*game,prefix);
        } else if(extraPostfight) {
            koumakyou::GameLifecycleProbe::TestExtraPostfightFlow(*game,prefix);
        } else if(argc>=3&&std::string(argv[2])=="--challenge-progress-only") {
            koumakyou::GameLifecycleProbe::TestChallengeProgress(*game);
        } else if(combatAudio) {
            koumakyou::GameLifecycleProbe::TestCombatAudio(*game);
        } else if(audioTest) {
            koumakyou::GameLifecycleProbe::TestPlayerBombAudio(*game);
        } else if(argc>=3 && std::string(argv[2])=="--world-clock-only") {
            koumakyou::GameLifecycleProbe::TestDialogueClearPolicy(*game);
            koumakyou::GameLifecycleProbe::TestWorldTimeClock(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage12-effects") {
            koumakyou::GameLifecycleProbe::TestStage12ExternalEffects(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage3-effects") {
            koumakyou::GameLifecycleProbe::TestStage3ExternalEffects(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-effects") {
            koumakyou::GameLifecycleProbe::TestStage4ExternalEffects(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage5-effects") {
            koumakyou::GameLifecycleProbe::TestStage5ExternalEffects(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-ambient-effects") {
            koumakyou::GameLifecycleProbe::TestStage6AmbientEffects(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--attached-effects") {
            koumakyou::GameLifecycleProbe::TestAttachedEffects(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--external-effects-finishing") {
            koumakyou::GameLifecycleProbe::TestExternalEffects(*game,prefix,1);
        } else if(argc>=3 && std::string(argv[2])=="--external-effects-final-exit") {
            koumakyou::GameLifecycleProbe::TestExtraRipples(*game,prefix,true);
        } else if(argc>=3 && std::string(argv[2])=="--external-effects") {
            koumakyou::GameLifecycleProbe::TestExternalEffects(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--extra-spells") {
            koumakyou::GameLifecycleProbe::TestExtraSpellPractice(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--extra-records-only") {
            koumakyou::GameLifecycleProbe::TestExtraEncounterRecords(*game);
        } else if(argc>=3 && std::string(argv[2])=="--rank-initialization-only") {
            koumakyou::GameLifecycleProbe::TestRankInitialization(*game);
        } else if(argc>=3 && std::string(argv[2])=="--rank-time-only") {
            koumakyou::GameLifecycleProbe::TestRankElapsedTime(*game);
        } else if(argc>=3 && std::string(argv[2])=="--beam-repeat-graze-only") {
            koumakyou::GameLifecycleProbe::TestBeamRepeatGraze(*game);
        } else if(argc>=3 && std::string(argv[2])=="--graze-effects-only") {
            koumakyou::GameLifecycleProbe::TestGrazeEffects(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--graze-rank-only") {
            koumakyou::GameLifecycleProbe::TestGrazeRankAndScore(*game);
        } else if(argc>=3 && std::string(argv[2])=="--rank-events-only") {
            koumakyou::GameLifecycleProbe::TestRankEventSettlement(*game);
        } else if(argc>=3 && std::string(argv[2])=="--campaign-difficulties") {
            for(int d=0;d<4;++d)for(int shot=0;shot<4;++shot)koumakyou::GameLifecycleProbe::TestCampaignTransitions(*game,prefix,d,shot);
        } else if(argc>=3 && std::string(argv[2])=="--campaign-only") {
            koumakyou::GameLifecycleProbe::TestCampaignTransitions(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-difficulty-spells") {
            koumakyou::GameLifecycleProbe::TestStage6DifficultyNonspells(*game,prefix,5);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-first-spell") {
            koumakyou::GameLifecycleProbe::TestStage6DifficultyNonspells(*game,prefix,true);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-difficulty-nonspells") {
            koumakyou::GameLifecycleProbe::TestStage6DifficultyNonspells(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-bats-only") {
            koumakyou::GameLifecycleProbe::TestStage6DifficultyNonspells(*game,prefix,0,true);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-difficulty-midboss") {
            koumakyou::GameLifecycleProbe::TestStage6DifficultyMidboss(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-pending-difficulties") {
            for(int d=1;d<4;++d)for(bool damage:{false,true}) {
                const auto label=prefix+"-d"+std::to_string(d)+(damage?"-damage":"-timeout");
                koumakyou::GameLifecycleProbe::TestStage6(*game,label,d,damage);
            }
        } else if(argc>=3 && std::string(argv[2])=="--stage6-portraits-only") {
            koumakyou::GameLifecycleProbe::TestStage6Portraits(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-only") {
            koumakyou::GameLifecycleProbe::TestStage6(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--extra-four-kind") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::FourOfAKind);
        } else if(argc>=3 && std::string(argv[2])=="--extra-kagome") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::KagomeKagome);
        } else if(argc>=3 && std::string(argv[2])=="--extra-kagome-bomb-only") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::KagomeKagome,true);
        } else if(argc>=3 && std::string(argv[2])=="--extra-starbow") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::StarbowBreak);
        } else if(argc>=3 && std::string(argv[2])=="--extra-starbow-bomb-only") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::StarbowBreak,true);
        } else if(argc>=3 && std::string(argv[2])=="--extra-catadioptric") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::Catadioptric);
        } else if(argc>=3 && std::string(argv[2])=="--extra-catadioptric-bomb-only") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::Catadioptric,true);
        } else if(argc>=3 && std::string(argv[2])=="--extra-formal-battles") {
            koumakyou::GameLifecycleProbe::TestExtraFullBattles(*game,prefix,0,0,1,true);
        } else if(argc>=3 && std::string(argv[2])=="--extra-formal-midboss") {
            koumakyou::GameLifecycleProbe::TestExtraPatchouliMidboss(*game,prefix,true);
        } else if(argc>=3 && std::string(argv[2])=="--extra-full-smoke") {
            koumakyou::GameLifecycleProbe::TestExtraFullBattles(*game,prefix,0,0,1);
        } else if(argc>=3 && std::string(argv[2])=="--extra-full-damage") {
            koumakyou::GameLifecycleProbe::TestExtraFullBattles(*game,prefix,0);
        } else if(argc>=3 && std::string(argv[2])=="--extra-full-timeout") {
            koumakyou::GameLifecycleProbe::TestExtraFullBattles(*game,prefix,1);
        } else if(argc>=3 && std::string(argv[2])=="--extra-full-zero") {
            koumakyou::GameLifecycleProbe::TestExtraFullBattles(*game,prefix,2);
        } else if(argc>=3 && std::string(argv[2])=="--extra-final-exits") {
            koumakyou::GameLifecycleProbe::TestExtraFinalExitCleanup(*game);
        } else if(argc>=3 && std::string(argv[2])=="--extra-heart") {
            koumakyou::GameLifecycleProbe::TestExtraHeart(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--extra-bloodstain-review") {
            koumakyou::GameLifecycleProbe::TestExtraBloodstain(*game,prefix,true);
        } else if(argc>=3 && std::string(argv[2])=="--extra-bloodstain") {
            koumakyou::GameLifecycleProbe::TestExtraBloodstain(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--extra-fragile-wing") {
            koumakyou::GameLifecycleProbe::TestExtraFragileWing(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--extra-encore-entrance") {
            koumakyou::GameLifecycleProbe::TestExtraEncoreEntrance(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--extra-ripples") {
            koumakyou::GameLifecycleProbe::TestExtraRipples(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--extra-survival") {
            koumakyou::GameLifecycleProbe::TestExtraSurvival(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--extra-clock") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::ClockOfThePast);
        } else if(argc>=3 && std::string(argv[2])=="--extra-maze") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::MazeOfLove);
        } else if(argc>=3 && std::string(argv[2])=="--extra-maze-bomb-only") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::MazeOfLove,true);
        } else if(argc>=3 && std::string(argv[2])=="--extra-four-kind-bomb-only") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::FourOfAKind,true);
        } else if(argc>=3 && std::string(argv[2])=="--extra-laevatein") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::Laevatein);
        } else if(argc>=3 && std::string(argv[2])=="--extra-laevatein-bomb-only") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::Laevatein,true);
        } else if(argc>=3 && std::string(argv[2])=="--extra-cranberry-bomb-only") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::CranberryTrap,true);
        } else if(argc>=3 && std::string(argv[2])=="--extra-cranberry") {
            koumakyou::GameLifecycleProbe::TestExtraFlandreSpell(*game,prefix,koumakyou::bosses::FlandreBoss::Behavior::CranberryTrap);
        } else if(argc>=3 && std::string(argv[2])=="--extra-flandre") {
            koumakyou::GameLifecycleProbe::TestExtraFlandre(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--extra-patchouli-midboss") {
            koumakyou::GameLifecycleProbe::TestExtraPatchouliMidboss(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--extra-patchouli") {
            koumakyou::GameLifecycleProbe::TestExtraPatchouli(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-lunatic-damage") {
            koumakyou::GameLifecycleProbe::TestStageDifficulties(*game,koumakyou::StageId::Stage6,prefix,true);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-difficulties") {
            koumakyou::GameLifecycleProbe::TestStageDifficulties(*game,koumakyou::StageId::Stage6,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage6-spells") {
            koumakyou::GameLifecycleProbe::TestDifficultySpellPractice(*game,prefix,5,6);
        } else if(argc>=3 && std::string(argv[2])=="--stage5-difficulties") {
            koumakyou::GameLifecycleProbe::TestStageDifficulties(*game,koumakyou::StageId::Stage5,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage5-spells") {
            koumakyou::GameLifecycleProbe::TestDifficultySpellPractice(*game,prefix,4,5);
        } else if(argc>=3 && std::string(argv[2])=="--stage5-pending-difficulties") {
            koumakyou::GameLifecycleProbe::TestStage5PendingDifficulties(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage5-difficulty-spells") {
            koumakyou::GameLifecycleProbe::TestStage5DifficultyCombat(*game,prefix,true);
        } else if(argc>=3 && std::string(argv[2])=="--stage5-difficulty-nonspells") {
            koumakyou::GameLifecycleProbe::TestStage5DifficultyCombat(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage5-difficulty-midboss") {
            koumakyou::GameLifecycleProbe::TestStage5DifficultyMidboss(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage5-only") {
            koumakyou::GameLifecycleProbe::TestStage5(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-only") {
            koumakyou::GameLifecycleProbe::TestStage4(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-combinations-combat") {
            koumakyou::GameLifecycleProbe::TestStage4DifficultyCombat(*game,prefix,16,21);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-pending-lunatic") {
            koumakyou::GameLifecycleProbe::TestStage4PendingDifficulties(*game,prefix,3,4);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-pending-difficulties") {
            koumakyou::GameLifecycleProbe::TestStage4PendingDifficulties(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-water-metal-combat") {
            koumakyou::GameLifecycleProbe::TestStage4DifficultyCombat(*game,prefix,12,16);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-earth-combat") {
            koumakyou::GameLifecycleProbe::TestStage4DifficultyCombat(*game,prefix,9,12);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-wood-combat") {
            koumakyou::GameLifecycleProbe::TestStage4DifficultyCombat(*game,prefix,6,9);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-difficulty-combat") {
            koumakyou::GameLifecycleProbe::TestStage4DifficultyCombat(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage3-only") {
            koumakyou::GameLifecycleProbe::TestStage3(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--difficulty-spells") {
            koumakyou::GameLifecycleProbe::TestDifficultySpellPractice(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-spells") {
            koumakyou::GameLifecycleProbe::TestDifficultySpellPractice(*game,prefix,3,4);
        } else if(argc>=3 && std::string(argv[2])=="--stage4-difficulties") {
            koumakyou::GameLifecycleProbe::TestStageDifficulties(*game,koumakyou::StageId::Stage4,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage3-difficulties") {
            koumakyou::GameLifecycleProbe::TestStageDifficulties(*game,koumakyou::StageId::Stage3,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage1-difficulties") {
            koumakyou::GameLifecycleProbe::TestStageDifficulties(*game,koumakyou::StageId::Stage1,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage2-difficulties") {
            koumakyou::GameLifecycleProbe::TestStageDifficulties(*game,koumakyou::StageId::Stage2,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage2-only") {
            koumakyou::GameLifecycleProbe::TestStage2(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--stage-only") {
            koumakyou::GameLifecycleProbe::TestWholeStage(*game,prefix);
            koumakyou::GameLifecycleProbe::TestActorLifecycle(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--lifecycle-only") {
            koumakyou::GameLifecycleProbe::TestBossEntryPreservesField(*game);
            koumakyou::GameLifecycleProbe::TestActorLifecycle(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--spatial-only") {
            koumakyou::GameLifecycleProbe::TestSpatialFrameRates(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--actor-contacts-only") {
            koumakyou::GameLifecycleProbe::TestActorContacts(*game);
        } else if(argc>=3 && std::string(argv[2])=="--viewport-only") {
            koumakyou::GameLifecycleProbe::TestUiFont(*game);
            koumakyou::GameLifecycleProbe::TestFullWindow(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--bomb-only") {
            koumakyou::GameLifecycleProbe::TestPlayerBombs(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--marisa-only") {
            koumakyou::GameLifecycleProbe::TestMarisa(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--options-capture") {
            koumakyou::GameLifecycleProbe::CaptureOptionAnimations(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--drops-capture") {
            koumakyou::GameLifecycleProbe::CaptureDrops(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--player-options-only") {
            koumakyou::GameLifecycleProbe::TestPlayerOptions(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--player-shots-only") {
            koumakyou::GameLifecycleProbe::TestPlayerShots(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--boss-hud-only") {
            koumakyou::GameLifecycleProbe::TestUiFont(*game);
            koumakyou::GameLifecycleProbe::TestBossTransitions(*game,prefix);
            koumakyou::GameLifecycleProbe::TestBossHud(*game,prefix);
        } else if(argc>=3 && std::string(argv[2])=="--dialogue-only") {
            koumakyou::GameLifecycleProbe::TestUiFont(*game);
            koumakyou::GameLifecycleProbe::TestDialogueTransitions(*game,prefix);
            koumakyou::GameLifecycleProbe::TestPortraits(*game,prefix);
            koumakyou::GameLifecycleProbe::TestNativeDialogue(*game,prefix);
        } else {
            koumakyou::GameLifecycleProbe::Run(*game, prefix);
        }
    }
    if(audioTest)CloseAudioDevice();
    CloseWindow();
    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove_all(scratch);
    std::cout << "Actor lifecycle integration tests passed\n";
}
