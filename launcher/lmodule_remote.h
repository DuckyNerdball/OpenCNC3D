/* Public modules protocol, implemented with lmodule.c so all launchers share it.
 * Catalogs and ZIPs are remote data. A catalog can select only a named release
 * asset; its file manifest can select only immutable runtime paths below. */
#include "lcfg.h"
#include "ljson.h"
#include "lnet.h"
#include <ctype.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define LMOD_PLATFORM "windows"
#define LMOD_MKDIR(p) _mkdir(p)
#define LMOD_RMDIR(p) _rmdir(p)
#else
#include <unistd.h>
#define LMOD_PLATFORM "macos"
#define LMOD_MKDIR(p) mkdir(p,0755)
#define LMOD_RMDIR(p) rmdir(p)
#endif
#define LMOD_CAP (4*1024*1024)
#define LMOD_FILES 20000

static char *lmod_line(char *text,char **state)
{
    char *p=text?text:*state,*end;if(!p||!*p)return NULL;end=strchr(p,'\n');
    if(end){*end=0;*state=end+1;}else *state=p+strlen(p);return p;
}
static int lmod_hex(const char *p)
{ int i;if(strlen(p)!=64)return 0;for(i=0;i<64;i++)if(!strchr("0123456789abcdef",p[i]))return 0;return 1; }
static int lmod_equal(const char *a,const char *b)
{ while(*a&&*b){if(tolower((unsigned char)*a++)!=tolower((unsigned char)*b++))return 0;}return !*a&&!*b; }
static const char *lmod_version(const char *s){return s&&s[0]=='v'?s+1:s;}
static const LJ_Value *lmod_release(const LJ_Value *root,const char *build)
{
    const LJ_Value *r=lj_get(root,"latest"),*p;const char *tag=lj_str(r,"tag");
    if(tag&&!strcmp(lmod_version(tag),lmod_version(build)))return r;
    for(p=lj_first(lj_get(root,"releases"));p;p=lj_next(p)){
        tag=lj_str(p,"tag");if(tag&&!strcmp(lmod_version(tag),lmod_version(build)))return p;
    }
    return NULL;
}
static const LJ_Value *lmod_asset(const LJ_Value *release,const char *name)
{
    const LJ_Value *a,*found=NULL;
    for(a=lj_first(lj_get(release,"assets"));a;a=lj_next(a)){
        const char *n=lj_str(a,"name");if(n&&!strcmp(n,name)){if(found)return NULL;found=a;}
    }
    return found;
}
/* Approved feed entries only. A newer preview or another platform's catalog
 * cannot hide the most recent complete module release for this platform. */
