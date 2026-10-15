#pragma once

#include "menu_hint_3ds.hpp"
#include <vector>
#include <cmath>

namespace mk64_3ds {
// Compose original lettering from the owner-provided archive at UI startup.
// Only owned decoded copies cross resource lookups; no Nintendo pixels are embedded.
constexpr unsigned kRandomHintKeyWidth = 44;
using RandomHintPixels = std::array<uint32_t,kRandomHintWidth*kRandomHintHeight>;
using AutoHintPixels = std::array<uint32_t,kAutoHintWidth*kAutoHintHeight>;
constexpr std::array<std::array<unsigned,2>,28> kHintCornerPixels{{
    {46,3},{46,16},{53,3},{53,16},{56,16},{61,16},{70,3},{70,16},
    {85,3},{86,3},{86,16},{94,3},{94,16},{98,4},{98,15},{99,3},
    {99,16},{105,16},{106,15},{107,16},{110,3},{113,16},{114,3},
    {114,16},{118,16},{121,12},{122,3},{122,11}
}};
struct MenuHintSource {
    unsigned width=0, height=0;
    std::vector<uint32_t> pixels;
    uint32_t At(unsigned x, unsigned y) const { return pixels[y*width+x]; }
    bool Is(unsigned w, unsigned h) const { return width==w && height==h && pixels.size()==w*h; }
};
inline unsigned HintCoverage(uint32_t pixel) {
    return pixel>>24 ? std::max({pixel&255U,(pixel>>8)&255U,(pixel>>16)&255U}) : 0;
}
inline uint32_t HintScale(uint32_t pixel, double scale) {
    uint32_t out=pixel&0xff000000U;
    for (unsigned c=0;c<3;++c)
        out|=static_cast<uint32_t>(std::lround(((pixel>>(c*8))&255)*scale))<<(c*8);
    return out;
}
struct HintGlyph {
    unsigned width=0, height=0;
    std::array<uint32_t,8*13> pixels{};
    uint32_t& At(unsigned x,unsigned y) { return pixels[y*8+x]; }
    uint32_t At(unsigned x,unsigned y) const { return pixels[y*8+x]; }
};
inline HintGlyph HintCrop(const MenuHintSource& src,unsigned x0,unsigned y0,unsigned w,unsigned h) {
    HintGlyph g{w,h};
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) g.At(x,y)=src.At(x0+x,y0+y);
    return g;
}
inline uint32_t HintHue(const MenuHintSource& option,unsigned row) {
    const auto color=option.At(37,std::max(4U,row));
    return HintScale(color,255.0/HintCoverage(color));
}
template <typename Read>
bool MakeRandomCupHint(RandomHintPixels& out, Read read) {
    const auto option=read("texture_l_option"), data=read("texture_r_data");
    const auto mario=read("texture_mode_mario_gp"), begin=read("texture_begin");
    const auto trials=read("texture_mode_time_trials");
    if(!option.Is(58,19)||!data.Is(58,19)||!mario.Is(64,18)||!begin.Is(64,18)||!trials.Is(64,18)) return false;
    for(unsigned y=4;y<=15;++y) if(!HintCoverage(option.At(37,y))) return false;
    std::array<HintGlyph,26> glyphs{};
    const auto set=[&](char c,HintGlyph glyph){glyphs[c-'A']=glyph;};
    const auto get=[&](char c)->HintGlyph& {return glyphs[c-'A'];};
    set('R',HintCrop(mario,31,2,6,13)); set('A',HintCrop(data,31,4,7,13));
    set('N',HintCrop(option,47,3,8,13)); set('D',HintCrop(data,24,4,7,13));
    set('O',HintCrop(option,17,3,7,13)); set('M',HintCrop(mario,18,2,7,13));
    set('P',HintCrop(option,24,3,7,13)); set('G',HintCrop(begin,29,2,7,13));
    // Register DATA's upper fringe separately; keep every solid native row.
    for(char c: {'A','D'}) for(unsigned x=0;x<get(c).width;++x) {
        get(c).At(x,0)=0xff000000U;
        get(c).At(x,12)=HintScale(get(c).At(x,11),131.0/255);
    }
    for(char c: {'R','A','N','D','O','M','P','G'}) {
        auto& g=get(c);
        for(unsigned y=0;y<g.height;++y) for(unsigned x=0;x<g.width;++x) {
            const auto coverage=HintCoverage(g.At(x,y));
            g.At(x,y)=coverage ? HintScale(HintHue(option,y+3),coverage/255.0) : 0;
        }
    }
    set('C',get('G')); set('U',get('O'));
    for(unsigned y=6;y<9;++y) for(unsigned x=3;x<7;++x) get('C').At(x,y)=0;
    for(unsigned y: {1U,2U}) for(unsigned x=0;x<7;++x)
        get('U').At(x,y)=HintScale(HintHue(option,y+3),HintCoverage(get('O').At(x,3))/255.0);
    set('S',HintCrop(trials,55,3,6,12)); set('E',HintCrop(begin,24,3,5,12));
    set('L',HintCrop(trials,50,3,5,12)); set('T',HintCrop(option,31,4,5,12));
    for(unsigned y=0;y<10;++y) get('L').At(4,y)=0; // Neighbouring S fringe, not L's foot.
    out.fill(0xff000000U);
    const auto put=[&](unsigned x,unsigned y,uint32_t p){out[y*kRandomHintWidth+x]=(p >> 24) ? p : 0xff000000U;};
    // Original key bevel/corners, extended by its unlettered middle column.
    for(unsigned y=0;y<19;++y) for(unsigned x=0;x<kRandomHintKeyWidth;++x) {
        const unsigned sx=x<2 ? x : x>=kRandomHintKeyWidth-2 ? 14+x-(kRandomHintKeyWidth-2) : 2;
        put(x,y,option.At(sx,y));
    }
    // Retain the native thin text-recess border; its interior stays transparent.
    for(unsigned x=kRandomHintKeyWidth;x<kRandomHintWidth;++x) {
        const unsigned sx=x==kRandomHintKeyWidth ? 16 : x==kRandomHintWidth-1 ? 57 : 20;
        put(x,0,option.At(sx,0)); put(x,18,option.At(sx,18));
    }
    for(unsigned y=1;y<18;++y) {
        put(kRandomHintKeyWidth,y,option.At(16,y)); put(kRandomHintWidth-1,y,option.At(57,y));
    }
    unsigned cursor=3;
    for(char c: "SELECT") {
        if(!c) break;
        const auto& g=get(c);
        for(unsigned y=0;y<g.height;++y) {
            unsigned peak=0;
            for(unsigned x=0;x<g.width;++x) peak=std::max(peak,HintCoverage(g.At(x,y)));
            for(unsigned x=0;x<g.width;++x) if(const auto cover=HintCoverage(g.At(x,y));cover) {
                const auto white=static_cast<uint32_t>(std::lround(cover*255.0/peak));
                put(cursor+x,(g.height==13 ? 3:4)+y,0xff000000U|white|(white<<8)|(white<<16));
            }
        }
        cursor+=g.width+1;
    }
    RandomHintPixels face{};
    cursor=kRandomHintKeyWidth+3;
    for(char c: "RANDOM CUP") {
        if(!c) break;
        if(c==' ') {cursor+=4;continue;}
        const auto& g=get(c);
        for(unsigned y=0;y<g.height;++y) for(unsigned x=0;x<g.width;++x)
            if(HintCoverage(g.At(x,y))) face[(y+3)*kRandomHintWidth+cursor+x]=g.At(x,y);
        cursor+=g.width+1;
    }
    // Trace every face pixel, including its dim antialiased fringe. A one-pixel
    // cardinal contour follows the actual glyph without filling diagonal corners
    // or clipping it against a differently positioned source sprite's black plate.
    for(unsigned y=1;y<18;++y) for(unsigned x=kRandomHintKeyWidth+1;x<kRandomHintWidth-1;++x)
        if(face[y*kRandomHintWidth+x]) {
            put(x-1,y,0xff000000U); put(x+1,y,0xff000000U);
            put(x,y-1,0xff000000U); put(x,y+1,0xff000000U);
        }
    // Hand-finished diagonal corners from the marked-up native-art preview.
    // Keep this fixed layout's other corner cuts and all colored face pixels.
    for(const auto& p:kHintCornerPixels) put(p[0],p[1],0xff000000U);
    for(size_t i=0;i<face.size();++i) if(face[i]) out[i]=face[i];
    return true;
}
template<typename Read>
bool MakeAutoHint(AutoHintPixels& out, Read read) {
    RandomHintPixels reference{};
    if (!MakeRandomCupHint(reference,read)) return false;
    const auto option=read("texture_l_option");
    if (!option.Is(58,19)) return false;
    out.fill(0xff000000U);
    const auto put=[&](int x,int y,uint32_t p) {
        if (x>=0 && x<int(kAutoHintWidth) && y>=0 && y<int(kAutoHintHeight))
            out[y*kAutoHintWidth+x]=(p >> 24) ? p : 0xff000000U;
    };
    for (unsigned y=0;y<19;++y) for (unsigned x=0;x<44;++x)
        put(x,y,reference[y*kRandomHintWidth+x]);
    for (unsigned x=44;x<81;++x) {
        const unsigned sx=x==44 ? 16 : x==80 ? 57 : 20;
        put(x,0,option.At(sx,0)); put(x,18,option.At(sx,18));
    }
    for (unsigned y=1;y<18;++y) {
        put(44,y,option.At(16,y)); put(80,y,option.At(57,y));
    }
    AutoHintPixels face{};
    const auto outline=[&](int x,int y) {if (x>44 && x<80 && y>0 && y<18) put(x,y,0xff000000U);};
    for (const auto& offsets : {std::array<int,2>{54,47}, {107,55}, {79,69}}) {
        const int src=offsets[0], dest=offsets[1];
        for (int y=3;y<16;++y) for (int x=src;x<src+7;++x) {
            const auto p=reference[y*kRandomHintWidth+x];
            if (HintCoverage(p)) face[y*kAutoHintWidth+x-src+dest]=p;
        }
        for (const auto& corner : kHintCornerPixels) {
            bool adjacent=false;
            for (int dy : {-1,1}) for (int dx : {-1,1}) {
                const int x=int(corner[0])+dx,y=int(corner[1])+dy;
                if (x>=src && x<src+7 && y>=3 && y<16 &&
                    HintCoverage(reference[y*kRandomHintWidth+x])) adjacent=true;
            }
            if (adjacent) outline(int(corner[0])-src+dest,corner[1]);
        }
    }
    for (unsigned y=4;y<16;++y) for (unsigned x=31;x<36;++x) {
        const auto coverage=HintCoverage(option.At(x,y));
        if (!coverage) continue;
        face[y*kAutoHintWidth+63+x-31]=HintScale(HintHue(option,y),coverage/255.0);
        for (int dy : {-1,1}) for (int dx : {-1,1}) outline(63+x-31+dx,y+dy);
    }
    for (int y=1;y<18;++y) for (int x=45;x<80;++x) if (face[y*kAutoHintWidth+x]) {
        outline(x-1,y); outline(x+1,y); outline(x,y-1); outline(x,y+1);
    }
    for (size_t i=0;i<face.size();++i) if (face[i]) out[i]=face[i];
    return true;
}
} // namespace mk64_3ds
