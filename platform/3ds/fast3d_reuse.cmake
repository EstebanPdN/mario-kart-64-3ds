# Apply after the normal interpreter compatibility patches. Each substitution
# must match exactly once so an upstream change cannot silently disable reuse.
macro(mk64_reuse_replace original replacement)
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${original}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "The Fast3D reuse insertion point changed: ${original}")
    endif()
    string(LENGTH "${original}" original_length)
    math(EXPR after_original "${found} + ${original_length}")
    string(SUBSTRING "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" ${after_original} -1 after_text)
    string(FIND "${after_text}" "${original}" duplicate)
    if(NOT duplicate EQUAL -1)
        message(FATAL_ERROR "The Fast3D reuse insertion point is ambiguous: ${original}")
    endif()
    string(REPLACE "${original}" "${replacement}"
        SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
endmacro()

mk64_reuse_replace("#include \"fast3d_math_3ds.hpp\""
    "#include \"fast3d_math_3ds.hpp\"\n#include \"fast3d_reuse_3ds.hpp\"")

mk64_reuse_replace([=[        // Original GBI where fixed point matrices are used
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j += 2) {
                int32_t int_part = addr[i * 2 + j / 2];
                uint32_t frac_part = addr[8 + i * 2 + j / 2];
                matrix[i][j] = (int32_t)((int_part & 0xffff0000) | (frac_part >> 16)) / 65536.0f;
                matrix[i][j + 1] = (int32_t)((int_part << 16) | (frac_part & 0xffff)) / 65536.0f;
            }
        }]=]
    [=[        // Interpolated matrix replacements have already been resolved
        // above; only the raw fixed-point decode is reused here.
        ::mk64_3ds::LoadFast3DFixedMatrix(matrix, addr);]=])

mk64_reuse_replace("inline bool contains(int8_t opcode) const {"
    "inline constexpr bool contains(int8_t opcode) const {")
mk64_reuse_replace("inline std::pair<const char*, GfxOpcodeHandlerFunc> at(int8_t opcode) const {"
    "inline constexpr std::pair<const char*, GfxOpcodeHandlerFunc> at(int8_t opcode) const {")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/source/fast3d_dispatch_3ds.inc.cpp")
file(READ "${CMAKE_CURRENT_LIST_DIR}/source/fast3d_dispatch_3ds.inc.cpp" dispatch_source)
mk64_reuse_replace("const char* GfxGetOpcodeName(int8_t opcode) {"
    "${dispatch_source}\nconst char* GfxGetOpcodeName(int8_t opcode) {")

# Replace only the runtime dispatch inside gfx_step, preserving the trace,
# LOAD_UCODE handling and final command advance. The filepath validation is
# identical to the upstream guard and still precedes invoking its handler.
set(dispatch_original [=[    if (otrHandlers.contains(opcode)) {
        // OTR filepath handlers expect w1 to be a valid string pointer.
        // Guard against null or N64-segment addresses that would crash in strlen/strncmp.
        if (opcode == OTR_G_VTX_OTR_FILEPATH || opcode == OTR_G_SETTIMG_OTR_FILEPATH ||
            opcode == OTR_G_DL_OTR_FILEPATH || opcode == OTR_G_PUSHCD || opcode == OTR_G_MTX_OTR_FILEPATH) {
            uintptr_t w1 = (uintptr_t)cmd->words.w1;
            if (w1 < 0x10000
#if UINTPTR_MAX > 0xFFFFFFFFu
                // On 64-bit: filter kernel/sentinel addresses.
                || w1 > 0x0000FFFFFFFFFFFFull
#endif
            ) {
                ++g_exec_stack.currCmd();
                return;
            }
        }
        if (otrHandlers.at(opcode).second(&cmd)) {
            return;
        }
    } else if (rdpHandlers.contains(opcode)) {
        if (rdpHandlers.at(opcode).second(&cmd)) {
            return;
        }
    } else if (ucode_handler_index < ucode_handlers.size()) {
        if (ucode_handlers[ucode_handler_index]->contains(opcode)) {
            if (ucode_handlers[ucode_handler_index]->at(opcode).second(&cmd)) {
                return;
            }
        } else {
            SPDLOG_CRITICAL("Unhandled OP code: 0x{:X}, for loaded ucode: {}", (uint8_t)opcode,
                            (uint32_t)ucode_handler_index);
        }
    } else {
        SPDLOG_CRITICAL("Unhandled OP code: 0x{:X}, invalid ucode: {}", (uint8_t)opcode, (uint32_t)ucode_handler_index);
    }]=])
set(dispatch_replacement [=[    if (opcode == OTR_G_VTX_OTR_FILEPATH || opcode == OTR_G_SETTIMG_OTR_FILEPATH ||
        opcode == OTR_G_DL_OTR_FILEPATH || opcode == OTR_G_PUSHCD || opcode == OTR_G_MTX_OTR_FILEPATH) {
        const uintptr_t w1 = (uintptr_t)cmd->words.w1;
        if (w1 < 0x10000
#if UINTPTR_MAX > 0xFFFFFFFFu
            || w1 > 0x0000FFFFFFFFFFFFull
#endif
        ) {
            ++g_exec_stack.currCmd();
            return;
        }
    }
    const auto handler3DS = Mk64OpcodeHandler3DS(opcode);
    if (handler3DS != nullptr) {
        if (handler3DS(&cmd)) return;
    } else {
        SPDLOG_CRITICAL("Unhandled OP code: 0x{:X}, for loaded ucode: {}", (uint8_t)opcode,
                        (uint32_t)ucode_handler_index);
    }]=])
mk64_reuse_replace("${dispatch_original}" "${dispatch_replacement}")
