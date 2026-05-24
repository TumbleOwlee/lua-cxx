#pragma once
#include "lua.h"

namespace luau::util {
void print_lua_stack(const char *const filename, int const line, lua_State *L);
}
