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
void TestLayoutAndMotion() {
    const Canvas minimized(0,0);assert(std::isfinite(minimized.scale));
    const auto canvas=Canvas::Full(800,600,400,400);
    const auto bounds=canvas.Rect(10,20,40,40);assert(bounds.x==20 && bounds.y==30 && bounds.width==80 && bounds.height==60);
    MenuMotion motion;motion.Advance(1,2,1);
    const float phase=motion.Phase(),weight=motion.Weight(2);
    motion.Advance(1,2,0);assert(motion.Phase()==phase && motion.Weight(2)==weight);
    motion.Advance(1,3,0.01F);assert(motion.Weight(2)>0 && motion.Weight(3)>0);
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
    TestSelection();TestComponents();TestLayoutAndMotion();TestCommandBar();
    std::puts("Independent UI selection, components, cancellation, layout and motion passed.");
}
