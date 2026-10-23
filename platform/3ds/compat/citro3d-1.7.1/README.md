# Citro3D 1.7.1 texture compatibility

`texture.c` and `internal.h` originate from devkitPro/citro3d tag `v1.7.1`,
matching the port toolchain (citro3d 1.7.1-2). See the adjacent zlib license.

The texture implementation is modified only in `C3D_TexBind`: reject units
outside 0..2 and test `tex` before querying its type. Binding null clears the
context pointer, allowing Citro3D to disable that sampler at its next draw.
Unmodified 1.7.1 dereferences null for units 1 and 2 before storing the pointer.

CMake places this object before the SDK archives for every renderer consumer.
Other Citro3D code, including its deferred context update, stays in the SDK.
Do not change the private context layout independently of the SDK version.
