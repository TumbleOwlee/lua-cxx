#pragma once

#include <expected>
#include <lua5.4/lua.hpp>
#include <string>
#include <type_traits>
#include <vector>

namespace lua {

namespace arg {

// ── Type converter trait ───────────────────────────────────────────────────
//
// Specialize lua::converter<T> for any type you want to pass through Lua.
// Required static members:
//   static T    pull (lua_State*, int idx)  – read from stack at idx
//   static void push (lua_State*, T)        – push onto stack
//   static bool check(lua_State*, int idx)  – non-destructive type check
//
template<typename T, typename = void>
struct converter; // intentionally undefined; specialization required

// integral types (int, long, size_t, uint16_t, …)
template<typename T>
struct converter<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
{
    static T pull(lua_State* L, int idx)
    {
        return static_cast<T>(lua_tointeger(L, idx));
    }
    static void push(lua_State* L, T v)
    {
        lua_pushinteger(L, static_cast<lua_Integer>(v));
    }
    static bool check(lua_State* L, int idx)
    {
        return lua_isinteger(L, idx);
    }
};

// enum class (routed through its underlying integer type)
template<typename T>
struct converter<T, std::enable_if_t<std::is_enum_v<T>>>
{
    using U = std::underlying_type_t<T>;
    static T pull(lua_State* L, int idx)
    {
        return static_cast<T>(lua_tointeger(L, idx));
    }
    static void push(lua_State* L, T v)
    {
        lua_pushinteger(L, static_cast<lua_Integer>(v));
    }
    static bool check(lua_State* L, int idx)
    {
        return lua_isinteger(L, idx);
    }
};

// floating point
template<typename T>
struct converter<T, std::enable_if_t<std::is_floating_point_v<T>>>
{
    static T pull(lua_State* L, int idx)
    {
        return static_cast<T>(lua_tonumber(L, idx));
    }
    static void push(lua_State* L, T v)
    {
        lua_pushnumber(L, static_cast<lua_Number>(v));
    }
    static bool check(lua_State* L, int idx)
    {
        return lua_isnumber(L, idx);
    }
};

// bool
template<>
struct converter<bool>
{
    static bool pull(lua_State* L, int idx)
    {
        return lua_toboolean(L, idx);
    }
    static void push(lua_State* L, bool v)
    {
        lua_pushboolean(L, v ? 1 : 0);
    }
    static bool check(lua_State* L, int idx)
    {
        return lua_isboolean(L, idx);
    }
};

// std::string
template<>
struct converter<std::string>
{
    static std::string pull(lua_State* L, int idx)
    {
        size_t len = 0;
        const char* s = lua_tolstring(L, idx, &len);
        return std::string(s, len);
    }
    static void push(lua_State* L, std::string const& v)
    {
        lua_pushlstring(L, v.data(), v.size());
    }
    static bool check(lua_State* L, int idx)
    {
        return lua_isstring(L, idx);
    }
};

// std::vector<T> – pull reads a 1-indexed Lua table; push creates one
template<typename T>
struct converter<std::vector<T>>
{
    static std::vector<T> pull(lua_State* L, int idx)
    {
        lua_Integer len = (lua_Integer)lua_rawlen(L, idx);
        std::vector<T> out;
        out.reserve((size_t)len);
        for (lua_Integer i = 1; i <= len; ++i) {
            lua_rawgeti(L, idx, i);
            out.push_back(converter<T>::pull(L, -1));
            lua_pop(L, 1);
        }
        return out;
    }
    static void push(lua_State* L, std::vector<T> const& v)
    {
        lua_createtable(L, (int)v.size(), 0);
        for (int i = 0; i < (int)v.size(); ++i) {
            converter<T>::push(L, v[i]);
            lua_rawseti(L, -2, i + 1);
        }
    }
    static bool check(lua_State* L, int idx)
    {
        return lua_istable(L, idx);
    }
};

} // namespace arg

namespace result {
// ── Return type converter ──────────────────────────────────────────────────
//
// Specialise lua::converter<R> to control how a C++ return value is
// pushed onto the Lua stack.  Returns the number of Lua return values.
//
template<typename R, typename = void>
struct converter
{
    static int push(lua_State* L, R&& v)
    {
        arg::converter<R>::push(L, std::forward<R>(v));
        return 1;
    }
};

// bool – push as Lua boolean
template<>
struct converter<bool>
{
    static int push(lua_State* L, bool v)
    {
        lua_pushboolean(L, v ? 1 : 0);
        return 1;
    }
};

// void – nothing to push
template<>
struct converter<void>
{
    static int push(lua_State*)
    {
        return 0;
    }
};

// std::expected<T, E> – push T on success, nil + "operation failed" on error
template<typename T, typename E>
struct converter<std::expected<T, E>>
{
    static int push(lua_State* L, std::expected<T, E>&& v)
    {
        if (v.has_value()) {
            return converter<T>::push(L, std::move(v.value()));
        }
        lua_pushnil(L);
        lua_pushstring(L, "operation failed");
        return 2;
    }
};

} // namespace result

} // namespace lua
