#ifndef CNC3D_WORLD_SNAPSHOT_BUFFER_H
#define CNC3D_WORLD_SNAPSHOT_BUFFER_H
/* The old capture remains available for older brains, memory refusal and diagnostics.
   Every field still reaches the same parser, on the same simulation heartbeat. */
static std::vector<char> g_worldSnapshot;
static size_t g_worldSnapshotOffset=0,g_worldSnapshotLength=0;
static bool g_worldSnapshotMemory=false;
static unsigned long long g_worldSnapshotHits=0,g_worldSnapshotFallbacks=0;
static unsigned long long g_worldSnapshotVerified=0,g_worldSnapshotBad=0;
static int world_snapshot_buffer_mode() {
    static const int mode=[](){const char* p=getenv("OPENCNC3D_WORLD_SNAPSHOT_BUFFER");
        return !p?1:!strcmp(p,"0")?0:!strcmp(p,"verify")?2:1;}();return mode;
}
static void world_snapshot_dump() {
    printf("WORLDSNAPSHOT|hits=%llu|fallbacks=%llu|verified=%llu|bad=%llu|capacity=%zu\n",
        g_worldSnapshotHits,g_worldSnapshotFallbacks,g_worldSnapshotVerified,g_worldSnapshotBad,g_worldSnapshot.capacity());
}
static char* world_snapshot_line(char* line,size_t size) {
    if(!g_worldSnapshotMemory)return fgets(line,(int)size,g_capture);
    if(!size || g_worldSnapshotOffset>=g_worldSnapshotLength)return NULL;
    size_t n=0;
    while(n+1<size && g_worldSnapshotOffset<g_worldSnapshotLength) {
        const char c=g_worldSnapshot[g_worldSnapshotOffset++];line[n++]=c;
        if(c=='\n')break;
    }
    line[n]=0;return line;
}
static bool world_snapshot_begin() {
    g_worldSnapshotMemory=false;
    const int mode=world_snapshot_buffer_mode();
    if(mode && BrainDumpBuffer) {
        try {
            const char* allow=getenv("OPENCNC3D_WORLD_SNAPSHOT_ALLOC_OK");
            if(allow&&!strcmp(allow,"0"))throw std::bad_alloc();
            if(g_worldSnapshot.empty())g_worldSnapshot.resize(65536);
            int needed=BrainDumpBuffer(g_worldSnapshot.data(),(int)g_worldSnapshot.size(),mode==2?0:1);
            if(needed>(int)g_worldSnapshot.size() && needed<=16*1024*1024) {
                std::vector<char> larger((size_t)needed);g_worldSnapshot.swap(larger);
                needed=BrainDumpBuffer(g_worldSnapshot.data(),(int)g_worldSnapshot.size(),mode==2?0:1);
            }
            if(needed>0 && needed<=(int)g_worldSnapshot.size() && g_worldSnapshot[needed-1]==0) {
                g_worldSnapshotOffset=0;g_worldSnapshotLength=(size_t)needed-1;
                if(mode==2) {
                    const char* shortCheck=getenv("OPENCNC3D_WORLD_SNAPSHOT_VERIFY_SHORT");
                    if(shortCheck && !strcmp(shortCheck,"1")) {
                        char tiny[3]={'a','b','c'};
                        const int required=BrainDumpBuffer(tiny+1,1,1);
                        if(required<=1 || tiny[0]!='a' || tiny[1]!=0 || tiny[2]!='c')g_worldSnapshotBad++;
                    }
                    if(!capture_brain_call(BrainDump))return false;
                    g_worldSnapshotVerified++;bool same=true;size_t at=0;
                    char block[4096];size_t count;
                    /* stdout text mode adds CR on Windows; the parser discards it.
                       Compare the complete logical byte stream, with no field masks. */
                    while((count=fread(block,1,sizeof block,g_capture))!=0)
                        for(size_t i=0;i<count;i++) {
                            if(block[i]=='\r')continue;
                            if(at>=g_worldSnapshotLength || block[i]!=g_worldSnapshot[at])same=false;
                            at++;
                        }
                    if(at!=g_worldSnapshotLength || ferror(g_capture))same=false;
                    if(!same){g_worldSnapshotBad++;fprintf(stderr,"WORLDSNAPSHOT|mismatch\n");}
                    rewind(g_capture);return true; /* Diagnostic renders the reference. */
                }
                g_worldSnapshotMemory=true;g_worldSnapshotHits++;return true;
            }
        } catch(const std::bad_alloc&) {}
        g_worldSnapshotFallbacks++;
    }
    return capture_brain_call(BrainDump)!=NULL;
}
#endif