static int lmod_stable_version(const char *s,int out[3])
{
    int i;const char *p=lmod_version(s);if(!p)return 0;
    for(i=0;i<3;i++){
        int n=0,digits=0;
        while(*p>='0'&&*p<='9'){if(n>1000000)return 0;n=n*10+*p++-'0';digits++;}
        if(!digits)return 0;out[i]=n;
        if(i<2){if(*p++!='.')return 0;}else if(*p)return 0;
    }
    return 1;
}
static int lmod_version_order(const char *a,const char *b)
{
    int x[3],y[3],i;
    if(!lmod_stable_version(a,x)||!lmod_stable_version(b,y))return 0;
    for(i=0;i<3;i++)if(x[i]!=y[i])return x[i]>y[i]?1:-1;
    return 0;
}
static int lmod_release_has_modules(const LJ_Value *release)
{
    const char *tag=lj_str(release,"tag");const LJ_Value *asset;char name[256];int v[3],i;
    const char *channel=lj_str(release,"channel");
    if(!lmod_stable_version(tag,v)||lj_bool(release,"draft",0))return 0;
    /* The production feed explicitly approves numeric Alpha releases on its
     * experimental channel, even when GitHub marks them prerelease. */
    if(lj_bool(release,"prerelease",0)&&(!channel||strcmp(channel,"experimental")))return 0;
    snprintf(name,sizeof name,"OpenCNC3D-v%s-modules.txt",lmod_version(tag));
    asset=lmod_asset(release,name);if(lj_num(asset,"id",0)<=0||lj_num(asset,"size",0)<=0)return 0;
    for(i=0;i<2;i++){
        snprintf(name,sizeof name,"OpenCNC3D-module-%s-%s-v%s.zip",i?"ra":"td",LMOD_PLATFORM,lmod_version(tag));
        asset=lmod_asset(release,name);if(lj_num(asset,"id",0)>0&&lj_num(asset,"size",0)>0)return 1;
    }
    return 0;
}
static const LJ_Value *lmod_latest_release(const LJ_Value *root)
{
    const LJ_Value *best=NULL,*p=lj_get(root,"latest");
    if(lmod_release_has_modules(p))best=p;
    for(p=lj_first(lj_get(root,"releases"));p;p=lj_next(p))
        if(lmod_release_has_modules(p)&&(!best||lmod_version_order(lj_str(p,"tag"),lj_str(best,"tag"))>0))best=p;
    return best;
}
int lmod_catalog_read(const char *build,const char *json,const char *catalog,LMOD_Catalog *out,char *err,int len)
{
    LJ_Value *root;const LJ_Value *release,*asset;char *copy,*line,*save=NULL;
    LMOD_Catalog result;int header=0,version=0,count=0;
    memset(&result,0,sizeof result);memset(out,0,sizeof *out);
    if(!build||!json||!catalog||strlen(catalog)>=LMOD_CAP)goto bad;
    root=lj_parse(json,err,len);if(!root)return 0;
    if(!lj_bool(root,"ok",1)){lj_free(root);goto bad;}
    release=lmod_release(root,build);if(!release){lj_free(root);goto bad;}
    copy=strdup(catalog);if(!copy){lj_free(root);goto bad;}
    for(line=lmod_line(copy,&save);line;line=lmod_line(NULL,&save)){
        char key[32],id[16],platform[24],name[256],sha[80],extra;long long size;int target;
        while(*line==' '||*line=='\t')line++;
        if(!*line||*line=='#'||*line=='\r')continue;
        if(sscanf(line,"modules %d %c",&target,&extra)==1){if(header++||target!=1)goto parsebad;continue;}
        if(sscanf(line,"version %99s %c",result.version,&extra)==1){if(version++||strcmp(lmod_version(build),result.version))goto parsebad;continue;}
        if(sscanf(line,"%31s %15s %23s %255s %lld %79s %c",key,id,platform,name,&size,sha,&extra)!=6||strcmp(key,"module"))goto parsebad;
        if(strcmp(id,"ra")&&strcmp(id,"td"))goto parsebad;
        if(strcmp(platform,"macos")&&strcmp(platform,"windows"))goto parsebad;
        if(strcmp(platform,LMOD_PLATFORM))continue;
        target=!strcmp(id,"ra");
        {char expected[256];snprintf(expected,sizeof expected,"OpenCNC3D-module-%s-%s-v%s.zip",id,platform,lmod_version(build));if(strcmp(name,expected))goto parsebad;}
        asset=lmod_asset(release,name);
        if(result.module[target].available||!lmod_hex(sha)||size<=0||size>4000000000LL||!asset||
           lj_num(asset,"id",0)<=0||lj_num(asset,"size",-1)!=size)goto parsebad;
        result.module[target].available=1;result.module[target].id=lj_num(asset,"id",0);
        result.module[target].size=size;snprintf(result.module[target].name,256,"%s",name);
        snprintf(result.module[target].sha,65,"%s",sha);count++;
    }
    free(copy);lj_free(root);
    if(header!=1||version!=1||!count)goto bad;
    *out=result;return 1;
parsebad:free(copy);lj_free(root);
bad:snprintf(err,(size_t)len,"No complete, verified module catalog matches the selected release.");return 0;
}
static int lmod_site_ok(const char *site)
{ return site&&(!strncmp(site,"https://",8)||!strncmp(site,"http://127.0.0.1:",17)||!strncmp(site,"http://localhost:",17)); }
static void lmod_asset_url(const char *site,long long id,char *out,size_t n)
{snprintf(out,n,"%s/api/download?asset=%lld",site,id);}
int lmod_catalog_fetch_at(const char *site,const char *build,LMOD_Catalog *out,char *err,int len)
{
    char url[1400],name[256];char *json=NULL,*text=NULL;LJ_Value *root=NULL;
    const LJ_Value *release,*asset;long long id,size;int ok=0;
    memset(out,0,sizeof *out);err[0]=0;if(!lmod_site_ok(site))goto done;
    snprintf(url,sizeof url,"%s/api/builds",site);json=ln_get_text(url,LCFG_KEY,LMOD_CAP,err,len);
    if(!json||strlen(json)>=LMOD_CAP)goto done;
    root=lj_parse(json,err,len);if(!root||!lj_bool(root,"ok",1))goto done;
    release=build&&*build?lmod_release(root,build):lmod_latest_release(root);
    if(!release){snprintf(err,(size_t)len,"No compatible public module release is available yet.");goto done;}
    build=lmod_version(lj_str(release,"tag"));
    snprintf(name,sizeof name,"OpenCNC3D-v%s-modules.txt",lmod_version(build));asset=lmod_asset(release,name);
    id=lj_num(asset,"id",0);size=lj_num(asset,"size",-1);
    if(id<=0||size<=0||size>=LMOD_CAP){snprintf(err,(size_t)len,"This build has no public module catalog yet.");goto done;}
    lmod_asset_url(site,id,url,sizeof url);text=ln_get_text(url,LCFG_KEY,LMOD_CAP,err,len);
    if(!text||strlen(text)!=(size_t)size)goto done;
    ok=lmod_catalog_read(build,json,text,out,err,len);if(ok)snprintf(out->site,sizeof out->site,"%s",site);
done:if(!ok&&!err[0])snprintf(err,(size_t)len,"The public module catalog could not be checked. Reopen the browser to retry.");
    lj_free(root);free(json);free(text);return ok;
}
int lmod_catalog_fetch(const char *build,LMOD_Catalog *out,char *err,int len)
{return lmod_catalog_fetch_at(LCFG_BASE[0]?LCFG_BASE:"https://opencnc3d.com",build,out,err,len);}

