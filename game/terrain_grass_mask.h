#ifndef OPENCNC3D_TERRAIN_GRASS_MASK_H
#define OPENCNC3D_TERRAIN_GRASS_MASK_H
#include <map>
#include "ground_mem.h"   /* the read's measured peak and its restore switch */

/* Explicit ground coverage from the Remaster Lab's saved Outline Checks.
   No surface is classified from RGB at runtime. The two-channel pack has:
   C3DGRAS\0, LE u32 version=1, cell, count; count 24-byte keys (name[16],
   u16 template, u16 icon, u8 variant, u8 game (0 TD/1 RA), u16 reserved),
   then count cell*cell LA8 images. L is grass coverage; A is baked ground
   shade in units of 1/128. Slot zero is neutral white for ordinary CLEAR1. */
struct TerrainGrassMasks {
    bool tried=false;
    std::string theater;
    GLuint texture=0;
    int cell=1,pitch=1,columns=1,width=1,height=1,gutter=0;
    std::vector<unsigned char> coverage; // authored mask downsampled to 24x24, about 1.2 MiB
    std::map<unsigned,int> td,td3d;
    std::map<unsigned,bool> softBoundary; // authored shore/river faces can meet ordinary ground
    std::map<std::string,int> ra;
};
static TerrainGrassMasks g_terrainGrassMasks;

static std::string terrain_grass_ra_key(const char* name,int icon,int variant)
{ char end[40];snprintf(end,sizeof end,"/%d/%d",icon,variant);return std::string(name)+end; }

/* One 24-byte key of the pack, for atlas slot `slot`, into M's maps; false for a key
   that makes the whole pack invalid. */
static bool terrain_grass_mask_key(TerrainGrassMasks& M,const unsigned char* key,unsigned slot)
{
    char name[17];memcpy(name,key,16);name[16]=0;
    const unsigned tid=(unsigned)key[16]|((unsigned)key[17]<<8);
    const unsigned icon=(unsigned)key[18]|((unsigned)key[19]<<8),var=key[20];
    if(var>3 || key[21]>1 || !*name)return false;
    if(key[22]==1 && key[21]==0)M.td3d[(tid<<16)|(icon<<2)|var]=(int)slot;
    else if(key[21])M.ra[terrain_grass_ra_key(name,(int)icon,(int)var)]=(int)slot;
    else M.td[(tid<<16)|(icon<<2)|var]=(int)slot;
    if(!key[21] && (!strncmp(name,"SH",2) || !strncmp(name,"RV",2)
        || !strncmp(name,"FORD",4) || !strncmp(name,"FALLS",5)))M.softBoundary[tid]=true;
    return true;
}

/* THE PACK READ WHOLE, as it always was (OPENCNC3D_PACK_WHOLE_READ=1): the file in one
   vector, then the atlas laid out from it. Fills M, `pixels` (the atlas, or left alone)
   and *count; true for a valid pack. *had says the file was read at all (an unreadable
   or out-of-range file is no pack, not an invalid one). */
static bool terrain_grass_masks_read_whole(FILE* f,TerrainGrassMasks& M,std::vector<unsigned char>& pixels,
                                           unsigned* countOut,bool* had)
{
    std::vector<unsigned char> data;
    if(fseek(f,0,SEEK_END)==0) {
        const long size=ftell(f);
        if(size>=20 && size<=192L*1024*1024 && fseek(f,0,SEEK_SET)==0) {
            data.resize((size_t)size);
            if(fread(&data[0],1,data.size(),f)!=data.size())data.clear();
        }
    }
    ground_mem_note();
    *had=!data.empty();
    unsigned count=0,cell=0;
    bool valid=data.size()>=20 && !memcmp(&data[0],"C3DGRAS\0",8) && cb_u32(&data[8])==1;
    if(valid) {
        cell=cb_u32(&data[12]);count=cb_u32(&data[16]);
        valid=cell>=24 && cell<=240 && count>0 && count<=4096
            && data.size()==20+(size_t)count*24+(size_t)count*cell*cell*2;
    }
    if(valid) {
        M.cell=(int)cell;M.gutter=2;M.pitch=M.cell+4;
        M.columns=(int)std::ceil(std::sqrt((double)count+1));
        M.width=M.columns*M.pitch;M.height=((int)count+M.columns)/M.columns*M.pitch;
        GLint maxSize=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxSize);
        valid=M.width<=maxSize && M.height<=maxSize;
    }
    if(valid) {
        memfail_point("grassmask");
        M.coverage.assign(((size_t)count+1)*24*24,255);
        for(unsigned slot=1;slot<=count;slot++)for(int y=0;y<24;y++)for(int x=0;x<24;x++) {
            unsigned sum=0,samples=0;
            const int x0=x*(int)cell/24,x1=(x+1)*(int)cell/24,y0=y*(int)cell/24,y1=(y+1)*(int)cell/24;
            for(int sy=y0;sy<y1;sy++)for(int sx=x0;sx<x1;sx++) {
                sum+=data[20+(size_t)count*24+((size_t)(slot-1)*cell*cell+sy*cell+sx)*2];samples++;
            }
            M.coverage[((size_t)slot*24+y)*24+x]=(unsigned char)(sum/std::max(1u,samples));
        }
        pixels.assign((size_t)M.width*M.height*2,0);
        ground_mem_note();
        for(unsigned slot=0;slot<=count;++slot) {
            const int gx=(int)(slot%M.columns)*M.pitch,gy=(int)(slot/M.columns)*M.pitch;
            for(int y=0;y<M.pitch;++y)for(int x=0;x<M.pitch;++x) {
                const int sx=std::max(0,std::min(M.cell-1,x-M.gutter));
                const int sy=std::max(0,std::min(M.cell-1,y-M.gutter));
                unsigned char* dest=&pixels[((size_t)(gy+y)*M.width+gx+x)*2];
                if(!slot){dest[0]=255;dest[1]=128;}
                else {
                    const size_t offset=20+(size_t)count*24+((size_t)(slot-1)*cell*cell+sy*cell+sx)*2;
                    dest[0]=data[offset];dest[1]=data[offset+1];
                }
            }
            if(!slot)continue;
            const unsigned char* key=&data[20+(size_t)(slot-1)*24];
            if(!terrain_grass_mask_key(M,key,slot)){valid=false;break;}
        }
    }
    *countOut=count;
    return valid;
}

