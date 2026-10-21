// 本文件实现通用 UI 组件的无业务交互和 raylib 绘制；选择更新可在没有窗口的测试中运行。
#include "ui/components.hpp"
#include "ui/effects.hpp"
#include <algorithm>

namespace koumakyou::ui {
void Label::Draw(const DrawContext& context,const char* text,Color color) const {
    const float size=FittedSize(context.font,text,fontSize*context.scale,bounds.width);
    const float width=MeasureText(context.font,text,size).x;
    const float x=bounds.x+(align==TextAlign::Center?(bounds.width-width)/2:align==TextAlign::Right?bounds.width-width:0);
    DrawText(context.font,text,{x,bounds.y+(bounds.height-size)/2},size,WithOpacity(color,context.opacity));
}
void Panel::Draw(const DrawContext& context) const {
    DrawRectangleRounded(bounds,roundness,12,WithOpacity(context.theme.panel,context.opacity));
    if(borderWidth>0)DrawRectangleRoundedLinesEx(bounds,roundness,12,borderWidth*context.scale,WithOpacity(context.theme.accent,context.opacity));
}
SelectionResult UpdateSelection(std::span<const Item> items,const MenuInput& input,int& selected,Navigation navigation) {
    if(input.back)return {false,false,true};
    const int previous=selected;
    int current=-1,first=-1;
    for(int index=0;index<static_cast<int>(items.size());++index)if(items[index].enabled) {
        if(first<0)first=index;
        if(items[index].id==selected)current=index;
    }
    if(first<0) {selected=-1;return {previous!=selected,false,false};}
    const bool hadSelection=current>=0;
    if(!hadSelection)current=first;
    const int direction=navigation==Navigation::BothAxes?input.Direction():navigation==Navigation::Vertical?input.vertical:0;
    if(direction!=0 && hadSelection) {
        // 有效项至少一个，扫描最多一整圈；全禁用已提前返回，不会在空菜单中无限循环。
        for(std::size_t step=0;step<items.size();++step) {
            current=(current+static_cast<int>(items.size())+(direction>0?1:-1))%static_cast<int>(items.size());
            if(items[current].enabled)break;
        }
    }
    bool clicked=false;
    for(int index=0;index<static_cast<int>(items.size());++index) {
        const auto& item=items[index];
        if(item.enabled && input.pointerActive && CheckCollisionPointRec(input.mouse,item.bounds)) {
            current=index;clicked=input.click;
            break; // 重叠区域按数组顺序优先，单次输入不能确认两个控件。
        }
    }
    selected=items[current].id;
    return {selected!=previous,input.confirm || clicked,false};
}

Interaction Button::Update(const MenuInput& input,bool focused) const {
    const bool hover=CheckCollisionPointRec(input.mouse,bounds);
    if(!enabled || input.back)return {hover,false,false};
    return {hover,hover && input.pointerActive,(hover && input.click) || (focused && input.confirm)};
}

void Button::Draw(const DrawContext& context,const char* label,Visual visual) const {
    const float weight=enabled?visual.weight:0;
    const float reveal=visual.reveal*(enabled?1:0.35F)*context.opacity;
    const auto& theme=context.theme;const float scale=context.scale;
    if(style==ButtonStyle::Text) {
        const float size=25*scale,width=MeasureText(context.font,label,size).x;
        GlowText(context.font,label,{bounds.x+bounds.width/2-width/2+(-8*weight+16*(1-visual.reveal))*scale,bounds.y+4*scale},size,
                 WithOpacity(Blend(Color{190,153,164,255},theme.accent,weight),reveal),WithOpacity(theme.accent,context.opacity),weight*context.pulse);
        FocusLine({bounds.x+20*scale,bounds.y+bounds.height},{bounds.x+bounds.width-20*scale,bounds.y+bounds.height},
                  scale,WithOpacity(theme.accent,context.opacity),weight*reveal,context.phase);
        return;
    }
    DrawRectangleRec(bounds,WithOpacity(theme.button,reveal));
    FocusLine({bounds.x,bounds.y+bounds.height},{bounds.x+bounds.width,bounds.y+bounds.height},scale,
              WithOpacity(theme.accent,reveal),weight,context.phase);
    const float size=FittedSize(context.font,label,22*scale,bounds.width-24*scale);
    GlowText(context.font,label,{bounds.x+12*scale,bounds.y+(bounds.height-size)/2},size,
             WithOpacity(Blend(theme.text,theme.accent,weight),reveal),WithOpacity(theme.accent,reveal),weight*context.pulse);
}

Interaction SelectableRow::Update(const MenuInput& input,int& selected) const {
    const auto result=Button{bounds,enabled}.Update(input);
    if(result.focused)selected=id;
    return result;
}
bool SelectableRow::Select(const MenuInput& input,int& selected) const {return Update(input,selected).activated;}

Color SelectableRow::Draw(const DrawContext& context,float weight,bool available) const {
    weight=enabled?weight:0;
    DrawRectangleRec(bounds,WithOpacity(available?Color{58,22,35,static_cast<unsigned char>(105+65*weight)}:
        Color{32,23,32,static_cast<unsigned char>(110+35*weight)},context.opacity));
    FocusLine({bounds.x+12*context.scale,bounds.y+bounds.height},
              {bounds.x+bounds.width-12*context.scale,bounds.y+bounds.height},context.scale,
              WithOpacity(context.theme.accent,context.opacity*(available?1:0.7F)),weight,context.phase);
    return available?Blend(Color{180,148,145,255},context.theme.accent,weight):Blend(context.theme.muted,Color{190,174,176,255},weight);
}

Interaction ArrowButton::Update(const MenuInput& input) const {return Button{bounds,enabled}.Update(input);}
void ArrowButton::Draw(const DrawContext& context) const {
    SelectionArrow(bounds,direction,context.theme.accent,context.opacity*(enabled?1:0.35F),context.phase,
                   enabled && CheckCollisionPointRec(GetMousePosition(),bounds));
}

int ValueSelector::Update(const MenuInput& input,bool focused) const {
    if(!enabled || input.back)return 0;
    if(input.Clicked(bounds))return input.Clicked(decrease)?-1:1;
    if(!focused)return 0;
    if(input.confirm)return 1;
    return input.vertical==0?input.horizontal:0;
}
void ValueSelector::DrawArrows(const DrawContext& context,int current,int minimum,int maximum,Visual visual) const {
    for(int direction:{-1,1}) {
        const auto bounds=direction<0?decrease:increase;
        const bool disabled=!enabled || (direction<0?current<=minimum:current>=maximum);
        const Color color=WithOpacity(disabled?Color{92,71,65,255}:
            Blend(Color{137,103,82,255},context.theme.accent,visual.weight),visual.reveal*context.opacity);
        DrawRectangleLinesEx(bounds,context.scale,color);
        DrawCentered(context.font,direction<0?"<":">",bounds.x+bounds.width/2,bounds.y+2*context.scale,23*context.scale,color);
    }
}

void Modal::Draw(const DrawContext& context,const char* title,Color titleColor,float titleSize) const {
    DrawRectangleRec(viewport,WithOpacity(context.theme.shade,context.opacity));
    Panel{panel}.Draw(context);
    DrawCentered(context.font,title,titlePosition.x,titlePosition.y,titleSize*context.scale,WithOpacity(titleColor,context.opacity));
}
} // namespace koumakyou::ui
