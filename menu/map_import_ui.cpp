#include "map_import_ui.h"
#include "dosmenu_shell.h"
#include "enhanced_menu.h"
#include "../audio/audioboot.h"
#include "../game/fullscreen.h"
extern "C" {
#include "pngwrite.h"
}
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
struct Entry { std::string name, path; bool directory; };
static std::string probe_source, probe_shots;
static int probe_run = 0, probe_enabled = 0;
static std::string remembered_path;
static bool inside(int x,int y,int bx,int by,int w,int h) { return x>=bx&&y>=by&&x<bx+w&&y<by+h; }
static std::string lower(std::string s) { for(char &c:s)c=(char)std::tolower((unsigned char)c);return s; }
static bool map_extension(const std::string &s) {
    size_t at=s.find_last_of('.');if(at==std::string::npos)return false;
    const std::string ext=lower(s.substr(at));return ext==".ini"||ext==".bin"||ext==".mpr"||ext==".map"||ext==".zip"||ext==".rar"||ext==".7z";
}
static std::string join(const std::string &a,const std::string &b) {
    if(a.empty())return b;return a+(a.back()=='/'||a.back()=='\\'?"":"/")+b;
}
static std::string parent(std::string path) {
    while(path.size()>1&&(path.back()=='/'||path.back()=='\\'))path.pop_back();
#ifdef _WIN32
    if(path.size()==2&&path[1]==':')return "";
#endif
    const size_t at=path.find_last_of("/\\");if(at==std::string::npos)return "";
    if(at==0)return "/";
#ifdef _WIN32
    if(at==2&&path[1]==':')return path.substr(0,3);
#endif
    return path.substr(0,at);
}
#ifdef _WIN32
static std::wstring wide(const std::string &s) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),-1,NULL,0);if(!n)return L"";
    std::vector<wchar_t> b(n);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),-1,b.data(),n);return b.data();
}
static std::string utf8(const wchar_t *s) {
    int n=WideCharToMultiByte(CP_UTF8,0,s,-1,NULL,0,NULL,NULL);if(!n)return "";
    std::vector<char>b(n);WideCharToMultiByte(CP_UTF8,0,s,-1,b.data(),n,NULL,NULL);return b.data();
}
#endif
static bool directory(const std::string &path) {
#ifdef _WIN32
    if(path.empty())return true;
    DWORD a=GetFileAttributesW(wide(path).c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;return stat(path.c_str(),&st)==0&&S_ISDIR(st.st_mode);
#endif
}
static std::string home() {
#ifdef _WIN32
    wchar_t b[32768];DWORD n=GetEnvironmentVariableW(L"USERPROFILE",b,32768);if(n&&n<32768)return utf8(b);
    n=GetCurrentDirectoryW(32768,b);return n&&n<32768?utf8(b):"C:/";
#else
    const char *p=std::getenv("HOME");if(p&&*p)return p;
    char b[4096];return getcwd(b,sizeof b)?b:"/";
#endif
}
static std::string absolute(std::string path,const std::string &base) {
#ifndef _WIN32
    if(path=="~")return home();if(path.size()>1&&path[0]=='~'&&path[1]=='/')return join(home(),path.substr(2));
    if(path.empty()||path[0]!='/')path=join(base,path);
#else
    if(path.size()<2||path[1]!=':'){if(path.size()<2||path.substr(0,2)!="\\\\")path=join(base,path);}
#endif
    return path;
}
static bool scan(const std::string &path,std::vector<Entry> &entries) {
    std::vector<Entry> fresh;
#ifdef _WIN32
    if(path.empty()) {
        DWORD drives=GetLogicalDrives();for(int i=0;i<26;i++)if(drives&(1u<<i)) {char d[]={char('A'+i),':','/',0};fresh.push_back({d,d,true});}
    } else {
        WIN32_FIND_DATAW data;HANDLE h=FindFirstFileW(wide(join(path,"*")).c_str(),&data);if(h==INVALID_HANDLE_VALUE)return false;
        do {std::string n=utf8(data.cFileName);if(n.empty()||n=="."||n==".."||(data.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN))continue;
            bool dir=(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;if(dir||map_extension(n))fresh.push_back({n,join(path,n),dir});
        } while(FindNextFileW(h,&data));FindClose(h);
    }
#else
    DIR *d=opendir(path.c_str());if(!d)return false;
    while(dirent *e=readdir(d)) {std::string n=e->d_name;if(n.empty()||n[0]=='.')continue;
        std::string full=join(path,n);struct stat st;if(stat(full.c_str(),&st))continue;
        bool dir=S_ISDIR(st.st_mode);if(dir||(S_ISREG(st.st_mode)&&map_extension(n)))fresh.push_back({n,full,dir});
    }closedir(d);
#endif
    std::sort(fresh.begin(),fresh.end(),[](const Entry&a,const Entry&b){return a.directory!=b.directory?a.directory>b.directory:lower(a.name)<lower(b.name);});
    entries.swap(fresh);return true;
}
static void copy(char *out,size_t size,const std::string &s){std::snprintf(out,size,"%s",s.c_str());}
static bool disabled(const DMS_MapImportState *st,int item) {
    if(item==MI_NONE)return true;
    if(st->busy)return true;
    if(st->completed)return item!=MI_BACK;
    if(item==MI_IMPORT)return !st->ready;
    if(item==MI_PREVIOUS)return st->top==0;
    if(item==MI_NEXT)return st->top+st->row_count>=st->total;
    return false;
}
struct Job {
    SDL_mutex *mutex=nullptr;
    SDL_Thread *thread=nullptr;
    const DMS_MapImportHooks *hooks=nullptr;
    std::string path;
    DMS_MapImportInfo info={};
    int importing=0,done=0,result=0,progress=0;
    char message[512]={};
};
static void progress(void *ptr,int amount,const char *message) {
    Job *j=(Job*)ptr;SDL_LockMutex(j->mutex);j->progress=std::max(j->progress,std::min(100,std::max(0,amount)));
    std::snprintf(j->message,sizeof j->message,"%s",message?message:"");SDL_UnlockMutex(j->mutex);
}
static int work(void *ptr) {
    Job *j=(Job*)ptr;
    int result=j->importing?j->hooks->run(j->path.c_str(),&j->info,progress,j):j->hooks->inspect(j->path.c_str(),&j->info);
    SDL_LockMutex(j->mutex);j->result=result;j->done=1;SDL_UnlockMutex(j->mutex);return 0;
}
static void rows(DMS_MapImportState &st,const std::vector<Entry>&entries,int selected) {
    st.total=(int)entries.size();st.top=std::max(0,std::min(st.top,std::max(0,st.total-MI_VISIBLE_ROWS)));
    st.row_count=std::min(MI_VISIBLE_ROWS,st.total-st.top);st.selected_row=selected>=st.top&&selected<st.top+st.row_count?selected-st.top:-1;
    for(int i=0;i<st.row_count;i++){copy(st.rows[i],sizeof st.rows[i],entries[st.top+i].name);st.row_directory[i]=entries[st.top+i].directory;}
}
static void frame(DMS *shell) { dms_redraw(shell);audio_frame(shell->cfg.au,16);dms_draw(shell);SDL_GL_SwapWindow(shell->win); }
static int event_hit(DMS *s,const DMS_MapImportState *st,int x,int y) {
    int dw,dh,ww,wh;SDL_GL_GetDrawableSize(s->win,&dw,&dh);SDL_GetWindowSize(s->win,&ww,&wh);
    int fx=x*dw/std::max(1,ww),fy=y*dh/std::max(1,wh);
    if(dms_enhanced_menu(s))return em_pick(fx,fy);
    return mi_hit_test(st,(fx-s->vpx)/s->scale,(fy-s->vpy)/s->scale);
}
static void capture(DMS *shell,const char *tag) {
    if(probe_shots.empty())return;
    if(dms_enhanced_menu(shell)) {
        int original_w,original_h;SDL_GetWindowSize(shell->win,&original_w,&original_h);
        for(int h:{720,1440}) {SDL_SetWindowSize(shell->win,h*16/9,h);SDL_PumpEvents();char filename[128];std::snprintf(filename,sizeof filename,"/%s-%dp.png",tag,h);em_write_shot((probe_shots+filename).c_str(),h*16/9,h);}
        SDL_SetWindowSize(shell->win,original_w,original_h);SDL_PumpEvents();
        int w,h;SDL_GL_GetDrawableSize(shell->win,&w,&h);em_viewport(w,h);
    }
    int width,height;SDL_GL_GetDrawableSize(shell->win,&width,&height);dms_draw(shell);
    std::vector<unsigned char> bottom(width*height*4),top(width*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,bottom.data());
    for(int y=0;y<height;y++)memcpy(top.data()+y*width*4,bottom.data()+(height-1-y)*width*4,width*4);
    png_write_rgba((probe_shots+"/"+tag+"-framebuffer.png").c_str(),top.data(),width,height);SDL_GL_SwapWindow(shell->win);
    printf("MAPIMPORTUI|capture|%s\n",tag);fflush(stdout);
}
static void click(DMS *s,const DMS_MapImportState *st,int item) {
    int x,y,w,h;
    if(dms_enhanced_menu(s)){if(!em_item_rect(item,&x,&y,&w,&h))return;x+=w/2;y+=h/2;x=int(x/s->px);y=int(y/s->py);}
    else {if(!mi_item_rect(st,item,&x,&y,&w,&h))return;x=int((s->vpx+(x+w/2)*s->scale)/s->px);y=int((s->vpy+(y+h/2)*s->scale)/s->py);}
    SDL_Event e={};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;e.button.clicks=1;SDL_PushEvent(&e);e.type=SDL_MOUSEBUTTONUP;SDL_PushEvent(&e);
}
}

extern "C" void dms_import_probe(const char *path,const char *shotdir,int run) {
    probe_enabled=path!=NULL;probe_source=path?path:"";probe_shots=shotdir?shotdir:"";probe_run=run;
}

extern "C" int dms_import_map(DMS *shell,const DMS_MapImportHooks *hooks) {
    if(!shell||!hooks||!hooks->inspect||!hooks->run)return DMS_CANCEL;
    DMS_MapImportState st={};st.selected_row=-1;st.pressed=MI_NONE;
    std::vector<Entry> entries;int selected=-1,pending=0,result=DMS_CANCEL,probe_stage=0;
    std::string selected_path;Job job;job.mutex=SDL_CreateMutex();job.hooks=hooks;if(!job.mutex)return DMS_CANCEL;
    auto browse=[&](const std::string &path){
        if(path.size()>=sizeof st.path){copy(st.message,sizeof st.message,"This folder path is too long.");return false;}
        if(!scan(path,entries)){copy(st.message,sizeof st.message,"This folder cannot be opened. Check the path and folder permissions.");return false;}
        copy(st.path,sizeof st.path,path);copy(st.location,sizeof st.location,path);remembered_path=path;
        selected=-1;selected_path.clear();st.top=0;st.ready=st.completed=0;st.focus=0;st.selected_file[0]=0;st.info={};
        copy(st.message,sizeof st.message,"Choose a C&C/Red Alert map or ZIP/RAR/7Z bundle. Folders open with Enter or a double-click.");rows(st,entries,selected);return true;
    };
    std::string initial=remembered_path.empty()?home():remembered_path;
    if(probe_enabled&&!probe_source.empty()){probe_source=absolute(probe_source,initial);initial=parent(probe_source);}
    if(!browse(initial))browse(home());
    auto start=[&](int importing){
        if(job.thread)return;
        job.importing=importing;job.done=0;job.result=0;job.progress=0;job.path=selected_path;job.info=st.info;job.message[0]=0;
        st.busy=1;st.importing=importing;st.progress=0;st.focus=0;
        copy(st.message,sizeof st.message,importing?"Preparing map import...":"Reading map and building preview...");
        job.thread=SDL_CreateThread(work,importing?"map-import":"map-inspect",&job);
        if(!job.thread){st.busy=st.importing=0;copy(st.message,sizeof st.message,"The map worker could not start. Please try again.");}
    };
    auto select=[&](int index){
        if(index<0||index>=(int)entries.size())return;
        selected=index;st.ready=st.completed=0;st.info={};selected_path=entries[index].path;
        copy(st.selected_file,sizeof st.selected_file,entries[index].name);
        rows(st,entries,selected);if(!entries[index].directory)start(0);
    };
    shell->importer=&st;SDL_ShowCursor(SDL_DISABLE);SDL_StartTextInput();frame(shell);
    unsigned probe_started=SDL_GetTicks();
    while(!pending||job.thread) {
        if(job.thread){
            SDL_LockMutex(job.mutex);int done=job.done;st.progress=job.progress;
            if(job.message[0])copy(st.message,sizeof st.message,job.message);SDL_UnlockMutex(job.mutex);
            if(done){SDL_WaitThread(job.thread,NULL);job.thread=nullptr;st.busy=0;st.info=job.info;
                st.ready=job.result>0&&!job.importing;st.completed=job.result>0&&job.importing;
                copy(st.message,sizeof st.message,st.info.message[0]?st.info.message:job.result>0?(job.importing?"Import complete.":"Map is ready to import."):"This map could not be imported.");
                if(st.completed)st.progress=100;
                printf("MAPIMPORTUI|%s|%s|%s\n",job.importing?"import":"inspect",job.result>0?"PASS":"FAIL",st.message);fflush(stdout);
            }
        }
        rows(st,entries,selected);frame(shell);
        if(probe_enabled) {
            if(probe_stage==0){capture(shell,"browser");if(probe_source.empty()){pending=1;result=DMS_CANCEL;}else {
                int index=-1;for(size_t i=0;i<entries.size();i++)if(entries[i].path==probe_source){index=(int)i;break;}
                if(index<0){copy(st.message,sizeof st.message,"Probe source is not visible in the browser.");pending=1;}
                else {st.top=index/MI_VISIBLE_ROWS*MI_VISIBLE_ROWS;rows(st,entries,selected);frame(shell);click(shell,&st,MI_ROW_BASE+index-st.top);probe_stage=1;}
            }}else if(probe_stage==1&&!st.busy){capture(shell,"preview");if(st.ready&&probe_run){click(shell,&st,MI_IMPORT);probe_stage=2;}else {pending=1;result=st.ready?1:DMS_CANCEL;}}
            else if(probe_stage==2&&st.importing){capture(shell,"progress");probe_stage=3;}
            else if(probe_stage==3&&!st.busy){capture(shell,st.completed?"complete":"error");pending=1;result=st.completed?1:DMS_CANCEL;}
            if(SDL_GetTicks()-probe_started>120000&&!pending){pending=1;result=DMS_CANCEL;copy(st.message,sizeof st.message,"The hidden import probe timed out; waiting for its worker to finish.");}
        }
        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            if(dms_enhanced_menu(shell))em_sound_event(&e,shell->win);
            if(fs_handle_event(&e))continue;
            int action=MI_NONE;
            if(e.type==SDL_QUIT){pending=1;result=DMS_QUIT;}
            if(e.type==SDL_KEYDOWN){
                SDL_Keycode key=e.key.keysym.sym;
                if(key==SDLK_ESCAPE){if(st.focus){st.focus=0;copy(st.location,sizeof st.location,st.path);}else {pending=1;result=st.completed?1:DMS_CANCEL;}}
                else if(!st.busy&&!st.completed){
                    if((e.key.keysym.mod&(KMOD_CTRL|KMOD_GUI))&&key==SDLK_l){st.focus=1;st.location[0]=0;}
                    else if(st.focus){
                        if(key==SDLK_RETURN||key==SDLK_KP_ENTER)action=MI_GO;
                        else if(key==SDLK_BACKSPACE){size_t n=strlen(st.location);if(n){do{--n;}while(n&&((unsigned char)st.location[n]&0xc0)==0x80);st.location[n]=0;}}
                    }else if(key==SDLK_i&&st.ready)action=MI_IMPORT;
                    else if(key==SDLK_BACKSPACE||(key==SDLK_UP&&(e.key.keysym.mod&KMOD_ALT)))action=MI_PARENT;
                    else if(key==SDLK_RETURN||key==SDLK_KP_ENTER){if(selected>=0&&entries[selected].directory)browse(entries[selected].path);else if(st.ready)action=MI_IMPORT;}
                    else if(key==SDLK_UP||key==SDLK_DOWN||key==SDLK_HOME||key==SDLK_END||key==SDLK_PAGEUP||key==SDLK_PAGEDOWN){
                        int index=selected<0?0:selected;
                        if(key==SDLK_UP)--index;if(key==SDLK_DOWN)++index;if(key==SDLK_HOME)index=0;if(key==SDLK_END)index=(int)entries.size()-1;if(key==SDLK_PAGEUP)index-=MI_VISIBLE_ROWS;if(key==SDLK_PAGEDOWN)index+=MI_VISIBLE_ROWS;
                        index=std::max(0,std::min(index,(int)entries.size()-1));if(index<st.top)st.top=index;if(index>=st.top+MI_VISIBLE_ROWS)st.top=index-MI_VISIBLE_ROWS+1;select(index);
                    }
                }else if(st.completed&&(key==SDLK_RETURN||key==SDLK_KP_ENTER)){pending=1;result=1;}
            }else if(e.type==SDL_TEXTINPUT&&st.focus&&!st.busy&&!st.completed){size_t n=strlen(st.location),add=strlen(e.text.text);if(n+add<sizeof st.location)memcpy(st.location+n,e.text.text,add+1);}
            else if(e.type==SDL_MOUSEWHEEL&&!st.busy&&!st.completed){st.top-=e.wheel.y*3;}
            else if(e.type==SDL_MOUSEMOTION&&!dms_enhanced_menu(shell)){shell->mx=(int)((e.motion.x*shell->px-shell->vpx)/shell->scale);shell->my=(int)((e.motion.y*shell->py-shell->vpy)/shell->scale);}
            else if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT){int hit=event_hit(shell,&st,e.button.x,e.button.y);if(!disabled(&st,hit)){st.pressed=hit;if(hit>=MI_ROW_BASE&&hit<MI_ROW_BASE+st.row_count){int index=st.top+hit-MI_ROW_BASE;if(entries[index].directory&&e.button.clicks>=2)browse(entries[index].path);else select(index);st.pressed=MI_NONE;}}}
            else if(e.type==SDL_MOUSEBUTTONUP&&e.button.button==SDL_BUTTON_LEFT){int hit=event_hit(shell,&st,e.button.x,e.button.y);if(hit==st.pressed&&!disabled(&st,hit))action=hit;st.pressed=MI_NONE;}
            if(action==MI_BACK){pending=1;result=st.completed?1:DMS_CANCEL;}
            else if(action==MI_HOME)browse(home());
            else if(action==MI_PARENT)browse(parent(st.path));
            else if(action==MI_LOCATION){st.focus=1;}
            else if(action==MI_GO){std::string path=absolute(st.location,st.path);if(directory(path))browse(path);else if(map_extension(path)){std::string dir=parent(path);if(browse(dir)){for(size_t i=0;i<entries.size();i++)if(entries[i].path==path){st.top=(int)i;select((int)i);break;}}}else copy(st.message,sizeof st.message,"Enter a folder or an original .ini, .bin, .mpr or .map file.");}
            else if(action==MI_IMPORT&&st.ready)start(1);
            else if(action==MI_PREVIOUS)st.top-=MI_VISIBLE_ROWS;
            else if(action==MI_NEXT)st.top+=MI_VISIBLE_ROWS;
        }
        if(pending&&job.thread)copy(st.message,sizeof st.message,"Finishing the current map operation before closing...");
        dms_pace(shell);
    }
    SDL_StopTextInput();shell->importer=nullptr;SDL_DestroyMutex(job.mutex);dms_redraw(shell);return result;
}