/* These names describe immutable shipped runtime files. The signed-by-release
 * list is still exact, and neither it nor an archive may name player data. */
static int lmod_runtime_name(int target,const char *name)
{
    static const char *const dirs[]={"content/","missions/","dosdata/","models3d/","n64fix/","ebld/","eplc/","evhc/","einf/","ecur/","vfx3d/","cinematic/","foley/","ambience/","newaudio/","desert-details/","stone-materials/",NULL};
    static const char *const ra_dirs[]={"data/","audio/","missions/","einf/","3d/","previews/","ebld/","eplc/","evhc/","efx/","ecur/","ore/",NULL};
    static const char *const ra_packs[]={"clearblend_RA_SNOW.pack","groundmask_RA_SNOW.pack","waterbase_RA_TEMPERAT.pack",
        "terrainmaterial_RA_SNOW.pack","terrainmaterial_RA_TEMPERAT.pack","terrainsurface_RA_SNOW.pack","terrainsurface_RA_TEMPERAT.pack",
        "terrainenh_RA_SNOW.pack","terrainenh_RA_TEMPERAT.pack","terrainseam_RA_SNOW.pack","terrainseam_RA_TEMPERAT.pack",NULL};
    static const char *const ra_local_packs[]={"terrainenh_RA_SNOW.pack","terrainenh_RA_TEMPERAT.pack",
        "terrainseam_RA_SNOW.pack","terrainseam_RA_TEMPERAT.pack",NULL};
    static const char *const root[]={"opencnc3d","opencnc3d-game.exe","cnc_eyes","cnc_eyes.exe","TiberianDawn.dylib","TiberianDawn.dll","TiberianDawnXL.dylib","TiberianDawnXL.dll","RedAlert.dylib","RedAlert.dll","SDL2.dll","libSDL2-2.0.0.dylib","libSDL3.dylib","CONQUER.MIX","temperate_grass_rgb.bin","netcheck","netcheck.exe","OpenCNC3D-Diagnostics.exe","skirmish-maps.txt","LICENSE","NOTICE.md",NULL};
    const char *p=name,*base,*dot;int i,ra_local=0;char lower[1024],*part,*slash;
    static const char *const protected_parts[]={"movies","music","saves","savegames","settings","user_maps","logs","shots","screenshots",".git",NULL};
    if(!name||strlen(name)>900||!lz_name_safe(name)||strchr(name,'\\')||name[0]=='.')return 0;
    if(target&&!strncmp(name,"ra3d/",5)){p=name+5;ra_local=1;}
    else if(!strncmp(name,"ra3d/",5))return 0;
    for(base=p;*base;base++)if(isspace((unsigned char)*base)||*base==':'||(unsigned char)*base>=128)return 0;
    for(i=0;p[i];i++)lower[i]=(char)tolower((unsigned char)p[i]);
    lower[i]=0;
    for(part=lower;part&&*part;part=slash?slash+1:NULL){
        slash=strchr(part,'/');if(slash)*slash=0;
        if(part[0]=='.')return 0;
        for(i=0;protected_parts[i];i++)if(!strcmp(part,protected_parts[i]))return 0;
        if(slash)*slash='/';
    }
    base=strrchr(p,'/');base=base?base+1:p;dot=strrchr(base,'.');
    if(!*base||base[0]=='.'||strstr(lower,"save")||strstr(lower,"user_maps")||strstr(lower,".opencnc3d")||
       (dot&&(lmod_equal(dot,".cfg")||lmod_equal(dot,".sav")||lmod_equal(dot,".vqa")||lmod_equal(dot,".log")||lmod_equal(dot,".ocpart")))||
       (lmod_equal(base,"SCORES.MIX")||lmod_equal(base,"TRANSIT.MIX")))return 0;
    if(!strcmp(name,"CHANGELOG.txt"))return 1;
    if(target&&!ra_local){
        if(!strchr(p,'/')){for(i=0;ra_packs[i];i++)if(!strcmp(p,ra_packs[i]))return 1;}
        if(!strchr(p,'/'))return dot&&!strcmp(dot,".pack")&&strncmp(p,"SC",2)&&strncmp(p,"sidebar",7)&&strcmp(p,"campaign.pack")&&strcmp(p,"dosclassic.pack")&&!strstr(p,"_RA_");
        if(!strncmp(p,"missions/",9)||!strncmp(p,"content/",8)||!strncmp(p,"dosdata/",8))return 0;
        for(i=0;dirs[i];i++)if(!strncmp(p,dirs[i],strlen(dirs[i])))return 1;
        return 0;
    }
    if(ra_local&&(!strcmp(p,"expansions.txt")||!strcmp(p,"ants.txt")||!strcmp(p,"expansions-manifest.json")))return 1;
    if(!strchr(p,'/')){
        for(i=0;root[i];i++)if(!strcmp(p,root[i]))return target?strncmp(p,"TiberianDawn",12)!=0:strncmp(p,"RedAlert",8)!=0;
        if(ra_local)for(i=0;ra_local_packs[i];i++)if(!strcmp(p,ra_local_packs[i]))return 1;
        return dot&&!strcmp(dot,".pack")&&(!strstr(p,"_RA_")||!strcmp(p,"waterbase_RA_TEMPERAT.pack"));
    }
    for(i=0;(ra_local?ra_dirs:dirs)[i];i++)if(!strncmp(p,(ra_local?ra_dirs:dirs)[i],strlen((ra_local?ra_dirs:dirs)[i])))return 1;
    return 0;
}
typedef struct {char **owned;int count;} LMOD_Files;
static void lmod_files_free(LMOD_Files *f){int i;for(i=0;i<f->count;i++)free(f->owned[i]);free(f->owned);memset(f,0,sizeof *f);}
static int lmod_files_parse(const char *text,int target,const char *build,LMOD_Files *out,char *err,int len)
{
    char *copy,*line,*save=NULL,manifest[80];int header=0,module=0,version=0,platform=0;
    memset(out,0,sizeof *out);if(!text||strlen(text)>=LMOD_CAP)goto bad;
    out->owned=(char **)calloc(LMOD_FILES+2,sizeof(char *));copy=strdup(text);
    if(!out->owned||!copy){free(copy);goto bad;}
    for(line=lmod_line(copy,&save);line;line=lmod_line(NULL,&save)){
        char key[32],value[100],sha[80],name[1024],extra;long long size;int fmt,i;
        if(!*line||*line=='#'||*line=='\r')continue;
        if(sscanf(line,"module-files %d %c",&fmt,&extra)==1){if(header++||fmt!=1)goto parsebad;continue;}
        if(sscanf(line,"%31s %99s %c",key,value,&extra)==2){
            if(!strcmp(key,"module")){if(module++||strcmp(value,target?"ra":"td"))goto parsebad;}
            else if(!strcmp(key,"version")){if(version++||(build&&strcmp(lmod_version(build),value)))goto parsebad;}
            else if(!strcmp(key,"platform")){if(platform++||strcmp(value,LMOD_PLATFORM))goto parsebad;}
            else goto parsebad;
            continue;
        }
        if(sscanf(line,"file %79s %lld %1023s %c",sha,&size,name,&extra)!=3||!lmod_hex(sha)||size<0||
           size>4000000000LL||!lmod_runtime_name(target,name)||out->count>=LMOD_FILES)goto parsebad;
        for(i=0;i<out->count;i++)if(lmod_equal(name,out->owned[i]))goto parsebad;
        out->owned[out->count]=strdup(name);if(!out->owned[out->count])goto parsebad;out->count++;
    }
    free(copy);if(header!=1||module!=1||version!=1||platform!=1||!out->count)goto bad;
    snprintf(manifest,sizeof manifest,"opencnc3d-module-%s.txt",target?"ra":"td");
    out->owned[out->count]=strdup(manifest);if(!out->owned[out->count])goto bad;out->count++;return 1;
parsebad:free(copy);
bad:lmod_files_free(out);snprintf(err,(size_t)len,"The module file manifest is invalid or names protected player data.");return 0;
}
static int lmod_install_public(const char *dir,int target,const char *build,const char *zip,
                                int (*cb)(void *,int,int),void *user,char *err,int len)
{
    char manifest[80],record[512];char *text;LMOD_Files files;const char *last[2];LZ_File extra[2];int ok;
#ifdef _WIN32
    static const char *const td_required[]={"TiberianDawn.dll","opencnc3d-game.exe","CONQUER.MIX"};
#else
    static const char *const td_required[]={"TiberianDawn.dylib","opencnc3d","CONQUER.MIX"};
#endif
    if(lz_is_private_developer(dir)){snprintf(err,(size_t)len,"This private source build is updated from its source checkout, not public game packages.");return 0;}
    if(!lmod_settle(dir,err,len))return 0;
    snprintf(manifest,sizeof manifest,"opencnc3d-module-%s.txt",target?"ra":"td");
    text=lz_read_text(zip,manifest,LMOD_CAP,err,len);if(!text)return 0;
    ok=lmod_files_parse(text,target,build,&files,err,len);free(text);if(!ok)return 0;
    last[0]=manifest;last[1]=NULL;memset(extra,0,sizeof extra);
    /* Bootstrap install record is committed after every module byte, under the
     * same lock and rollback journal. Existing install/settings records stay. */
    if(!lz_is_install(dir)){
        snprintf(record,sizeof record,"version %s\nplatform %s\nmodule_install 1\n",lmod_version(build),LMOD_PLATFORM);
        extra[0].name="opencnc3d-install.txt";extra[0].bytes=record;extra[0].len=strlen(record);
    }
    {int i,j,n=target?lmod_ra_nneeds:3;const char *const *needs=target?lmod_ra_needs:td_required;
     for(i=0;i<n;i++){for(j=0;j<files.count;j++)if(!strcmp(needs[i],files.owned[j]))break;
         if(j==files.count){snprintf(err,(size_t)len,"The public module manifest is missing %.120s",needs[i]);lmod_files_free(&files);return 0;}}
    }
    /* Every listed file must exist in the archive: a phantom ownership entry
     * must never become permission to remove a player's existing file later. */
    ok=lz_install_owned_extra(dir,zip,(const char *const *)files.owned,
                              (const char *const *)files.owned,files.count,last,extra,cb,user,err,len);
#ifdef _WIN32
    /* A module of 64-bit programs over an install that held 32-bit ones leaves their
     * SDL2.dll behind, since unpacking only writes; lsdl_tidy.h says when it may go. */
    if(ok)lsdl_tidy(dir);
#endif
    lmod_files_free(&files);return ok;
}
static int lmod_removable(int target,const char *name)
{
    int i;if(!strncmp(name,"opencnc3d-module-",17))return 1;
    if(target)return !strncmp(name,"ra3d/",5);
    if(!strcmp(name,"TiberianDawnXL.dylib")||!strcmp(name,"TiberianDawnXL.dll"))return 1;
    if(!strncmp(name,"content/",8)||!strncmp(name,"missions/",9)||!strncmp(name,"dosdata/",8))return 1;
    if(!strcmp(name,"opencnc3d")||!strcmp(name,"opencnc3d-game.exe")||!strcmp(name,"cnc_eyes")||!strcmp(name,"cnc_eyes.exe")||
       !strcmp(name,"netcheck")||!strcmp(name,"netcheck.exe")||!strcmp(name,"OpenCNC3D-Diagnostics.exe"))return 1;
    for(i=0;lmod_td_files[i];i++)if(!strcmp(name,lmod_td_files[i]))return 1;
    return 0;
}
/* A module's ownership list as the folder holds it: the copy a removal has staged
 * aside first, then the live one. 1 parsed into `out`, 0 when the folder has neither
 * (an install older than the lists, which the legacy names stand for), -1 when one is
 * there and cannot be read or trusted. */
