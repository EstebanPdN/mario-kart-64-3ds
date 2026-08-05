#pragma once

#define CVAR_INTERNAL_RESOLUTION "gInternalResolution"
#define CVAR_MSAA_VALUE "gMsaaValue"

namespace Ship {

class ConsoleVariableStore {
  public:
    float GetFloat(const char*, float fallback) const {
        return fallback;
    }

    int GetInteger(const char*, int fallback) const {
        return fallback;
    }
};

} // namespace Ship
