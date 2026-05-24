#pragma once
#include "lua.h"
#include "lualib.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace luau {

// --- FIXED STRING NTTP ---
template <std::size_t N>
struct FixedString {
    char data[N];
    constexpr FixedString(const char (&s)[N]) { std::copy(s, s + N, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};
template <std::size_t N>
FixedString(const char (&)[N]) -> FixedString<N>;

// --- TYPE TAG (stable per-T identity, independent of Lua name) ---
template <typename T>
struct TypeTag {
    static void *get() {
        static char tag = 0;
        return &tag;
    }
};

// --- TYPE CONVERTER ---
template <typename T>
struct Converter {
    static T from_stack(lua_State *L, int index) {
        if constexpr (std::is_same_v<T, int>)
            return (int)luaL_checkinteger(L, index);
        else if constexpr (std::is_same_v<T, double>)
            return (double)luaL_checknumber(L, index);
        else if constexpr (std::is_same_v<T, std::string>)
            return std::string(luaL_checkstring(L, index));
        else if constexpr (std::is_same_v<T, bool>)
            return (bool)lua_toboolean(L, index);
        else
            return T();
    }
};

// --- METHOD BINDER ---
template <typename T, typename Ret, typename... Args>
class MethodBinder {
    using MethodPtr = Ret (T::*)(Args...);

public:
    static int invoke(lua_State *L, T *obj, MethodPtr method) {
        return dispatch(L, obj, method, std::make_index_sequence<sizeof...(Args)>{});
    }

private:
    template <size_t... Is>
    static int dispatch(lua_State *L, T *obj, MethodPtr method, std::index_sequence<Is...>) {
        if constexpr (std::is_void_v<Ret>) {
            (obj->*method)(Converter<Args>::from_stack(L, Is + 2)...);
            return 0;
        } else {
            Ret result = (obj->*method)(Converter<Args>::from_stack(L, Is + 2)...);
            if constexpr (std::is_same_v<Ret, int>)
                lua_pushinteger(L, result);
            else if constexpr (std::is_same_v<Ret, double>)
                lua_pushnumber(L, result);
            else if constexpr (std::is_same_v<Ret, std::string>)
                lua_pushstring(L, result.c_str());
            else if constexpr (std::is_same_v<Ret, bool>)
                lua_pushboolean(L, result);
            return 1;
        }
    }
};

template <typename T, typename Ret, typename... Args>
class Method {
public:
    Method(std::string name, Ret (T::*method)(Args...)) : name(name), method(method) {}

    std::string name;
    Ret (T::*method)(Args...);
};

// --- STATIC REGISTRIES (per bound type T) ---
template <typename T>
class ConstructorRegistry {
public:
    static auto get() -> std::unordered_map<int, std::function<T *(lua_State *)>> & {
        static std::unordered_map<int, std::function<T *(lua_State *)>> ctors;
        return ctors;
    }
};

template <typename T>
class MethodRegistry {
public:
    static auto get() -> std::unordered_map<std::string, std::function<int(lua_State *)>> & {
        static std::unordered_map<std::string, std::function<int(lua_State *)>> methods;
        return methods;
    }
};

// --- CLASS BINDING ---
template <typename T, FixedString Name>
class Class {
    static_assert(Name.view().size() > 0, "Lua class name cannot be empty");

    lua_State *state;
    std::string metatable_name;

    template <typename... CtorArgs>
    struct ConstructorInvoker {
        template <size_t... Is>
        static T *make(lua_State *L, std::index_sequence<Is...>) {
            return new T(Converter<CtorArgs>::from_stack(L, Is + 1)...);
        }
        static T *invoke(lua_State *L) {
            return make(L, std::make_index_sequence<sizeof...(CtorArgs)>{});
        }
    };

public:
    Class(lua_State *L) : state(L), metatable_name(std::string(Name.view()) + "_mt") { bind(); }

    void bind() {
        // Get or create the per-state tracking table in the registry
        lua_getfield(state, LUA_REGISTRYINDEX, "__luauclass");
        if (lua_isnil(state, -1)) {
            lua_pop(state, 1);
            lua_newtable(state);
            lua_pushvalue(state, -1);
            lua_setfield(state, LUA_REGISTRYINDEX, "__luauclass");
        }
        // stack: [__luauclass]

        // Check: same C++ type already registered in this state?
        lua_pushlightuserdata(state, TypeTag<T>::get());
        lua_gettable(state, -2);
        if (!lua_isnil(state, -1)) {
            std::string existing_name = lua_tostring(state, -1);
            lua_pop(state, 2);
            std::cerr << "Class: type already registered as '" << existing_name << "'" << std::endl;
            abort();
        }
        lua_pop(state, 1);

        // Check: name already taken by a different type in this state?
        lua_pushstring(state, Name.data);
        lua_gettable(state, -2);
        if (!lua_isnil(state, -1)) {
            lua_pop(state, 2);
            std::cerr << "Class: name '" << Name.data << "' is already in use" << std::endl;
            abort();
        }
        lua_pop(state, 1);

        // Record: type_tag -> name
        lua_pushlightuserdata(state, TypeTag<T>::get());
        lua_pushstring(state, Name.data);
        lua_settable(state, -3);

        // Record: name -> type_tag (for name-collision detection)
        lua_pushstring(state, Name.data);
        lua_pushlightuserdata(state, TypeTag<T>::get());
        lua_settable(state, -3);

        lua_pop(state, 1); // pop __luauclass

        // Set up metatable with __index table
        luaL_newmetatable(state, metatable_name.c_str());
        lua_newtable(state);
        lua_setfield(state, -2, "__index");
        lua_pop(state, 1);

        // Create class table and register .new constructor
        lua_newtable(state);
        std::string ctor_name = metatable_name + ".new";
        lua_pushstring(state, metatable_name.c_str());
        lua_pushcclosurek(
            state,
            [](lua_State *L) -> int {
                const char *metatable_name = lua_tostring(L, lua_upvalueindex(1));
                int arg_count = lua_gettop(L);
                auto &ctors = ConstructorRegistry<T>::get();
                auto it = ctors.find(arg_count);
                if (it == ctors.end())
                    luaL_error(L, "no constructor with %d argument(s)", arg_count);

                T **ud = (T **)lua_newuserdatadtor(L, sizeof(T *), [](void *data) {
                    T **p = (T **)data;
                    if (*p) {
                        delete *p;
                        *p = nullptr;
                    }
                });
                *ud = nullptr;
                *ud = it->second(L);

                luaL_newmetatable(L, metatable_name);
                lua_setmetatable(L, -2);
                return 1;
            },
            ctor_name.c_str(),
            1,
            nullptr);
        lua_setfield(state, -2, "new");
        lua_setglobal(state, Name.data);
    }

    template <typename... CtorArgs>
    void constructor() {
        ConstructorRegistry<T>::get()[sizeof...(CtorArgs)] = [](lua_State *L) -> T * {
            return ConstructorInvoker<CtorArgs...>::invoke(L);
        };
    }

    template <typename Ret, typename... Args>
    void method(Method<T, Ret, Args...> method) {
        luaL_newmetatable(state, metatable_name.c_str());
        lua_getfield(state, -1, "__index");

        std::string closure_name = metatable_name + ":" + method.name;
        lua_pushstring(state, method.name.c_str());
        lua_pushcclosurek(
            state,
            [](lua_State *L) -> int {
                const char *method_name = lua_tostring(L, lua_upvalueindex(1));
                auto &methods = MethodRegistry<T>::get();
                auto it = methods.find(method_name);
                if (it != methods.end())
                    return it->second(L);
                return 0;
            },
            closure_name.c_str(),
            1,
            nullptr);
        lua_setfield(state, -2, method.name.c_str());

        auto method_ptr = method.method;
        auto handler = [metatable_name = metatable_name, method_ptr](lua_State *L) -> int {
            T **ud = (T **)luaL_checkudata(L, 1, metatable_name.c_str());
            return MethodBinder<T, Ret, Args...>::invoke(L, *ud, method_ptr);
        };

        MethodRegistry<T>::get()[method.name] = std::move(handler);
        lua_pop(state, 2);
    }

    template <typename Ret, typename... Args>
    void method_inner(std::string method_name, Ret (T::*method_ptr)(Args...)) {}
};

} // namespace luau
