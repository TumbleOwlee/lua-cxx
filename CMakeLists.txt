# Set version requirement
cmake_minimum_required(VERSION 3.25)

# Define project
project(lua
    VERSION 0.0.1
    DESCRIPTION "Draft implementation of a lua based EVSE driver."
    LANGUAGES C CXX)

# Export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default to Debug
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE Debug)
endif()
message("Build type: ${CMAKE_BUILD_TYPE}")

# Create list of all source files
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*" )

# Get Lua
find_package(Lua REQUIRED)

# Import lobrary if not already done
if(Lua_FOUND AND NOT TARGET Lua::Lua)
  add_library(Lua::Lua INTERFACE IMPORTED)
  set_target_properties(
    Lua::Lua
    PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}"
  )
endif()

# Create executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Link lua
target_link_libraries(${PROJECT_NAME} Lua::Lua)
