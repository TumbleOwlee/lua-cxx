#include "luau_utils.h"
#include <cstdio>

void luau::util::print_lua_stack(const char *const filename, int const line, lua_State *L) {
    int top = lua_gettop(L);
    printf("[%s:%d] Lua stack (top=%d):\n", filename, line, top);
    for (int i = 1; i <= top; ++i) {
        int type = lua_type(L, i);
        printf("  [%2d] %s: ", i, lua_typename(L, type));
        switch (type) {
        case LUA_TNIL:
            printf("nil");
            break;
        case LUA_TBOOLEAN:
            printf(lua_toboolean(L, i) ? "true" : "false");
            break;
        case LUA_TNUMBER:
#if LUA_VERSION_NUM >= 503
            if (lua_isinteger(L, i))
                printf("%lld (integer)", (long long)lua_tointeger(L, i));
            else
                printf("%g (number)", lua_tonumber(L, i));
#else
            printf("%g (number)", lua_tonumber(L, i));
#endif
            break;
        case LUA_TSTRING:
            printf("'%s'", lua_tostring(L, i));
            break;
        case LUA_TTABLE:
            printf("table: %p", (void *)lua_topointer(L, i));
            break;
        case LUA_TFUNCTION:
            printf("function: %p", (void *)lua_topointer(L, i));
            break;
        case LUA_TUSERDATA:
            printf("userdata: %p", (void *)lua_touserdata(L, i));
            break;
        case LUA_TTHREAD:
            printf("thread: %p", (void *)lua_tothread(L, i));
            break;
        case LUA_TLIGHTUSERDATA:
            printf("light userdata: %p", lua_touserdata(L, i));
            break;
        default:
            printf("unknown");
        }
        printf("\n");
    }
}
