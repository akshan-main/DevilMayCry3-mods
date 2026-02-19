cmake_minimum_required(VERSION 3.10)
project(dmc3mod)

set(CMAKE_CXX_STANDARD 11)

if(MSVC)
    add_compile_options(/W3 /GS-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# core (static lib)
add_library(core STATIC core/core.cpp core/core.h)
target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(core PUBLIC psapi)

# injector.exe
add_executable(injector targets/2006/injector.cpp)
set_target_properties(injector PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")

# mod2006.dll
add_library(mod2006 SHARED targets/2006/mod.cpp)
target_link_libraries(mod2006 PRIVATE core d3d9)
set_target_properties(mod2006 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")

# copy ini
add_custom_command(TARGET mod2006 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config/dmc3_mod_2006.ini"
        "$<TARGET_FILE_DIR:mod2006>/dmc3_mod.ini")
