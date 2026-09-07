set(before "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
string(REPLACE "tex_height[i] = tex_size_bytes / line_size;"
    "tex_height[i] = ::mk64_3ds::Fast3DTextureRows(tex_size_bytes, line_size);"
    SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
string(REPLACE [=[    struct GfxClipParameters clip_parameters = mRapi->GetClipParameters();

    for (int i = 0; i < 3; i++) {]=]
    [=[    struct GfxClipParameters clip_parameters = mRapi->GetClipParameters();

    // These values are constant across all three vertices. Hoist divisions
    // explicitly: the VBO writes and backend calls otherwise obstruct this
    // optimization in the alias-conservative ARM build.
    float inverseWidth3DS[2] = {}, inverseHeight3DS[2] = {};
    float clampS3DS[2] = {}, clampT3DS[2] = {};
    for (int t = 0; t < 2; ++t) {
        if (!usedTextures[t]) continue;
        inverseWidth3DS[t] = 1.0f / tex_width[t];
        inverseHeight3DS[t] = 1.0f / tex_height[t];
        clampS3DS[t] = (tex_width2[t] - 0.5f) * inverseWidth3DS[t];
        clampT3DS[t] = (tex_height2[t] - 0.5f) * inverseHeight3DS[t];
    }
    for (int i = 0; i < 3; i++) {]=]
    SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
foreach(pair IN ITEMS "u / tex_width[t]|u * inverseWidth3DS[t]"
                      "v / tex_height[t]|v * inverseHeight3DS[t]"
                      "(tex_width2[t] - 0.5f) / tex_width[t]|clampS3DS[t]"
                      "(tex_height2[t] - 0.5f) / tex_height[t]|clampT3DS[t]")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 original)
    list(GET parts 1 replacement)
    string(REPLACE "${original}" "${replacement}"
        SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
endforeach()
if(before STREQUAL SPAGHETTIKART_3DS_INTERPRETER_TEXT)
    message(FATAL_ERROR "The Fast3D triangle math insertion points changed")
endif()
