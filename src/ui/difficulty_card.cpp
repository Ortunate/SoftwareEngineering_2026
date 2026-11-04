// 本文件绘制共用难度札的纸色、英文标题、中文副文案、面数和花饰细线。
#include "ui/difficulty_card.hpp"
#include "ui/typography.hpp"
#include "ui/effects.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace koumakyou::ui {
void DifficultyCard::Draw(const DrawContext& context,bool selected) const {
    if(difficulty<0 || difficulty>4)return;
    constexpr std::array<const char*,5> names{"EASY","NORMAL","HARD","LUNATIC","EXTRA"};
    // 对照原作中文选卡文案；所有入口共用，Score查询不依据可玩门禁隐藏难度。
    constexpr std::array<const char*,5> descriptions{"面向不擅长STG的人","面向大多数人的难度","一般街机STG的难度",
        "面向有些奇怪的人的难度","收手吧！会疯的哦？"};
    constexpr std::array<Color,5> colors{{{81,131,96,255},{66,101,145,255},{141,71,117,255},{150,56,64,255},{115,61,139,255}}};
    const float opacity=context.opacity*(available?1:0.45F),scale=context.scale;
    // 五卡查询的单张高度略小；按实际高度收缩字号，避免标题压到中间花饰线。
    const float textScale=std::min(scale,bounds.height/104);
    const auto text=[&](const char* value,float size,Color color,float top,float inset=0) {
        const float width=bounds.width*(1-2*inset);
        size=FittedSize(context.font,value,size*textScale,width);
        const Vector2 point{bounds.x+(bounds.width-MeasureText(context.font,value,size).x)/2,bounds.y+top*bounds.height};
        DrawText(context.font,value,point,size,color);
    };
    DrawRectangleRec(bounds,WithOpacity(selected?context.theme.text:Color{101,85,83,155},opacity));
    DrawRectangleLinesEx(bounds,2*scale,WithOpacity(selected?context.theme.accent:Color{127,98,84,180},opacity));
    const Color ink=WithOpacity(selected?colors[difficulty]:Color{169,151,144,255},opacity);
    text(names[difficulty],42,ink,0.08F);
    const float left=bounds.x+bounds.width*0.12F,right=bounds.x+bounds.width*0.88F,y=bounds.y+bounds.height*0.48F,r=3*scale;
    DrawLineEx({left+10*scale,y},{right-10*scale,y},scale,ink);
    for(float x:{left+5*scale,right-5*scale})for(int petal=0;petal<5;++petal) {
        const float angle=petal*2*PI/5;
        DrawCircleV({x+std::cos(angle)*r,y+std::sin(angle)*r},r*0.6F,ink);
    }
    const Color descriptionInk=selected?WithOpacity(Color{63,43,43,255},opacity):ink;
    text(descriptions[difficulty],22,descriptionInk,difficulty==4?0.60F:0.56F,0.08F);
    if(difficulty<4)text(difficulty==0?"(共5面)":"(共6面)",18,descriptionInk,0.79F,0.08F);
}
} // namespace koumakyou::ui
