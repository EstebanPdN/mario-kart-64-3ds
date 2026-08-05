#pragma once

// libultraship's renderer interface only needs the opaque texture handle type.
// The full Dear ImGui dependency is intentionally excluded from the 3DS runtime.
#define ImTextureID void*