static int lmod_manifest_read(const char *dir,int target,LMOD_Files *out)
{
    char rel[100],path[1400],err[256];char *text;FILE *f;long n;int ok;
    memset(out,0,sizeof *out);
    snprintf(rel,sizeof rel,"opencnc3d-module-%s.txt.module-removing",target?"ra":"td");
    if(!lz_inside(dir,rel,path,sizeof path)||lz_is_symlink(path))return -1;
    f=fopen(path,"rb");if(!f){
        snprintf(rel,sizeof rel,"opencnc3d-module-%s.txt",target?"ra":"td");
        if(!lz_inside(dir,rel,path,sizeof path)||lz_is_symlink(path))return -1;
        f=fopen(path,"rb");
    }
    if(!f)return 0;
    if(fseek(f,0,SEEK_END)||(n=ftell(f))<=0||n>=LMOD_CAP||fseek(f,0,SEEK_SET)){fclose(f);return -1;}
    text=(char *)malloc((size_t)n+1);if(!text){fclose(f);return -1;}
    if(fread(text,1,(size_t)n,f)!=(size_t)n||memchr(text,0,(size_t)n)){fclose(f);free(text);return -1;}fclose(f);text[n]=0;
    ok=lmod_files_parse(text,target,NULL,out,err,sizeof err);free(text);
    return ok?1:-1;
}
static const char *const *lmod_owned_files(const char *dir,int target)
{
    static LMOD_Files memory[2];int rc;
    lmod_files_free(&memory[target]);
    rc=lmod_manifest_read(dir,target,&memory[target]);
    if(rc<0)return NULL;
    if(rc==0)return target?lmod_ra_files:lmod_td_files;
    {int i,nout=0;LMOD_Files *m=&memory[target];for(i=0;i<m->count;i++){
        if(lmod_removable(target,m->owned[i]))m->owned[nout++]=m->owned[i];else free(m->owned[i]);
    }m->count=nout;m->owned[nout]=NULL;}
    return (const char *const *)memory[target].owned;
}

