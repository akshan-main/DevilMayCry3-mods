cmake_minimum_required(VERSION 3.10)
project(dmc3mod)

set(CMAKE_CXX_STANDARD 11)

if(MSVC)
    add_compile_options(/W3 /GS-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# core - shared between both targets, compiles as whatever arch the build is
add_library(core STATIC core/core.cpp core/core.h)
target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(core PUBLIC psapi)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    # ---- x64: HD Collection (Steam) ----
    # build with: cmake .. -A x64  (or run build_hd.bat)

    add_library(modhd SHARED targets/hd/mod.cpp)
    target_link_libraries(modhd PRIVATE core)
    set_target_properties(modhd PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out_hd")

    # dinput8.dll proxy loader
    add_library(dinput8proxy SHARED targets/hd/proxy.cpp)
    set_target_properties(dinput8proxy PROPERTIES
        OUTPUT_NAME "dinput8"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out_hd")

    add_custom_command(TARGET modhd POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/config/dmc3_mod_hd.ini"
            "$<TARGET_FILE_DIR:modhd>/dmc3_mod.ini")

else()
    # ---- x86: 2006 Ubisoft port ----
    # build with: cmake .. -A Win32  (or run build.bat)

    add_executable(injector targets/2006/injector.cpp)
    set_target_properties(injector PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")

    add_library(mod2006 SHARED targets/2006/mod.cpp)
    target_link_libraries(mod2006 PRIVATE core d3d9)
    set_target_properties(mod2006 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out")

    add_custom_command(TARGET mod2006 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/config/dmc3_mod_2006.ini"
            "$<TARGET_FILE_DIR:mod2006>/dmc3_mod.ini")

endif()
