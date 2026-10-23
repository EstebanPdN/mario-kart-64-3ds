if(MK64_3DS_NATIVE_KART_LAB)
    set(anchor "    gDisplayListHead = gGfxPool->gfxPool;")
    string(FIND "${SPAGHETTIKART_3DS_MAIN_TEXT}" "${anchor}" location)
    if(location LESS 0)
        message(FATAL_ERROR "Native packet capture ownership point changed")
    endif()
    # Race setup may construct an obsolete scene before config_gfx_pool starts
    # the actual frame. Tie the capture to the graphics-buffer generation.
    string(REPLACE "${anchor}" "${anchor}\n    Mk64NativeKartCaptureReset();"
        SPAGHETTIKART_3DS_MAIN_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
    if(MK64_3DS_NATIVE_DYNAMIC)
        string(REPLACE "    Mk64NativeKartCaptureReset();"
            "    Mk64NativeKartCaptureReset();\n    Mk64NativeFrameBegin();"
            SPAGHETTIKART_3DS_MAIN_TEXT "${SPAGHETTIKART_3DS_MAIN_TEXT}")
        string(PREPEND SPAGHETTIKART_3DS_MAIN_TEXT "#include \"native_dynamic_3ds.h\"\n")
    endif()
    string(PREPEND SPAGHETTIKART_3DS_MAIN_TEXT "#include \"native_kart_lab_3ds.h\"\n")
endif()
