/* ZIP/RAR/7Z bundles. Read via libarchive, never a shell/external extractor.
 * Only bounded map inputs enter a private temporary tree; no archive asset is
 * installed as executable code, a menu, renderer pack or shared game rule file. */
#pragma once
#include <set>
#ifdef OPENCNC3D_MAP_ARCHIVE_STATIC
#include <archive.h>
#include <archive_entry.h>
__attribute__((used)) static const char map_archive_builtin_mark[] = "\0MAPARCHIVE|builtin|3.8.10";
#endif
#ifndef _WIN32
#include <dlfcn.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif
namespace mapimport { namespace detail {
inline bool archive_extension(const std::string& path) {
    const std::string e=ext(path);return e==".zip"||e==".rar"||e==".7z";
}
struct ArchiveApi {
    void* library=NULL;
    void* (*create)()=NULL;
    int (*filters)(void*)=NULL;
    int (*zip)(void*)=NULL; int (*rar)(void*)=NULL; int (*rar5)(void*)=NULL; int (*seven)(void*)=NULL;
    int (*open)(void*,const void*,size_t)=NULL;
    int (*next)(void*,void**)=NULL;
    intptr_t (*data)(void*,void*,size_t)=NULL;
    int (*skip)(void*)=NULL; int (*free)(void*)=NULL;
    const char* (*error)(void*)=NULL;
    const char* (*path)(void*)=NULL; const char* (*path_utf8)(void*)=NULL;
    const char* (*symlink)(void*)=NULL; const char* (*hardlink)(void*)=NULL;
    long long (*size)(void*)=NULL; unsigned int (*type)(void*)=NULL;
    int (*encrypted)(void*)=NULL;
    void* symbol(const char* name) {
#ifdef _WIN32
        return (void*)GetProcAddress((HMODULE)library,name);
#else
        return dlsym(library,name);
#endif
    }
    ArchiveApi() {
#ifdef OPENCNC3D_MAP_ARCHIVE_STATIC
        create=[]()->void*{return archive_read_new();};
        filters=[](void* p){return archive_read_support_filter_all((archive*)p);};
        zip=[](void* p){return archive_read_support_format_zip((archive*)p);};
        rar=[](void* p){return archive_read_support_format_rar((archive*)p);};
        rar5=[](void* p){return archive_read_support_format_rar5((archive*)p);};
        seven=[](void* p){return archive_read_support_format_7zip((archive*)p);};
        open=[](void* p,const void* b,size_t n){return archive_read_open_memory((archive*)p,b,n);};
        next=[](void* p,void** out){archive_entry* e=NULL;int r=archive_read_next_header((archive*)p,&e);*out=e;return r;};
        data=[](void* p,void* b,size_t n)->intptr_t{return archive_read_data((archive*)p,b,n);};
        skip=[](void* p){return archive_read_data_skip((archive*)p);};
        free=[](void* p){return archive_read_free((archive*)p);};
        error=[](void* p){return archive_error_string((archive*)p);};
        path=[](void* p){return archive_entry_pathname((archive_entry*)p);};
        path_utf8=[](void* p){return archive_entry_pathname_utf8((archive_entry*)p);};
        symlink=[](void* p){return archive_entry_symlink((archive_entry*)p);};
        hardlink=[](void* p){return archive_entry_hardlink((archive_entry*)p);};
        size=[](void* p)->long long{return archive_entry_size((archive_entry*)p);};
        type=[](void* p)->unsigned int{return archive_entry_filetype((archive_entry*)p);};
        encrypted=[](void* p){return archive_entry_is_encrypted((archive_entry*)p);};
#else
#ifdef _WIN32
        // Only a paired dependency beside this executable, never a DLL found via
        // the current map directory or an arbitrary PATH component.
        wchar_t filename[32768];DWORD n=GetModuleFileNameW(NULL,filename,32768);
        if(n&&n<32768){std::wstring p(filename);size_t at=p.find_last_of(L"/\\");if(at!=std::wstring::npos){p.resize(at+1);p+=L"archive.dll";library=(void*)LoadLibraryW(p.c_str());}}
#else
#ifdef __APPLE__
        char executable[4096],resolved[4096];uint32_t length=sizeof executable;
        if(_NSGetExecutablePath(executable,&length)==0&&realpath(executable,resolved)) {
            const std::string paired=join(parent(resolved),"libmaparchive.dylib");
            if(regular(paired))library=dlopen(paired.c_str(),RTLD_NOW|RTLD_LOCAL);
            else library=dlopen("/usr/lib/libarchive.2.dylib",RTLD_NOW|RTLD_LOCAL);
        }
#else
        library=dlopen("libarchive.so.13",RTLD_NOW|RTLD_LOCAL);
#endif
#endif
        if(!library)return;
#define MI_ARCHIVE(member,name) member=reinterpret_cast<decltype(member)>(symbol(name))
        MI_ARCHIVE(create,"archive_read_new");MI_ARCHIVE(filters,"archive_read_support_filter_all");
        MI_ARCHIVE(zip,"archive_read_support_format_zip");MI_ARCHIVE(rar,"archive_read_support_format_rar");MI_ARCHIVE(rar5,"archive_read_support_format_rar5");MI_ARCHIVE(seven,"archive_read_support_format_7zip");
        MI_ARCHIVE(open,"archive_read_open_memory");MI_ARCHIVE(next,"archive_read_next_header");MI_ARCHIVE(data,"archive_read_data");MI_ARCHIVE(skip,"archive_read_data_skip");MI_ARCHIVE(free,"archive_read_free");MI_ARCHIVE(error,"archive_error_string");
        MI_ARCHIVE(path,"archive_entry_pathname");MI_ARCHIVE(path_utf8,"archive_entry_pathname_utf8");MI_ARCHIVE(symlink,"archive_entry_symlink");MI_ARCHIVE(hardlink,"archive_entry_hardlink");MI_ARCHIVE(size,"archive_entry_size");MI_ARCHIVE(type,"archive_entry_filetype");MI_ARCHIVE(encrypted,"archive_entry_is_encrypted");
#undef MI_ARCHIVE
#endif
    }
    bool ready() const {return create&&filters&&zip&&rar&&seven&&open&&next&&data&&skip&&free&&error&&path&&symlink&&hardlink&&size&&type;}
};
inline ArchiveApi& archive_api(){static ArchiveApi api;return api;}
inline bool archive_path(std::string& name) {
    std::replace(name.begin(),name.end(),'\\','/');
    if(name.empty()||name.size()>768||name[0]=='/'||name.find(':')!=std::string::npos)return false;
    size_t at=0;while(at<name.size()) {
        size_t end=name.find('/',at);if(end==std::string::npos)end=name.size();std::string part=name.substr(at,end-at);
        if(part.empty()||part=="."||part==".."||part.back()=='.'||part.back()==' ')return false;
        for(unsigned char c:part)if(c<32||c=='<'||c=='>'||c=='"'||c=='|'||c=='?'||c=='*')return false;
        std::string base=lower(stem(part));if(base=="con"||base=="prn"||base=="aux"||base=="nul"||(base.size()==4&&(base.substr(0,3)=="com"||base.substr(0,3)=="lpt")&&base[3]>='1'&&base[3]<='9'))return false;
        at=end+1;
    }return true;
}
struct ArchiveFiles {
    std::string root;
    std::vector<std::string> files,dirs,candidates;
    ~ArchiveFiles(){for(auto i=files.rbegin();i!=files.rend();++i)file_remove(*i);for(auto i=dirs.rbegin();i!=dirs.rend();++i)remove_dir(*i);if(!root.empty())remove_dir(root);}
    bool temporary(Info& out) {
#ifdef _WIN32
        wchar_t temp[32768],name[32768];if(!GetTempPathW(32768,temp)||!GetTempFileNameW(temp,L"cmi",0,name))return fail(out,"Cannot create a private archive workspace.");
        DeleteFileW(name);if(!CreateDirectoryW(name,NULL))return fail(out,"Cannot create a private archive workspace.");root=utf8(name);
#else
        const char* base=getenv("TMPDIR");std::string path=join(base&&*base?base:"/tmp","cnc3d-map-import-XXXXXX");std::vector<char> b(path.begin(),path.end());b.push_back(0);char* p=mkdtemp(b.data());if(!p)return fail(out,"Cannot create a private archive workspace.");root=p;
#endif
        return true;
    }
    bool parents(const std::string& relative) {
        size_t at=0;while((at=relative.find('/',at))!=std::string::npos){std::string p=join(root,relative.substr(0,at));if(!directory(p)){if(make_dir(p)!=0)return false;dirs.push_back(p);}++at;}return true;
    }
    bool extract(const std::string& source,Info& out) {
        FileStat st;if(file_stat(source,&st)||!S_ISREG(st.st_mode)||st.st_size<0||(uint64_t)st.st_size>64u*1024u*1024u)return fail(out,"Archive must be a readable file smaller than 64 MB.");
        Bytes packed((size_t)st.st_size);FILE* f=file_open(source,"rb");if(!f)return fail(out,"Cannot open this archive.");bool ok=fread(packed.data(),1,packed.size(),f)==packed.size()&&!ferror(f);fclose(f);if(!ok)return fail(out,"Archive changed or could not be read completely.");
        ArchiveApi& api=archive_api();if(!api.ready())return fail(out,"Archive support is missing from this installation. Reinstall its archive runtime dependency.");
        void* reader=api.create();if(!reader)return fail(out,"Cannot allocate archive reader.");
        struct Close{ArchiveApi& api;void* reader;~Close(){api.free(reader);}} close={api,reader};
        api.filters(reader);api.zip(reader);api.rar(reader);if(api.rar5)api.rar5(reader);api.seven(reader);
        auto bad=[&](const char* message){const char* reason=api.error(reader);return fail(out,std::string(message)+(reason?std::string(": ")+reason:""));};
        if(api.open(reader,packed.data(),packed.size())<0)return bad("Cannot open ZIP/RAR/7Z archive");
        if(!temporary(out))return false;
        size_t total=0;int entries=0;std::set<std::string> names;void* entry=NULL;int status;
        while((status=api.next(reader,&entry))==0) {
            if(++entries>4096)return fail(out,"Archive contains more than 4096 entries.");
            const char* p=api.path_utf8?api.path_utf8(entry):NULL;if(!p)p=api.path(entry);std::string name=p?p:"";
            const unsigned type=api.type(entry);
            while(type==0040000&&!name.empty()&&(name.back()=='/'||name.back()=='\\'))name.pop_back();
            if(!archive_path(name)||api.symlink(entry)||api.hardlink(entry)||(type!=0100000&&type!=0040000))return fail(out,"Archive contains an unsafe path, link or special file; nothing was installed.");
            if(!names.insert(lower(name)).second)return fail(out,"Archive contains duplicate case-insensitive paths.");
            if(api.encrypted&&api.encrypted(entry))return fail(out,"Password-protected archives must be unlocked before import.");
            long long size=api.size(entry);if(size<0||(uint64_t)size>64u*1024u*1024u||total+(uint64_t)size>128u*1024u*1024u)return fail(out,"Archive exceeds the 128 MB unpacked-data limit.");total+=(size_t)size;
            std::string e=ext(name);bool wanted=e==".ini"||e==".bin"||e==".mpr"||e==".map"||e==".hgt"||e==".json";
            if(type==0040000||!wanted){if(api.skip(reader)<0)return bad("Cannot read archive entry");continue;}
            if((uint64_t)size>MAP_IMPORT_INPUT_LIMIT)return fail(out,"An archived map input exceeds 4 MB.");
            if(!parents(name))return fail(out,"Cannot create archive map workspace.");
            Bytes data;unsigned char chunk[32768];intptr_t got;
            while((got=api.data(reader,chunk,sizeof chunk))>0){if(data.size()+(size_t)got>MAP_IMPORT_INPUT_LIMIT)return fail(out,"Archived map input expands beyond 4 MB.");data.insert(data.end(),chunk,chunk+got);}
            if(got<0)return bad("Cannot decompress archive map");
            std::string dest=join(root,name);files.push_back(dest);if(!write(dest,data))return fail(out,"Cannot write private archive map workspace.");
        }
        if(status!=1)return bad("Archive is incomplete or damaged");
        for(const std::string& file:files) {
            std::string e=ext(file);
            if(e==".ini"||e==".mpr"||e==".map") {
                // Distributions sometimes include shared RULES/TUTORIAL INIs.
                // Only scenario candidates with an actual [Map] are considered.
                Bytes b;Info ignored;Ini ini;if(read(file,b,ignored)&&parse(b,ini,ignored)&&ini.count("map"))candidates.push_back(file);
            } else if(e==".bin") {bool ambiguous=false;std::string ini=sibling(file,".ini",ambiguous);if(ini.empty()&&!ambiguous){std::string json=sibling(file,".json",ambiguous);if(!json.empty()&&!ambiguous)candidates.push_back(file);}}
        }
        std::sort(candidates.begin(),candidates.end(),[](const std::string& a,const std::string& b){return lower(a)<lower(b);});
        if(candidates.empty())return fail(out,"No C&C or Red Alert map scenarios were found in this archive.");
        return true;
    }
};
inline bool bundle_info(const std::string& source,const ArchiveFiles& files,Info& out) {
    int sp=0,mp=0,ra=0;Info first;
    for(const std::string& path:files.candidates){Info info;if(!inspect_map(path,info))return fail(out,leaf(path)+": "+info.message);if(sp+mp==0)first=info;if(info.multiplayer)++mp;else ++sp;if(info.red_alert)++ra;}
    out=first;out.bundle_count=sp+mp;out.name=stem(source);out.kind=std::to_string(sp+mp)+" maps: "+std::to_string(sp)+" SP / "+std::to_string(mp)+" MP";out.game=ra==0?"Tiberian Dawn":ra==sp+mp?"Red Alert":"C&C + Red Alert";
    out.message="Archive contains "+out.kind+". Import adds every map; preview shows the first map.";
    for(const std::string& path:files.candidates){Info info;inspect_map(path,info);if(info.recovered){out.message=info.name+": missing INI; terrain-only MP. Objects/resources/scripts absent. "+std::to_string(sp+mp)+" maps will import.";break;}}
    return true;
}
} // detail
inline bool inspect(const std::string& source,Info& out) {
    if(!detail::archive_extension(source))return inspect_map(source,out);
    out=Info();detail::ArchiveFiles files;if(!files.extract(source,out))return false;return detail::bundle_info(source,files,out);
}
inline bool run(const std::string& source,const std::string& td_dir,const std::string& ra_dir,Info& out,Progress progress=NULL,void* user=NULL) {
    if(!detail::archive_extension(source))return run_map(source,td_dir,ra_dir,out,progress,user);
    out=Info();detail::report(progress,user,1,"Opening archive and checking map entries");detail::ArchiveFiles files;if(!files.extract(source,out)||!detail::bundle_info(source,files,out))return false;
    const int count=(int)files.candidates.size();
    for(int i=0;i<count;++i) {
        struct State{Progress cb;void* user;int index,count;};State state={progress,user,i,count};
        auto step=[](void* ptr,int n,const char* stage){State& s=*(State*)ptr;std::string message="Map "+std::to_string(s.index+1)+"/"+std::to_string(s.count)+": "+stage;detail::report(s.cb,s.user,5+(s.index*100+n)*95/(s.count*100),message.c_str());};
        Info map;if(!run_map(files.candidates[i],td_dir,ra_dir,map,step,&state))return detail::fail(out,std::to_string(i)+" of "+std::to_string(count)+" maps imported. "+detail::leaf(files.candidates[i])+": "+map.message);
        ImportedMap result;result.name=map.name;result.scenario=map.scenario;result.red_alert=map.red_alert;result.multiplayer=map.multiplayer;out.imported_maps.push_back(result);
        if(i==0)out.scenario=map.scenario;
    }
    out.message="Imported all "+std::to_string(count)+" maps from "+detail::leaf(source)+". Singleplayer maps are in User Maps; multiplayer maps are in Skirmish and Multiplayer.";
    detail::report(progress,user,100,"Archive import complete");return true;
}
} // mapimport