/* THE PACK READ SLOT BY SLOT: the header and the keys, then each slot's cell x cell LA8
   picture read straight into its place in the atlas (and its coverage), so only the
   atlas and one slot are ever held, never the whole file. The same atlas, maps and
   coverage as the whole read, byte for byte; a read that fails part way is no pack. */
static bool terrain_grass_masks_read_slots(FILE* f,TerrainGrassMasks& M,std::vector<unsigned char>& pixels,
                                           unsigned* countOut,bool* had)
{
    *had=false;*countOut=0;
    if(fseek(f,0,SEEK_END)!=0)return false;
    const long size=ftell(f);
    if(size<20 || size>192L*1024*1024 || fseek(f,0,SEEK_SET)!=0)return false;
    unsigned char head[20];
    if(fread(head,1,sizeof head,f)!=sizeof head)return false;
    *had=true;
    unsigned count=0,cell=0;
    bool valid=!memcmp(head,"C3DGRAS\0",8) && cb_u32(head+8)==1;
    if(valid) {
        cell=cb_u32(head+12);count=cb_u32(head+16);
        valid=cell>=24 && cell<=240 && count>0 && count<=4096
            && (size_t)size==20+(size_t)count*24+(size_t)count*cell*cell*2;
    }
    *countOut=count;
    if(valid) {
        M.cell=(int)cell;M.gutter=2;M.pitch=M.cell+4;
        M.columns=(int)std::ceil(std::sqrt((double)count+1));
        M.width=M.columns*M.pitch;M.height=((int)count+M.columns)/M.columns*M.pitch;
        GLint maxSize=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxSize);
        valid=M.width<=maxSize && M.height<=maxSize;
    }
    if(!valid)return false;
    std::vector<unsigned char> keys((size_t)count*24);
    if(fread(&keys[0],1,keys.size(),f)!=keys.size()){*had=false;return false;}
    memfail_point("grassmask");
    M.coverage.assign(((size_t)count+1)*24*24,255);
    pixels.assign((size_t)M.width*M.height*2,0);
    std::vector<unsigned char> one((size_t)cell*cell*2);
    ground_mem_note();
    for(int y=0;y<M.pitch;++y)for(int x=0;x<M.pitch;++x) {
        unsigned char* dest=&pixels[((size_t)y*M.width+x)*2];
        dest[0]=255;dest[1]=128;
    }
    for(unsigned slot=1;slot<=count;++slot) {
        if(fread(&one[0],1,one.size(),f)!=one.size()){*had=false;return false;}
        for(int y=0;y<24;y++)for(int x=0;x<24;x++) {
            unsigned sum=0,samples=0;
            const int x0=x*(int)cell/24,x1=(x+1)*(int)cell/24,y0=y*(int)cell/24,y1=(y+1)*(int)cell/24;
            for(int sy=y0;sy<y1;sy++)for(int sx=x0;sx<x1;sx++) {
                sum+=one[((size_t)sy*cell+sx)*2];samples++;
            }
            M.coverage[((size_t)slot*24+y)*24+x]=(unsigned char)(sum/std::max(1u,samples));
        }
        const int gx=(int)(slot%M.columns)*M.pitch,gy=(int)(slot/M.columns)*M.pitch;
        for(int y=0;y<M.pitch;++y)for(int x=0;x<M.pitch;++x) {
            const int sx=std::max(0,std::min(M.cell-1,x-M.gutter));
            const int sy=std::max(0,std::min(M.cell-1,y-M.gutter));
            unsigned char* dest=&pixels[((size_t)(gy+y)*M.width+gx+x)*2];
            const size_t offset=((size_t)sy*cell+sx)*2;
            dest[0]=one[offset];dest[1]=one[offset+1];
        }
        if(!terrain_grass_mask_key(M,&keys[(size_t)(slot-1)*24],slot))return false;
    }
    return true;
}

