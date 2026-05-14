#pragma once

#include "binder.h"
#include <lua5.4/lua.hpp>
#include <vector>

namespace lua {

// Interface for Objects mappable into lua state
class GlobalObject
{
    friend class Runtime;

public:
    GlobalObject() = default;

    // Return configuration of name and methods
    virtual auto luaConfig() -> std::vector<BoundMethod> = 0;
};

} // namespace lua
