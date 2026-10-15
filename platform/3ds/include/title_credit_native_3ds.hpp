#pragma once

#include "native_sprite_3ds.hpp"
#include <cstring>

namespace mk64_3ds {
// Reuse the actual copyright lettering from the owner's archive. Missing
// letters are assembled from that same face's stems, bowls and crossbars.
// This header contains geometry/operations only, never extracted font pixels.
namespace credit_font {
using native_sprite::Image;
struct Glyph { unsigned width=0, top=0, height=0; std::vector<uint8_t> face; };
inline uint8_t At(const Glyph& g,unsigned x,unsigned y) {
    return x<g.width && y<g.height ? g.face[size_t(y)*g.width+x] : 0;
}
inline void Put(Glyph& g,unsigned x,unsigned y,unsigned value) {
    if(x<g.width && y<g.height) g.face[size_t(y)*g.width+x]=std::max<unsigned>(At(g,x,y),value);
}
inline Glyph Empty(unsigned width,unsigned height,unsigned top=0) {
    return {width,top,height,std::vector<uint8_t>(width*height)};
}
inline Glyph Crop(const Image& image,unsigned x,unsigned y,unsigned width,unsigned height,unsigned top) {
    auto g=Empty(width,height,top);
    for(unsigned yy=0;yy<height;++yy) for(unsigned xx=0;xx<width;++xx) {
        const auto pixel=image.At(x+xx,y+yy);
        // The copyright is grayscale RGBA16. Its RGB is the actual face
        // coverage over the existing black contour, including gray edge pixels.
        Put(g,xx,yy,((pixel&255U)*(pixel>>24)+127)/255);
    }
    return g;
}
inline Glyph Letter(char c,const Image& source) {
    const auto n=Crop(source,76,6,7,8,2), o=Crop(source,113,6,8,8,2);
    const auto d=Crop(source,105,4,8,10,0), e=Crop(source,89,6,8,8,2);
    const auto cap=Crop(source,62,4,9,10,0);
    switch(c) {
        case 'N':return cap;
        case 'n':return n;
        case 'o':return o;
        case 'd':return d;
        case 'e':return e;
        case 't':return Crop(source,84,4,5,10,0);
        case ' ':return Empty(4,10);
        case 'b': {
            auto g=Empty(8,10);
            for(unsigned y=0;y<10;++y)for(unsigned x=0;x<8;++x)Put(g,x,y,At(d,7-x,y));
            return g;
        }
        case 'r': {
            auto g=Empty(5,8,2);
            for(unsigned y=0;y<8;++y)for(unsigned x=0;x<5;++x)
                Put(g,x,y,At(n,x,y)*(y<3 || x<3));
            return g;
        }
        case 'P': {
            auto g=Empty(8,10);
            for(unsigned y=0;y<10;++y)for(unsigned x=0;x<3;++x)Put(g,x,y,At(cap,x,y));
            // A full-height capital stem and a six-row bowl with the same
            // rounded corners as the original lowercase o.
            constexpr unsigned rows[]={0,1,2,4,6,7};
            for(unsigned y=0;y<6;++y)for(unsigned x=1;x<8;++x)Put(g,x,y,At(o,x,rows[y]));
            return g;
        }
        case 'E': {
            auto g=Empty(8,10);
            for(unsigned y=0;y<10;++y)for(unsigned x=0;x<3;++x)Put(g,x,y,At(cap,x,y));
            for(unsigned y:{0U,1U,2U,4U,5U,7U,8U})for(unsigned x=1;x<(y==4||y==5?7U:8U);++x) {
                unsigned value=At(cap,1,y==0?0:y==8?8:1);
                if(x==7)value=value*7/8;
                Put(g,x,y,value);
            }
            return g;
        }
        case 's': {
            auto g=Empty(8,8,2);
            for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x) {
                if(y<2 || y>=6)Put(g,x,y,At(o,x,y));
                else if((y<4 && x<3)||(y>=4 && x>=5))Put(g,x,y,At(o,x,y));
                if((y==3||y==4)&&x>0&&x<7)Put(g,x,y,std::max<unsigned>(At(e,x,3),At(e,x,4)));
            }
            return g;
        }
        case 'a': {
            auto g=Empty(8,8,2);
            for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x) {
                if(y<2 || y>=4)Put(g,x,y,At(o,x,y));
                if(x>=5)Put(g,x,y,At(d,x,y+2));
                if(y==3 || y==4)Put(g,x,y,std::max<unsigned>(At(e,x,3),At(e,x,4)));
            }
            return g;
        }
        case 'y': {
            auto g=Empty(8,12,2);
            // A descending y needs diagonal arms, not a flipped n/u. Draw
            // their coverage on the same native grid and match the cap's white.
            for(unsigned y=0;y<12;++y)for(unsigned x=0;x<8;++x) {
                unsigned covered=0;
                for(unsigned sy=0;sy<4;++sy)for(unsigned sx=0;sx<4;++sx) {
                    const float xx=x+(sx+.5f)/4, yy=y+(sy+.5f)/4;
                    const float left=xx-.35f*yy, right=xx+.34f*yy;
                    const bool arms=yy>=1 && yy<7 &&
                        ((left>=.2f&&left<2.6f)||(right>=5.3f&&right<7.8f));
                    const bool tail=yy>=6 && yy<10.2f && right>=5.3f&&right<7.8f;
                    // The hook has a rounded lower terminal; do not crop a
                    // rectangular foot at the old lowercase baseline.
                    const float dx=(xx-2.2f)/1.7f,dy=(yy-9.5f)/1.3f;
                    const bool foot=yy>=9 && dx*dx+dy*dy<=1;
                    covered+=arms||tail||foot;
                }
                Put(g,x,y,(covered*At(cap,1,1)+8)/16);
            }
            return g;
        }
        default:return Empty(4,10);
    }
}
} // namespace credit_font

