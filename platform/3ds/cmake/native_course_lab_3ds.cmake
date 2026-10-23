# Temporary development bridge. A matched static course root is drawn directly
# from the native cache and never pushed onto the source command stack.
if(MK64_3DS_NATIVE_LUIGI_LAB)
    set(native_before "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
    string(PREPEND SPAGHETTIKART_3DS_INTERPRETER_TEXT
        "namespace Fast { class Interpreter; }\nextern \"C\" bool Mk64NativeLuigiLabDispatch(Fast::Interpreter*,const void*) __attribute__((weak));\n")
    set(anchor "    F3DGfx* subGFX = (F3DGfx*)gfx->SegAddr(cmd->words.w1);")
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${anchor}" position)
    if(position LESS 0)
        message(FATAL_ERROR "Native course dispatch insertion point changed")
    endif()
    string(REPLACE "${anchor}" "${anchor}\n    if(C0(16,1)==0 && Mk64NativeLuigiLabDispatch && Mk64NativeLuigiLabDispatch(gfx,subGFX))return false;"
        SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
endif()

if(MK64_3DS_NATIVE_KART_LAB)
    string(PREPEND SPAGHETTIKART_3DS_INTERPRETER_TEXT
        "namespace Fast { class Interpreter; }\nextern \"C\" bool Mk64NativeKartDispatch(Fast::Interpreter*,const void*) __attribute__((weak));\nextern \"C\" void Mk64NativeKartBeginReplay() __attribute__((weak));\nextern \"C\" void Mk64NativeKartEndReplay() __attribute__((weak));\nextern \"C\" void Mk64NativeKartRestoreInheritedTexture(Fast::Interpreter*) __attribute__((weak));\n")
    macro(native_kart_replace before after)
        string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${before}" location)
        if(location LESS 0)
            message(FATAL_ERROR "Native kart interpreter migration point changed: ${before}")
        endif()
        string(REPLACE "${before}" "${after}" SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
    endmacro()
    native_kart_replace("    g_exec_stack.start((F3DGfx*)commands);"
        "    if(Mk64NativeKartBeginReplay)Mk64NativeKartBeginReplay();\n    g_exec_stack.start((F3DGfx*)commands);")
    if(MK64_3DS_NATIVE_PARTICLE_COMPARE_LAB)
        native_kart_replace("        gfx_step();"
            "        if(Mk64NativeKartDispatch && Mk64NativeKartDispatch(this,cmd)) {\n            sTriangleRun3DS.Invalidate();\n            if(g_exec_stack.currCmd()!=cmd)continue;\n        }\n        gfx_step();")
    else()
        native_kart_replace("        gfx_step();"
            "        if(Mk64NativeKartDispatch && Mk64NativeKartDispatch(this,cmd))sTriangleRun3DS.Invalidate();\n        gfx_step();")
    endif()
    native_kart_replace("    }\n\n    Flush();\n    mGfxFrameBuffer = 0;"
        "    }\n\n    Flush();\n    if(Mk64NativeKartEndReplay)Mk64NativeKartEndReplay();\n    mGfxFrameBuffer = 0;")
    native_kart_replace("    auto* nativeRenderer3DS = static_cast<GfxRenderingAPICitro3D*>(mRapi);"
        "    if(Mk64NativeKartRestoreInheritedTexture)Mk64NativeKartRestoreInheritedTexture(this);\n    auto* nativeRenderer3DS = static_cast<GfxRenderingAPICitro3D*>(mRapi);")
endif()

if(MK64_3DS_NATIVE_FULL_GAME)
    string(PREPEND SPAGHETTIKART_3DS_INTERPRETER_TEXT "#include \"native_program_3ds.h\"\n")
    # A marked resource can use the resident mesh or its compiled ARM procedure.
    # Both routes return without pushing any source display-list instructions.
    set(programBefore "    if(C0(16,1)==0 && Mk64NativeLuigiLabDispatch && Mk64NativeLuigiLabDispatch(gfx,subGFX))return false;")
    set(programAfter "${programBefore}\n    if(C0(16,1)==0 && subGFX && (uint32_t(subGFX[0].words.w0)>>24)==0x33) {\n        const uint64_t hash=(uint64_t(subGFX[1].words.w0)<<32)|uint32_t(subGFX[1].words.w1);\n        if(Mk64NativeProgramDraw(gfx,hash))return false;\n    }")
    string(REPLACE "${programBefore}" "${programAfter}" SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "bool gfx_dl_otr_hash_handler_custom(" programBegin)
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "bool gfx_dl_index_handler(" programEnd)
    math(EXPR programLength "${programEnd}-${programBegin}")
    string(SUBSTRING "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" ${programBegin} ${programLength} programOriginal)
    string(REPLACE "        F3DGfx* gfx = (F3DGfx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);"
        "        if(Mk64NativeProgramDraw(mInstance,hash))return false;\n        F3DGfx* gfx = (F3DGfx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);"
        programChanged "${programOriginal}")
    if(programOriginal STREQUAL programChanged)
        message(FATAL_ERROR "Native program hash dispatch anchor changed")
    endif()
    string(REPLACE "${programOriginal}" "${programChanged}" SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
    string(APPEND SPAGHETTIKART_3DS_INTERPRETER_TEXT "\nextern \"C\" void Mk64NativeProgramTriangleBoundary(int triangle) { if(triangle==0)Fast::sTriangleRun3DS.Invalidate();else Fast::sTriangleRun3DS.active=triangle>0; }\n")
endif()
