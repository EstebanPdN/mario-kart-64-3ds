// Native-grid sprite preparation. Included inside the bottom UI's private owner.
// All archive-dependent pixels stay in owner-provided runtime data. Generation
// happens only while a resource is loaded, never while drawing or switching styles.

mk64_3ds::native_sprite::Image ReadUiPixels(const UiTexture& source) {
    mk64_3ds::native_sprite::Image image{source.width, source.height, {}};
    image.pixels.resize(size_t(image.width) * image.height);
    const auto* pixels = static_cast<const uint32_t*>(source.texture.data);
    for (unsigned y=0; y<image.height; ++y) for (unsigned x=0; x<image.width; ++x) {
        const size_t tile = (size_t(y/8) * (source.backingWidth/8) + x/8) * 64;
        image.pixels[size_t(y)*image.width+x] = __builtin_bswap32(pixels[tile+MortonOffset8x8(x%8,y%8)]);
    }
    return image;
}

void AddNativeVariant(UiTexture& destination, const mk64_3ds::native_sprite::Image& image,
                      bool alphaOnly = false, unsigned logicalWidth = 0, unsigned logicalHeight = 0,
                      Mk64UiTextures3DS style=MK64_UI_TEXTURES_3DS_IMPROVED) {
    if (!image.Valid() || destination.nativeCount == destination.native.size()) return;
    NativeUiVariant variant{};
    variant.style=style;
    const auto bw=NextPowerOfTwo(image.width), bh=NextPowerOfTwo(image.height);
    if (!C3D_TexInit(&variant.texture,bw,bh,alphaOnly ? GPU_LA4 : GPU_RGBA8)) return;
    const size_t bytes=size_t(bw)*bh*(alphaOnly ? 1 : 4);
    std::memset(variant.texture.data,0,bytes);
    for (unsigned y=0;y<image.height;++y) for (unsigned x=0;x<image.width;++x) {
        const size_t offset=(size_t(y/8)*(bw/8)+x/8)*64+MortonOffset8x8(x%8,y%8);
        const auto pixel=image.pixels[size_t(y)*image.width+x];
        if (alphaOnly) static_cast<uint8_t*>(variant.texture.data)[offset]=0xf0U | (pixel>>28);
        else static_cast<uint32_t*>(variant.texture.data)[offset]=__builtin_bswap32(pixel);
    }
    if (!Mk64System3DSCleanDataCache(variant.texture.data,bytes)) {
        C3D_TexDelete(&variant.texture); return;
    }
    variant.width=image.width; variant.height=image.height;
    variant.logicalWidth=logicalWidth ? logicalWidth : destination.width;
    variant.logicalHeight=logicalHeight ? logicalHeight : destination.height;
    variant.subTexture={variant.width,variant.height,0,1,float(image.width)/bw,1-float(image.height)/bh};
    C3D_TexSetFilter(&variant.texture,GPU_NEAREST,GPU_NEAREST);
    C3D_TexSetWrap(&variant.texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
    destination.native[destination.nativeCount++]=variant;
}

bool AddPackVariant(UiTexture& texture,const char* name,unsigned width,unsigned height,bool white=false) {
    const auto packed=mk64_3ds::improved::LocalPack().Load(name,width,height);
    if (!packed) return false;
    mk64_3ds::native_sprite::Image image{packed.width,packed.height,{}};
    image.pixels.resize(size_t(image.width)*image.height);
    const auto* p=reinterpret_cast<const uint8_t*>(packed.pixels->data());
    for (size_t i=0;i<image.pixels.size();++i) {
        const size_t at=packed.nativeGpuTiled?mk64_3ds::MortonTexel(i%64,63-i/64,64):i*4;
        image.pixels[i]=packed.nativeGpuTiled
            ? (uint32_t(p[at+3]) | uint32_t(p[at+2])<<8 | uint32_t(p[at+1])<<16 | uint32_t(p[at])<<24)
            : (uint32_t(p[at]) | uint32_t(p[at+1])<<8 | uint32_t(p[at+2])<<16 | uint32_t(p[at+3])<<24);
    }
    if (white) for(auto& pixel:image.pixels) pixel=mk64_3ds::WhiteMinimapPixel(pixel);
    AddNativeVariant(texture,image,false,packed.logicalWidth,packed.logicalHeight);
    return true;
}

void LoadNativeVariants(UiTexture& texture,bool activeMap=false) {
    using namespace mk64_3ds;
    const char* name=texture.resourceName;
    const auto add=[&](unsigned w,unsigned h) {
        if (!AddPackVariant(texture,name,w,h)) {
            // A missing optional pack uses the authentic source shape. Never
            // substitute the old inaccurate hand-traced map or invented font.
            AddNativeVariant(texture,native_sprite::ScaleBilinear(ReadUiPixels(texture),w,h));
        }
    };
    const bool map=std::strstr(name,"textures/other_textures/minimap_");
    const bool portrait=std::strstr(name,"common_texture_portrait_") || !std::strcmp(name,kPortraitBorderResource);
    const bool rank=std::strstr(name,"common_texture_hud_type_C_rank_font_");
    const bool place=std::strstr(name,"common_texture_hud_") &&
        (std::strstr(name,"st") || std::strstr(name,"nd") || std::strstr(name,"rd") || std::strstr(name,"th"));
    if (map) {
        // Menu maps have their own final-size pack variant. Shrinking the
        // larger race variant with nearest sampling drops thin curve pixels.
        const std::array<float,2> firstArea = activeMap
            ? std::array<float,2>{149,184}
            : std::array<float,2>{149*.8f,184*.8f};
        for (const auto area:{firstArea,std::array<float,2>{110,140},{68,176}}) {
            const float scale=std::min(float(area[0])/texture.width,float(area[1])/texture.height);
            add(std::lround(texture.width*scale),std::lround(texture.height*scale));
            if (!activeMap) break;
        }
    } else if (portrait) {for (unsigned size:{18U,30U,40U,48U}) add(size,size);}
    else if (!std::strcmp(name,kHudLapResource)) {add(40,17);add(48,20);}
    else if (std::strstr(name,"common_texture_hud_lap_") && !std::strstr(name,"_time")) {add(34,17);add(41,20);}
    else if (!std::strcmp(name,kHudTimeResource) || !std::strcmp(name,kHudTotalTimeResource)) add(32,16);
    else if (!std::strcmp(name,kHudTimerDigitsResource)) add(texture.width,texture.height);
    else if (std::strstr(name,"common_texture_item_window_")) {add(40,32);add(48,38);}
    else if (rank) {for(unsigned size:{15U,20U,24U}) add(size,size);}
    else if (place) {add(80,40);add(160,80);}
    else {
        const float scale=std::min(1.5f,512.f/std::max(texture.width,texture.height));
        add(std::lround(texture.width*scale),std::lround(texture.height*scale));
    }
}

NativeUiVariant* NativeVariantFor(UiTexture& texture,float width,float height) {
    const auto mode=Mk64Settings3DSGetUiTextures();
    const auto style=mk64_3ds::ImprovedUiTextures(mode) ? MK64_UI_TEXTURES_3DS_IMPROVED : MK64_UI_TEXTURES_3DS_ORIGINAL;
    if (style==MK64_UI_TEXTURES_3DS_ORIGINAL || !texture.nativeCount) return nullptr;
    NativeUiVariant* result=nullptr;
    float score=1e30f;
    for (unsigned i=0;i<texture.nativeCount;++i) {
        auto& candidate=texture.native[i];
        if (candidate.style!=style) continue;
        const float error=std::abs(candidate.width-width)+std::abs(candidate.height-height);
        if (error<score) {score=error;result=&candidate;}
    }
    return result;
}