extern "C" int mi_item_rect(const DMS_MapImportState *st,int item,int*x,int*y,int*w,int*h) {
    *h=10;
    switch(item){
    case MI_HOME:*x=22;*y=25;*w=40;break;
    case MI_PARENT:*x=66;*y=25;*w=40;break;
    case MI_LOCATION:*x=22;*y=40;*w=228;break;
    case MI_GO:*x=258;*y=40;*w=40;break;
    case MI_BACK:*x=22;*y=176;*w=62;break;
    case MI_IMPORT:*x=236;*y=176;*w=62;break;
    case MI_PREVIOUS:*x=22;*y=151;*w=56;break;
    case MI_NEXT:*x=126;*y=151;*w=56;break;
    default:if(item<MI_ROW_BASE||item>=MI_ROW_BASE+st->row_count)return 0;*x=24;*y=62+(item-MI_ROW_BASE)*9;*w=156;*h=9;break;
    }return 1;
}
extern "C" int mi_hit_test(const DMS_MapImportState *st,int x,int y) {
    for(int item:{MI_HOME,MI_PARENT,MI_LOCATION,MI_GO,MI_BACK,MI_IMPORT,MI_PREVIOUS,MI_NEXT}) {int bx,by,w,h;if(mi_item_rect(st,item,&bx,&by,&w,&h)&&inside(x,y,bx,by,w,h)&&!disabled(st,item))return item;}
    for(int i=0;i<st->row_count;i++){int bx,by,w,h;mi_item_rect(st,MI_ROW_BASE+i,&bx,&by,&w,&h);if(inside(x,y,bx,by,w,h)&&!disabled(st,MI_ROW_BASE+i))return MI_ROW_BASE+i;}return MI_NONE;
}
extern "C" void mi_draw(DB_Surface *s,const DB_Pack *p,const DMS_MapImportState *st) {
    const DB_Font *font=db_font(p,"GRAD6FNT");if(!font)return;
    unsigned char normal[16],bright[16],dim[16];db_font_palette_grad(normal,DM_TEXT_MEDIUM,DB_TBLACK);db_font_palette_grad(bright,DM_TEXT_BRIGHT,DB_TBLACK);db_font_palette_grad(dim,DM_TEXT_DISABLED,DB_TBLACK);
    auto print=[&](std::string t,int x,int y,int width,const unsigned char *pal){while(!t.empty()&&db_string_width(font,t.c_str(),DB_FONT6_XSPACING)>width)t.pop_back();db_print(s,font,t.c_str(),x,y,pal,DB_FONT6_XSPACING);};
    auto button=[&](int item,const char*label){int x,y,w,h;mi_item_rect(st,item,&x,&y,&w,&h);dm_green_button(s,x,y,w,h,st->pressed==item,0);print(label,x+4,y+1,w-8,disabled(st,item)?dim:normal);};
    dm_green_dialog(s,16,6,288,188);print(st->completed?"MAP IMPORTED":"IMPORT MAP",128,14,160,bright);
    button(MI_HOME,"Home");button(MI_PARENT,"Up");print("MAPS / ZIP / RAR / 7Z",114,27,182,dim);button(MI_GO,"Go");
    dm_green_dialog(s,21,39,230,12);print(st->location,24,42,222,st->focus?bright:normal);
    dm_green_dialog(s,21,59,162,87);
    for(int i=0;i<st->row_count;i++){if(i==st->selected_row)db_fill_rect(s,24,62+i*9,179,70+i*9,DM_GREEN_BKGD);print(std::string(st->row_directory[i]?"> ":"  ")+st->rows[i],25,63+i*9,153,i==st->selected_row?bright:normal);}
    if(!st->row_count)print("No maps in this folder",25,64,153,dim);
    button(MI_PREVIOUS,"Previous");button(MI_NEXT,"Next");
    dm_green_dialog(s,190,59,108,87);
    if(st->info.preview_w>0&&st->info.preview_h>0&&st->info.preview_w<=128&&st->info.preview_h<=128){
        /* Quantize only for Classic's own palette; Enhanced displays the original RGBA. */
        static unsigned char lut[4096];static const DB_Pack *last=nullptr;
        if(last!=p){for(int c=0;c<4096;c++){int best=0,dist=0x7fffffff;for(int n=0;n<256;n++){int r=((c>>8)&15)*17-p->pal8[n*3],g=((c>>4)&15)*17-p->pal8[n*3+1],b=(c&15)*17-p->pal8[n*3+2];int d=r*r+g*g+b*b;if(d<dist){dist=d;best=n;}}lut[c]=(unsigned char)best;}last=p;}
        const int pw=st->info.preview_w,ph=st->info.preview_h;float scale=std::min(96.f/pw,54.f/ph);int w=std::max(1,int(pw*scale)),h=std::max(1,int(ph*scale));int x=196+(96-w)/2,y=63+(54-h)/2;
        for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){const unsigned char *c=&st->info.preview[((yy*ph/h)*pw+xx*pw/w)*4];db_fill_rect(s,x+xx,y+yy,x+xx,y+yy,lut[(c[0]>>4)*256+(c[1]>>4)*16+(c[2]>>4)]);}
    }else print(st->busy?"Reading map...":"Map preview",199,82,91,dim);
    print(st->info.game,195,120,99,normal);print(st->info.kind,195,130,99,normal);char detail[100];std::snprintf(detail,sizeof detail,"%dx%d  %d starts",st->info.width,st->info.height,st->info.starts);if(st->info.width)print(detail,192,150,106,dim);
    if(st->importing||st->completed){db_fill_rect(s,22,164,297,170,DB_TBLACK);db_fill_rect(s,23,165,23+273*std::max(0,std::min(100,st->progress))/100,169,DM_GREEN_BKGD);}
    else print(st->message,22,164,276,dim);
    button(MI_BACK,st->completed?"Done":"Back");if(!st->completed)button(MI_IMPORT,"Import");
}
