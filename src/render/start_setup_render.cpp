// 本文件参照新典绘制完整 Start 流程：难度札、人物双层立绘、御札/模式、关卡记录、能力评级与最终确认横幅。
#include "app/game.hpp"
#include "render/start_setup_layout.hpp"
#include "ui/page.hpp"
#include "ui/typography.hpp"
#include "ui/effects.hpp"
#include <array>
#include <cmath>

namespace koumakyou {
namespace {
constexpr Color kIvory{252,246,236,255},kGold{233,204,139,255},kBlue{162,194,247,255},kRose{207,119,132,255};

/// @brief 在屏幕矩形中按宽度缩小文字；size 为屏幕像素，top 为矩形内相对高度，center 控制对齐。
void Label(Font font,const char* text,Rectangle bounds,float size,Color color,float top=0,bool center=true,float glow=0) {
    size=ui::FittedSize(font,text,size,bounds.width);
    Vector2 point{bounds.x,bounds.y+top*bounds.height};
    if(center)point.x+=(bounds.width-ui::MeasureText(font,text,size).x)/2;
    ui::GlowText(font,text,point,size,color,color,glow);
}

/// @brief 花饰细线采用屏幕坐标；不变更命中矩形，缩放只影响线宽与花瓣尺寸。
void Ornament(Rectangle bounds,float scale,Color color) {
    const float y=bounds.y+bounds.height,r=3*scale;
    DrawLineEx({bounds.x+10*scale,y},{bounds.x+bounds.width-10*scale,y},scale,color);
    for(float x:{bounds.x+5*scale,bounds.x+bounds.width-5*scale})
        for(int petal=0;petal<5;++petal) {
            const float angle=petal*2*PI/5;
            DrawCircleV({x+std::cos(angle)*r,y+std::sin(angle)*r},r*0.6F,color);
        }
}

/// @brief 借用人物纹理；前景完整等比展示，后层截取各自不同姿势的全身图上部并放大覆盖。
void Portrait(Texture2D texture,Rectangle slot,bool backdrop,float reveal) {
    if(!IsTextureValid(texture))return;
    Rectangle source{0,0,float(texture.width),float(texture.height)*(backdrop?0.7F:1)};
    const float scale=backdrop?std::max(slot.width/source.width,slot.height/source.height):std::min(slot.width/source.width,slot.height/source.height);
    if(backdrop) {
        const float width=slot.width/scale,height=slot.height/scale;
        source.x=(source.width-width)/2;source.width=width;source.height=height;
    } else {
        const float width=source.width*scale,height=source.height*scale;
        slot={slot.x+(slot.width-width)/2,slot.y+slot.height-height,width,height};
    }
    slot.x+=(backdrop?-18:18)*(1-reveal);
    // 两位角色均使用自然原色的现代图，以同一色调和 27% 透明度融入后层；灵梦原效果不变。
    DrawTexturePro(texture,source,slot,{},0,Fade(backdrop?Color{173,130,135,255}:WHITE,(backdrop?0.27F:1)*reveal));
}

/// @brief 难度札 index 为 Easy/Normal/Hard/Lunatic 的 0～3；未开放条目仅展示，不能暗示已可游玩。
void DifficultyCard(Font font,Rectangle card,float scale,int index,bool selected,float opacity=1) {
    constexpr std::array<const char*,4> names{"EASY","NORMAL","HARD","LUNATIC"};
    constexpr std::array<Color,4> colors{{{81,131,96,255},{66,101,145,255},{141,71,117,255},{150,56,64,255}}};
    DrawRectangleRec(card,ui::WithOpacity(selected?kIvory:Color{101,85,83,155},opacity));
    DrawRectangleLinesEx(card,2*scale,ui::WithOpacity(selected?kGold:Color{127,98,84,180},opacity));
    const Color ink=ui::WithOpacity(selected?colors[index]:Color{169,151,144,255},opacity);
    Label(font,names[index],card,42*scale,ink,0.08F);
    Rectangle rule{card.x+card.width*0.12F,card.y+card.height*0.48F,card.width*0.76F,0};
    Ornament(rule,scale,ink);
    Label(font,selected?"面向大多数人的难度":"尚未开放",card,22*scale,selected?ui::WithOpacity(Color{63,43,43,255},opacity):ink,0.60F);
    if(selected)Label(font,"第一面",card,18*scale,ui::WithOpacity(Color{90,65,63,255},opacity),0.81F);
}

/// @brief 两行御札摘要与选择项共用样式；combination 为灵/梦/魔/恋的 0～3，weight/reveal 为 0～1。
void Shot(Font font,Rectangle row,float scale,int combination,float weight,float reveal,float opacity=1) {
    const auto& profile=spell::Characters()[combination];
    const Color color=ui::WithOpacity(ui::Blend(Color{156,127,121,255},combination%2==0?kBlue:kRose,weight),reveal*opacity);
    Label(font,profile.shot,row,29*scale,color,0.05F,false,weight*0.65F);
    Ornament({row.x,row.y+row.height*0.48F,row.width,0},scale,color);
    Label(font,profile.bomb,row,27*scale,color,0.55F);
}
/// @brief 同一层的文字、细线和底色共用透明度，保留颜色原有的 Alpha；不持有字体资源。
struct Painter {
    Font font;
    float scale,opacity;
    void Text(const char* text,Rectangle bounds,float size,Color color,float top=0,bool center=true,float glow=0) const {
        Label(font,text,bounds,size,ui::WithOpacity(color,opacity),top,center,glow);
    }
    void Rule(Rectangle bounds,Color color) const { Ornament(bounds,scale,ui::WithOpacity(color,opacity)); }
    void Card(Rectangle bounds,int index,bool selected) const { DifficultyCard(font,bounds,scale,index,selected,opacity); }
};

/// @brief 两个屏幕矩形连续插值；t 为 0～1，窗口缩放时由当前布局重算，不能缓存旧窗口像素。
Rectangle Mix(Rectangle a,Rectangle b,float t) {
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.width+(b.width-a.width)*t,a.height+(b.height-a.height)*t};
}
} // namespace

