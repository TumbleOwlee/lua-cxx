#pragma once

#include <cstdint>
#include <cstring>
#include <expected>
#include <string>
#include <vector>

#include "lua/binder.h"
#include "lua/converter.h"
#include "lua/object.h"
#include "lua/runtime.h"

namespace modbus {

struct Host
{
    std::string url;
    uint16_t port = 0;
};

class Address
{
    friend class Modbus;

public:
    Address(uint16_t address);

private:
    uint16_t address;
};

enum class Read
{
    Coil = 1,
    DiscreteInput = 2,
    HoldingRegister = 3,
    InputRegister = 4,
};

enum class Write
{
    Coil = 1,
    MultipleCoils = 2,
    SingleRegister = 3,
    MultipleRegister = 4,
};

enum class Error
{
    Failed,
};

class Modbus : public lua::GlobalObject
{
public:
    using RegisterValues = std::vector<uint16_t>;

    Modbus(Host host);

    auto read(Read read, Address address, size_t count) -> std::expected<RegisterValues, Error>;

    auto write(Write write, Address address, RegisterValues&& values) -> bool;

    auto luaConfig() -> std::vector<lua::BoundMethod> override
    {
        return { lua::BoundMethod::bind<&Modbus::write>("write"), lua::BoundMethod::bind<&Modbus::read>("read") };
    }

private:
    Host host;
};

} // namespace modbus

// lua::converter specialization for modbus::Address so it can be used as a
// method argument in lua::bind<> bindings (reads as an integer from Lua).
template<>
struct lua::arg::converter<modbus::Address>
{
    static modbus::Address pull(lua_State* L, int idx)
    {
        return modbus::Address(static_cast<uint16_t>(lua_tointeger(L, idx)));
    }

    static bool check(lua_State* L, int idx)
    {
        return lua_isinteger(L, idx);
    }
};

static constexpr auto enumName(std::string_view const s) -> char const* const
{
    for (auto i = s.size() - 1; i >= 0; --i) {
        if (s[i] == ':') {
            return &s[i + 1];
        }
    }
    return s.data();
}

#define ENUM(V) { .name = enumName(#V), .value = V }

template<>
struct lua::EnumConverter<modbus::Read>
{
    static constexpr EnumValue<modbus::Read> const Values[] = { ENUM(modbus::Read::Coil),
                                                                ENUM(modbus::Read::DiscreteInput),
                                                                ENUM(modbus::Read::HoldingRegister),
                                                                ENUM(modbus::Read::InputRegister) };
};

template<>
struct lua::EnumConverter<modbus::Write>
{
    static constexpr EnumValue<modbus::Write> const Values[] = { ENUM(modbus::Write::Coil),
                                                                 ENUM(modbus::Write::MultipleCoils),
                                                                 ENUM(modbus::Write::SingleRegister),
                                                                 ENUM(modbus::Write::MultipleRegister) };
};
