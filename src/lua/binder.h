#pragma once

#include "converter.h"

#include <lua5.4/lua.hpp>
#include <tuple>
#include <type_traits>

namespace lua {

namespace method {

// ── Method binder ──────────────────────────────────────────────────────────
//
// lua::binder<&Class::method>::call is a lua_CFunction that:
//   1. checks the argument count (1 self + sizeof...(Args))
//   2. checks each argument type
//   3. extracts arguments from the stack
//   4. retrieves the C++ object from the Lua userdata at index 1
//   5. calls the method and pushes the result
//
template<auto Method>
struct Binder; // defined via partial specialization below

// non-const member function
template<typename Obj, typename Ret, typename... Args, Ret (Obj::*Method)(Args...)>
struct Binder<Method>
{
private:
    // Validate that every argument at indices [start, start+N) matches its type
    template<size_t... Is>
    static bool validate(lua_State* L, int start, std::index_sequence<Is...>)
    {
        return (arg::converter<std::decay_t<Args>>::check(L, start + (int)Is) && ...);
    }

    // Extract arguments from the stack into a tuple
    template<size_t... Is>
    static auto extract(lua_State* L, int start, std::index_sequence<Is...>)
    {
        return std::tuple<std::decay_t<Args>...>{ arg::converter<std::decay_t<Args>>::pull(L, start + (int)Is)... };
    }

public:
    static int call(lua_State* L)
    {
        constexpr int expected = (int)sizeof...(Args) + 1; // +1 for self
        int nargs = lua_gettop(L);
        if (nargs != expected) {
            lua_pushnil(L);
            lua_pushstring(L, nargs < expected ? "too few arguments" : "too many arguments");
            return 2;
        }

        // Validate argument types
        if (!validate(L, 2, std::index_sequence_for<Args...>{})) {
            lua_pushnil(L);
            lua_pushstring(L, "argument type mismatch");
            return 2;
        }

        // Extract self
        void* ud = lua_touserdata(L, 1);
        if (ud == nullptr) {
            lua_pushnil(L);
            lua_pushstring(L, "expected userdata as self");
            return 2;
        }
        Obj* self = *reinterpret_cast<Obj**>(ud);
        if (self == nullptr) {
            lua_pushnil(L);
            lua_pushstring(L, "null userdata pointer");
            return 2;
        }

        // Extract arguments and call
        auto args = extract(L, 2, std::index_sequence_for<Args...>{});
        if constexpr (std::is_void_v<Ret>) {
            std::apply(
                [self](auto&&... a) {
                    (self->*Method)(std::forward<decltype(a)>(a)...);
                },
                std::move(args));
            return 0;
        } else {
            auto result = std::apply(
                [self](auto&&... a) {
                    return (self->*Method)(std::forward<decltype(a)>(a)...);
                },
                std::move(args));
            return result::converter<Ret>::push(L, std::move(result));
        }
    }
};

// const member function — same body, different pointer type
template<typename Obj, typename Ret, typename... Args, Ret (Obj::*Method)(Args...) const>
struct Binder<Method>
{
private:
    template<size_t... Is>
    static bool validate(lua_State* L, int start, std::index_sequence<Is...>)
    {
        return (arg::converter<std::decay_t<Args>>::check(L, start + (int)Is) && ...);
    }

    template<size_t... Is>
    static auto extract(lua_State* L, int start, std::index_sequence<Is...>)
    {
        return std::tuple<std::decay_t<Args>...>{ arg::converter<std::decay_t<Args>>::pull(L, start + (int)Is)... };
    }

public:
    static int call(lua_State* L)
    {
        constexpr int expected = (int)sizeof...(Args) + 1;
        int nargs = lua_gettop(L);
        if (nargs != expected) {
            lua_pushnil(L);
            lua_pushstring(L, nargs < expected ? "too few arguments" : "too many arguments");
            return 2;
        }
        if (!validate(L, 2, std::index_sequence_for<Args...>{})) {
            lua_pushnil(L);
            lua_pushstring(L, "argument type mismatch");
            return 2;
        }
        void* ud = lua_touserdata(L, 1);
        if (ud == nullptr) {
            lua_pushnil(L);
            lua_pushstring(L, "expected userdata as self");
            return 2;
        }
        const Obj* self = *reinterpret_cast<const Obj**>(ud);
        if (self == nullptr) {
            lua_pushnil(L);
            lua_pushstring(L, "null userdata pointer");
            return 2;
        }
        auto args = extract(L, 2, std::index_sequence_for<Args...>{});
        if constexpr (std::is_void_v<Ret>) {
            std::apply(
                [self](auto&&... a) {
                    (self->*Method)(std::forward<decltype(a)>(a)...);
                },
                std::move(args));
            return 0;
        } else {
            auto result = std::apply(
                [self](auto&&... a) {
                    return (self->*Method)(std::forward<decltype(a)>(a)...);
                },
                std::move(args));
            return result::converter<Ret>::push(L, std::move(result));
        }
    }
};

// const member function — same body, different pointer type
template<typename Ret, typename... Args, Ret (*Method)(Args...)>
struct Binder<Method>
{
private:
    template<size_t... Is>
    static bool validate(lua_State* L, int start, std::index_sequence<Is...>)
    {
        return (arg::converter<std::decay_t<Args>>::check(L, start + (int)Is) && ...);
    }

    template<size_t... Is>
    static auto extract(lua_State* L, int start, std::index_sequence<Is...>)
    {
        return std::tuple<std::decay_t<Args>...>{ arg::converter<std::decay_t<Args>>::pull(L, start + (int)Is)... };
    }

public:
    static int call(lua_State* L)
    {
        constexpr int expected = (int)sizeof...(Args);
        int nargs = lua_gettop(L);
        if (nargs != expected) {
            lua_pushnil(L);
            lua_pushstring(L, nargs < expected ? "too few arguments" : "too many arguments");
            return 2;
        }
        if (!validate(L, 1, std::index_sequence_for<Args...>{})) {
            lua_pushnil(L);
            lua_pushstring(L, "argument type mismatch");
            return 2;
        }
        auto args = extract(L, 1, std::index_sequence_for<Args...>{});
        if constexpr (std::is_void_v<Ret>) {
            std::apply(
                [](auto&&... a) {
                    (*Method)(std::forward<decltype(a)>(a)...);
                },
                std::move(args));
            return 0;
        } else {
            auto result = std::apply(
                [](auto&&... a) {
                    return (*Method)(std::forward<decltype(a)>(a)...);
                },
                std::move(args));
            return result::converter<Ret>::push(L, std::move(result));
        }
    }
};

} // namespace method

struct BoundMethod
{
    friend class Runtime;

public:
    template<auto Method>
    static auto bind(char const* const name) -> BoundMethod
    {
        return std::move(BoundMethod(name, &lua::method::Binder<Method>::call));
    }

private:
    std::string name;
    lua_CFunction fn;

    BoundMethod(char const* const name, lua_CFunction fn)
        : name(name)
        , fn(fn)
    {
    }
};

} // namespace lua