/* EVERYTHING EITHER GAME OWNS, for taking the whole installation out. The single
 * game Remove keeps what the other game shares (lmod_removable); this list does not
 * filter, because nothing is left to share with. It is the union of both ownership
 * lists exactly as parsed (lmod_files_parse has already refused any name that is not
 * an immutable runtime file, so saves, settings, maps and media cannot be in it),
 * or each game's legacy names where the folder holds no list for it. The two lists
 * themselves come last, so a cleanup that stops part way keeps them and can be
 * rebuilt. Left out: the SDL library the running launcher itself may have loaded,
 * which the uninstaller deletes once the launcher has exited. */
static int lmod_all_add(LMOD_Files *all,int *cap,const char *name)
{
    int i;
    if(lmod_equal(name,"SDL2.dll")||!strcmp(name,"libSDL2-2.0.0.dylib"))return 1;
    for(i=0;i<all->count;i++)if(lmod_equal(all->owned[i],name))return 1;
    if(all->count+2>*cap){
        int grow=*cap?*cap*2:1024;char **more=(char **)realloc(all->owned,(size_t)grow*sizeof(char *));
        if(!more)return 0;
        all->owned=more;*cap=grow;
    }
    all->owned[all->count]=strdup(name);if(!all->owned[all->count])return 0;
    all->count++;all->owned[all->count]=NULL;return 1;
}
static const char *const *lmod_all_files(const char *dir)
{
    static LMOD_Files all;LMOD_Files one;int cap=0,t,i,rc,listed[2]={0,0};
    lmod_files_free(&all);
    for(t=0;t<2;t++){
        rc=lmod_manifest_read(dir,t,&one);
        if(rc<0){lmod_files_free(&all);return NULL;}
        if(rc==0){
            const char *const *legacy=t?lmod_ra_files:lmod_td_files;
            for(i=0;legacy[i];i++)if(!lmod_all_add(&all,&cap,legacy[i])){lmod_files_free(&all);return NULL;}
            continue;
        }
        listed[t]=1;
        for(i=0;i<one.count;i++)
            if(strncmp(one.owned[i],"opencnc3d-module-",17)&&!lmod_all_add(&all,&cap,one.owned[i])){
                lmod_files_free(&one);lmod_files_free(&all);return NULL;
            }
        lmod_files_free(&one);
    }
    for(t=0;t<2;t++)if(listed[t]&&!lmod_all_add(&all,&cap,t?"opencnc3d-module-ra.txt":"opencnc3d-module-td.txt")){
        lmod_files_free(&all);return NULL;
    }
    return (const char *const *)all.owned;
}

