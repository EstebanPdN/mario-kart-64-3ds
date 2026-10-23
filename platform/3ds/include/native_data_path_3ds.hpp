#pragma once

// Local native candidates include their prepared data in the installed RomFS.
// These owner-derived resources must stay out of public distributions.
#define MK64_NATIVE_DATA_ROOT "romfs:/"
