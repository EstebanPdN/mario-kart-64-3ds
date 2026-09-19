# Apply only to the generated 3DS interpreter, preserving the upstream checkout.
# This fragment is included after native geometry declarations are installed.
function(mk64_native_lighting_replace before after)
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${before}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Native lighting interpreter anchor missing: ${before}")
    endif()
    string(LENGTH "${before}" anchor_length)
    math(EXPR suffix_position "${position} + ${anchor_length}")
    string(SUBSTRING "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" ${suffix_position} -1 suffix)
    string(FIND "${suffix}" "${before}" repeated)
    if(NOT repeated EQUAL -1)
        message(FATAL_ERROR "Native lighting interpreter anchor is not unique: ${before}")
    endif()
    string(REPLACE "${before}" "${after}" SPAGHETTIKART_3DS_INTERPRETER_TEXT
                   "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
    set(SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" PARENT_SCOPE)
endfunction()

mk64_native_lighting_replace([=[        const F3DVtx_tn* vn = &vertices[i].n;]=]
[=[        const F3DVtx_tn* vn = &vertices[i].n;
        sNativeLights3DS.Disable(dest_index);]=])

# Light coefficient refresh remains above this branch. Capture the exact
# load-time state; position/fog/alpha and texture generation stay in the
# original interpreter. More complex lights use its unchanged RGB loop.
mk64_native_lighting_replace([=[            int r = mRsp->current_lights[mRsp->current_num_lights - 1].l.col[0];]=]
[=[            mk64_3ds::NativeLightingState nativeLightState;
            const bool nativeLightMode = mk64_3ds::gNativeGeometry3DS.enabled &&
                mRsp->current_num_lights == 2 &&
                (mRsp->geometry_mode & (G_LIGHTING_POSITIONAL | G_TEXTURE_GEN)) == 0;
            if (nativeLightMode) {
                for (int c = 0; c < 3; ++c) {
                    nativeLightState.direction[c] = mRsp->current_lights_coeffs[0][c];
                    nativeLightState.ambient[c] = mRsp->current_lights[1].l.col[c];
                    nativeLightState.diffuse[c] = mRsp->current_lights[0].l.col[c];
                }
            }
            if (nativeLightMode && nativeLightState.IsFinite()) {
                sNativeLights3DS.Capture(dest_index, nativeLightState, vn->n);
                ++gMk64NativeLightLoaded3DS;
            } else {
            int r = mRsp->current_lights[mRsp->current_num_lights - 1].l.col[0];]=])

mk64_native_lighting_replace([=[            d->color.b = b > 255 ? 255 : b;

            if (mRsp->geometry_mode & G_TEXTURE_GEN) {]=]
[=[            d->color.b = b > 255 ? 255 : b;
            }

            if (mRsp->geometry_mode & G_TEXTURE_GEN) {]=])
