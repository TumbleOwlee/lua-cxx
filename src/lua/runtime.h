#pragma once

#include "binder.h"
#include "object.h"

#include <iostream>
#include <lua5.4/lua.hpp>
#include <memory>
#include <string>

namespace lua {

template<typename Enum>
struct EnumConverter;

// Example implementation of EnumConverter
//
// template<>
// struct EnumConverter<Type>
// {
//     static constexpr EnumValue<Type> const Values[] = { ... };
// };

template<typename Enum>
struct EnumValue
{
    char const* const name;
    Enum value;
};

class Runtime
{
private:
    struct Chunk
    {
        std::string_view const& data;
        size_t used = 0;
    };

    static auto chunk_reader(lua_State*, void* data, size_t* size) -> char const*;

public:
    // Allow move only
    Runtime(Runtime&&);
    auto operator=(Runtime&&) -> Runtime&;

    static auto withLibs() -> Runtime;

    static auto sandboxed() -> Runtime;

    template<typename T>
    auto addEnum(std::string name) -> void
    {
        lua_newtable(state.get()); // enum table
        auto& values = EnumConverter<T>::Values;
        for (auto& v : values) {
            std::cerr << v.name << std::endl;
            lua_pushinteger(state.get(), static_cast<int>(v.value));
            lua_setfield(state.get(), -2, v.name);
        }
        lua_setglobal(state.get(), name.c_str());
    }

    auto addGlobalObject(std::string const name, GlobalObject* global) -> bool;

    template<auto Method>
    void addGlobalMethod(std::string const name)
    {
        lua_pushcfunction(state.get(), &method::Binder<Method>::call);
        lua_setglobal(state.get(), name.c_str());
    }

    auto load(std::string_view const& script) -> bool;

    template<typename... Args>
    auto call(std::string method, Args... args) -> bool
    {
        lua_getglobal(state.get(), method.c_str());
        if (!lua_isfunction(state.get(), -1)) {
            std::cerr << "Lua: '" << method << "' is not a function" << std::endl;
            lua_pop(state.get(), 1);
            return false;
        }

        (arg::converter<Args>::push(state.get(), std::forward<Args>(args)), ...);

        constexpr int nargs = static_cast<int>(sizeof...(Args));
        if (lua_pcall(state.get(), nargs, 0, 0) != LUA_OK) {
            std::cerr << "Lua error in '" << method << "': " << lua_tostring(state.get(), -1) << std::endl;
            lua_pop(state.get(), 1);
            return false;
        }
        return true;
    }

private:
    // Custom deleter for lua state
    struct StateDeleter
    {
        auto operator()(lua_State* state) -> void
        {
            lua_close(state);
        }
    };

    // lua state
    std::unique_ptr<lua_State, StateDeleter> state;

    Runtime();

    // Forbid copy
    Runtime(Runtime const&) = delete;
    auto operator=(Runtime const&) -> Runtime& = delete;

    auto addGlobalMetaTable(std::string const& name, std::vector<BoundMethod> const& methods) -> void;

    auto attachGlobalInstance(std::string const& name, GlobalObject* global, std::vector<BoundMethod> const& config)
        -> bool;
};

} // namespace lua
