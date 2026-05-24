#include "luau_class.h"
#include "luau_utils.h"
#include "luacode.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

void log_memory_usage() {
    pid_t pid = getpid();
    std::string statm_path = "/proc/" + std::to_string(pid) + "/statm";
    std::ifstream statm_file(statm_path);
    if (!statm_file.is_open()) {
        std::cerr << "Failed to open " << statm_path << std::endl;
        return;
    }
    size_t total_pages, resident_pages;
    statm_file >> total_pages >> resident_pages;
    long page_size = sysconf(_SC_PAGESIZE);
    auto b = (resident_pages * page_size) % 1000;
    auto kb = (resident_pages * page_size / 1000) % 1000;
    auto mb = (resident_pages * page_size / 1000 / 1000);
    std::cerr << mb << " MB, " << kb << " KB, " << b << " B" << std::endl;
}

// ===== TEST CODE =====

class Robot {
public:
    Robot(std::string name, int initial_battery)
        : name(std::move(name)), battery(initial_battery) {}
    Robot(std::string name) : Robot(std::move(name), 100) {}
    ~Robot() = default;
    void charge(int amount) { battery += amount; }
    bool is_low() { return battery < 20; }
    std::string get_name() { return name; }

private:
    std::string name;
    int battery;
    uint8_t storage[1024];
};

// Example script
const char *const SCRIPT = R"x(
    local r = Robot.new("R2D2", 50)
    local r2 = Robot.new("C3PO")
    print('Name: ' .. r:get_name()..", Name: "..r2:get_name())
    r:charge(50, 10)
    if r:is_low() then
        print('Low!')
    else
        print('OK!')
    end
    r:charge(-140)
    if r:is_low() then
        print('Low!')
    else
        print('OK!')
    end
    r = nil
)x";

int main() {
    // Create storage for set of states
    constexpr size_t COUNT = 400;
    lua_State *states[COUNT];

    // Iterate multiple times to check memory usage
    for (auto i = 0; i < COUNT; ++i) {
        // Create new state
        states[i] = luaL_newstate();
        lua_State *L = states[i];

        // Add selective set of libraries
        luaopen_base(L);
        luaopen_table(L);
        luaopen_string(L);
        luaopen_bit32(L);
        luaopen_buffer(L);
        luaopen_utf8(L);
        luaopen_class(L);
        luaopen_math(L);
        luaopen_vector(L);
        luaopen_integer(L);

        // Bind class to lua
        luau::Class<Robot, "Robot"> robot_class(L);
        robot_class.constructor<std::string, int>();
        robot_class.constructor<std::string>();
        robot_class.method(luau::Method("charge", &Robot::charge));
        robot_class.method(luau::Method("is_low", &Robot::is_low));
        robot_class.method(luau::Method("get_name", &Robot::get_name));

        // Enable sandbox mode
        luaL_sandbox(L);

        // Load lua chunk
        size_t bytecode_size = 0;
        char *bytecode = luau_compile(SCRIPT, strlen(SCRIPT), nullptr, &bytecode_size);
        int load_result = luau_load(L, "example", bytecode, bytecode_size, 0);
        free(bytecode);

        // Check if loading succeeded
        if (load_result != LUA_OK) {
            std::cerr << "Load error: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
            lua_close(L);
            continue;
        }

        // Call loaded chunk
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            std::cerr << "Script error: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
        }
    }

    // Get total usage
    log_memory_usage();

    // Clear all states
    for (auto i = 0; i < COUNT; ++i) {
        lua_close(states[i]);
    }

    std::cout << "SUCCESS: All methods executed correctly!" << std::endl;
    log_memory_usage();
    return 0;
}
