// 本文件绘制原生结局背景/中文文本及响应式的前十成绩、选存和八字符命名页面。
#include "app/game.hpp"
#include "render/postgame_layout.hpp"
#include "ui/page.hpp"
#include "ui/typography.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace koumakyou {
namespace {
constexpr std::array<const char*,4> kShotNames{"灵梦 · 灵之御札","灵梦 · 梦之御札","魔理沙 · 魔之御札","魔理沙 · 恋之御札"};
constexpr Color kGold{233,204,139,255},kIvory{252,246,236,255},kMuted{164,137,131,255};

/// @brief 按 UTF-8 码点换行，保留原文本换行和缩进；fontSize、maximum 为屏幕逻辑像素。
std::vector<std::string> Wrap(std::string_view text,Font font,float fontSize,float maximum) {
    std::vector<std::string> lines;std::string current;
    for(std::size_t offset=0;offset<text.size();) {
        int bytes=0;const int codepoint=GetCodepointNext(text.data()+offset,&bytes);
        if(bytes<=0)break;
        const auto next=std::string(text.substr(offset,bytes));offset+=bytes;
        if(codepoint=='\n'){lines.push_back(current);current.clear();continue;}
        if(!current.empty() && ui::MeasureText(font,(current+next).c_str(),fontSize).x>maximum) {
            lines.push_back(current);current.clear();
        }
        current+=next;
    }
    lines.push_back(current);return lines;
}
void Label(const menu::Canvas& canvas,Font font,const char* text,float x,float y,float width,float size,Color color,
           ui::TextAlign align=ui::TextAlign::Left) {
    ui::Label{canvas.Rect(x,y,width,size+12),size,align}.Draw({font,canvas.scale},text,color);
}
/// @brief 表内记录已按分数排序；highlight 为待插入或最近保存的 0～9 排名，-1 不强调。
void Ranking(const menu::Canvas& canvas,Font font,const score::Table& table,float x,float y,float width,float step,int highlight) {
    Label(canvas,font,"排名",x,y-34,width*0.13F,18,kGold);
    Label(canvas,font,"NAME",x+width*0.15F,y-34,width*0.34F,18,kGold);
    Label(canvas,font,"SCORE",x+width*0.49F,y-34,width*0.48F,18,kGold,ui::TextAlign::Right);
    for(int i=0;i<score::kSlots;++i) {
        const float top=y+i*step;
        const bool chosen=i==highlight;
        if(chosen)DrawRectangleRec(canvas.Rect(x-8,top-2,width+16,step-2),Color{116,57,42,140});
        const Color color=chosen?kGold:table[i]?kIvory:kMuted;
        Label(canvas,font,TextFormat("%02d",i+1),x,top,width*0.13F,21,color);
        Label(canvas,font,table[i]?table[i]->name.c_str():"--------",x+width*0.15F,top,width*0.34F,21,color);
        const std::string value=table[i]?std::to_string(table[i]->value):"---------";
        Label(canvas,font,value.c_str(),x+width*0.49F,top,width*0.48F,21,color,ui::TextAlign::Right);
    }
}
} // namespace

void Game::DrawEnding() const {
    const float width=GetScreenWidth(),height=ui::CommandBar::ContentHeight(width,GetScreenHeight());
    const bool compact=width<height*1.15F;
    const float padding=std::max(12.0F,width*0.025F);
    ClearBackground({250,248,243,255});
    const auto texture=assets_.GetVisual(endingPlayer_.Background()).texture;
    Rectangle image{padding,compact?60.0F:0.0F,width-2*padding,0};
    if(IsTextureValid(texture)) {
        const float scale=std::min(image.width/texture.width,(compact?height*0.40F:height)/texture.height);
        image.width=texture.width*scale;image.height=texture.height*scale;
        image.x=(width-image.width)/2;
        if(!compact)image.y=(height-image.height)/2;
        DrawTexturePro(texture,{0,0,static_cast<float>(texture.width),static_cast<float>(texture.height)},image,{},0,WHITE);
    }
    const Font font=assets_.Common().uiFont;
    const float headerSize=std::clamp(width/60,14.0F,22.0F);
    ui::DrawText(font,ending::Title(endingPlayer_.Variant()).data(),{padding,18},headerSize,{107,83,75,255});
    if(!endingPlayer_.Lines().empty()) {
        const float fontSize=std::clamp(width/42,14.0F,27.0F),lineHeight=fontSize*1.45F;
        struct Row {std::string text;Color color;};std::vector<Row> rows;
        const float panelWidth=width-2*padding;
        for(const auto& block:endingPlayer_.Lines())
            for(auto& line:Wrap(block.text,font,fontSize,panelWidth-2*padding))rows.push_back({std::move(line),block.color});
        const float panelHeight=rows.size()*lineHeight+2*padding;
        const float panelY=compact?image.y+image.height+padding:std::max(image.y,height-panelHeight-padding);
        DrawRectangleRec({padding,panelY,panelWidth,panelHeight},{255,253,247,235});
        DrawLineEx({padding,panelY},{width-padding,panelY},1,{210,191,165,180});
        float y=panelY+padding;
        for(const auto& row:rows){ui::DrawText(font,row.text.c_str(),{padding*2,y},fontSize,row.color);y+=lineHeight;}
    }
    // 淡白覆盖背景和文字，不覆盖全局按键栏；不能把演出透明度写回纹理或内容表。
    DrawRectangleRec({0,0,width,height},Fade(WHITE,endingPlayer_.WhiteOpacity()));
}