inline native_sprite::Image MakeNativeTitleCredit(const native_sprite::Image& source) {
    if (!source.Valid() || source.width!=124 || source.height!=17) return {};
    using namespace credit_font;
    constexpr const char* text="Ported by EstebanPdN";
    unsigned width=3;
    for(const char* p=text;*p;++p)width+=Letter(*p,source).width+1;
    // Derive the canvas from every glyph plus its outline/drop shadow. A
    // descender must never depend on the copyright texture's fixed height.
    unsigned height=0;
    for(const char* p=text;*p;++p) {
        const auto g=Letter(*p,source);
        height=std::max(height,1+g.top+g.height+3);
    }
    Image result{width,height,std::vector<uint32_t>(size_t(width)*height)};
    std::vector<uint8_t> coverage(width*height);
    unsigned cursor=1;
    for(const char* p=text;*p;++p) {
        const auto g=Letter(*p,source);
        for(unsigned y=0;y<g.height;++y)for(unsigned x=0;x<g.width;++x)
            coverage[size_t(1+g.top+y)*width+cursor+x]=At(g,x,y);
        cursor+=g.width+1;
    }
    const auto black=[&](int x,int y) {
        if(x>=0&&y>=0&&unsigned(x)<width&&unsigned(y)<height)result.pixels[size_t(y)*width+x]=0xff000000U;
    };
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)if(coverage[size_t(y)*width+x]>=32) {
        black(x-1,y);black(x+1,y);black(x,y-1);black(x,y+1);black(x+1,y+2);
    }
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const unsigned value=coverage[size_t(y)*width+x];
        if(!value)continue;
        // Composite the original gray face over its black contour. Preserve
        // opaque coverage within that contour instead of tinting gray twice.
        auto& pixel=result.pixels[size_t(y)*width+x];
        pixel=pixel>>24 ? 0xff000000U|value|(value<<8)|(value<<16) : (value<<24)|0x00ffffffU;
    }
    return result;
}
} // namespace mk64_3ds
