cmake_minimum_required(VERSION 3.20)
project(Bejeweled3Access CXX)

# The game is a 32-bit process; this must be configured with the MSVC x86
# toolchain (vcvars32). See docs/building.md.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Build with the 32-bit (x86) MSVC toolchain")
endif()

set(PRISM_ROOT "${CMAKE_SOURCE_DIR}/deps/prism-x86" CACHE PATH "32-bit Prism install")

# Every src/mod/*.cpp is part of the mod.
file(GLOB MOD_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/mod/*.cpp")

add_library(winmm SHARED
  src/proxy/winmm_proxy.cpp
  src/proxy/winmm_exports.def
  ${MOD_SOURCES}
)
set_target_properties(winmm PROPERTIES OUTPUT_NAME winmm CXX_STANDARD 17)
target_include_directories(winmm PRIVATE src/proxy "${PRISM_ROOT}/include")
target_compile_definitions(winmm PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
# Static CRT so the game folder needs no extra runtime DLLs.
set_property(TARGET winmm PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_options(winmm PRIVATE /W4 /utf-8)
# prism.dll is delay-loaded so a missing DLL is logged instead of stopping the game.
target_link_libraries(winmm PRIVATE "${PRISM_ROOT}/lib/prism.lib" delayimp user32 bcrypt)
target_link_options(winmm PRIVATE /DELAYLOAD:prism.dll /SAFESEH:NO)
