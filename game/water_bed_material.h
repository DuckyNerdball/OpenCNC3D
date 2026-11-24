#ifndef OPENCNC3D_WATER_BED_MATERIAL_H
#define OPENCNC3D_WATER_BED_MATERIAL_H
/* Reuse detail from the current map's actual cliff albedo. The saved ground
   matte and land alpha select a solid mineral patch; RGB never classifies rock.
   Only luminance detail tiles vertically. The nearest cliff retains its own
   colour, without repeating a whole painted cliff silhouette below the water.

   The sheet's texels are reached through `px(index)`, which hands back the four bytes
   of texel `index` (row * sheet width + column) of level 0. Two routes supply them:
   the whole sheet read back into memory (water_bed_material_make), or bands of rows
   read as the cells need them (water_bed_material_make_banded). Both run the same
   arithmetic below, statement for statement, so they pick the same patch and build
   the same pattern. */

/* The score of the six-texel square at native (x, z) of cell c, whose ground matte is
   mask slot `slot`; -1 when the square is not all rock. */
template <class Px>
static float water_bed_patch_score(const Px& px, const std::vector<unsigned char>& masks, int slot,
                                   const PackCell& c, const PackTex& sheet, int x, int z)
{
    bool rock=true;
    for(int dz=0;dz<6&&rock;dz++)for(int dx=0;dx<6;dx++) {
        if(masks[((size_t)slot*24+z+dz)*24+x+dx]>8){rock=false;break;}
        const int tx=std::max(0,std::min(sheet.w-1,(int)((c.u0+(c.u1-c.u0)*(x+dx+.5f)/24)*sheet.uw)));
        const int tz=std::max(0,std::min(sheet.h-1,(int)((c.v0+(c.v1-c.v0)*(z+dz+.5f)/24)*sheet.uh)));
        if(px((size_t)tz*sheet.w+tx)[3]<250){rock=false;break;}
    }
    if(!rock)return -1.0f;
    float sum=0,sq=0;
    for(int dz=0;dz<6;dz++)for(int dx=0;dx<6;dx++) {
        const int tx=(int)((c.u0+(c.u1-c.u0)*(x+dx+.5f)/24)*sheet.uw),tz=(int)((c.v0+(c.v1-c.v0)*(z+dz+.5f)/24)*sheet.uh);
        const unsigned char* p=px((size_t)tz*sheet.w+tx);const float l=(.299f*p[0]+.587f*p[1]+.114f*p[2])/255;
        sum+=l;sq+=l*l;
    }
    const float variance=sq/36-(sum/36)*(sum/36);
    return 1.0f/(.001f+fabsf(variance-.004f));
}

/* The best square of one cell, in the order the squares are tried: the first of the
   highest score, or false when no square of the cell is all rock. */
template <class Px>
static bool water_bed_cell_best(const Px& px, const std::vector<unsigned char>& masks, int slot,
                                const PackCell& c, const PackTex& sheet, float* best, int* bx, int* bz)
{
    bool any=false;
    // A six-native-texel square is one quarter of a cell, large enough to
    // contain source cracks while excluding painted shore water and snow.
    for(int z=1;z<=17;z+=2)for(int x=1;x<=17;x+=2) {
        const float score=water_bed_patch_score(px,masks,slot,c,sheet,x,z);
        if(score<0)continue;
        if(!any || score>*best){*best=score;*bx=x;*bz=z;any=true;}
    }
    return any;
}

/* The ground-matte slot of cell i, or 0 when the cell is not a candidate. */
static int water_bed_slot(size_t i)
{
    const auto& masks=g_terrainGrassMasks.coverage;
    if(i>=g_waterRa.bedMasks.size())return 0;
    const int slot=g_waterRa.bedMasks[i];
    if(slot<=0 || (size_t)(slot+1)*576>masks.size())return 0;
    return slot;
}

/* Where the pattern below reads detail texel (x, z) of the chosen square. */
static void water_bed_detail_at(const PackCell& c,const PackTex& sheet,int bx,int bz,int x,int z,int* tx,int* tz)
{
    const int N=64;
    const float fx=(bx+(x+.5f)*6/N)/24,fz=(bz+(z+.5f)*6/N)/24;
    *tx=(int)((c.u0+(c.u1-c.u0)*fx)*sheet.uw);*tz=(int)((c.v0+(c.v1-c.v0)*fz)*sheet.uh);
}

