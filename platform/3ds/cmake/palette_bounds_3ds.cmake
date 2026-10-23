string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "void Interpreter::GfxDpLoadTlut(" paletteBegin)
string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "void Interpreter::GfxDpLoadBlock(" paletteEnd)
if(paletteBegin LESS 0 OR paletteEnd LESS paletteBegin)
    message(FATAL_ERROR "Palette bounds function boundary changed")
endif()
math(EXPR paletteLength "${paletteEnd}-${paletteBegin}")
string(SUBSTRING "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" ${paletteBegin} ${paletteLength} paletteBefore)
set(anchor "    uint32_t byteCount = entryCount * 2;")
set(replacement [=[
    uint32_t byteCount = entryCount * 2;
    auto copyPalette3DS=[&](void* destination,const uint8_t* data,size_t count) {
        if(!mk64_3ds::CopyPaletteBytes(destination,data,count,
            paletteOwner3DS ? paletteOwnerBase3DS : nullptr,
            paletteOwner3DS ? paletteOwner3DS->ImageDataSize : 0))
            throw std::runtime_error("Palette source is outside its resource owner");
    };
]=])
string(FIND "${paletteBefore}" "${anchor}" paletteAnchor)
if(paletteAnchor LESS 0)
    message(FATAL_ERROR "Palette bounds insertion changed")
endif()
string(REPLACE "${anchor}" "${replacement}" paletteAfter "${paletteBefore}")
foreach(copy IN ITEMS
    "memcpy(mRdp->palette_staging[0], src, 256)"
    "memcpy(mRdp->palette_staging[1], src + 256, 256)"
    "memcpy(mRdp->palette_staging[0] + paletteByteOffset, src, copyLen)"
    "memcpy(mRdp->palette_staging[1] + offset, src, copyLen)")
    string(FIND "${paletteAfter}" "${copy}" paletteCopy)
    if(paletteCopy LESS 0)
        message(FATAL_ERROR "Palette copy site changed: ${copy}")
    endif()
    string(REPLACE "memcpy(" "copyPalette3DS(" bounded "${copy}")
    string(REPLACE "${copy}" "${bounded}" paletteAfter "${paletteAfter}")
endforeach()
string(REPLACE "${paletteBefore}" "${paletteAfter}" SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
string(PREPEND SPAGHETTIKART_3DS_INTERPRETER_TEXT "#include \"palette_copy_3ds.hpp\"\n")
