#include "PhysicsAssetUiGuide.h"
#include "PhysicsAssetHelpDialog.h"
#include "PhysicsAssetHelpData.h"
#include <mx/mxWindow.h>
#include <mx/mxWidget.h>
#include <mx/mxLabel.h>
#include <mx/mxChoice.h>
#include <mx/mxButton.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <utility>
#include <map>

#if defined(PHYSUI_PORTABLE_TEST)
// Portable harness: platform window code intentionally replaced with no-op services.
#elif defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#else
#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QObject>
#include <QScrollArea>
#include <QScreen>
#include <QWidget>
#include <QVector>
#endif

namespace physui {
namespace {
int g_percent=125;
#if defined(PHYSUI_PORTABLE_TEST)
std::map<mxWidget*,std::pair<void*,void(*)(void*)>> g_focus;
// Portable harness: platform window code intentionally replaced with no-op services.
#elif defined(_MSC_VER)
std::vector<HWND> g_windows;
#endif
int Normalize(int p) {return p==100||p==125||p==150?p:125;}
#if defined(PHYSUI_PORTABLE_TEST)
// Portable harness: platform window code intentionally replaced with no-op services.
#elif defined(_MSC_VER)
struct NativeItem {HWND control; RECT base;};
struct NativeScaler {
    HWND window=nullptr;
    mxWindow *widget=nullptr;
    int originalWidth=0,originalHeight=0,percent=125;
    int scrollX=0,scrollY=0;
    HFONT font=nullptr;
    std::vector<NativeItem> items;
};
constexpr UINT_PTR kScalerId=0x50485953;
static int ApplyDpi(int logical, int scale) {return MulDiv(logical,scale,100);}
static void ResizeNative(NativeScaler *s)
{
    const int width=ApplyDpi(s->originalWidth,s->percent),height=ApplyDpi(s->originalHeight,s->percent);
    RECT area{};GetClientRect(s->window,&area);
    SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
    info.nMin=0;info.nMax=std::max(width-1,0);info.nPage=area.right;info.nPos=s->scrollX;
    SetScrollInfo(s->window,SB_HORZ,&info,TRUE);s->scrollX=GetScrollPos(s->window,SB_HORZ);
    info.nMax=std::max(height-1,0);info.nPage=area.bottom;info.nPos=s->scrollY;
    SetScrollInfo(s->window,SB_VERT,&info,TRUE);s->scrollY=GetScrollPos(s->window,SB_VERT);
    for(const auto &item:s->items)
    {
        if(!IsWindow(item.control)) continue;
        const auto &r=item.base;
        MoveWindow(item.control,ApplyDpi(r.left,s->percent)-s->scrollX,
                   ApplyDpi(r.top,s->percent)-s->scrollY,
                   std::max(2,ApplyDpi(r.right-r.left,s->percent)),
                   std::max(2,ApplyDpi(r.bottom-r.top,s->percent)),TRUE);
    }
}
static LRESULT CALLBACK ScalerProc(HWND window,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR, DWORD_PTR data)
{
    auto *s=reinterpret_cast<NativeScaler*>(data);
    if(msg==WM_NCDESTROY)
    {
        RemoveWindowSubclass(window,ScalerProc,kScalerId);
        if(s->font)DeleteObject(s->font);
        delete s;
        return DefSubclassProc(window,msg,wp,lp);
    }
    if(msg==WM_SIZE) {auto result=DefSubclassProc(window,msg,wp,lp);ResizeNative(s);return result;}
    if(msg==WM_VSCROLL||msg==WM_HSCROLL||msg==WM_MOUSEWHEEL)
    {
        const bool vertical=msg!=WM_HSCROLL;
        const int bar=vertical?SB_VERT:SB_HORZ;
        SCROLLINFO si{};si.cbSize=sizeof(si);si.fMask=SIF_ALL;GetScrollInfo(window,bar,&si);
        int pos=si.nPos;
        const int cmd=msg==WM_MOUSEWHEEL?(GET_WHEEL_DELTA_WPARAM(wp)>0?SB_LINEUP:SB_LINEDOWN):LOWORD(wp);
        switch(cmd) {
            case SB_LINEUP:pos-=28;break;case SB_LINEDOWN:pos+=28;break;
            case SB_PAGEUP:pos-=static_cast<int>(si.nPage);break;
            case SB_PAGEDOWN:pos+=static_cast<int>(si.nPage);break;
            case SB_THUMBTRACK:pos=si.nTrackPos;break;
            default:break;
        }
        const int maxPos=std::max(0,si.nMax-static_cast<int>(si.nPage)+1);
        pos=std::max(0,std::min(pos,maxPos));
        if(vertical)s->scrollY=pos;else s->scrollX=pos;
        ResizeNative(s);
        return 0;
    }
    return DefSubclassProc(window,msg,wp,lp);
}
static BOOL CALLBACK StoreChild(HWND child, LPARAM context)
{
    auto *s=reinterpret_cast<NativeScaler*>(context);
    if(GetParent(child)!=s->window)return TRUE;
    RECT r{};GetWindowRect(child,&r);MapWindowPoints(nullptr,s->window,reinterpret_cast<POINT*>(&r),2);
    s->items.push_back({child,r});return TRUE;
}
struct FocusCallback {void *owner;void (*callback)(void*);};
constexpr UINT_PTR kFocusId=0x50484643;
static LRESULT CALLBACK FocusProc(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR, DWORD_PTR data)
{
    auto *cb=reinterpret_cast<FocusCallback*>(data);
    if(msg==WM_SETFOCUS && cb->callback)cb->callback(cb->owner);
    if(msg==WM_NCDESTROY){RemoveWindowSubclass(h,FocusProc,kFocusId);delete cb;}
    return DefSubclassProc(h,msg,w,l);
}
#else
class QtScaler : public QObject {
public:
    struct Item {QWidget *widget;QRect geometry;QFont font;};
    explicit QtScaler(QWidget *host,int width,int height) : QObject(host),m_host(host),m_width(width),m_height(height)
    {
        const auto children=host->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly);
        m_area=new QScrollArea(host);
        m_area->setFrameShape(QFrame::NoFrame);m_area->setWidgetResizable(false);
        m_content=new QWidget(m_area);
        for(QWidget *child:children) {
            m_items.push_back({child,child->geometry(),child->font()});
            child->setParent(m_content);child->show();
        }
        m_area->setWidget(m_content);
        // Newly constructed Qt children may remain hidden when their parent
        // was already shown. Explicitly show the scroll viewport: otherwise
        // moving every editor control into it can leave a blank dialog.
        m_area->show();
        setObjectName("PhysScaleManager");
        host->installEventFilter(this);
        m_area->setGeometry(host->rect());
    }
    bool eventFilter(QObject *watched,QEvent *e) override
    {
        if(watched==m_host && e->type()==QEvent::Resize) m_area->setGeometry(m_host->rect());
        return QObject::eventFilter(watched,e);
    }
    void Apply(int percent)
    {
        const double scale=percent/100.0;
        for(const auto &item:m_items) {
            if(!item.widget)continue;
            const QRect &r=item.geometry;
            item.widget->setGeometry(qRound(r.x()*scale),qRound(r.y()*scale),
                    std::max(2,qRound(r.width()*scale)),std::max(2,qRound(r.height()*scale)));
            QFont f=item.font;
            if(f.pointSizeF()>0)f.setPointSizeF(std::max(9.0,f.pointSizeF()*scale));
            else if(f.pixelSize()>0)f.setPixelSize(std::max(12,qRound(f.pixelSize()*scale)));
            item.widget->setFont(f);
        }
        const int w=qRound(m_width*scale),h=qRound(m_height*scale);
        m_content->setFixedSize(w,h);
        const QScreen *screen=m_host->screen();
        if(!screen)screen=QApplication::primaryScreen();
        QRect avail=screen?screen->availableGeometry():QRect(0,0,1200,900);
        m_host->resize(std::min(w+18,std::max(300,avail.width()-65)),
                       std::min(h+18,std::max(280,avail.height()-90)));
        m_area->setGeometry(m_host->rect());
    }
private:
    QWidget *m_host=nullptr,*m_content=nullptr;
    QScrollArea *m_area=nullptr;
    int m_width,m_height;
    QVector<Item> m_items;
};
class QtFocus : public QObject {
public:
    QtFocus(QWidget *field,void *owner,void (*func)(void*)) : QObject(field),m_owner(owner),m_func(func)
    {setObjectName("PhysFocusHandler");field->installEventFilter(this);}
    bool eventFilter(QObject *o,QEvent *event) override
    {
        if(event->type()==QEvent::FocusIn && m_func)m_func(m_owner);
        return QObject::eventFilter(o,event);
    }
private:
    void *m_owner;
    void (*m_func)(void*);
};
#endif
}
#if defined(PHYSUI_PORTABLE_TEST)
void TestFocus(mxWidget *w){auto it=g_focus.find(w);if(it!=g_focus.end())it->second.second(it->second.first);}
#endif
int CurrentTextScale(){return g_percent;}
void SetTextScale(int percent)
{
    g_percent=Normalize(percent);
#if defined(PHYSUI_PORTABLE_TEST)
// Portable harness: platform window code intentionally replaced with no-op services.
#elif defined(_MSC_VER)
    g_windows.erase(std::remove_if(g_windows.begin(),g_windows.end(),[](HWND hwnd){
        DWORD_PTR value=0;return !IsWindow(hwnd)||!GetWindowSubclass(hwnd,ScalerProc,kScalerId,&value);
    }),g_windows.end());
    for(HWND hwnd:g_windows){DWORD_PTR value=0;if(GetWindowSubclass(hwnd,ScalerProc,kScalerId,&value)){
        RefreshScaling(reinterpret_cast<NativeScaler*>(value)->widget);
    }}
#elif !defined(PHYSUI_PORTABLE_TEST)
    const auto top=QApplication::topLevelWidgets();
    for(QWidget *w:top)if(auto *scaler=static_cast<QtScaler*>(w->findChild<QObject*>("PhysScaleManager")))scaler->Apply(g_percent);
#endif
}
int PercentFromChoice(int index){return index==0?100:index==2?150:125;}
int ChoiceFromPercent(int percent){return percent==100?0:percent==150?2:1;}
void InstallScrollableScaling(mxWindow *window,int width,int height)
{
#if defined(PHYSUI_PORTABLE_TEST)
    (void)width;(void)height;
#endif
    if(!window || !window->getHandle())return;
#if defined(PHYSUI_PORTABLE_TEST)
// Portable harness: platform window code intentionally replaced with no-op services.
#elif defined(_MSC_VER)
    HWND hwnd=static_cast<HWND>(window->getHandle());
    DWORD_PTR existing=0;
    if(GetWindowSubclass(hwnd,ScalerProc,kScalerId,&existing))return;
    g_windows.push_back(hwnd);
    auto *s=new NativeScaler;s->window=hwnd;s->widget=window;s->originalWidth=width;s->originalHeight=height;
    EnumChildWindows(hwnd,StoreChild,reinterpret_cast<LPARAM>(s));
    const auto style=GetWindowLongPtr(hwnd,GWL_STYLE);
    SetWindowLongPtr(hwnd,GWL_STYLE,style|WS_VSCROLL|WS_HSCROLL);
    SetWindowSubclass(hwnd,ScalerProc,kScalerId,reinterpret_cast<DWORD_PTR>(s));
#elif !defined(PHYSUI_PORTABLE_TEST)
    QWidget *host=static_cast<QWidget*>(window->getHandle());
    if(host->findChild<QObject*>("PhysScaleManager"))return;
    new QtScaler(host,width,height);
#endif
    RefreshScaling(window);
}
void RefreshScaling(mxWindow *window)
{
    if(!window||!window->getHandle())return;
#if defined(PHYSUI_PORTABLE_TEST)
// Portable harness: platform window code intentionally replaced with no-op services.
#elif defined(_MSC_VER)
    HWND hwnd=static_cast<HWND>(window->getHandle());DWORD_PTR data=0;
    if(!GetWindowSubclass(hwnd,ScalerProc,kScalerId,&data))return;
    auto *s=reinterpret_cast<NativeScaler*>(data);s->percent=g_percent;s->scrollX=s->scrollY=0;
    HFONT oldFont=s->font;
    HDC dc=GetDC(hwnd);const int dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(hwnd,dc);
    const int fontPoints=std::max(10,MulDiv(10,g_percent,100));
    s->font=CreateFontA(-MulDiv(fontPoints,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"Segoe UI");
    for(const auto &item:s->items)SendMessage(item.control,WM_SETFONT,reinterpret_cast<WPARAM>(s->font),TRUE);
    if(oldFont)DeleteObject(oldFont);
    MONITORINFO info{};info.cbSize=sizeof(info);GetMonitorInfo(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&info);
    const int availW=info.rcWork.right-info.rcWork.left-65,availH=info.rcWork.bottom-info.rcWork.top-90;
    RECT r{};GetWindowRect(hwnd,&r);
    RECT client={0,0,std::min(MulDiv(s->originalWidth,g_percent,100)+18,availW),
                      std::min(MulDiv(s->originalHeight,g_percent,100)+18,availH)};
    AdjustWindowRectEx(&client,GetWindowLong(hwnd,GWL_STYLE),FALSE,GetWindowLong(hwnd,GWL_EXSTYLE));
    SetWindowPos(hwnd,nullptr,0,0,client.right-client.left,client.bottom-client.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    ResizeNative(s);
#elif !defined(PHYSUI_PORTABLE_TEST)
    QWidget *host=static_cast<QWidget*>(window->getHandle());
    if(auto *scaler=static_cast<QtScaler*>(host->findChild<QObject*>("PhysScaleManager")))scaler->Apply(g_percent);
#endif
}
void BindFocus(mxWidget *field,void *owner,void (*onFocus)(void*))
{
    if(!field || !field->getHandle() || !onFocus)return;
#if defined(PHYSUI_PORTABLE_TEST)
    g_focus[field]={owner,onFocus};
// Portable harness: platform window code intentionally replaced with no-op services.
#elif defined(_MSC_VER)
    HWND control=static_cast<HWND>(field->getHandle());
    UnbindFocus(field); // Also prevents a leak if the control is rebound.
    auto *cb=new FocusCallback{owner,onFocus};
    if(!SetWindowSubclass(control,FocusProc,kFocusId,reinterpret_cast<DWORD_PTR>(cb)))delete cb;
#elif !defined(PHYSUI_PORTABLE_TEST)
    QWidget *widget=static_cast<QWidget*>(field->getHandle());
    // Rebinding must not accumulate callbacks to previously freed owners.
    for(QObject *child:widget->children())if(child->objectName()=="PhysFocusHandler")
        delete child;
    new QtFocus(widget,owner,onFocus);
#endif
}
void UnbindFocus(mxWidget *field)
{
    if(!field)return;
#if defined(PHYSUI_PORTABLE_TEST)
    g_focus.erase(field);
#elif defined(_MSC_VER)
    HWND control=static_cast<HWND>(field->getHandle());
    if(!control||!IsWindow(control))return;
    DWORD_PTR data=0;
    if(GetWindowSubclass(control,FocusProc,kFocusId,&data)) {
        RemoveWindowSubclass(control,FocusProc,kFocusId);
        delete reinterpret_cast<FocusCallback*>(data);
    }
#else
    QWidget *widget=static_cast<QWidget*>(field->getHandle());
    if(!widget)return;
    for(QObject *child:widget->children())if(child->objectName()=="PhysFocusHandler")
        delete child;
#endif
}
}

PhysicsAssetContextGuide::PhysicsAssetContextGuide(mxWindow *parent,int y,int width,const char *section,int selectAction,int detailsAction)
    :m_section(section),m_parent(parent)
{
    new mxLabel(parent,10,y+3,145,20,"Parameter guidance");
    m_choice=new mxChoice(parent,155,y,width-320,24,selectAction);
    new mxButton(parent,width-155,y,145,25,"Full explanation",detailsAction);
    m_note=new mxLabel(parent,10,y+32,width-20,28,"");
    for(const auto &entry:physhelp::Catalog())if(m_section==entry.section){m_names.emplace_back(entry.name);m_choice->add(entry.name);}
    if(!m_names.empty()){m_choice->select(0);SelectionChanged();}
}
void PhysicsAssetContextGuide::SelectionChanged()
{
    const int i=m_choice->getSelectedIndex();
    if(i<0||i>=static_cast<int>(m_names.size()))return;
    const auto *entry=physhelp::Find(m_section,m_names[i]);
    if(!entry)return;
    // The separate Help dialog contains the unabridged description and examples.
    std::string brief=std::string(entry->name)+": "+entry->brief;
    if(brief.size()>125)brief=brief.substr(0,122)+"...";
    m_note->setLabel(brief.c_str());
}
PhysicsAssetContextGuide::~PhysicsAssetContextGuide()
{
    // mxToolKit widgets outlive this helper in editor/dialog destructors.
    // Remove native event hooks before releasing callback state.
    for(auto *binding:m_bindings){physui::UnbindFocus(binding->field);delete binding;}
}
void PhysicsAssetContextGuide::Select(const char *name)
{
    if(!name)return;
    auto it=std::find(m_names.begin(),m_names.end(),name);
    if(it==m_names.end())return;
    m_choice->select(static_cast<int>(it-m_names.begin()));SelectionChanged();
}
void PhysicsAssetContextGuide::ShowDetails(PhysicsAssetHelpDialog *&dialog)
{
    if(!dialog)dialog=new PhysicsAssetHelpDialog(m_parent);
    int i=m_choice->getSelectedIndex();
    if(i>=0&&i<static_cast<int>(m_names.size()))dialog->Open(m_section,m_names[i]);
    else dialog->Open(m_section);
}
void PhysicsAssetContextGuide::FocusField(void *bound)
{
    auto *b=static_cast<Bound*>(bound);
    if(b && b->self)b->self->Select(b->name.c_str());
}
void PhysicsAssetContextGuide::Bind(mxWidget *field,const char *name)
{
    if(!name||!physhelp::Find(m_section,name))return;
    auto *binding=new Bound{this,name,field};m_bindings.push_back(binding);
    physui::BindFocus(field,binding,&FocusField);
}