/* The pattern from the chosen square of cell `chosen`, uploaded as g_waterBedPatternTex. */
template <class Px>
static void water_bed_material_finish(const Px& px,const PackTex& sheet,const std::vector<PackCell>& cells,int chosen,int bx,int bz)
{
    const int N=64;std::vector<float> detail(N*N);double mean=0;
    const auto& c=cells[chosen];
    for(int z=0;z<N;z++)for(int x=0;x<N;x++) {
        int tx=0,tz=0;water_bed_detail_at(c,sheet,bx,bz,x,z,&tx,&tz);
        const unsigned char* p=px((size_t)tz*sheet.w+tx);detail[z*N+x]=(.299f*p[0]+.587f*p[1]+.114f*p[2])/255;mean+=detail[z*N+x];
    }
    mean/=N*N;
    // Mirrored quarters give matching values and tangents at all repeat edges.
    // Store source luminance relative to its mean, retaining the real crack
    // contrast rather than flattening the newly exposed vertical cliff sides.
    std::vector<unsigned char> pixels(N*N);
    for(int z=0;z<N;z++)for(int x=0;x<N;x++) {
        const int sx=x<N/2?x*2:(N-1-x)*2,sz=z<N/2?z*2:(N-1-z)*2;
        const float l=.5f*detail[sz*N+sx]/std::max(.02f,(float)mean);
        pixels[z*N+x]=(unsigned char)(std::max(.125f,std::min(1.0f,l))*255+.5f);
    }
    glGenTextures(1,&g_waterBedPatternTex);glBindTexture(GL_TEXTURE_2D,g_waterBedPatternTex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,N,N,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,pixels.data());glBindTexture(GL_TEXTURE_2D,0);
    fprintf(stderr,"WATER|bed-material|source-cell=%d,%d|native-patch=%d,%d,6,6|explicit-rock-matte|detail=64x64|colour=local-cliff\n",c.x,c.y,bx,bz);
}

/* From the whole sheet in memory (four bytes a texel). */
static void water_bed_material_make(const std::vector<unsigned char>& atlas,
                                    const PackTex& sheet,const std::vector<PackCell>& cells)
{
    g_waterBedPatternTex=0;
    if(!water_src_ra() || atlas.empty() || g_terrainGrassMasks.coverage.empty())return;
    const auto& masks=g_terrainGrassMasks.coverage;
    const auto px=[&](size_t i)->const unsigned char*{return &atlas[i*4];};
    int chosen=-1,bx=0,bz=0;float best=-1;
    for(size_t i=0;i<cells.size() && i<g_waterRa.bedMasks.size();i++) {
        const int slot=water_bed_slot(i);if(!slot)continue;
        float s=0;int x=0,z=0;
        if(water_bed_cell_best(px,masks,slot,cells[i],sheet,&s,&x,&z) && s>best){best=s;chosen=(int)i;bx=x;bz=z;}
    }
    if(chosen<0)return;
    water_bed_material_finish(px,sheet,cells,chosen,bx,bz);
}

/* THE SAME, FROM BANDS OF ROWS. Every square a cell can try lies inside the rows that
   cell's rectangle covers, so the candidate cells are grouped into windows of at least
   `rows` sheet rows, each window is read once and the cells wholly inside it are scored
   from it, and the best of all is taken in cell order exactly as above (the first of the
   highest score). The chosen square's pattern is then read from one more window around
   it. The sheet is never held whole: `rd` is the reader the caller opened on it, and
   `held` is told what this holds, for the memory ledger. */
