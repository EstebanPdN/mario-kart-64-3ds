#pragma once

#ifdef __cplusplus
#include <array>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#endif

// libultraship's umbrella header declares a few fixed-width types twice using
// typedefs that are ABI-equivalent but not C type-compatible on devkitARM.
// Keep the complete C API while relying on os.h/exception.h for those duplicate
// declarations.
#include <libultraship/libultra/abi.h>
#include <libultraship/libultra/controller.h>
#include <libultraship/libultra/convert.h>
#include <libultraship/libultra/exception.h>
#include <libultraship/libultra/gbi.h>
#include <libultraship/libultra/gs2dex.h>
#include <libultraship/libultra/gu.h>
#include <libultraship/libultra/os.h>
#include <libultraship/libultra/internal.h>
#include <libultraship/libultra/mbi.h>
#include <libultraship/libultra/message.h>
#include <libultraship/libultra/motor.h>
#include <libultraship/libultra/pfs.h>
#include <libultraship/libultra/pi.h>
#include <libultraship/libultra/printf.h>
#include <libultraship/libultra/r4300.h>
#include <libultraship/libultra/rcp.h>
#include <libultraship/libultra/rdp.h>
#include <libultraship/libultra/sptask.h>
#include <libultraship/libultra/thread.h>
#include <libultraship/libultra/time.h>
#include <libultraship/libultra/types.h>
#include <libultraship/libultra/vi.h>

#ifndef MK64_3DS_LIBULTRA_ONLY
#include <libultraship/bridge.h>
#include <libultraship/color.h>
#include <libultraship/luslog.h>
#endif