int lmod_install_file(const char *dir,int target,const char *build,const char *zip,const char *expected,
                       int (*cb)(void *,int,int),void *user,char *err,int len)
{
    LZ_Lock lock;int own=0,ok;char sha[65];
    if(lz_is_private_developer(dir)){snprintf(err,(size_t)len,"This private source build is updated from its source checkout, not public game packages.");return 0;}
    if(target<0||target>1||!expected||!lmod_hex(expected)){snprintf(err,(size_t)len,"The module checksum is missing or invalid.");return 0;}
    if(!lz_sha256_file(zip,sha,err,len))return 0;
    if(strcmp(sha,expected)){snprintf(err,(size_t)len,"The module failed its checksum. No game files were changed.");return 0;}
    if(!lz_held_here(dir)){if(lz_lock_new(dir,&lock)!=1){snprintf(err,(size_t)len,"Another launcher is using this installation.");return 0;}own=1;}
    ok=lmod_install_public(dir,target,build,zip,cb,user,err,len);
    /* A Mac installer package delivers its game as the download folder's package; once
     * that is installed it goes like any other (lmod_package_done). */
    if(ok&&lmod_is_cache_package(dir,target,zip))lmod_package_done(dir,target);
    if(own)lz_unlock(&lock);return ok;
}

/* Whole TD updates precede modules. Their fingerprinted global record may
 * advance while an older module ownership list stays behind. Bootstrap records
 * never prove that TD bytes were replaced; RA always keeps its own identity. */
