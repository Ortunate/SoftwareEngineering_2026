// 本文件实现通用按键横条的响应式布局与绘制，不依赖页面身份、输入绑定或游戏素材。
#include "ui/command_bar.hpp"
#include "ui/effects.hpp"
#include <algorithm>
#include <cmath>

namespace koumakyou::ui {
CommandBar::CommandBar(float width,float height) {
    width=std::max(1.0F,width);height=std::max(1.0F,height);
    scale_=std::clamp(std::min(width/960,height/640),0.65F,1.4F);
    // 所有页面只预留一行正文和上下内边距；窗口变窄或提示变多只能收缩文字，不能增加行数。
    const float h=std::min(height*0.24F,28*scale_);
    const float top=std::floor(height-h);
    bounds_={0,top,width,height-top};
}
float CommandBar::ContentHeight(float width,float height) {
    return std::max(1.0F,CommandBar(width,height).Bounds().y);
}
Rectangle CommandBar::Cell(int index,int count) const {
    count=std::clamp(count,0,kMaxHints);
    if(index<0 || index>=count)return {};
    const float padding=std::min(4*scale_,bounds_.height*0.1F);
    const float h=bounds_.height-2*padding,w=bounds_.width/count;
    return {index*w,bounds_.y+padding,w,h};
}
void CommandBar::Draw(const DrawContext& context,std::span<const KeyHint> hints) const {
    DrawRectangleRec(bounds_,WithOpacity(Color{20,10,17,245},context.opacity));
    DrawLineEx({bounds_.x,bounds_.y},{bounds_.x+bounds_.width,bounds_.y},std::max(0.5F,scale_),
               WithOpacity(context.theme.accent,context.opacity*0.5F));
    const int count=std::min(static_cast<int>(hints.size()),kMaxHints);
    for(int index=0;index<count;++index) {
        const auto& hint=hints[index];const auto cell=Cell(index,count);
        float size=std::min(14*context.scale,cell.height*0.7F);
        const float gap=hint.keys[0]?std::min(8*scale_,cell.width*0.1F):0;
        const float keyPadding=gap;
        const float textWidth=MeasureText(context.font,hint.keys,size).x+MeasureText(context.font,hint.action,size).x;
        const float maximum=std::max(0.1F,cell.width-std::min(16*scale_,cell.width*0.1F));
        if(textWidth+keyPadding+gap>maximum && textWidth>0)
            size*=std::max(0.001F,maximum-keyPadding-gap)/textWidth;
        const float keyWidth=MeasureText(context.font,hint.keys,size).x+keyPadding;
        const float actionWidth=MeasureText(context.font,hint.action,size).x;
        const float x=cell.x+(cell.width-keyWidth-gap-actionWidth)/2,y=cell.y+(cell.height-size)/2;
        const float opacity=context.opacity*(hint.enabled?1:0.4F);
        if(hint.keys[0]) {
            DrawRectangleRounded({x,y-2*scale_,keyWidth,size+4*scale_},0.2F,6,WithOpacity(Color{70,35,43,220},opacity));
            DrawText(context.font,hint.keys,{x+keyPadding/2,y},size,WithOpacity(context.theme.accent,opacity));
        }
        DrawText(context.font,hint.action,{x+keyWidth+gap,y},size,WithOpacity(context.theme.text,opacity));
    }
}
} // namespace koumakyou::ui
