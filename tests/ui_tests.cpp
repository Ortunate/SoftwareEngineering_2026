// 本文件只链接独立 UI 模块，验证通用交互不依赖 Game、图形窗口、音频或素材生命周期。
#include "ui/ui.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
using namespace koumakyou::ui;
/// @brief 构造与原生采集相同的单帧指针快照；坐标单位为窗口逻辑像素。
MenuInput Pointer(float x,float y,bool click=false) {
    MenuInput input;input.mouse={x,y};input.pointerActive=true;input.click=click;return input;
}
void TestSelection() {
    const std::array<Item,3> items{{{10,{0,0,100,30}},{20,{0,40,100,30},false},{40,{0,80,100,30}}}};
    int selected=10;MenuInput key;key.vertical=1;
    auto result=UpdateSelection(items,key,selected);
    assert(result.changed && selected==40 && !result.confirmed);
    result=UpdateSelection(items,key,selected);assert(selected==10 && result.changed);
    key.vertical=-1;result=UpdateSelection(items,key,selected);assert(selected==40);
    result=UpdateSelection(items,Pointer(20,10),selected);assert(selected==10 && !result.confirmed);
    // 静止指针留在旧行上，键盘必须仍能导航到其他行。
    key.mouse={20,10};key.vertical=1;
    result=UpdateSelection(items,key,selected);assert(selected==40);
    result=UpdateSelection(items,Pointer(20,50,true),selected);assert(selected==40 && !result.confirmed);
    result=UpdateSelection(items,Pointer(200,200,true),selected);assert(!result.confirmed);
    result=UpdateSelection(items,Pointer(20,10,true),selected);assert(selected==10 && result.confirmed);
    auto back=Pointer(20,90,true);back.back=back.confirm=true;
    result=UpdateSelection(items,back,selected);assert(result.back && !result.confirmed && selected==10);
    key={};key.horizontal=-1;
    result=UpdateSelection(items,key,selected,Navigation::Vertical);assert(selected==10 && !result.changed);
    result=UpdateSelection(items,key,selected);assert(selected==40);
    const std::array<Item,1> disabled{{{10,{0,0,100,30},false}}};
    key.confirm=true;result=UpdateSelection(disabled,key,selected);assert(selected==-1 && !result.confirmed);
    result=UpdateSelection({},key,selected);assert(selected==-1 && !result.confirmed);
    // 重叠命中按定义顺序优先，禁止把一次点击确认成两个操作。
    const std::array<Item,2> overlapping{{{3,{0,0,100,30}},{4,{0,0,100,30}}}};
    result=UpdateSelection(overlapping,Pointer(10,10,true),selected);assert(selected==3 && result.confirmed);
}
void TestComponents() {
    Button button{{10,10,100,40}};
    MenuInput confirm;confirm.confirm=true;
    assert(button.Update(confirm,true).activated && !button.Update(confirm,false).activated);
    assert(button.Update(Pointer(20,20,true)).activated);
    button.enabled=false;assert(!button.Update(confirm,true).activated && !button.Update(Pointer(20,20,true)).focused);
    SelectableRow parent{{10,10,100,40},7},child{{130,10,100,40},2};
    int selection=0;
    assert(parent.Update(Pointer(20,20),selection).focused && selection==7);
    assert(child.Update(Pointer(140,20),selection).focused && selection==2);
    assert(!child.Select(confirm,selection));
    auto back=Pointer(20,20,true);back.back=true;
    assert(!parent.Select(back,selection) && selection==2);
    const ArrowButton arrow{{250,10,44,60},1};
    assert(arrow.Update(Pointer(260,20,true)).activated && !arrow.Update(confirm).activated);
    const ValueSelector value{{0,100,300,40},{100,100,40,40},{260,100,40,40}};
    assert(value.Update(Pointer(110,110,true),false)==-1);
    assert(value.Update(Pointer(10,110,true),false)==1);
    assert(value.Update(Pointer(400,400,true),true)==0);
    assert(value.Update(confirm,true)==1 && value.Update(confirm,false)==0);
    MenuInput right;right.horizontal=1;assert(value.Update(right,true)==1);
    right.vertical=1;assert(value.Update(right,true)==0);
    assert(value.Update(back,true)==0);
}
/// @brief 同一组件覆盖8/32字符约束、ASCII过滤、取消优先、字符按钮及保存意图，无需窗口或游戏。
void TestNameEntry() {
    const NameEntryLayout layout(1100,760);const NameEntry editor(layout);
    for(std::size_t limit:{8u,32u}) {
        const NameEntryConfig config{limit};NameEntryState state;std::string value;
        NameEntryInput typed;typed.text=U"WZX Az09-_!";
        auto result=editor.Update(value,state,config,typed);
        assert(result.edited && value==std::string("WZX Az09-_!").substr(0,limit));
        assert(state.selected==NameEntryState::kSave && NameEntry::Valid(value,config));
        typed.text=std::u32string(40,U'Q');editor.Update(value,state,config,typed);assert(value.size()==limit);
        typed={};typed.selectAll=true;typed.text=U"A";editor.Update(value,state,config,typed);assert(state.replaceOnType && value.size()==limit);
        typed={};typed.text=U"中\n\x7f\u0080😀";result=editor.Update(value,state,config,typed);assert(!result.edited && state.replaceOnType);
        typed={};typed.text=U"New";editor.Update(value,state,config,typed);assert(value=="New" && !state.replaceOnType);
        typed={};typed.erase=true;editor.Update(value,state,config,typed);assert(value=="Ne");
        typed={};typed.controls.confirm=true;result=editor.Update(value,state,config,typed);assert(result.action==NameEntryAction::Submit && value=="Ne");
        typed.text=U"BAD";typed.controls.back=true;typed.erase=true;
        result=editor.Update(value,state,config,typed);assert(result.action==NameEntryAction::Cancel && value=="Ne");
        // 屏幕字符、空格、删除与操作按钮和物理输入使用同一编辑状态。
        const auto click=[&](Rectangle rect) {NameEntryInput input;input.controls=Pointer(rect.x+rect.width/2,rect.y+rect.height/2,true);return editor.Update(value,state,config,input);};
        state.replaceOnType=true;click(layout.Character(0));assert(value=="A");
        click(layout.Character(NameEntryState::kKeyCount-1));assert(value=="A ");click(layout.Action(2));assert(value=="A");
        result=click(layout.Action(0));assert(result.action==NameEntryAction::Submit);
        result=click(layout.Action(1));assert(result.action==NameEntryAction::Cancel);
        assert(!NameEntry::Valid("",config) && !NameEntry::Valid("   ",config) && !NameEntry::Valid("中文",config));
        assert(!NameEntry::Valid(std::string(limit+1,'A'),config) && !NameEntry::Valid("A\t",config));
        // 初次按Enter只提交默认保存焦点；键盘直接打字后不会激活先前选择的字符按钮。
        state.selected=2;typed={};typed.text=U"Z";typed.controls.confirm=true;
        result=editor.Update(value,state,config,typed);assert(result.action==NameEntryAction::Submit && value=="AZ");
    }
    // 鼠标字符表完整覆盖95个可打印ASCII，方向键按行列而非串行编号移动。
    std::string characters;NameEntryState keys;
    for(int i=0;i<NameEntryState::kKeyCount;++i) {
        std::string value;NameEntryInput click;const auto rect=layout.Character(i);
        click.controls=Pointer(rect.x+rect.width/2,rect.y+rect.height/2,true);editor.Update(value,keys,{8},click);
        assert(value.size()==1 && characters.find(value)==std::string::npos);characters+=value;
    }
    assert(characters.size()==95);
    keys.selected=0;NameEntryInput arrows;arrows.controls.horizontal=-1;std::string value;
    editor.Update(value,keys,{8},arrows);assert(keys.selected==15);
    arrows.controls={};arrows.controls.vertical=1;editor.Update(value,keys,{8},arrows);assert(keys.selected==31);
    for(Vector2 size:{Vector2{390,844},{800,600},{1100,760},{3440,1440}}) {
        const NameEntryLayout responsive(size.x,size.y);const auto panel=responsive.Panel();
        const auto inside=[&](Rectangle rect) {
            assert(rect.x>=panel.x && rect.y>=panel.y && rect.x+rect.width<=panel.x+panel.width+.001F && rect.y+rect.height<=panel.y+panel.height+.001F);
        };
        inside(responsive.Field());for(int i=0;i<NameEntryState::kKeyCount;++i)inside(responsive.Character(i));
        for(int i=0;i<3;++i)inside(responsive.Action(i));
        assert(panel.y+panel.height<=CommandBar::ContentHeight(size.x,size.y)+.001F);
    }
}
void TestLayoutAndMotion() {
    const Canvas minimized(0,0);assert(std::isfinite(minimized.scale));
    const auto canvas=Canvas::Full(800,600,400,400);
    const auto bounds=canvas.Rect(10,20,40,40);assert(bounds.x==20 && bounds.y==30 && bounds.width==80 && bounds.height==60);
    MenuMotion motion;motion.Advance(1,2,1);
    const float phase=motion.Phase(),weight=motion.Weight(2);
    motion.Advance(1,2,0);assert(motion.Phase()==phase && motion.Weight(2)==weight);
    motion.Advance(1,3,0.01F);assert(motion.Weight(2)>0 && motion.Weight(3)>0);
}
/// @brief 过渡守恒且不依赖帧率；快速反向和零时间换页不得重置已显现的页面。
void TestPageMotion() {
    PageMotion<3> slow,fast;
    assert(slow.Title()==1 && !slow.Visible());
    for(int i=0;i<5;++i)slow.Advance(0,1.0F/30);
    for(int i=0;i<40;++i)fast.Advance(0,1.0F/240);
    assert(std::abs(slow.Layer(0)-fast.Layer(0))<1e-5F);
    const float previous=slow.Layer(0),title=slow.Title();
    slow.Advance(1,0);assert(slow.Layer(0)==previous && slow.Title()==title && slow.Layer(1)==0);
    slow.Advance(1,.08F);
    const float next=slow.Layer(1);slow.Advance(-1,0);assert(slow.Layer(1)==next);
    for(int i=0;i<10;++i) {
        slow.Advance(-1,.03F);
        assert(std::abs(slow.Title()+slow.Layer(0)+slow.Layer(1)+slow.Layer(2)-1)<1e-5F);
    }
    slow.Advance(-1,1);assert(slow.Title()==1 && !slow.Visible());
    slow.Advance(2,.1F);slow.Advance(-2,0);assert(slow.Title()==1 && slow.Layer(2)==0);
}
/// @brief 横条在极小、宽、窄窗口均保持单行，提示数量不改变高度，各项横向排列且不重叠。
void TestCommandBar() {
    for(Vector2 size:{Vector2{1,1},{320,180},{320,240},{390,844},{900,900},{1280,720},{1600,400},{3840,2160}}) {
        const CommandBar bar(size.x,size.y);const auto bounds=bar.Bounds();
        assert(bounds.x==0 && bounds.width==size.x && bounds.y>=0 && bounds.y+bounds.height==size.y);
        assert(std::isfinite(bar.Scale()) && CommandBar::ContentHeight(size.x,size.y)>=1);
        assert(bounds.height<=std::ceil(28*bar.Scale()));
        for(int count=1;count<=CommandBar::kMaxHints;++count)for(int index=0;index<count;++index) {
            const auto cell=bar.Cell(index,count);
            assert(cell.x>=0 && cell.y>=bounds.y && cell.x+cell.width<=size.x+0.001F && cell.y+cell.height<=size.y+0.001F);
            assert(cell.y==bar.Cell(0,count).y && cell.height==bar.Cell(0,count).height);
            for(int other=0;other<index;++other)assert(!CheckCollisionRecs(cell,bar.Cell(other,count)));
        }
        assert(bar.Cell(-1,3).width==0 && bar.Cell(3,3).width==0 && bar.Cell(0,0).width==0);
    }
}
}
int main() {
    TestSelection();TestComponents();TestNameEntry();TestLayoutAndMotion();TestPageMotion();TestCommandBar();
    std::puts("Independent UI selection, components, cancellation, layout and motion passed.");
}