static int lmod_record_version(const char *dir,const char *rel,char value[100],int *full)
{
    char path[1400],line[1200],data[100],extra;FILE *f;int found=0,hashed=0,bootstrap=0;
    value[0]=0;if(full)*full=0;
    if(!lz_inside(dir,rel,path,sizeof path)||lz_is_symlink(path))return -1;
    f=fopen(path,"rb");if(!f)return 0;
    while(fgets(line,sizeof line,f)){
        if(!found&&sscanf(line,"version %99s %c",value,&extra)==1){found=1;if(!full)break;}
        if(sscanf(line,"data_id %99s %c",data,&extra)==1&&lmod_hex(data))hashed=1;
        if(!strncmp(line,"module_install ",15))bootstrap=1;
    }
    fclose(f);if(full)*full=found&&hashed&&!bootstrap;return found;
}
static int lmod_record_order(const char *a,const char *b)
{
    char x[100],y[100];char *p;int v[3];
    snprintf(x,sizeof x,"%s",lmod_version(a));snprintf(y,sizeof y,"%s",lmod_version(b));
    p=strpbrk(x,"+-");if(p)*p=0;p=strpbrk(y,"+-");if(p)*p=0;
    if(!lmod_stable_version(x,v))return -1;
    if(!lmod_stable_version(y,v))return 1;
    return lmod_version_order(x,y);
}
static int lmod_installed_version(const char *dir,int target,char value[100])
{
    char rel[100],legacy[100];int found,old,full=0;
    snprintf(rel,sizeof rel,"opencnc3d-module-%s.txt",target?"ra":"td");
    found=lmod_record_version(dir,rel,value,NULL);if(found<0)return 0;
    if(target)return found;
    old=lmod_record_version(dir,"opencnc3d-install.txt",legacy,&full);
    if(old>0&&(!found||(full&&lmod_record_order(legacy,value)>=0))){
        snprintf(value,100,"%s",legacy);return 1;
    }
    return found;
}
int lmod_module_version(const char *dir,int target,char *version,int len)
{
    char value[100];
    if(!version||len<=0)return 0;version[0]=0;
    if(target<0||target>1||!lmod_installed_version(dir,target,value))return 0;
    snprintf(version,(size_t)len,"%s",lmod_version(value));return 1;
}
int lmod_module_current(const char *dir,int target,const char *build)
{char value[100];return build&&lmod_installed_version(dir,target,value)&&!strcmp(lmod_version(build),lmod_version(value));}
int lmod_module_update_available(const char *dir,int target,const char *build)
{
    char value[100],wanted[100],*suffix;int v[3],w[3];
    if(!build||!*build)return 0;
    if(!lmod_installed_version(dir,target,value))return 1;
    if(!strcmp(lmod_version(value),lmod_version(build)))return 0;
    snprintf(wanted,sizeof wanted,"%s",lmod_version(build));
    suffix=strchr(value,'+');if(suffix)*suffix=0;
    suffix=strchr(wanted,'+');if(suffix)*suffix=0;
    /* A current/newer module must never be replaced by an older feed entry. */
    if(lmod_stable_version(value,v)&&lmod_stable_version(wanted,w))return lmod_version_order(wanted,value)>0;
    return 1; /* legacy installation without a comparable release identity */
}

/* The asset identity in the part name prevents a different release/hash ever
 * resuming over old bytes. HTTP Range verifies offsets; SHA verifies the whole. */
