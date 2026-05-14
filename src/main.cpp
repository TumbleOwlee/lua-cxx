#include "lua/runtime.h"
#include "modbus.h"

#include <chrono>
#include <iomanip>
#include <iostream>

// Simple example script
const char* const LUA_SCRIPT = R"x(
REGISTER = {
    current = {
        address = 432,
        len = 2,
    },
    power = {
        address = 110,
        len = 123,
    }
}

function ReadRegister(count)
    local vals, err = Modbus:read(Read.InputRegister, REGISTER["current"]["address"], count)
    if err then
        print("Error: "..err)
    else
        for i, v in ipairs(vals) do
            print(i .. ": " .. v)
        end
    end
end

function WriteRegister()
    local ok = Modbus:write(2, 111, { 10, 11})
    if not ok then
        print("Error: write failed")
    end
end
)x";

// Customized print
static auto print(std::string msg) -> void
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::cerr << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S %Z") << "] " << msg << std::endl;
}

// Check result and abort on error
static auto check(bool result, char const* const message) -> void
{
    if (!result) {
        std::cerr << message << std::endl;
        abort();
    }
}

int main(int argc, char** argv)
{
    // Create Modbus device
    modbus::Modbus modbus(modbus::Host{ .url = "127.0.0.1", .port = 1502 });

    // Create Lua runtime
    auto runtime = lua::Runtime::sandboxed();

    // Overwrite global print method with custom
    runtime.addGlobalMethod<print>("print");

    // Add enums
    runtime.addEnum<modbus::Read>("Read");
    runtime.addEnum<modbus::Write>("Write");

    // Attach global instance of Modbus to runtime
    check(runtime.addGlobalObject("Modbus", dynamic_cast<lua::GlobalObject*>(&modbus)),
          "Failed to attach object to lua runtime.");

    // Load lua script
    check(runtime.load(LUA_SCRIPT), "Failed to load script into lua runtime.");
    std::cerr << "Loaded script successfully." << std::endl;

    // Call methods
    runtime.call("ReadRegister", 12345);
    runtime.call("WriteRegister");

    return 0;
}
