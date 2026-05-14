#include "runtime.h"

#include <iostream>

auto lua::Runtime::chunk_reader(lua_State*, void* data, size_t* size) -> char const*
{
    auto chunk = reinterpret_cast<Chunk*>(data);
    if (chunk->used < chunk->data.size()) {
        *size = chunk->data.size();
        chunk->used += *size;
        return chunk->data.data();
    } else {
        *size = 0;
        return nullptr;
    }
}

// Allow move only
lua::Runtime::Runtime(Runtime&&) = default;
auto lua::Runtime::operator=(Runtime&&) -> Runtime& = default;

auto lua::Runtime::withLibs() -> Runtime
{
    Runtime runtime;
    luaL_openlibs(runtime.state.get());
    return std::move(runtime);
}

auto lua::Runtime::sandboxed() -> Runtime
{
    Runtime runtime;
    luaopen_base(runtime.state.get());
    luaopen_table(runtime.state.get());
    luaopen_string(runtime.state.get());
    luaopen_math(runtime.state.get());
    luaopen_utf8(runtime.state.get());
    return std::move(runtime);
}

auto lua::Runtime::addGlobalObject(std::string const name, GlobalObject* global) -> bool
{
    auto methods = global->luaConfig();
    addGlobalMetaTable(name, methods);
    return attachGlobalInstance(name, global, methods);
}

auto lua::Runtime::load(std::string_view const& script) -> bool
{
    Chunk chunk = { .data = script, .used = 0 };
    auto res = lua_load(state.get(), chunk_reader, &chunk, "lua script", nullptr);
    if (res != LUA_OK) {
        std::cerr << "Lua load error: " << lua_tostring(state.get(), -1) << std::endl;
        lua_pop(state.get(), 1);
        return false;
    }
    // Execute the compiled chunk to define globals (e.g. function init()...end)
    res = lua_pcall(state.get(), 0, 0, 0);
    if (res != LUA_OK) {
        std::cerr << "Lua script error: " << lua_tostring(state.get(), -1) << std::endl;
        lua_pop(state.get(), 1);
        return false;
    }
    return true;
}

lua::Runtime::Runtime()
    : state(luaL_newstate())
{
}

auto lua::Runtime::addGlobalMetaTable(std::string const& name, std::vector<BoundMethod> const& methods) -> void
{
    lua_newtable(state.get()); // [mt]
    lua_newtable(state.get()); // [mt, obj]

    for (auto& method : methods) {
        lua_pushstring(state.get(), method.name.c_str());
        lua_pushcfunction(state.get(), method.fn);
        lua_settable(state.get(), -3);
    }

    lua_pushstring(state.get(), "__index");
    lua_pushvalue(state.get(), -2);
    lua_settable(state.get(), -4); // mt['__index'] = obj → [mt, obj]

    lua_pop(state.get(), 1); // [mt]

    auto meta = name + "_mt";
    lua_pushstring(state.get(), meta.c_str());
    lua_pushvalue(state.get(), -2);
    lua_settable(state.get(),
                 LUA_REGISTRYINDEX); // registry[metatable] = mt → [mt]

    lua_pop(state.get(), 1); // []
}

auto lua::Runtime::attachGlobalInstance(std::string const& name,
                                        GlobalObject* global,
                                        std::vector<BoundMethod> const& config) -> bool
{
    // Create storage for instance reference
    void** instance = reinterpret_cast<void**>(lua_newuserdata(state.get(), sizeof(void*)));
    *instance = global;

    // Store reference in registry
    std::string meta = name + "_mt";
    lua_pushstring(state.get(), meta.c_str());
    lua_gettable(state.get(), LUA_REGISTRYINDEX);
    if (!lua_istable(state.get(), -1)) {
        lua_pop(state.get(), 1);
        lua_pop(state.get(), 1);
        return false;
    }
    lua_setmetatable(state.get(), -2);
    lua_setglobal(state.get(), name.c_str());
    return true;
}