void Game::DrawScoreEntry() const {
    const menu::PostgameLayout layout(GetScreenWidth(),GetScreenHeight());const auto& c=layout.canvas;
    const bool compact=layout.compact;const Font font=assets_.Common().uiFont;
    ui::DrawPageSurface(c,menuMotion_);
    Label(c,font,state_==RunState::ScoreName?"为成绩命名":"保存最高分",40,48,c.width-80,36,kIvory,ui::TextAlign::Center);
    const auto description=std::string(scoreMode_?"挑战模式":"标准模式")+"  ·  "+kShotNames[scoreShot_]+"  ·  NORMAL";
    Label(c,font,description.c_str(),40,112,c.width-80,19,kGold,ui::TextAlign::Center);
    const auto rank=scoreProfile_.Position(scoreMode_!=0,scoreShot_,1,pendingScore_.value);
    Ranking(c,font,scoreProfile_.Get(scoreMode_!=0,scoreShot_),48,compact?212:202,compact?664:560,compact?29:35,rank?*rank:-1);
    const float x=compact?48:690,width=compact?664:340;
    if(state_==RunState::ScoreSave) {
        Label(c,font,"本轮总分",x,compact?548:202,width,20,kGold);
        Label(c,font,std::to_string(pendingScore_.value).c_str(),x,compact?586:245,width,32,kIvory);
        Label(c,font,rank?TextFormat("可登记至第 %d 位",*rank+1):"本次分数未进入前十",x,compact?645:300,width,20,kGold);
        const ui::DrawContext context{font,c.scale};
        ui::Button{layout.Choice(0),rank.has_value()}.Draw(context,"保存并命名",{selectedScoreSave_==0?1.0F:0.0F,1});
        ui::Button{layout.Choice(1)}.Draw(context,rank?"不保存，返回主菜单":"返回主菜单",{selectedScoreSave_==1?1.0F:0.0F,1});
    } else {
        Label(c,font,"NAME · 八字符命名",x,compact?565:265,width,20,kGold);
        const float top=compact?608:310,cell=compact?56.0F:39.0F;
        for(int i=0;i<score::kNameLength;++i) {
            const auto bounds=c.Rect(x+i*cell,top,cell-5,42);
            DrawRectangleRec(bounds,replaceScoreName_?Color{100,55,49,180}:Color{34,20,29,230});
            DrawRectangleLinesEx(bounds,c.scale,Color{149,113,84,180});
            const char value[2]={i<static_cast<int>(scoreName_.size())?scoreName_[i]:'_',0};
            ui::Label{bounds,25,ui::TextAlign::Center}.Draw({font,c.scale},value,kIvory);
        }
        Label(c,font,"可直接键盘输入",x,compact?675:380,width,19,kIvory);
        Label(c,font,"Enter 保存 · Backspace 删除",x,compact?719:424,width,18,kMuted);
        Label(c,font,"Ctrl+A 重新输入",x,compact?761:466,width,18,kMuted);
    }
    if(!scoreSaveError_.empty())Label(c,font,scoreSaveError_.c_str(),compact?48:665,compact?1006:666,compact?664:370,18,{255,147,130,255});
    Label(c,font,"确认保存后才写入排行榜",48,c.height-57,c.width-96,17,kMuted,ui::TextAlign::Center);
}

void Game::DrawScorePage() const {
    const menu::PostgameLayout layout(GetScreenWidth(),GetScreenHeight());const auto& c=layout.canvas;
    const Font font=assets_.Common().uiFont;
    ui::DrawPageSurface(c,menuMotion_);
    Label(c,font,scorePageFromRun_?"成绩已保存":"最高分排行榜",40,48,c.width-80,36,kIvory,ui::TextAlign::Center);
    Label(c,font,scoreMode_?"挑战模式":"标准模式",88,layout.compact?124:116,160,20,kGold,ui::TextAlign::Center);
    Label(c,font,kShotNames[scoreShot_],389,layout.compact?124:116,280,20,kGold,ui::TextAlign::Center);
    const ui::DrawContext context{font,c.scale};
    for(int direction:{-1,1}) {
        ui::ArrowButton{layout.ModeArrow(direction),direction}.Draw(context);
        ui::ArrowButton{layout.ShotArrow(direction),direction}.Draw(context);
    }
    Ranking(c,font,scoreProfile_.Get(scoreMode_!=0,scoreShot_),64,layout.compact?225:215,c.width-128,layout.compact?49:38,scoreHighlight_);
    Label(c,font,"NORMAL · 完整主线通关记录",48,c.height-104,c.width-96,19,kGold,ui::TextAlign::Center);
    Label(c,font,"标准 / 挑战与四种御札分别记录，保留最高十名",48,c.height-60,c.width-96,17,kMuted,ui::TextAlign::Center);
}
} // namespace koumakyou
