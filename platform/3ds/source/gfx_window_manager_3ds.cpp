#include "gfx_window_manager_3ds.h"

#include <3ds.h>

extern "C" bool Mk64Diagnostics3DSOwnsHid(void) __attribute__((weak));

namespace Fast {

void GfxWindowBackend3DS::Init(const char*, const char*, bool, uint32_t width, uint32_t height,
                              int32_t, int32_t) {
    mWidth = width == 800 ? 800 : 400;
    mHeight = height == 0 ? 240 : height;
    mFullScreen = true;
    mIsRunning = true;
    mTargetFps = 60;
}

void GfxWindowBackend3DS::Close() {
    mIsRunning = false;
}

void GfxWindowBackend3DS::SetKeyboardCallbacks(bool (*onKeyDown)(int), bool (*onKeyUp)(int),
                                                 void (*onAllKeysUp)()) {
    mOnKeyDown = onKeyDown;
    mOnKeyUp = onKeyUp;
    (void) onAllKeysUp;
}

void GfxWindowBackend3DS::SetMouseCallbacks(bool (*onMouseButtonDown)(int), bool (*onMouseButtonUp)(int)) {
    mOnMouseButtonDown = onMouseButtonDown;
    mOnMouseButtonUp = onMouseButtonUp;
}

void GfxWindowBackend3DS::SetFullscreenChangedCallback(void (*onFullscreenChanged)(bool)) {
    mOnFullscreenChanged = onFullscreenChanged;
}

void GfxWindowBackend3DS::SetFullscreen(bool fullscreen) {
    mFullScreen = fullscreen;
    if (mOnFullscreenChanged != nullptr) {
        mOnFullscreenChanged(fullscreen);
    }
}

void GfxWindowBackend3DS::GetActiveWindowRefreshRate(uint32_t* refreshRate) {
    if (refreshRate != nullptr) {
        *refreshRate = 60;
    }
}

void GfxWindowBackend3DS::SetCursorVisibility(bool) {
}

void GfxWindowBackend3DS::SetMousePos(int32_t, int32_t) {
}

void GfxWindowBackend3DS::GetMousePos(int32_t* x, int32_t* y) {
    if (x != nullptr) {
        *x = 0;
    }
    if (y != nullptr) {
        *y = 0;
    }
}

void GfxWindowBackend3DS::GetMouseDelta(int32_t* x, int32_t* y) {
    GetMousePos(x, y);
}

void GfxWindowBackend3DS::GetMouseWheel(float* x, float* y) {
    if (x != nullptr) {
        *x = 0.0f;
    }
    if (y != nullptr) {
        *y = 0.0f;
    }
}

bool GfxWindowBackend3DS::GetMouseState(uint32_t) {
    return false;
}

void GfxWindowBackend3DS::SetMouseCapture(bool) {
}

bool GfxWindowBackend3DS::IsMouseCaptured() {
    return false;
}

void GfxWindowBackend3DS::GetDimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) {
    if (width != nullptr) {
        *width = mWidth;
    }
    if (height != nullptr) {
        *height = mHeight;
    }
    if (posX != nullptr) {
        *posX = 0;
    }
    if (posY != nullptr) {
        *posY = 0;
    }
}

void GfxWindowBackend3DS::SetDimensions(uint32_t width, uint32_t height, int32_t, int32_t) {
    mWidth = width == 800 ? 800 : 400;
    mHeight = height == 0 ? 240 : height;
}

Ship::WindowRect GfxWindowBackend3DS::GetPrimaryMonitorRect() {
    return { 0, 0, static_cast<int32_t>(mWidth), static_cast<int32_t>(mHeight) };
}

void GfxWindowBackend3DS::HandleEvents() {
    // The diagnostics worker owns HID while the game is running so L+R+A can
    // still be observed if the main thread is blocked in the renderer.
    if (Mk64Diagnostics3DSOwnsHid == nullptr || !Mk64Diagnostics3DSOwnsHid()) {
        hidScanInput();
    }
    if (!aptMainLoop()) {
        mIsRunning = false;
    }
}

bool GfxWindowBackend3DS::IsFrameReady() {
    return mIsRunning;
}

void GfxWindowBackend3DS::SwapBuffersBegin() {
}

void GfxWindowBackend3DS::SwapBuffersEnd() {
}

double GfxWindowBackend3DS::GetTime() {
    return static_cast<double>(osGetTime()) / 1000.0;
}

int GfxWindowBackend3DS::GetTargetFps() {
    return static_cast<int>(mTargetFps);
}

void GfxWindowBackend3DS::SetTargetFps(int fps) {
    mTargetFps = fps > 0 ? static_cast<uint32_t>(fps) : 60;
}

void GfxWindowBackend3DS::SetMaxFrameLatency(int) {
}

const char* GfxWindowBackend3DS::GetKeyName(int) {
    return "3DS Button";
}

bool GfxWindowBackend3DS::CanDisableVsync() {
    return false;
}

bool GfxWindowBackend3DS::IsRunning() {
    return mIsRunning;
}

void GfxWindowBackend3DS::Destroy() {
    mIsRunning = false;
}

bool GfxWindowBackend3DS::IsFullscreen() {
    return true;
}

} // namespace Fast
