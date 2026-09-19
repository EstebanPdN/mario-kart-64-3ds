# Run after the baseline interpreter adaptations, immediately before writing
# generated/interpreter_3ds.cpp. Every replacement checks its exact anchor.
macro(mk64_native_replace old_text new_text description)
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${old_text}" mk64_native_anchor)
    if(mk64_native_anchor EQUAL -1)
        message(FATAL_ERROR "Native geometry integration changed: ${description}")
    endif()
    string(REPLACE "${old_text}" "${new_text}"
        SPAGHETTIKART_3DS_INTERPRETER_TEXT "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
endmacro()

mk64_native_replace(
    "#include \"fast/types.h\""
    "#include \"fast/types.h\"\n#include \"gfx_citro3d.h\"\n#include \"native_geometry_3ds.hpp\"\n#include \"native_lighting_3ds.h\"\n#include \"fast3d_reuse_3ds.hpp\""
    "includes")

mk64_native_replace(
    "static float sMk64AspectCorrection3DS = 1.0f;"
    "static float sMk64AspectCorrection3DS = 1.0f;\nstatic ::mk64_3ds::NativeLightingVertices<MAX_VERTICES + 4> sNativeLights3DS;"
    "lighting sidecar")

mk64_native_replace(
    "        Mk64ResetTextureOwners3DS();"
    "        Mk64ResetTextureOwners3DS();\n        ::mk64_3ds::gNativeGeometry3DS.ResetVertices();\n        sNativeLights3DS.Reset();"
    "interpreter destruction")

mk64_native_replace(
    [=[    for (size_t i = 0; i < n_vertices; i++, dest_index++) {
        const F3DVtx_t* v = &vertices[i].v;]=]
    [=[    ::mk64_3ds::gNativeGeometry3DS.BeginLoad(
        dest_index, n_vertices, mRsp->MP_matrix, AdjXForAspectRatio(1.0f));
    const auto* positionBlock3DS = ::mk64_3ds::BeginFast3DPositionBlock(
        vertices, n_vertices, mRsp->MP_matrix);
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {
        const F3DVtx_t* v = &vertices[i].v;]=]
    "vertex load begin")

mk64_native_replace(
    [=[        float x = v->ob[0] * mRsp->MP_matrix[0][0] + v->ob[1] * mRsp->MP_matrix[1][0] +
                  v->ob[2] * mRsp->MP_matrix[2][0] + mRsp->MP_matrix[3][0];
        float y = v->ob[0] * mRsp->MP_matrix[0][1] + v->ob[1] * mRsp->MP_matrix[1][1] +
                  v->ob[2] * mRsp->MP_matrix[2][1] + mRsp->MP_matrix[3][1];
        float z = v->ob[0] * mRsp->MP_matrix[0][2] + v->ob[1] * mRsp->MP_matrix[1][2] +
                  v->ob[2] * mRsp->MP_matrix[2][2] + mRsp->MP_matrix[3][2];
        float w = v->ob[0] * mRsp->MP_matrix[0][3] + v->ob[1] * mRsp->MP_matrix[1][3] +
                  v->ob[2] * mRsp->MP_matrix[2][3] + mRsp->MP_matrix[3][3];]=]
    [=[        if (positionBlock3DS != nullptr) {
            ::mk64_3ds::gNativeGeometry3DS.CapturePosition(
                dest_index, v->ob, *d, positionBlock3DS[i].z, positionBlock3DS[i].w);
        } else {
            ::mk64_3ds::gNativeGeometry3DS.Capture(dest_index, v->ob, *d);
        }
        float z = d->z;
        float w = d->w;]=]
    "deferred position transform")

mk64_native_replace("        x = AdjXForAspectRatio(x);" "" "captured aspect")

mk64_native_replace(
    [=[        // trivial clip rejection
        d->clip_rej = 0;
        if (x < -w) {
            d->clip_rej |= 1; // CLIP_LEFT
        }
        if (x > w) {
            d->clip_rej |= 2; // CLIP_RIGHT
        }
        if (y < -w) {
            d->clip_rej |= 4; // CLIP_BOTTOM
        }
        if (y > w) {
            d->clip_rej |= 8; // CLIP_TOP
        }
        // if (z < -w) d->clip_rej |= 16; // CLIP_NEAR
        if (z > w) {
            d->clip_rej |= 32; // CLIP_FAR
        }

        d->x = x;
        d->y = y;
        d->z = z;
        d->w = w;]=]
    [=[        // Capture retained exact Z/W and the far-plane bit. X/Y outcodes
        // are materialized only for CPU triangles and G_CULLDL.]=]
    "deferred outcodes")

mk64_native_replace(
    "            return gfx->mRsp->loaded_vertices[index3DS].clip_rej;"
    "            ::mk64_3ds::gNativeGeometry3DS.Materialize(index3DS, gfx->mRsp->loaded_vertices[index3DS]);\n            return gfx->mRsp->loaded_vertices[index3DS].clip_rej;"
    "G_CULLDL materialization")

mk64_native_replace(
    "    // if (rand()%2) return;"
    [=[    auto* nativeRenderer3DS = static_cast<GfxRenderingAPICitro3D*>(mRapi);
    const uint8_t vertexIndices3DS[3] = {vtx1_idx, vtx2_idx, vtx3_idx};
    const uint32_t nativeCullBoth3DS = get_attr(CULL_BOTH);
    const uint32_t nativeCullMode3DS = mRsp->geometry_mode & nativeCullBoth3DS;
    auto nativeCull3DS = ::mk64_3ds::NativeGeometryCull::None;
    if (nativeCullMode3DS == get_attr(CULL_FRONT)) nativeCull3DS = ::mk64_3ds::NativeGeometryCull::Front;
    if (nativeCullMode3DS == get_attr(CULL_BACK)) nativeCull3DS = ::mk64_3ds::NativeGeometryCull::Back;
    if ((mRsp->extra_geometry_mode & G_EX_INVERT_CULLING) != 0) {
        if (nativeCull3DS == ::mk64_3ds::NativeGeometryCull::Front)
            nativeCull3DS = ::mk64_3ds::NativeGeometryCull::Back;
        else if (nativeCull3DS == ::mk64_3ds::NativeGeometryCull::Back)
            nativeCull3DS = ::mk64_3ds::NativeGeometryCull::Front;
    }
    const auto& nativeDraw3DS = ::mk64_3ds::gNativeGeometry3DS.PrepareTriangle(
        vtx1_idx, vtx2_idx, vtx3_idx, mRsp->loaded_vertices,
        !is_rect && nativeCullMode3DS == 0, nativeCull3DS);
    // Face-culling lists retain CPU rejection before texture lookup, packing
    // and submission. Offloading only the final cull kept invisible triangles
    // on the expensive ARM11 interpreter path and split their matrix batches.
]=]
    "triangle preparation")

mk64_native_replace(
    "    bool zbuffer_enabled = (mRsp->geometry_mode & G_ZBUFFER) == G_ZBUFFER;"
    [=[    if (!nativeRenderer3DS->NativeGeometryMatches(nativeDraw3DS.enabled ? &nativeDraw3DS : nullptr)) {
        Flush();
        nativeRenderer3DS->SetNativeGeometry(nativeDraw3DS.enabled ? &nativeDraw3DS : nullptr);
        ++::mk64_3ds::gNativeGeometry3DS.counters.stateSwitches;
    }
    bool zbuffer_enabled = (mRsp->geometry_mode & G_ZBUFFER) == G_ZBUFFER;]=]
    "surviving triangle state")

mk64_native_replace(
    "    if ((mRsp->geometry_mode & cull_both) != 0 &&"
    "    if (!nativeDraw3DS.enabled && (mRsp->geometry_mode & cull_both) != 0 &&"
    "native face culling")

mk64_native_replace(
    "    struct GfxClipParameters clip_parameters = mRapi->GetClipParameters();"
    [=[    const bool nativeLighting3DS = ::mk64_3ds::gNativeGeometry3DS.enabled &&
        !is_rect && numInputs == 1 && comb->shader_input_mapping[0][0] == G_CCMUX_SHADE &&
        sNativeLights3DS.CanDraw(vtx1_idx, vtx2_idx, vtx3_idx) &&
        v1->w >= ::mk64_3ds::NativeGeometryState::kClipWEpsilon &&
        v2->w >= ::mk64_3ds::NativeGeometryState::kClipWEpsilon &&
        v3->w >= ::mk64_3ds::NativeGeometryState::kClipWEpsilon;
    const auto* lightState3DS = nativeLighting3DS ? &sNativeLights3DS.Get(vtx1_idx).state : nullptr;
    if (!nativeRenderer3DS->NativeLightingMatches(lightState3DS)) {
        Flush();
        nativeRenderer3DS->SetNativeLighting(lightState3DS);
    }
    if (nativeLighting3DS) {
        ++::gMk64NativeLightTriangles3DS;
    } else {
        for (int i = 0; i < 3; ++i)
            if (sNativeLights3DS.Materialize(vertexIndices3DS[i], v_arr[i]->color))
                ++::gMk64NativeLightMaterialized3DS;
    }
    struct GfxClipParameters clip_parameters = mRapi->GetClipParameters();]=]
    "lighting draw selection")

mk64_native_replace(
    [=[        mBufVbo[mBufVboLen++] = v_arr[i]->x;
        mBufVbo[mBufVboLen++] = clip_parameters.invertY ? -v_arr[i]->y : v_arr[i]->y;
        mBufVbo[mBufVboLen++] = z;
        mBufVbo[mBufVboLen++] = w;]=]
    [=[        if (nativeDraw3DS.enabled) {
            const auto& object3DS = ::mk64_3ds::gNativeGeometry3DS.GetVertex(vertexIndices3DS[i]);
            mBufVbo[mBufVboLen++] = object3DS.object[0];
            mBufVbo[mBufVboLen++] = object3DS.object[1];
            mBufVbo[mBufVboLen++] = object3DS.object[2];
            mBufVbo[mBufVboLen++] = w;
        } else {
            mBufVbo[mBufVboLen++] = v_arr[i]->x;
            mBufVbo[mBufVboLen++] = clip_parameters.invertY ? -v_arr[i]->y : v_arr[i]->y;
            mBufVbo[mBufVboLen++] = z;
            mBufVbo[mBufVboLen++] = w;
        }]=]
    "object position emission")

mk64_native_replace(
    [=[                if (k == 0) {
                    mBufVbo[mBufVboLen++] = color->r / 255.0f;]=]
    [=[                if (k == 0 && nativeLighting3DS) {
                    const auto& lightingVertex3DS = sNativeLights3DS.Get(vertexIndices3DS[i]);
                    for (int component3DS = 0; component3DS < 3; ++component3DS)
                        mBufVbo[mBufVboLen++] = ::mk64_3ds::EncodeLightingNormal(
                            lightingVertex3DS.normal[component3DS]) / 255.0f;
                } else if (k == 0) {
                    mBufVbo[mBufVboLen++] = color->r / 255.0f;]=]
    "normal emission")

include("${CMAKE_CURRENT_LIST_DIR}/native_lighting_3ds.cmake")
