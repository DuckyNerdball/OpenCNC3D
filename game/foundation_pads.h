#ifndef OPENCNC3D_FOUNDATION_PADS_H
#define OPENCNC3D_FOUNDATION_PADS_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

/* Touching building footprints share one level, derived from their original
   map heights. This is the user's 3 November 2026 smooth-ground policy: building
   elevations may adjust when a neighbouring foundation joins or leaves a group.
   Shared terrain corners eliminate vertical foundation seams. A smooth radial
   skirt blends each platform into the original surrounding ground. */
struct FoundationPad {
    int x,z,w,h,rows;
    unsigned char top;
};
struct FoundationStep {
    int x,z,axis; // axis 0 east edge, 1 south edge
    unsigned char a0,a1,b0,b1;
    void vertices(float base,float p[4][3]) const {
        const float x0=(float)(x+(axis==0)),z0=(float)(z+(axis==1));
        const float x1=x0+(axis==1),z1=z0+(axis==0);
        const float q[4][3]={{x0,std::min(a0,b0)*(1.0f/64.0f)-base,z0},
                             {x1,std::min(a1,b1)*(1.0f/64.0f)-base,z1},
                             {x0,std::max(a0,b0)*(1.0f/64.0f)-base,z0},
                             {x1,std::max(a1,b1)*(1.0f/64.0f)-base,z1}};
        for(int i=0;i<4;++i)for(int k=0;k<3;++k)p[i][k]=q[i][k];
    }
};
struct FoundationGrid {
    int w=0,h=0;
    std::vector<unsigned char> corner,on,foot,cell,kind; // kind: 0 ground, 1 apron, 2 footprint

    void build(int width,int height,const unsigned char* raw,const std::vector<FoundationPad>& sourcePads) {
        w=width;h=height;
        corner.assign((size_t)(w+1)*(h+1),0);on.assign(corner.size(),0);foot.assign(corner.size(),0);
        cell.assign((size_t)w*h,0);kind.assign(cell.size(),0);
        std::vector<FoundationPad> pads=sourcePads;
        std::vector<int> parent(pads.size()),owner(corner.size(),-1);
        for(size_t i=0;i<parent.size();++i)parent[i]=(int)i;
        const auto root=[&](int i) {while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
        for(size_t i=0;i<pads.size();++i) {
            const FoundationPad& p=pads[i];
            for(int z=std::max(0,p.z);z<=std::min(h,p.z+p.h);++z)
                for(int x=std::max(0,p.x);x<=std::min(w,p.x+p.w);++x) {
                    const size_t k=(size_t)z*(w+1)+x;
                    if(owner[k]<0)owner[k]=(int)i;
                    else {const int a=root((int)i),b=root(owner[k]);if(a!=b)parent[std::max(a,b)]=std::min(a,b);}
                }
        }
        std::vector<unsigned char> level(pads.size(),0);
        for(size_t i=0;i<pads.size();++i)level[root((int)i)]=std::max(level[root((int)i)],pads[i].top);
        for(size_t i=0;i<pads.size();++i)pads[i].top=level[root((int)i)];
        for(const FoundationPad& p:pads) {
            for(int dz=0;dz<=p.h;++dz)for(int dx=0;dx<=p.w;++dx) {
                const int x=p.x+dx,z=p.z+dz;if(x<0||z<0||x>w||z>h)continue;
                const size_t k=(size_t)z*(w+1)+x;
                on[k]=foot[k]=1;corner[k]=p.top;
            }
            for(int dz=0;dz<p.rows;++dz)for(int dx=0;dx<p.w;++dx) {
                const int x=p.x+dx,z=p.z+dz;if(x<0||z<0||x>=w||z>=h)continue;
                const size_t k=(size_t)z*w+x;const unsigned char claim=dz<p.h?2:1;
                if(claim>kind[k]||(claim==kind[k]&&p.top>cell[k])) {kind[k]=claim;cell[k]=p.top;}
            }
        }
        // With platform levels established, the exposed ground approaches them
        // through a three-cell smooth skirt instead of a flat apron and wall.
        // Integer accumulation makes overlapping skirts independent of pad order.
        std::vector<uint64_t> rise(corner.size(),0), weight(corner.size(),0);
        const int radius=3, radius2=radius*radius;
        const uint64_t full=(uint64_t)radius2*radius2*radius2;
        for(const FoundationPad& p:pads) {
            for(int z=std::max(0,p.z-radius);z<=std::min(h,p.z+p.h+radius);++z)
                for(int x=std::max(0,p.x-radius);x<=std::min(w,p.x+p.w+radius);++x) {
                    const size_t k=(size_t)z*(w+1)+x;
                    if(foot[k] || p.top<=raw[k])continue;
                    const int dx=std::max(p.x-x,std::max(0,x-(p.x+p.w)));
                    const int dz=std::max(p.z-z,std::max(0,z-(p.z+p.h)));
                    const int distance2=dx*dx+dz*dz;
                    if(distance2>=radius2)continue;
                    const uint64_t q=radius2-distance2;
                    const uint64_t falloff=q*q*(radius2+2*distance2);
                    rise[k]+=(uint64_t)(p.top-raw[k])*falloff*falloff;
                    weight[k]+=falloff;
                }
        }
        for(size_t k=0;k<corner.size();++k)if(weight[k]) {
            const uint64_t denominator=weight[k]*full;
            const unsigned lift=(unsigned)((rise[k]+denominator/2)/denominator);
            if(lift) {on[k]=1;corner[k]=(unsigned char)(raw[k]+lift);}
        }
    }
    unsigned char at(const unsigned char* raw,int x,int z) const {
        const size_t k=(size_t)z*(w+1)+x;return on[k]?corner[k]:raw[k];
    }
    // Kept as the common terrain-reader interface. Every adjoining cell now
    // sees the same shared corner; no cell has an independent vertical plane.
    unsigned char cell_corner(const unsigned char* raw,int,int,int cx,int cz) const {
        return at(raw,cx,cz);
    }
    std::vector<FoundationStep> steps(const unsigned char* raw) const {
        std::vector<FoundationStep> out;
        for(int z=0;z<h;++z)for(int x=0;x<w;++x)for(int axis=0;axis<2;++axis) {
            const int nx=x+(axis==0),nz=z+(axis==1);
            if(nx>=w||nz>=h||(!kind[(size_t)z*w+x]&&!kind[(size_t)nz*w+nx]))continue;
            const int x0=x+(axis==0),z0=z+(axis==1),x1=x0+(axis==1),z1=z0+(axis==0);
            FoundationStep s={x,z,axis,cell_corner(raw,x,z,x0,z0),cell_corner(raw,x,z,x1,z1),
                              cell_corner(raw,nx,nz,x0,z0),cell_corner(raw,nx,nz,x1,z1)};
            if(s.a0!=s.b0||s.a1!=s.b1)out.push_back(s);
        }
        return out;
    }
};

#endif
