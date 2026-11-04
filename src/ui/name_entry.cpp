// 本文件实现公用命名界面的ASCII编辑、字符按钮、操作意图和只读绘制，不执行文件或游戏操作。
#include "ui/name_entry.hpp"
#include "ui/command_bar.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace koumakyou::ui {
namespace {
constexpr std::string_view kCharacters=
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ ";
static_assert(kCharacters.size()==NameEntryState::kKeyCount);
float PanelTop(bool compact) {return compact?150.0F:40.0F;}
}
NameEntryInput NameEntryInput::Read() {
    NameEntryInput input;input.controls=MenuInput::Read();
    // 字母键和空格仅走字符队列，不能把输入中的WASD、Z或X解释成导航、确认或取消。
    input.controls.vertical=IsKeyPressed(KEY_DOWN)-IsKeyPressed(KEY_UP);
    input.controls.horizontal=IsKeyPressed(KEY_RIGHT)-IsKeyPressed(KEY_LEFT);
    input.controls.confirm=IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_KP_ENTER);
    input.controls.back=IsKeyPressed(KEY_ESCAPE)||IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    input.erase=IsKeyPressed(KEY_BACKSPACE)||IsKeyPressedRepeat(KEY_BACKSPACE);
    input.selectAll=(IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL))&&IsKeyPressed(KEY_A);
    input.next=IsKeyPressed(KEY_TAB);
    for(int code=GetCharPressed();code;code=GetCharPressed())input.text.push_back(static_cast<char32_t>(code));
    return input;
}
NameEntryLayout::NameEntryLayout(int windowWidth,int windowHeight)
    :canvas(Canvas::Full(windowWidth,CommandBar::ContentHeight(windowWidth,windowHeight),
        windowWidth<windowHeight*1.15F?760:1100,windowWidth<windowHeight*1.15F?1100:760)),
     width(std::max(1,windowWidth)),height(std::max(1,windowHeight)),compact(canvas.width<1000){}