typedef struct {int (*cb)(void *,int,int);void *user;} LMOD_Download;
static int lmod_download_progress(void *user,long long done,long long total)
{LMOD_Download *p=(LMOD_Download *)user;return !p->cb||p->cb(p->user,total>0?(int)(done*700/total):0,1000);}
static int lmod_extract_progress(void *user,int done,int total)
{LMOD_Download *p=(LMOD_Download *)user;return !p->cb||p->cb(p->user,700+(total>0?done*300/total:0),1000);}
static int lmod_replace(const char *from,const char *to)
{
#ifdef _WIN32
    return MoveFileExA(from,to,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)?0:-1;
#else
    return rename(from,to);
#endif
}
static int lmod_write_side(const char *dir,int target,const char *suffix,const char *text,char *err,int len)
{
    char path[1400],temp[1420];FILE *f;int ok;
    if(!lmod_cache_path(dir,target,suffix,path))return 0;
    snprintf(temp,sizeof temp,"%s.new",path);if(lz_is_symlink(temp))return 0;
    f=fopen(temp,"wb");if(!f)return 0;ok=fputs(text,f)>=0;if(fclose(f))ok=0;
    if(!ok){remove(temp);return 0;}if(lmod_replace(temp,path)){remove(temp);snprintf(err,(size_t)len,"The verified package could not be cached.");return 0;}return 1;
}
int lmod_install_remote(const char *dir,int target,const char *build,const LMOD_Catalog *catalog,
                         int (*cb)(void *,int,int),void *user,char *err,int len)
{
    const LMOD_Asset *asset;LMOD_Download progress;LZ_Lock lock;int own=0,ok=0,rc;long status;
    char cache[1400],part[1400],rel[160],zip[1400],url[1400],sha[65],side[128];
    err[0]=0;
    if(lz_is_private_developer(dir)){snprintf(err,(size_t)len,"This private source build is updated from its source checkout, not public game packages.");return 0;}
    if(!catalog||target<0||target>1||strcmp(lmod_version(build),catalog->version)||!lmod_site_ok(catalog->site))goto invalid;
    asset=&catalog->module[target];if(!asset->available||asset->id<=0||asset->size<=0||!lmod_hex(asset->sha))goto invalid;
    if(!lz_held_here(dir)){if(lz_lock_new(dir,&lock)!=1){snprintf(err,(size_t)len,"Another launcher is using this installation.");return 0;}own=1;}
    if(!lz_inside(dir,".opencnc3d-modules",cache,sizeof cache)||lz_is_symlink(cache))goto done;
    if(LMOD_MKDIR(cache)&&errno!=EEXIST)goto done;
    snprintf(rel,sizeof rel,".opencnc3d-modules/%s-%lld-%.64s.ocpart",target?"ra":"td",asset->id,asset->sha);
    if(!lz_inside(dir,rel,part,sizeof part)||lz_is_symlink(part)||!lmod_cache_path(dir,target,"",zip))goto done;
    /* A piece of this game's package from another release or another upload can never be
     * resumed (the name carries the asset's id and checksum), so it goes before the
     * download starts; the piece this download continues stays. */
    lmod_cache_drop(dir,target,rel+sizeof ".opencnc3d-modules/"-1,0);
    progress.cb=cb;progress.user=user;
    if(!lz_sha256_file(zip,sha,err,len)||strcmp(sha,asset->sha)||ln_file_size(zip)!=asset->size){
        lmod_asset_url(catalog->site,asset->id,url,sizeof url);
        rc=ln_get_range(url,LCFG_KEY,part,0,asset->size,30,NULL,lmod_download_progress,&progress,&status,err,len);
        if(rc!=LN_RANGE_OK)goto done;
        if(ln_file_size(part)!=asset->size||!lz_sha256_file(part,sha,err,len)||strcmp(sha,asset->sha)){
            remove(part);snprintf(err,(size_t)len,"The downloaded module failed its checksum. No game files were changed.");goto done;
        }
        if(lmod_replace(part,zip)){snprintf(err,(size_t)len,"The verified module package could not take its cache name.");goto done;}
    }
    snprintf(side,sizeof side,"%s\n",asset->sha);if(!lmod_write_side(dir,target,".sha256",side,err,len))goto done;
    snprintf(side,sizeof side,"%s\n",catalog->version);if(!lmod_write_side(dir,target,".version",side,err,len))goto done;
    if(cb&&!cb(user,700,1000))goto done;
    ok=lmod_install_public(dir,target,build,zip,lmod_extract_progress,&progress,err,len);
    if(ok)lmod_package_done(dir,target);
done:if(!ok&&!err[0])snprintf(err,(size_t)len,"The module installation stopped before completion. No partial game was installed.");if(own)lz_unlock(&lock);return ok;
invalid:snprintf(err,(size_t)len,"A verified public package is not available for this launcher build.");return 0;
}
