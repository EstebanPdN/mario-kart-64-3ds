#include "native_sprite_3ds.hpp"
#include "art/native_map_art_3ds.hpp"
#include "title_credit_native_3ds.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
    using namespace mk64_3ds;
    using namespace native_sprite;
    // Exact two-color interpolation, including a transparent texel with hidden
    // blue RGB. That hidden color must not bleed into the visible white edge.
    Image pair{2,1,{0xff0000ffU,0xff00ff00U}};
    auto scaled=ScaleBilinear(pair,3,1);
    assert(scaled.Valid() && scaled.At(0,0)==pair.At(0,0) && scaled.At(2,0)==pair.At(1,0));
    assert(scaled.At(1,0)==0xff008080U);
    pair.pixels={0xffffffffU,0x00ff0000U};
    scaled=ScaleBilinear(pair,3,1);
    assert(scaled.At(1,0)==0x80ffffffU && scaled.At(2,0)==0);
    Image source{8,8,std::vector<uint32_t>(64)};
    for(unsigned y=1;y<7;++y) {source.pixels[y*8+1]=0xffffffffU;source.pixels[y*8+6]=0xffffffffU;}
    const auto separated=ScaleBilinear(source,149,184);
    for(unsigned y=0;y<184;++y) assert(separated.At(74,y)==0);
    assert(!ScaleBilinear({},40,40).Valid());
    assert(!ScaleBilinear(source,513,40).Valid());
    assert(!ScaleBilinear(source,40,0).Valid());

    // A synthetic font fixture proves source-letter identity without embedding
    // any extracted Nintendo pixels. The y's face and drop shadow must extend
    // below the lowercase baseline and leave transparent canvas below them.
    Image copyright{124,17,std::vector<uint32_t>(124*17)};
    for(unsigned y=4;y<14;++y)for(unsigned x=62;x<121;++x) {
        unsigned shade=180+(x+y)%76;
        copyright.pixels[y*124+x]=0xff000000U|shade|(shade<<8)|(shade<<16);
    }
    for(const auto letter:"Nnode") {
        if(!letter)break;
        const auto glyph=credit_font::Letter(letter,copyright);
        unsigned x=letter=='N'?62:letter=='n'?76:letter=='o'?113:letter=='d'?105:89;
        unsigned y=letter=='N'||letter=='d'?4:6;
        for(unsigned yy=0;yy<glyph.height;++yy)for(unsigned xx=0;xx<glyph.width;++xx)
            assert(credit_font::At(glyph,xx,yy)==(copyright.At(x+xx,y+yy)&255));
    }
    const auto y=credit_font::Letter('y',copyright);
    bool descender=false;
    for(unsigned yy=8;yy<y.height;++yy)for(unsigned x=0;x<y.width;++x)descender|=credit_font::At(y,x,yy)>0;
    assert(descender);
    const auto credit=MakeNativeTitleCredit(copyright);
    assert(credit.Valid() && credit.width<400 && credit.height<=20);
    unsigned lastInk=0;bool white=false,black=false;
    for(unsigned yy=0;yy<credit.height;++yy)for(unsigned x=0;x<credit.width;++x) {
        const auto pixel=credit.At(x,yy);
        if(pixel>>24)lastInk=std::max(lastInk,yy);
        white|=(pixel&255)>200 && (pixel>>24)>0;
        black|=pixel==0xff000000U;
        if(yy==credit.height-1)assert(pixel==0);
    }
    assert(white && black && lastInk>=14 && lastInk+1<credit.height);
    assert(!MakeNativeTitleCredit(source).Valid());

    // Every authored course must decode at all three output sizes. Verify the
    // complete RLE stream and tintable coverage, including malformed streams.
    using namespace native_art;
    assert(sizeof(kCourseMaps)/sizeof(kCourseMaps[0])==20);
    for(const auto& course:kCourseMaps) {
        assert(!std::strncmp(course.resource,"minimap_",8));
        for(const auto& canvas:course.sizes) {
            const auto map=DecodeMask(canvas);
            assert(map.Valid() && map.width<=149 && map.height<=184);
            bool ink=false,padding=false;
            for(auto pixel:map.pixels) {
                const unsigned a=pixel>>24;
                assert(a%17==0 && (!a || (pixel&0xffffffU)==0xffffffU));
                ink|=a==255;padding|=a==0;
            }
            assert(ink && padding);
        }
    }
    const uint16_t empty[]={0},tooLong[]={0x005f},tooShort[]={0x003f};
    assert(!DecodeMask({2,2,empty,1}).Valid());
    assert(!DecodeMask({2,2,tooLong,1}).Valid());
    assert(!DecodeMask({2,2,tooShort,1}).Valid());
    assert(!DecodeMask({513,1,tooShort,1}).Valid());
    std::puts("PASS: bilinear color/alpha, transparent gaps, source lettering identity, complete descender/shadow and 60 bounded hand-authored map masks.");
}