Rectangle NameEntryLayout::Panel() const {return canvas.Rect(compact?24:100,PanelTop(compact),compact?712:900,compact?800:680);}
Rectangle NameEntryLayout::Field() const {return canvas.Rect(compact?64:140,PanelTop(compact)+120,canvas.width-(compact?128:280),64);}
Rectangle NameEntryLayout::Character(int index) const {
    const float step=compact?40:44;
    return canvas.Rect(canvas.width/2-NameEntryState::kColumns*step/2+(index%NameEntryState::kColumns)*step,
        PanelTop(compact)+(compact?230:215)+(index/NameEntryState::kColumns)*(compact?48:44),step-4,compact?44:40);
}
Rectangle NameEntryLayout::Action(int index) const {return canvas.Rect(canvas.width/2-250+index*170,PanelTop(compact)+(compact?560:510),155,48);}
bool NameEntry::Valid(std::string_view value,const NameEntryConfig& config) {
    return !value.empty() && value.size()<=config.maxLength && value.find_first_not_of(' ')!=std::string_view::npos &&
        std::all_of(value.begin(),value.end(),[](unsigned char code){return code>=32 && code<=126;});
}
NameEntryResult NameEntry::Update(std::string& value,NameEntryState& state,const NameEntryConfig& config,const NameEntryInput& input) const {
    NameEntryResult result;
    // 返回先于编辑，确保取消与字符/确认同帧时不会修改调用方仍需保留的名称。
    if(input.controls.back){result.action=NameEntryAction::Cancel;return result;}
    const int previous=state.selected;
    if(state.selected<0 || state.selected>=NameEntryState::kItemCount)state.selected=NameEntryState::kSave;
    const auto append=[&](char32_t code) {
        if(code<32 || code>126 || config.maxLength==0)return;
        if(state.replaceOnType){value.clear();state.replaceOnType=false;}
        if(value.size()<config.maxLength){value.push_back(static_cast<char>(code));result.edited=true;}
    };
    const auto erase=[&]() {
        state.replaceOnType=false;
        if(!value.empty()){value.pop_back();result.edited=true;}
    };
    if(input.selectAll){state.replaceOnType=true;state.selected=NameEntryState::kSave;}
    else {
        for(char32_t code:input.text)append(code);
        // 直接输入后回到保存按钮；随后Enter提交，不会意外激活此前悬停的字母按钮。
        if(result.edited)state.selected=NameEntryState::kSave;
    }
    if(input.erase)erase();
    if(input.next)state.selected=(state.selected+1)%NameEntryState::kItemCount;
    std::array<Item,NameEntryState::kItemCount> items{};
    for(int index=0;index<NameEntryState::kKeyCount;++index)items[index]={index,layout_.Character(index)};
    for(int index=0;index<3;++index)items[NameEntryState::kSave+index]={NameEntryState::kSave+index,layout_.Action(index)};
    // 字符区按二维网格移动；操作按钮作为最后一行，纵向取最近列，不能沿一维编号串行跳转。
    auto controls=input.controls;
    if(controls.vertical) {
        const int row=state.selected<NameEntryState::kKeyCount?state.selected/NameEntryState::kColumns:NameEntryState::kRows;
        const int nextRow=(row+controls.vertical+NameEntryState::kRows+1)%(NameEntryState::kRows+1);
        const auto current=items[state.selected].bounds;
        const float x=current.x+current.width/2;
        float distance=1e30F;
        for(const auto& item:items) {
            const int targetRow=item.id<NameEntryState::kKeyCount?item.id/NameEntryState::kColumns:NameEntryState::kRows;
            if(targetRow!=nextRow)continue;
            const float candidate=std::abs(item.bounds.x+item.bounds.width/2-x);
            if(candidate<distance){distance=candidate;state.selected=item.id;}
        }
    } else if(controls.horizontal) {
        const int first=state.selected<NameEntryState::kKeyCount?state.selected/NameEntryState::kColumns*NameEntryState::kColumns:NameEntryState::kSave;
        const int count=state.selected<NameEntryState::kKeyCount?std::min(NameEntryState::kColumns,NameEntryState::kKeyCount-first):3;
        state.selected=first+(state.selected-first+controls.horizontal+count)%count;
    }
    controls.vertical=controls.horizontal=0;
    const auto selection=UpdateSelection(items,controls,state.selected);
    result.selectionChanged=state.selected!=previous;result.activated=selection.confirmed;
    if(!selection.confirmed)return result;
    if(state.selected<NameEntryState::kKeyCount)append(kCharacters[state.selected]);
    else if(state.selected==NameEntryState::kDelete)erase();
    else result.action=state.selected==NameEntryState::kSave?NameEntryAction::Submit:NameEntryAction::Cancel;
    return result;
}
void NameEntry::Draw(const DrawContext& context,const std::string& value,const NameEntryState& state,const NameEntryConfig& config,const NameEntryText& text) const {
    const auto& c=layout_.canvas;const float top=PanelTop(layout_.compact);
    auto paint=context;paint.scale=c.scale;
    Modal{{0,0,static_cast<float>(layout_.width),static_cast<float>(layout_.height)},layout_.Panel(),c.Point(c.width/2,top+24)}.Draw(paint,text.title,paint.theme.accent);
    Label{c.Rect(64,top+78,c.width-128,32),19,TextAlign::Center}.Draw(paint,text.subtitle,paint.theme.text);
    auto field=paint;if(state.replaceOnType)field.theme.panel={100,55,49,220};
    ui::Panel{layout_.Field()}.Draw(field);
    const auto box=layout_.Field();
    // 八格与原作名称宽度一致；占位符仅用于绘制，不进入调用方保存的字符串。
    if(config.maxLength==8) {
        const float step=std::min(box.width/8,48*c.scale),left=box.x+(box.width-step*8)/2;
        for(int i=0;i<8;++i) {
            const char letter[]{i<static_cast<int>(value.size())?value[i]:'_',0};
            Label{{left+i*step,box.y,step,box.height},26,TextAlign::Center}.Draw(paint,letter,paint.theme.text);
        }
    } else Label{{box.x+12*c.scale,box.y,box.width-24*c.scale,box.height},26}.Draw(paint,(value+"_").c_str(),paint.theme.text);
    const auto count=std::to_string(value.size())+" / "+std::to_string(config.maxLength)+" · ASCII";
    Label{c.Rect(64,top+188,c.width-128,28),17,TextAlign::Center}.Draw(paint,count.c_str(),paint.theme.accent);
    for(int index=0;index<NameEntryState::kKeyCount;++index) {
        const char label[]{kCharacters[index],0};
        Button{layout_.Character(index)}.Draw(paint,kCharacters[index]==' '?"空格":label,{state.selected==index?1.0F:0.0F,1});
    }
    const std::array<const char*,3> actions{text.save,"返回","删除"};
    for(int index=0;index<3;++index)Button{layout_.Action(index)}.Draw(paint,actions[index],{state.selected==NameEntryState::kSave+index?1.0F:0.0F,1});
    Label{c.Rect(64,top+(layout_.compact?635:573),c.width-128,52),18,TextAlign::Center}.Draw(paint,text.error,paint.theme.accent);
}
void NameEntry::DrawHints(const DrawContext& context) const {
    const CommandBar bar(layout_.width,layout_.height);auto paint=context;paint.scale=bar.Scale();
    const std::array<KeyHint,6> hints{{{"键盘字符","输入名称"},{"方向键 / Tab","选择按钮"},{"Enter / 左键","确认"},
        {"Backspace","删除"},{"Ctrl+A","重新输入"},{"Esc / 右键","返回"}}};
    bar.Draw(paint,hints);
}
} // namespace koumakyou::ui