void Game::DrawStartSetup() const {
    const menu::StartSetupLayout layout(GetScreenWidth(),GetScreenHeight());
    const auto& canvas=layout.canvas;
    const auto& common=assets_.Common();
    const Font font=common.uiFont;
    const auto& motion=startSetupMotion_;
    const float s=canvas.scale,setup=motion.Setup(),character=motion.Character();
    const auto drawShot=[&](Rectangle row,int index,float weight,float opacity) {
        for(int role=0;role<2;++role) {
            const float alpha=opacity*startCharacterMotion_.Weight(role);
            if(alpha>0.001F)Shot(font,row,s,role*2+index,weight,1,alpha);
        }
    };
    // 首页仍在下层绘制，背景色和装饰随选择页整体淡入；无需截图纹理，原生窗口和离屏截图行为相同。
    if(motion.Title()>0)DrawMainMenu(state_==RunState::MainMenu?nullptr:&titleMenuMotion_);
    if(setup<=0)return;
    ui::DrawPageSurface(canvas,menuMotion_,setup);
    if(spellPractice_) {DrawSpellPractice(setup);return;}
    const Painter page{font,s,setup};
    page.Text(practiceMode_?(spellPractice_?"SPELL PRACTICE":"PRACTICE START"):"START",
              canvas.Rect(42,canvas.height-108,layout.compact?290:400,70),52*s,Color{122,48,51,150},0,false);
    const float difficulty=motion.Layer(StartSetupStep::Difficulty);
    if(difficulty>0) {
        const Painter p{font,s,difficulty};
        auto title=canvas.Rect(layout.compact?80:360,45,layout.compact?600:560,58);
        title.y-=12*s*(1-difficulty);
        p.Text("请选择难度。",title,46*s,kIvory,0,true,0.5F);p.Rule(title,kRose);
        for(int index:{0,2,3}) {
            const float entry=motion.CardEntry(index);
            auto card=layout.Difficulty(index);
            card.x+=48*s*(1-entry)-18*s*(1-difficulty);
            Painter{font,s,difficulty*entry}.Card(card,index,false);
        }
    }
    for(int role=0;role<2 && character>0;++role) {
        const float alpha=character*startCharacterMotion_.Weight(role);
        if(alpha<=0.001F)continue;
        const auto& profile=spell::Characters()[role*2];
        BeginScissorMode(22*s,22*s,GetScreenWidth()-44*s,GetScreenHeight()-44*s);
        Portrait(role==0?common.reimuPortrait:common.marisaBackdrop,layout.PortraitBackdrop(),true,alpha);
        Portrait(common.spellPortraits[role],layout.Portrait(),false,alpha);
        EndScissorMode();
        const Painter p{font,s,alpha};
        const auto info=layout.CharacterInfo();
        p.Text(profile.title,info,26*s,kIvory,0,true,0.2F);
        p.Text(profile.displayName,info,54*s,kIvory,0.17F,true,0.3F);
        p.Rule({info.x,info.y+info.height*0.46F,info.width,0},kRose);
        constexpr std::array<const char*,3> stats{"移动速度","攻击范围","攻击力"};
        constexpr std::array<const char*,5> stars{"★","★★","★★★","★★★★","★★★★★"};
        for(int index=0;index<3;++index) {
            const Rectangle row{info.x,info.y+info.height*(0.51F+index*0.16F),info.width,info.height*0.16F};
            p.Text(stats[index],{row.x,row.y,row.width*0.48F,row.height},25*s,kGold,0,false);
            p.Text(stars[profile.stars[index]-1],{row.x+row.width*0.5F,row.y,row.width*0.5F,row.height},27*s,kGold,0,false);
        }
    }
    // Normal 札沿连续路径移到人物页左下，放在人物之上保持移动途中可读，后续步骤不重复淡入。
    const float entry=motion.CardEntry(1),normal=character+difficulty*entry;
    if(normal>0) {
        auto card=layout.Difficulty(1);card.x+=48*s*(1-entry)-18*s*(1-difficulty);
        const float progress=character/std::max(0.00001F,character+difficulty);
        Painter{font,s,normal}.Card(Mix(card,layout.Badge(),progress),1,true);
    }
    // 可见页面各自保留当前权重；快速确认或返回只改变目标，旧标题与选项从当前画面继续退场。
    for(const auto step:{StartSetupStep::Character,StartSetupStep::Shot,StartSetupStep::Mode}) {
        const float alpha=motion.Layer(step);
        if(alpha<=0)continue;
        const Painter p{font,s,alpha};
        if(step==StartSetupStep::Character) {
            auto title=canvas.Rect(55,layout.compact?38:70,layout.compact?650:400,65);
            title.x+=18*s*(1-alpha);
            p.Text("请选择主人公。",title,42*s,kGold,0,false,0.4F);p.Rule(title,kGold);
            for(int direction:{-1,1}) {
                const auto button=layout.CharacterArrow(direction);
                ui::ArrowButton{button,direction}.Draw({font,s,alpha,menuMotion_.Phase()});
            }
            continue;
        }
        auto heading=layout.Heading();heading.x+=18*s*(1-alpha);
        const bool mode=step==StartSetupStep::Mode;
        const Color accent=mode?kRose:kBlue;
        p.Text(mode?"要以哪一种模式":"请选择使用的御札。",
               heading,(layout.compact?38:44)*s,accent,0,true,0.5F);
        if(mode)p.Text("开始游戏？",heading,40*s,accent,layout.compact?0.48F:0.45F,true,0.5F);
        p.Rule(heading,accent);
        for(int index=0;index<2;++index) {
            const float weight=setupChoiceMotion_[static_cast<int>(step)].Weight(index);
            auto row=layout.Choice(index);row.x+=18*s*(1-alpha)-4*s*weight;
            if(step==StartSetupStep::Shot)drawShot(row,index,weight,alpha);
            else {
                const Color color=ui::Blend(Color{161,138,132,255},mode?kIvory:kBlue,weight);
                p.Text(index==0?"标准模式":"挑战模式",
                       row,34*s,color,0,false,weight*0.5F);
                p.Rule({row.x,row.y+row.height*0.42F,row.width,0},color);
                p.Text(index==0?"残机数量有限的基本模式":"无限残机，挑战最少 MISS",
                       row,23*s,color,0.53F,false);
            }
        }
    }
    const float ready=motion.Layer(StartSetupStep::Ready);
    const float stages=practiceMode_ && !spellPractice_ ? motion.Layer(StartSetupStep::Stage)+ready : 0;
    if(stages>0) {
        const Painter p{font,s,stages};
        auto heading=layout.StageHeading();heading.x+=18*s*(1-stages);
        const char* recordMode=challengeMode_?"挑战模式 · 最高得分":"标准模式 · 最高得分";
        if(layout.compact) {
            p.Text("请选择关卡。",heading,32*s,kGold,0,false,0.4F);
            p.Text(recordMode,heading,20*s,kIvory,0.63F,false);
        } else {
            // 标题与模式同排，六行记录留在确认横幅上方，Ready 时仍能读到全部历史分数。
            p.Text("请选择关卡。",{heading.x,heading.y,heading.width*0.51F,heading.height},28*s,kGold,0,false,0.4F);
            p.Text(recordMode,{heading.x+heading.width*0.51F,heading.y,heading.width*0.49F,heading.height},18*s,kIvory,0.2F,false);
        }
        const auto& selection=setupChoiceMotion_[static_cast<int>(StartSetupStep::Stage)];
        for(int index=0;index<kStageScoreCount;++index) {
            const bool available=kPracticeStages[index].has_value();
            const float weight=available?selection.Weight(index):0;
            auto row=layout.StageRow(index);row.x+=18*s*(1-stages)-4*s*weight;
            const Color color=available?ui::Blend(Color{161,138,132,255},kGold,weight):Color{127,106,106,180};
            DrawRectangleRec(row,ui::WithOpacity(Color{46,22,29,static_cast<unsigned char>(45+70*weight)},stages));
            const float split=row.width*0.36F;
            p.Text(TextFormat("STAGE %d",index+1),{row.x+8*s,row.y,split-12*s,row.height},(layout.compact?25:21)*s,color,0.1F,false,weight*0.35F);
            const char* score=TextFormat("%09lld",stageHighScores_.Best(challengeMode_,index+1));
            const float scoreSize=ui::FittedSize(common.hudFont,score,(layout.compact?24:22)*s,row.width-split-8*s);
            const float scoreWidth=ui::MeasureText(common.hudFont,score,scoreSize).x;
            Label(common.hudFont,score,{row.x+row.width-8*s-scoreWidth,row.y,scoreWidth,row.height},
                  scoreSize,ui::WithOpacity(color,stages),0.1F,false,weight*0.35F);
            ui::DrawPageRowRule(canvas,row,selection,weight,stages);
        }
        const auto last=layout.StageRow(kStageScoreCount-1);
        p.Text("未开放关卡不可选择",{last.x,last.y+last.height+4*s,last.width,20*s},16*s,Color{161,138,132,255},0,false);
    }
    const float shot=motion.Layer(StartSetupStep::Mode)+motion.Layer(StartSetupStep::Stage)+ready;
    if(shot>0) {
        auto row=Mix(layout.ShotSummary(),layout.StageShotSummary(),stages);row.y+=10*s*(1-shot);
        drawShot(row,selectedShot_,1,shot);
    }
    const float modeSummary=motion.Layer(StartSetupStep::Stage)+ready;
    if(modeSummary>0) {
        const Painter p{font,s,modeSummary};
        auto summary=Mix(layout.ModeSummary(),layout.StageModeSummary(),stages);summary.y+=10*s*(1-modeSummary);
        p.Text(challengeMode_?"挑战模式":"标准模式",summary,30*s,kIvory,0,false);
        p.Rule({summary.x,summary.y+summary.height*0.40F,summary.width,0},kGold);
        p.Text(challengeMode_?"无限残机，挑战最少 MISS":"残机数量有限的基本模式",summary,22*s,kIvory,0.52F,false);
    }
    if(ready>0) {
        const Painter p{font,s,ready};
        auto band=layout.ReadyBand();band.y+=18*s*(1-ready);
        DrawRectangleRec(band,ui::WithOpacity(Color{15,2,8,205},ready));
        DrawRectangleGradientV(band.x,band.y,band.width,band.height,
                              ui::WithOpacity(Color{71,10,19,60},ready),ui::WithOpacity(Color{135,16,32,180},ready));
        p.Text("准备好了吗？",{band.x+band.width*0.15F,band.y+28*s,band.width*0.7F,58*s},56*s,kIvory,0,true,0.6F);
        auto button=layout.ReadyButton();button.y+=18*s*(1-ready);
        const float hover=CheckCollisionPointRec(GetMousePosition(),layout.ReadyButton())?1:0.55F;
        p.Text("START GAME",button,46*s,kIvory,0.12F,true,hover*menuMotion_.Pulse());
        p.Rule(button,ui::WithOpacity(kGold,0.65F));
    }
}
} // namespace koumakyou
