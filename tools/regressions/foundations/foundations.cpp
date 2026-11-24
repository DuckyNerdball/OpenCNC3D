#include <cassert>
#include <cstddef>
#include <cstdio>
#include <random>
#include "../../../game/foundation_pads.h"

static FoundationPad pad(const std::vector<unsigned char>& raw,int w,int x,int z,int width,int height,int rows=0)
{
    FoundationPad p={x,z,width,height,rows?rows:height,0};
    for(int dz=0;dz<=height;++dz)for(int dx=0;dx<=width;++dx)
        p.top=std::max(p.top,raw[(size_t)(z+dz)*(w+1)+x+dx]);
    return p;
}

// Independent reference: repeatedly propagate levels between intersecting
// rectangles. The implementation instead joins owners of shared grid corners.
static std::vector<unsigned char> levels(const std::vector<FoundationPad>& pads)
{
    std::vector<unsigned char> result;
    for(const auto& p:pads)result.push_back(p.top);
    bool changed;
    do {
        changed=false;
        for(size_t i=0;i<pads.size();++i)for(size_t j=0;j<i;++j) {
            const auto& a=pads[i];const auto& b=pads[j];
            if(a.x>b.x+b.w||b.x>a.x+a.w||a.z>b.z+b.h||b.z>a.z+a.h)continue;
            const unsigned char top=std::max(result[i],result[j]);
            changed=changed||result[i]!=top||result[j]!=top;
            result[i]=result[j]=top;
        }
    } while(changed);
    return result;
}

static void verify(const FoundationGrid& g,const std::vector<unsigned char>& raw,const std::vector<FoundationPad>& pads)
{
    const auto expected=levels(pads);
    for(size_t i=0;i<pads.size();++i) {
        const auto& p=pads[i];
        for(int z=p.z;z<p.z+p.h;++z)for(int x=p.x;x<p.x+p.w;++x)
            for(int dz=0;dz<2;++dz)for(int dx=0;dx<2;++dx)
                assert(g.cell_corner(raw.data(),x,z,x+dx,z+dz)==expected[i]);
    }
    for(int z=0;z<=g.h;++z)for(int x=0;x<=g.w;++x) {
        const auto height=g.at(raw.data(),x,z);
        assert(height>=raw[(size_t)z*(g.w+1)+x]);
        if(x<g.w&&z<g.h)for(int axis=0;axis<2;++axis) {
            const int nx=x+(axis==0),nz=z+(axis==1);
            if(nx>=g.w||nz>=g.h)continue;
            const int cx=x+(axis==0),cz=z+(axis==1);
            for(int end=0;end<2;++end) {
                const int px=cx+end*(axis==1),pz=cz+end*(axis==0);
                assert(g.cell_corner(raw.data(),x,z,px,pz)==g.cell_corner(raw.data(),nx,nz,px,pz));
            }
        }
    }
    assert(g.steps(raw.data()).empty());
}

int main()
{
    int cases=0;
    const int w=16,h=16;std::vector<unsigned char> raw((w+1)*(h+1),64);
    raw[4*(w+1)+7]=128;
    const auto low=pad(raw,w,3,3,2,2),high=pad(raw,w,5,3,2,2);
    FoundationGrid a,b;
    a.build(w,h,raw.data(),{low,high});verify(a,raw,{low,high});
    assert(a.at(raw.data(),3,3)==128);++cases;
    a.build(w,h,raw.data(),{low});verify(a,raw,{low});
    assert(a.at(raw.data(),3,3)==64);++cases;
    // Aprons slope into the surrounding ground, including beside lower pads.
    auto apron=pad(raw,w,3,1,2,2,3),below=pad(raw,w,3,3,2,2);
    a.build(w,h,raw.data(),{apron,below});verify(a,raw,{apron,below});++cases;
    a.build(w,h,raw.data(),{});verify(a,raw,{});
    for(int z=0;z<=h;++z)for(int x=0;x<=w;++x)assert(a.at(raw.data(),x,z)==raw[z*(w+1)+x]);++cases;
    // A connected plateau joins the original ground through a three-cell skirt.
    std::vector<unsigned char> plain((w+1)*(h+1),64);plain[7*(w+1)+7]=128;
    auto raised=pad(plain,w,6,6,2,2,3);a.build(w,h,plain.data(),{raised});verify(a,plain,{raised});
    assert(a.at(plain.data(),3,7)==64);
    assert(a.at(plain.data(),4,7)>64);
    assert(a.at(plain.data(),5,7)>a.at(plain.data(),4,7));
    assert(a.at(plain.data(),6,7)==128);++cases;
    // Randomly sized nonoverlapping footprints can touch by an edge or corner.
    std::mt19937 rng(4242);
    for(int trial=0;trial<100;++trial) {
        for(auto& height:raw)height=(unsigned char)(32+rng()%192);
        const auto original=raw;std::vector<FoundationPad> pads;
        for(int z=1;z<=13;z+=3)for(int x=1;x<=13;x+=3)
            pads.push_back(pad(raw,w,x,z,2+rng()%2,2+rng()%2));
        std::shuffle(pads.begin(),pads.end(),rng);
        for(size_t n=1;n<=pads.size();++n) {
            std::vector<FoundationPad> subset(pads.begin(),pads.begin()+n);
            a.build(w,h,raw.data(),subset);verify(a,raw,subset);
            std::reverse(subset.begin(),subset.end());b.build(w,h,raw.data(),subset);
            assert(a.corner==b.corner&&a.on==b.on&&a.cell==b.cell&&a.kind==b.kind);
            assert(raw==original);++cases;
        }
    }
    const int maximum=256;raw.assign((maximum+1)*(maximum+1),64);raw.back()=255;
    const auto edge=pad(raw,maximum,254,254,2,2);a.build(maximum,maximum,raw.data(),{edge});verify(a,raw,{edge});++cases;
    puts("FOUNDATIONS|connected levels, continuous edges, smooth skirts, order independence, removal, raw map preserved");
    printf("FOUNDATIONS|cases=%d|PASS\n",cases);
}
