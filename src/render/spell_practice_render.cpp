// 本文件用项目现有底板、衬线字体、行强调和过渡绘制新典式符卡练习三列目录与四组合人物摘要。
#include "app/game.hpp"
#include "render/spell_practice_layout.hpp"
#include "ui/page.hpp"
#include "ui/effects.hpp"
#include "ui/typography.hpp"
#include <cmath>
namespace koumakyou {
namespace {
constexpr Color kGold{233,204,139,255},kIvory{252,246,236,255},kRose{207,119,132,255},kGray{133,117,123,255};
/// @brief 在屏幕矩形内按宽度收缩文字；alpha 为独立图层权重，不改变鼠标命中范围。
void Text(Font font,const char* text,Rectangle bounds,float size,Color color,float alpha=1) {
    ui::Label{bounds,size*1.12F}.Draw({font,1,alpha},text,color);
}
/// @brief 保持原人物纹理等比；裁图已去掉图集内嵌的称号文字，不能随窗口拉伸。
void Portrait(Texture2D texture,Rectangle slot,float alpha) {
    if(!IsTextureValid(texture) || alpha<=0)return;
    const float scale=std::min(slot.width/texture.width,slot.height/texture.height);
    const float width=scale*texture.width,height=scale*texture.height;
    DrawTexturePro(texture,{0,0,float(texture.width),float(texture.height)},
        {slot.x+(slot.width-width)/2,slot.y+slot.height-height,width,height},{},0,Fade(WHITE,alpha));
}
}
void Game::DrawSpellPractice(float opacity) const {
    const menu::SpellPracticeLayout l(GetScreenWidth(),GetScreenHeight());
    const auto& c=l.canvas;const float s=c.scale;
    const auto& common=assets_.Common();const Font font=common.hudFont;
    const auto& choice=spellSelection_;
    const float portraitOpacity=opacity;
    if(state_==RunState::StartSetup)opacity*=menuMotion_.Reveal();
    Text(font,"Spell Practice",l.Title(),48*s,kGold,opacity);
    const char* heading=choice.page==spell::Page::Stage?"关卡选择":"符卡选择";
    const auto headingBox=l.Heading();
    Text(font,heading,{headingBox.x,headingBox.y,headingBox.width*0.37F,headingBox.height},30*s,kIvory,opacity);
    Text(font,TextFormat("《 %s 》",spell::Characters()[choice.character].name),
         {headingBox.x+headingBox.width*0.38F,headingBox.y,headingBox.width*0.62F,headingBox.height},23*s,kRose,opacity);
    const auto row=[&](Rectangle bounds,int index,bool available,const ui::MenuMotion& motion,float strength=1) {
        const float weight=motion.Weight(index)*strength;
        return ui::SelectableRow{bounds,index}.Draw({font,s,opacity,motion.Phase()},weight,available);
    };
    const auto columns=l.Columns();
    if(choice.page==spell::Page::Stage) {
        Text(font,"STAGE",{columns.x,columns.y,columns.width*0.3F,columns.height},19*s,kGold,opacity);
        Text(font,"BOSS",{columns.x+columns.width*0.39F,columns.y,columns.width*0.3F,columns.height},19*s,kGold,opacity);
        Text(font,"取得 / 解锁 / 总数",{columns.x+columns.width*0.72F,columns.y,columns.width*0.28F,columns.height},17*s,kGold,opacity);
        for(int stage=0;stage<spell::kStageCount;++stage) {
            const auto& definition=spell::Stages()[stage];int captured=0,available=0,unlocked=0;
            for(const auto& card:spell::Cards())if(card.stage==stage) {
                available+=spell::Available(&card,choice.character,spellRecords_);
                unlocked+=spellRecords_.Seen(choice.character,card.number);
                const auto& record=spellRecords_.Get(choice.character,card.number);
                captured+=record.captured>0 || record.mainCaptured>0;
            }
            const auto bounds=l.StageRow(stage);const Color color=row(bounds,stage,available>0,menuMotion_);
            Text(font,definition.label,{bounds.x+10*s,bounds.y,bounds.width*0.3F,bounds.height*0.52F},24*s,color,opacity);
            Text(font,definition.boss,{bounds.x+bounds.width*0.39F,bounds.y,bounds.width*0.32F,bounds.height*0.52F},21*s,color,opacity);
            Text(font,TextFormat("%d / %d / %d",captured,unlocked,definition.total),
                 {bounds.x+bounds.width*0.73F,bounds.y,bounds.width*0.25F,bounds.height*0.52F},20*s,color,opacity);
            Text(font,definition.title,{bounds.x+22*s,bounds.y+bounds.height*0.52F,bounds.width-32*s,bounds.height*0.48F},17*s,color,opacity);
        }
    } else {
        Text(font,"符卡",{columns.x,columns.y,columns.width*0.33F,columns.height},20*s,
             choice.page==spell::Page::Group?kGold:kIvory,opacity);
        Text(font,"按难度",{columns.x+columns.width*0.37F,columns.y,columns.width*0.63F,columns.height},20*s,
             choice.page==spell::Page::Difficulty?kGold:kIvory,opacity);
        const int groups=spell::Stages()[choice.stage].groups;
        const int offset=choice.group/menu::SpellPracticeLayout::kVisibleGroups*menu::SpellPracticeLayout::kVisibleGroups;
        for(int index=0;index<menu::SpellPracticeLayout::kVisibleGroups && offset+index<groups;++index) {
            const int group=offset+index;bool available=false;
            for(int difficulty=0;difficulty<5;++difficulty)available|=spell::Available(spell::Find(choice.stage,group,difficulty),choice.character,spellRecords_);
            const auto bounds=l.GroupRow(index);
            const Color color=row(bounds,group,available,spellGroupMotion_,choice.page==spell::Page::Group?1:0.65F);
            Text(font,TextFormat("Spell Card %d",group+1),{bounds.x+8*s,bounds.y,bounds.width-16*s,bounds.height*0.53F},23*s,color,opacity);
            Text(font,TextFormat("使用者 %s",spell::GroupUser(choice.stage,group)),
                 {bounds.x+12*s,bounds.y+bounds.height*0.53F,bounds.width-22*s,bounds.height*0.47F},17*s,color,opacity);
        }
        if(groups>menu::SpellPracticeLayout::kVisibleGroups) {
            Text(font,"上一页",l.GroupPage(-1),19*s,kGray,opacity);
            Text(font,"下一页",l.GroupPage(1),19*s,kGray,opacity);
        }
        for(int index=0;index<(choice.stage==6?1:4);++index) {
            const int difficulty=choice.stage==6?4:index;
            const auto* card=spell::Find(choice.stage,choice.group,difficulty);
            const bool available=spell::Available(card,choice.character,spellRecords_);
            const auto bounds=l.Difficulty(index,choice.stage==6);
            const Color color=row(bounds,difficulty,available,spellDifficultyMotion_,choice.page==spell::Page::Difficulty?1:0.35F);
            const float line= bounds.height/(choice.stage==6?5:4);
            Text(font,spell::kDifficulties[difficulty],{bounds.x+12*s,bounds.y,bounds.width*0.57F,line},24*s,color,opacity);
            if(card)Text(font,!spell::Playable(card,choice.character)?"尚未实现":available?"已解锁":"未解锁",
                {bounds.x+bounds.width*0.65F,bounds.y,bounds.width*0.35F-12*s,line},16*s,color,opacity);
            if(card) {
                Text(font,TextFormat("No.%03d  %s",card->number,card->name),
                    {bounds.x+12*s,bounds.y+line,bounds.width-24*s,line},21*s,color,opacity);
                const auto& record=spellRecords_.Get(choice.character,card->number);
                Text(font,TextFormat("最高得分 %09lld",record.bestScore),
                    {bounds.x+22*s,bounds.y+line*2,bounds.width-32*s,line},18*s,color,opacity);
                Text(font,TextFormat("取得 %d/%d  本篇 %d/%d",record.captured,record.attempts,record.mainCaptured,record.mainAttempts),
                    {bounds.x+22*s,bounds.y+line*3,bounds.width-32*s,line},17*s,color,opacity);
            } else Text(font,"------",{bounds.x+24*s,bounds.y+line,bounds.width-36*s,line},21*s,kGray,opacity);
        }
    }
    // 同一人物的两种御札共享一层身体和能力摘要，切武器时不把相同图片叠成更暗的半透明层。
    for(int portrait=0;portrait<2;++portrait) {
        const float alpha=portraitOpacity*(spellCharacterMotion_.Weight(portrait*2)+spellCharacterMotion_.Weight(portrait*2+1));
        if(alpha<0.001F)continue;
        const auto& data=spell::Characters()[portrait*2];
        Portrait(common.spellPortraits[portrait],l.Portrait(),alpha);
        const auto info=l.CharacterInfo();const float unit=info.height/8;
        Text(font,data.title,{info.x,info.y,info.width,unit},23*s,kIvory,alpha);
        Text(font,data.displayName,{info.x,info.y+unit,info.width,unit*1.5F},40*s,kIvory,alpha);
        constexpr std::array<const char*,3> labels{"移动速度","攻击范围","攻击力"};
        constexpr std::array<const char*,5> stars{"★","★★","★★★","★★★★","★★★★★"};
        for(int index=0;index<3;++index) {
            Text(font,labels[index],{info.x,info.y+unit*(2.7F+index),info.width*0.5F,unit},21*s,kGold,alpha);
            Text(font,stars[data.stars[index]-1],{info.x+info.width*0.51F,info.y+unit*(2.7F+index),info.width*0.49F,unit},23*s,kGold,alpha);
        }
    }
    for(int character=0;character<spell::kCharacterCount;++character) {
        const float alpha=portraitOpacity*spellCharacterMotion_.Weight(character);if(alpha<0.001F)continue;
        const auto& data=spell::Characters()[character];
        const auto info=l.CharacterInfo();const float unit=info.height/8;
        Text(font,data.shot,{info.x,info.y+unit*6,info.width,unit},21*s,kRose,alpha);
        Text(font,data.bomb,{info.x,info.y+unit*7,info.width,unit},22*s,kRose,alpha);
    }
    for(int direction:{-1,1}) {
        const auto arrow=l.Arrow(direction);
        ui::ArrowButton{arrow,direction}.Draw({font,s,portraitOpacity,menuMotion_.Phase()});
    }
    const auto footer=l.Footer();
    Text(font,choice.character>=2?"魔理沙玩法尚未实现，当前组合仅供浏览。":"未解锁符卡需以当前组合在关卡内遇见；尚未实现符卡仅供浏览。",
        footer,16*s,kGray,opacity);
}
} // namespace koumakyou
