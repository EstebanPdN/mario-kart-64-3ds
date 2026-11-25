# Clouds and stars are flat screen-space sprites drawn in the middle of the
# world pass, where nothing separates them from the HUD: mark them in the display
# list so that both eyes place them at infinity. Falling snow stays on the plane.
set(MK64_STEREO_SKY_SOURCE "${SPAGHETTIKART_ROOT}/src/engine/sky/Sky.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MK64_STEREO_SKY_SOURCE}")
file(READ "${MK64_STEREO_SKY_SOURCE}" MK64_STEREO_SKY_TEXT)
set(MK64_STEREO_SKY_ORIGINAL "${MK64_STEREO_SKY_TEXT}")
string(REPLACE
    [=[    for (auto& cloud : Sky::Instance->GetSkyActors()) {
        if (cloud->mScreen == screen) {
            cloud->Draw(screen, arg0);
        }
    }]=]
    [=[    Track* stereoTrack3DS = GetWorld()->GetTrack();
    const bool stereoSky3DS = stereoTrack3DS == nullptr || stereoTrack3DS->mCloudType != Track::CloudType::SNOW;
    if (stereoSky3DS) gDma1p(gDisplayListHead++, G_NOOP, 0, 1, ::mk64_3ds::kStereoSkyLayerNoop);
    for (auto& cloud : Sky::Instance->GetSkyActors()) {
        if (cloud->mScreen == screen) {
            cloud->Draw(screen, arg0);
        }
    }
    if (stereoSky3DS) gDma1p(gDisplayListHead++, G_NOOP, 0, 0, ::mk64_3ds::kStereoSkyLayerNoop);]=]
    MK64_STEREO_SKY_TEXT "${MK64_STEREO_SKY_TEXT}")
if(MK64_STEREO_SKY_TEXT STREQUAL MK64_STEREO_SKY_ORIGINAL)
    message(FATAL_ERROR "The upstream sky actor draw loop changed")
endif()
string(REPLACE "#include \"Sky.h\"" "#include \"engine/sky/Sky.h\"" MK64_STEREO_SKY_TEXT "${MK64_STEREO_SKY_TEXT}")
set(MK64_STEREO_SKY_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated/Sky_3ds.cpp")
file(WRITE "${MK64_STEREO_SKY_GENERATED}" "#include \"stereo_3ds.hpp\"\n${MK64_STEREO_SKY_TEXT}")
list(REMOVE_ITEM SPAGHETTIKART_ENGINE_SKY "${MK64_STEREO_SKY_SOURCE}")
list(APPEND SPAGHETTIKART_ENGINE_SKY "${MK64_STEREO_SKY_GENERATED}")
