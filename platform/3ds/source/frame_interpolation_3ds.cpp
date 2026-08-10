#include "port/interpolation/FrameInterpolation.h"

#include <unordered_map>

// The desktop interpolation recorder allocates a tree of maps and vectors on
// every simulation frame. That path terminated the Old 3DS v0.11 build before
// its first visible frame. Keep the vanilla 30 Hz simulation/render path
// allocation-free until a bounded 3DS-specific recorder is available.
std::unordered_map<Mtx*, MtxF> FrameInterpolation_Interpolate(float) {
    return {};
}

void FrameInterpolation_ApplyMatrixTransformations(Mat4*, FVector, IRotator, FVector) {
}

extern "C" {

void FrameInterpolation_ShouldInterpolateFrame(bool) {
}

bool check_if_recording() {
    return false;
}

void FrameInterpolation_StartRecord() {
}

void FrameInterpolation_StopRecord() {
}

void FrameInterpolation_RecordMarker(const char*, int) {
}

void FrameInterpolation_RecordOpenChild(const void*, uintptr_t) {
}

void FrameInterpolation_RecordCloseChild() {
}

void FrameInterpolation_DontInterpolateCamera() {
}

int FrameInterpolation_GetCameraEpoch() {
    return 0;
}

void FrameInterpolation_RecordActorPosRotMatrix() {
}

void FrameInterpolation_RecordMatrixPosRotXYZ(Mat4*, Vec3f, Vec3s) {
}

void FrameInterpolation_RecordMatrixPosRotScaleXY(Mat4*, s32, s32, u16, f32) {
}

void FrameInterpolation_Record_SetTextMatrix(Mat4*, f32, f32, f32, f32) {
}

void FrameInterpolation_RecordMatrixMult(Mat4*, MtxF*, u8) {
}

void FrameInterpolation_RecordMatrixTranslate(Mat4*, Vec3f) {
}

void FrameInterpolation_RecordMatrixScale(Mat4*, f32) {
}

void FrameInterpolation_RecordMatrixRotate1Coord(Mat4*, u32, s16) {
}

void FrameInterpolation_RecordMatrixMtxFToMtx(MtxF*, Mtx*) {
}

void FrameInterpolation_RecordMatrixToMtx(Mtx*, char*, s32) {
}

void FrameInterpolation_RecordMatrixReplaceRotation(MtxF*) {
}

void FrameInterpolation_RecordSkinMatrixMtxFToMtx(MtxF*, Mtx*) {
}

void FrameInterpolation_RecordSetTransformMatrix(Mat4*, Vec3f, Vec3f, u16, f32) {
}

void FrameInterpolation_RecordSetMatrixTransformation(Mat4*, Vec3f, Vec3su, f32) {
}

void FrameInterpolation_RecordCalculateOrientationMatrix(Mat3*, f32, f32, f32, s16) {
}

void FrameInterpolation_RecordTranslateRotate(Mat4*, Vec3f, Vec3s) {
}

}
