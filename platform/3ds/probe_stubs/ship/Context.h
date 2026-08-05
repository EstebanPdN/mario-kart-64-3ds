#pragma once

#include "ship/config/ConsoleVariable.h"
#include "ship/resource/ResourceManager.h"

namespace Ship {

class Context {
  public:
    static Context* GetRawInstance() {
        static Context instance;
        return &instance;
    }

    ResourceManager* GetResourceManager() {
        return &mResourceManager;
    }

    ConsoleVariableStore* GetConsoleVariables() {
        return &mConsoleVariables;
    }

  private:
    ResourceManager mResourceManager;
    ConsoleVariableStore mConsoleVariables;
};

} // namespace Ship
