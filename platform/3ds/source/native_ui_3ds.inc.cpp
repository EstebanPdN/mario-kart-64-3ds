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
                      Mk64UiTextures3DS style=MK64_UI_TEXTURES_3DS_MODIFIED) {
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

void AddNativeArtwork(UiTexture& texture,const mk64_3ds::native_art::Sprite& sprite,
                      unsigned logicalWidth=0,unsigned logicalHeight=0) {
    const mk64_3ds::native_sprite::Image image{sprite.width,sprite.height,
        std::vector<uint32_t>(sprite.pixels,sprite.pixels+sprite.width*sprite.height)};
    AddNativeVariant(texture,image,false,logicalWidth,logicalHeight);
}

void AddNativeArtwork(UiTexture& texture,const mk64_3ds::native_art::MaskSprite& sprite) {
    AddNativeVariant(texture,mk64_3ds::native_art::DecodeMask(sprite),true);
}

void LoadNativeVariants(UiTexture& texture,bool activeMap=false) {
    using namespace mk64_3ds;
    using namespace native_art;
    const char* name=texture.resourceName;
    const auto scaleText=[&](const native_art::Sprite& target) {
        AddNativeVariant(texture,native_sprite::ScaleBilinear(ReadUiPixels(texture),target.width,target.height),
            false,0,0,MK64_UI_TEXTURES_3DS_SCALED);
    };
    if (std::strstr(name,"common_texture_hud_lap_")) {
        const Sprite* normal[]={&kLapCount1,&kLapCount2,&kLapCount3};
        const Sprite* top[]={&kLapCount1Top,&kLapCount2Top,&kLapCount3Top};
        for (unsigned i=0;i<3;++i) if (std::strcmp(name,kLapCountResources[i])==0) {
            AddNativeArtwork(texture,*normal[i]); AddNativeArtwork(texture,*top[i]);
            scaleText(*normal[i]);scaleText(*top[i]); return;
        }
    }
    if (!std::strcmp(name,kHudLapResource)) {AddNativeArtwork(texture,kLap);AddNativeArtwork(texture,kLapTop);scaleText(kLap);scaleText(kLapTop);return;}
    if (!std::strcmp(name,kHudTimeResource)) {AddNativeArtwork(texture,kTime);scaleText(kTime);return;}
    if (!std::strcmp(name,kHudTotalTimeResource)) {AddNativeArtwork(texture,kTotal);scaleText(kTotal);return;}
    if (!std::strcmp(name,kHudTimerDigitsResource)) {AddNativeArtwork(texture,kTimerDigits,96,16);
        const auto source=ReadUiPixels(texture);AddNativeVariant(texture,source,false,0,0,MK64_UI_TEXTURES_3DS_SCALED);return;}
    const bool portrait=std::strstr(name,"common_texture_portrait_") || !std::strcmp(name,kPortraitBorderResource);
    const bool map=std::strstr(name,"textures/other_textures/minimap_");
    const bool item=std::strstr(name,"common_texture_item_window_");
    const bool rank=std::strstr(name,"common_texture_hud_type_C_rank_font_");
    const bool place=std::strstr(name,"common_texture_hud_") &&
        (std::strstr(name,"st") || std::strstr(name,"nd") || std::strstr(name,"rd") || std::strstr(name,"th"));
    if (map) for (const auto& course:kCourseMaps) {
        const char* basename=std::strrchr(name,'/');
        if (!basename || std::strcmp(basename+1,course.resource)) continue;
        AddNativeArtwork(texture,course.sizes[0]);
        if (activeMap) {AddNativeArtwork(texture,course.sizes[1]);AddNativeArtwork(texture,course.sizes[2]);}
        break;
    }
    if (!portrait && !map && !item && !rank && !place) return;
    const auto source=ReadUiPixels(texture);
    const auto add=[&](unsigned w,unsigned h,bool alphaOnly=false) {
        AddNativeVariant(texture,native_sprite::ScaleBilinear(source,w,h),
            alphaOnly,0,0,MK64_UI_TEXTURES_3DS_SCALED);
    };
    if (portrait) for (unsigned size : {18U,30U,40U,48U}) add(size,size);
    else if (map) {
        for (const auto area : {std::array<unsigned,2>{149,184},{110,140},{68,176}}) {
            const float scale=std::min(float(area[0])/source.width,float(area[1])/source.height);
            add(std::lround(source.width*scale),std::lround(source.height*scale),true);
            if (!activeMap) break; // Conveyor owns one LA4 variant per style/course.
        }
    } else if (item) add(48,38);
    else if (rank) for (unsigned size : {15U,20U,24U}) add(size,size);
    else if (place) {add(80,40);add(160,80);}
}

NativeUiVariant* NativeVariantFor(UiTexture& texture,float width,float height) {
    const auto style=Mk64Settings3DSGetUiTextures();
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
