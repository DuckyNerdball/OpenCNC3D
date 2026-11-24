/* Old launcher, future approved module catalog, real download/extraction.
 * Hold the legacy updater at AVAILABLE to catch wrong-game and repeated Update. */
#define lu_phase fixture_phase
#define lu_latest fixture_latest
#define main launcher_entry
#include "../../launcher/launcher.c"
#undef main
#undef lu_phase
#undef lu_latest
#include <assert.h>
void fixture_latest(LU_State *u,char *out,int len){(void)u;snprintf(out,(size_t)len,"0.99.1");}
LU_Phase fixture_phase(const LU_State *u){(void)u;return LU_AVAILABLE;}
static void legacy_versions(const char *dir)
{
    char folder[1400],path[1500],value[100];FILE *f;
    snprintf(folder,sizeof folder,"%s/legacy-records",dir);assert(!mkdir(folder,0755));
    snprintf(path,sizeof path,"%s/opencnc3d-module-td.txt",folder);f=fopen(path,"wb");assert(f);fputs("version 0.9.0-alpha+old\n",f);fclose(f);
    snprintf(path,sizeof path,"%s/opencnc3d-module-ra.txt",folder);f=fopen(path,"wb");assert(f);fputs("version 0.9.0\n",f);fclose(f);
    snprintf(path,sizeof path,"%s/opencnc3d-install.txt",folder);f=fopen(path,"wb");assert(f);fputs("version 0.9.1\ndata_id 1111111111111111111111111111111111111111111111111111111111111111\n",f);fclose(f);
    assert(lmod_module_version(folder,0,value,sizeof value)&&!strcmp(value,"0.9.1"));
    assert(!lmod_module_update_available(folder,0,"0.9.1"));
    assert(lmod_module_version(folder,1,value,sizeof value)&&!strcmp(value,"0.9.0"));
    assert(lmod_module_update_available(folder,1,"0.9.1"));
    f=fopen(path,"ab");assert(f);fputs("module_install 1\n",f);fclose(f);
    assert(lmod_module_version(folder,0,value,sizeof value)&&!strcmp(value,"0.9.0-alpha+old"));
    f=fopen(path,"wb");assert(f);fputs("version 0.9.1\ndata_id unknown\n",f);fclose(f);
    assert(lmod_module_update_available(folder,0,"0.9.1"));
    f=fopen(path,"wb");assert(f);fputs("version 0.8.0\ndata_id 1111111111111111111111111111111111111111111111111111111111111111\n",f);fclose(f);
    assert(lmod_module_version(folder,0,value,sizeof value)&&!strcmp(value,"0.9.0-alpha+old"));
    assert(!remove(path));snprintf(path,sizeof path,"%s/opencnc3d-module-td.txt",folder);assert(!remove(path));
    snprintf(path,sizeof path,"%s/opencnc3d-module-ra.txt",folder);assert(!remove(path));assert(!rmdir(folder));
    puts("PUBLIC-LEGACY-VERSION|whole-TD-upgrade=1|RA-kept-separate=1|bootstrap-not-authority=1|unknown-data-not-authority=1|older-global-not-authority=1|PASS");
}
int main(int argc,char **argv)
{
    L_App a;char err[512];int target,i,guard;
    assert(argc==5&&SDL_Init(0)==0);legacy_versions(argv[1]);memset(&a,0,sizeof a);
    snprintf(a.dir,sizeof a.dir,"%s",argv[1]);target=atoi(argv[4]);
    a.up=lu_create(a.dir,NULL);a.private_source=lz_is_private_developer(a.dir);
    a.mod[0].name="Dawn3D";a.mod[1].name="RA3D";
    a.mod[0].installed=lmod_td_present(a.dir);a.mod[1].installed=lmod_ra_present(a.dir,NULL,0);
    snprintf(a.media_manifest,sizeof a.media_manifest,"%s",argv[3]);
    if(a.private_source){
        a.catalog_allowed=1;l_catalog_start(&a);assert(!a.catalog_worker);
        assert(SDL_AtomicGet(&a.catalog_phase)==0);
        {char version[100];l_read_installed(&a);a.mod_sel=0;
         assert(!strcmp(l_selected_version(&a,version),"0.0.1"));}
    }else{
        assert(lmod_catalog_fetch_at(argv[2],NULL,&a.catalog,err,sizeof err));
        assert(!strcmp(a.catalog.version,"0.99.1")&&strcmp(a.catalog.version,LCFG_BUILD));
        SDL_AtomicSet(&a.catalog_phase,2);
    }
    l_module_versions(&a);
    for(i=0;i<2;i++){
        a.mod_sel=i;l_set_view(&a,L_VIEW_MODULES);
        assert(!a.mod[i].needs_update||a.mod[i].installed);
        assert(!strcmp(a.btn[L_BTN_PLAY].label,a.mod[i].needs_update?"Update":"Launch"));
        assert(a.btn[L_BTN_PLAY].disabled==!a.mod[i].installed);
    }
    if(target>=0){
        a.mod_sel=target;l_set_view(&a,L_VIEW_MODULES);assert(a.mod[target].needs_update);
        l_activate(&a,L_BTN_PLAY);assert(a.view==L_VIEW_MODULE_OPTIONS&&a.module_target==target);
        a.want_movies=a.want_music=0;l_set_buttons(&a);l_activate(&a,L_BTN_PLAY);
        assert(a.view==L_VIEW_MODULE_RUN);
        for(guard=0;guard<10000&&(a.view==L_VIEW_MODULE_RUN||a.view==L_VIEW_MEDIA_RUN);guard++){
            l_module_tick(&a);l_m_tick(&a);
            if(SDL_AtomicGet(&a.module_phase)==-1){fprintf(stderr,"future update: %s\n",a.module_error);assert(0);}
            SDL_Delay(1);
        }
        assert(a.view==L_VIEW_MODULES&&lmod_module_current(a.dir,target,"0.99.1"));
        l_set_buttons(&a);assert(!strcmp(a.btn[L_BTN_PLAY].label,"Launch")&&!a.btn[L_BTN_PLAY].disabled);
        {char version[100];L_App restarted;memset(&restarted,0,sizeof restarted);
         assert(!strcmp(l_selected_version(&a,version),"0.99.1"));
         snprintf(restarted.dir,sizeof restarted.dir,"%s",a.dir);restarted.mod_sel=target;
         restarted.mod[0].installed=lmod_td_present(a.dir);restarted.mod[1].installed=lmod_ra_present(a.dir,NULL,0);
         l_read_installed(&restarted);assert(!strcmp(l_selected_version(&restarted,version),"0.99.1"));
         if(!target){SDL_AtomicSet(&a.catalog_phase,-1);assert(!l_legacy_update_available(&a,LU_AVAILABLE));}
         printf("PUBLIC-VERSION-RESTART|target=%d|installed=%s|PASS\n",target,l_selected_version(&restarted,version));}
    }else assert(!a.mod[0].needs_update&&!a.mod[1].needs_update);
    printf("FUTURE-MODULE|target=%d|td=%d|ra=%d|private=%d|PASS\n",target,a.mod[0].installed,a.mod[1].installed,a.private_source);
    l_free_components(&a);db_pack_free(a.ra_pack);lu_destroy(a.up);SDL_Quit();return 0;
}