static void water_bed_material_make_banded(GlBandReader& rd,const PackTex& sheet,const std::vector<PackCell>& cells,
                                           int rows,void (*held)(long long))
{
    g_waterBedPatternTex=0;
    if(!water_src_ra() || !rd.open || g_terrainGrassMasks.coverage.empty())return;
    const auto& masks=g_terrainGrassMasks.coverage;
    const long long W=sheet.w,H=sheet.h,last=W*H-1;
    if(W<=0 || H<=0)return;
    /* the first and last texel index a cell's squares can read, from the position
       arithmetic the score uses, with a row of margin either side */
    struct Cand { int cell, slot; long long lo, hi; bool done, any; float best; int bx, bz; };
    std::vector<Cand> cand;
    for(size_t i=0;i<cells.size() && i<g_waterRa.bedMasks.size();i++) {
        const int slot=water_bed_slot(i);if(!slot)continue;
        const PackCell& c=cells[i];
        int cx0=0x7fffffff,cx1=-0x7fffffff,cz0=0x7fffffff,cz1=-0x7fffffff;
        int ux0=0x7fffffff,ux1=-0x7fffffff,uz0=0x7fffffff,uz1=-0x7fffffff;
        for(int k=1;k<=22;k++) {
            const int ux=(int)((c.u0+(c.u1-c.u0)*(k+.5f)/24)*sheet.uw),uz=(int)((c.v0+(c.v1-c.v0)*(k+.5f)/24)*sheet.uh);
            const int tx=std::max(0,std::min(sheet.w-1,ux)),tz=std::max(0,std::min(sheet.h-1,uz));
            cx0=std::min(cx0,tx);cx1=std::max(cx1,tx);cz0=std::min(cz0,tz);cz1=std::max(cz1,tz);
            ux0=std::min(ux0,ux);ux1=std::max(ux1,ux);uz0=std::min(uz0,uz);uz1=std::max(uz1,uz);
        }
        long long lo=std::min((long long)cz0*W+cx0,(long long)uz0*W+ux0),hi=std::max((long long)cz1*W+cx1,(long long)uz1*W+ux1);
        lo=std::max(0LL,lo-W);hi=std::min(last,hi+W);
        Cand one={(int)i,slot,lo,hi,false,false,-1.0f,0,0};
        cand.push_back(one);
    }
    if(cand.empty())return;
    std::vector<int> order(cand.size());
    for(size_t k=0;k<order.size();k++)order[k]=(int)k;
    std::sort(order.begin(),order.end(),[&](int a,int b){return cand[a].lo<cand[b].lo || (cand[a].lo==cand[b].lo && a<b);});
    int span=rows;
    for(const Cand& c:cand)span=std::max(span,(int)(c.hi/W-c.lo/W+1));
    std::vector<unsigned char> win((size_t)W*span*4);
    long long book=(long long)win.size()+(long long)(cand.capacity()*sizeof(Cand)+order.size()*sizeof(int));
    held(book);
    long long winLo=0,winTexels=W*span,outside=0;
    /* every read lands in the window by construction; a position the arithmetic puts
       outside it is held to the window's edge and counted */
    const auto px=[&](size_t i)->const unsigned char*{
        long long at=(long long)i-winLo;
        if(at<0 || at>=winTexels){outside++;at=at<0?0:winTexels-1;}
        return &win[(size_t)at*4];
    };
    for(size_t k=0;k<order.size();k++) {
        if(cand[order[k]].done)continue;
        const int y0=(int)(cand[order[k]].lo/W);
        const int n=std::min(std::max(rows,(int)(cand[order[k]].hi/W)-y0+1),(int)H-y0);
        rd.rows(y0,n,&win[0]);
        winLo=(long long)y0*W;
        const long long winHi=(long long)(y0+n)*W-1;
        for(size_t j=k;j<order.size();j++) {
            Cand& c=cand[order[j]];
            if(c.lo>winHi)break;
            if(c.done || c.lo<winLo || c.hi>winHi)continue;
            c.any=water_bed_cell_best(px,masks,c.slot,cells[c.cell],sheet,&c.best,&c.bx,&c.bz);
            c.done=true;
        }
    }
    int chosen=-1,bx=0,bz=0;float best=-1;
    for(const Cand& c:cand)if(c.any && c.best>best){best=c.best;chosen=c.cell;bx=c.bx;bz=c.bz;}
    if(chosen>=0) {
        const PackCell& c=cells[chosen];
        long long lo=last,hi=0;
        for(int z=0;z<64;z++)for(int x=0;x<64;x++) {
            int tx=0,tz=0;water_bed_detail_at(c,sheet,bx,bz,x,z,&tx,&tz);
            const long long at=(long long)tz*W+tx;lo=std::min(lo,at);hi=std::max(hi,at);
        }
        lo=std::max(0LL,lo-W);hi=std::min(last,hi+W);
        const int y0=(int)(lo/W),n=(int)(hi/W)-y0+1;
        if(n>span){held((long long)W*(n-span)*4);book+=(long long)W*(n-span)*4;win.resize((size_t)W*n*4);winTexels=W*n;span=n;}
        rd.rows(y0,n,&win[0]);winLo=(long long)y0*W;
        water_bed_material_finish(px,sheet,cells,chosen,bx,bz);
    }
    if(outside)memledger_add("water_bed_outside",outside,MEMLEDGER_COUNT);
    held(-book);
}
#endif
