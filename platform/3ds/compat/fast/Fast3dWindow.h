#pragma once

// The vanilla 3DS build does not compile the desktop editor.  Engine headers
// still include this desktop-only header indirectly, so keep the dependency
// boundary empty on this platform.