static void terrain_grass_masks_ensure(void)
{
    TerrainGrassMasks& M=g_terrainGrassMasks;
    const std::string theater = g_raModule && !strcmp(g_pack.theater,"RASNOW") ? "RA_SNOW"
        : !strcmp(g_pack.theater,"DESERT") ? "DESERT" : !strcmp(g_pack.theater,"SAND") ? "SAND" : "TEMPERAT";
    if(M.tried && M.theater==theater)return;
    if(M.texture)glDeleteTextures(1,&M.texture);
    M=TerrainGrassMasks(); M.theater=theater; M.tried=true;
    GroundMemPeak peak("mask_cpu_peak");
    const char* overridePath=getenv("OPENCNC3D_GRASSMASK");
    const bool disabled=overridePath && (!strcmp(overridePath,"off") || !strcmp(overridePath,"none"));
    std::string path;
    FILE* f=NULL;
    if(!disabled) {
        const char* roots[]={"","game/","../game/","../"};
        for(size_t i=0;i<sizeof roots/sizeof roots[0];++i) {
            path=(overridePath && *overridePath)?overridePath:std::string(roots[i])+(theater=="RA_SNOW"?"groundmask_RA_SNOW.pack":theater=="DESERT"?"groundmask_DESERT.pack":theater=="SAND"?"groundmask_SAND.pack":"grassmask_TEMPERAT.pack");
            f=fopen(path.c_str(),"rb");
            if(f)break;
        }
    }
    unsigned count=0;
    bool valid=false,had=false,caught=false;
    std::vector<unsigned char> pixels(2);pixels[0]=255;pixels[1]=128;
    /* A MASK THAT DOES NOT FIT IN MEMORY is drawn as no mask (neutral, as a missing pack
       is), and says so; where it fits nothing here changes what is built. */
    try {
        if(f)valid=ground_pack_whole_read()?terrain_grass_masks_read_whole(f,M,pixels,&count,&had)
                                           :terrain_grass_masks_read_slots(f,M,pixels,&count,&had);
    } catch(const std::bad_alloc&) {
        valid=false;had=false;caught=true;
    }
    if(f)fclose(f);
    if(caught) {
        printf("MEM|caught|grassmask|theater=%s|then=neutral\n",theater.c_str());
        fflush(stdout);
    }
    if(!valid) {
        M.coverage.clear();M.td.clear();M.td3d.clear();M.ra.clear();M.softBoundary.clear();M.cell=M.pitch=M.columns=M.width=M.height=1;M.gutter=0;
        std::vector<unsigned char>(2).swap(pixels);pixels[0]=255;pixels[1]=128;
        if(had)fprintf(stderr,"GRASSMASK|invalid pack|%s\n",path.c_str());
    }
    GLint active=GL_TEXTURE0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    glPushAttrib(GL_TEXTURE_BIT);glActiveTexture(GL_TEXTURE0);
    glGenTextures(1,&M.texture);glBindTexture(GL_TEXTURE_2D,M.texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    GLint unpack=4;glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE_ALPHA,M.width,M.height,0,GL_LUMINANCE_ALPHA,GL_UNSIGNED_BYTE,&pixels[0]);
    glPixelStorei(GL_UNPACK_ALIGNMENT,unpack);glPopAttrib();glActiveTexture((GLenum)active);
    if(theater=="DESERT")fprintf(stderr,"GROUNDMASK|desert|cells=%u|neutral=%d|valid=%d\n",valid?count:0,had?0:1,valid?1:0);
    else fprintf(stderr,"GRASSMASK|loaded|cells=%u|td=%lu|ra=%lu|atlas=%dx%d|valid=%d\n",
            valid?count:0,(unsigned long)M.td.size(),(unsigned long)M.ra.size(),M.width,M.height,valid?1:0);
}

static int terrain_grass_mask_td(unsigned tid,unsigned icon,int variant)
{
    terrain_grass_masks_ensure();
    if(desert_detail_replaces(tid)) {
        const auto& alt=g_terrainGrassMasks.td3d;auto i=alt.find((tid<<16)|(icon<<2)|(unsigned)variant);
        if(i!=alt.end())return i->second;
    }
    const std::map<unsigned,int>& m=g_terrainGrassMasks.td;
    std::map<unsigned,int>::const_iterator it=m.find((tid<<16)|(icon<<2)|(unsigned)variant);
    return it==m.end()?-1:it->second;
}
static int terrain_grass_mask_ra(const char* name,int icon,int variant)
{
    terrain_grass_masks_ensure();
    const std::map<std::string,int>& m=g_terrainGrassMasks.ra;
    std::map<std::string,int>::const_iterator it=m.find(terrain_grass_ra_key(name,icon,variant));
    return it==m.end()?-1:it->second;
}
static void terrain_grass_mask_uv(int slot,float x,float y,float* out)
{
    const TerrainGrassMasks& M=g_terrainGrassMasks;
    if(slot<0)slot=0;
    out[0]=((slot%M.columns)*M.pitch+M.gutter+x*M.cell)/(float)M.width;
    out[1]=((slot/M.columns)*M.pitch+M.gutter+y*M.cell)/(float)M.height;
}
#endif
