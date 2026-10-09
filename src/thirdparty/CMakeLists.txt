set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_MODULE_STD OFF)

pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

file(GLOB _celt_dir   "${CMAKE_CURRENT_SOURCE_DIR}/celt-v*")
file(GLOB _mojoal_dir "${CMAKE_CURRENT_SOURCE_DIR}/mojoAL")
file(GLOB _dxvk_dir   "${CMAKE_CURRENT_SOURCE_DIR}/dxvk-*")

file(GLOB _celt_src "${_celt_dir}/libcelt/*.c")
list(FILTER _celt_src EXCLUDE REGEX "static_modes_")
add_library(celt STATIC ${_celt_src})
target_link_libraries(celt PRIVATE ks_common)
target_include_directories(celt PUBLIC "${_celt_dir}/libcelt")
target_compile_definitions(celt PRIVATE USE_ALLOCA FLOAT_APPROX CUSTOM_MODES)

file(GLOB _mojo_src "${_mojoal_dir}/*.c")
add_library(mojoAL STATIC ${_mojo_src})
target_link_libraries(mojoAL PRIVATE ks_common)
target_include_directories(mojoAL PRIVATE "${_mojoal_dir}/AL")
target_include_directories(mojoAL PUBLIC "${_mojoal_dir}")
target_compile_definitions(mojoAL PRIVATE AL_LIBTYPE_STATIC)

find_program(GLSLANG NAMES glslang glslangValidator REQUIRED)

set(KS_DXVK_GENERATED "${CMAKE_BINARY_DIR}/dxvk-generated")
file(MAKE_DIRECTORY "${KS_DXVK_GENERATED}")

set(_shader_dirs
    "${_dxvk_dir}/src/dxvk/shaders"
    "${_dxvk_dir}/src/dxvk/hud/shaders"
    "${_dxvk_dir}/src/d3d9/shaders")
set(_shaders "")
set(_glsl "")
foreach(_d ${_shader_dirs})
    file(GLOB _s "${_d}/*.vert" "${_d}/*.frag" "${_d}/*.comp")
    file(GLOB _g "${_d}/*.glsl")
    list(APPEND _shaders ${_s})
    list(APPEND _glsl ${_g})
endforeach()

set(_shader_outs "")
set(_glslang_extra "")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_glslang_extra -gVS)
endif()
foreach(_s ${_shaders})
    get_filename_component(_name "${_s}" NAME_WE)
    set(_out "${KS_DXVK_GENERATED}/${_name}.h")
    add_custom_command(OUTPUT "${_out}"
        COMMAND "${GLSLANG}" --quiet --target-env vulkan1.3 --vn "${_name}"
                ${_glslang_extra} "${_s}" -o "${_out}"
        DEPENDS "${_s}" ${_glsl}
        COMMENT "glslang ${_name}"
        VERBATIM)
    list(APPEND _shader_outs "${_out}")
endforeach()

set(VCS_TAG "v3.0.1")
set(BUILD_TARGET "x86_64")
set(BUILD_COMPILER "gcc")
set(BUILD_COMPILER_VERSION "16.2.0")
configure_file("${_dxvk_dir}/version.h.in"   "${KS_DXVK_GENERATED}/version.h"   @ONLY)
configure_file("${_dxvk_dir}/buildenv.h.in"  "${KS_DXVK_GENERATED}/buildenv.h"  @ONLY)

add_custom_target(dxvk_generated ALL
    DEPENDS ${_shader_outs} "${KS_DXVK_GENERATED}/version.h" "${KS_DXVK_GENERATED}/buildenv.h")

add_library(dxvk_headers INTERFACE)
target_include_directories(dxvk_headers INTERFACE
    "${KS_DXVK_GENERATED}"
    "${_dxvk_dir}/include/native/windows"
    "${_dxvk_dir}/include/native/directx")

file(GLOB _jolt_dir "${CMAKE_CURRENT_SOURCE_DIR}/JoltPhysics-*")
file(GLOB_RECURSE _jolt_src CONFIGURE_DEPENDS "${_jolt_dir}/Jolt/*.cpp")
list(FILTER _jolt_src EXCLUDE REGEX "/Jolt/(Compute|Shaders)/")
add_library(joltphysics STATIC ${_jolt_src})
target_include_directories(joltphysics PUBLIC "${_jolt_dir}")
target_link_libraries(joltphysics PRIVATE ks_common)
target_compile_definitions(joltphysics PUBLIC JPH_DEBUG_RENDERER)
set_target_properties(joltphysics PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 30)

file(GLOB _rmlui_dir "${CMAKE_CURRENT_SOURCE_DIR}/RmlUi-*")
set(_patched "${CMAKE_BINARY_DIR}/patched")
file(MAKE_DIRECTORY "${_patched}")

set(_dxvk_native "${_patched}/dxvk-native")
file(COPY "${_dxvk_dir}/include/native/windows" "${_dxvk_dir}/include/native/directx"
     DESTINATION "${_dxvk_native}")
file(READ "${_dxvk_native}/windows/windows_base.h" _wb)
string(REPLACE [==[#ifdef __cplusplus
#include <cstdint>
#include <cstring>
#else
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#endif // __cplusplus
]==] "" _wb "${_wb}")
file(WRITE "${_dxvk_native}/windows/windows_base.h" "${_wb}")
file(READ "${_dxvk_native}/directx/d3d9.h" _d9)
string(REPLACE "#include <stdlib.h>\n" "" _d9 "${_d9}")
file(WRITE "${_dxvk_native}/directx/d3d9.h" "${_d9}")
target_include_directories(ks_common_base INTERFACE
    "${_dxvk_native}/windows" "${_dxvk_native}/directx")

file(READ "${_rmlui_dir}/Source/Core/Elements/WidgetTextInput.cpp" _wt)
string(REPLACE [==[bool WidgetTextInput::AddCharacters(String string)
{]==] [==[bool WidgetTextInput::AddCharacters(String string)
{
	if (parent->HasAttribute("readonly")) return false;]==] _wt "${_wt}")
string(REPLACE [==[bool WidgetTextInput::DeleteCharacters(CursorMovement direction)
{]==] [==[bool WidgetTextInput::DeleteCharacters(CursorMovement direction)
{
	if (parent->HasAttribute("readonly")) return false;]==] _wt "${_wt}")
string(REPLACE [==[void WidgetTextInput::DeleteSelection()
{]==] [==[void WidgetTextInput::DeleteSelection()
{
	if (parent->HasAttribute("readonly")) return;]==] _wt "${_wt}")
file(WRITE "${_patched}/WidgetTextInput.cpp" "${_wt}")

file(GLOB_RECURSE _rmlcore CONFIGURE_DEPENDS "${_rmlui_dir}/Source/Core/*.cpp")
list(FILTER _rmlcore EXCLUDE REGEX "WidgetTextInput[.]cpp$")
list(APPEND _rmlcore "${_patched}/WidgetTextInput.cpp")
add_library(RmlCore SHARED ${_rmlcore})
target_include_directories(RmlCore PRIVATE
    "${_rmlui_dir}/Include" "${_rmlui_dir}/Source/Core/Elements")
target_link_libraries(RmlCore PRIVATE ks_common PkgConfig::FREETYPE)
target_compile_definitions(RmlCore PRIVATE RMLUI_FONT_ENGINE_FREETYPE)
set_target_properties(RmlCore PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

file(GLOB_RECURSE _rmldbg CONFIGURE_DEPENDS "${_rmlui_dir}/Source/Debugger/*.cpp")
add_library(RmlDebugger STATIC ${_rmldbg})
target_include_directories(RmlDebugger PRIVATE "${_rmlui_dir}/Include")
target_link_libraries(RmlDebugger PRIVATE RmlCore)

add_library(rmlui INTERFACE)
target_include_directories(rmlui INTERFACE "${_rmlui_dir}/Include")

file(GLOB_RECURSE _dxvk_util CONFIGURE_DEPENDS
    "${_dxvk_dir}/src/util/*.cpp" "${_dxvk_dir}/src/util/*.c")
file(READ "${_dxvk_dir}/src/util/com/com_guid.cpp" _cg)
string(REPLACE
    [==[#include "../../d3d11/d3d11_interfaces.h"

#include "../../dxgi/dxgi_interfaces.h"]==]
    [==[#include "../log/log.h"]==] _cg "${_cg}")
file(WRITE "${_patched}/com_guid.cpp" "${_cg}")
list(FILTER _dxvk_util EXCLUDE REGEX "com/com_guid[.]cpp$")
list(APPEND _dxvk_util "${_patched}/com_guid.cpp")
add_library(dxvk_util STATIC ${_dxvk_util})
target_compile_options(dxvk_util PRIVATE -std=c++17 -fno-reflection)
target_include_directories(dxvk_util PRIVATE
    "${_dxvk_dir}/src/util" "${_dxvk_dir}/src/util/com"
    "${_dxvk_dir}/include" "${_dxvk_dir}/include/native"
    "${_dxvk_dir}/include/native/windows" "${_dxvk_dir}/include/native/directx"
    "${_dxvk_dir}/include/vulkan/include" "${_dxvk_dir}/include/spirv/include")
target_compile_definitions(dxvk_util PRIVATE DXVK_NATIVE DXVK_WSI_SDL3 DXVK_PLATFORM_LINUX)

set(_dxvk_inc
    "${_dxvk_dir}/include" "${_dxvk_dir}/include/native"
    "${_dxvk_dir}/include/native/windows" "${_dxvk_dir}/include/native/directx"
    "${_dxvk_dir}/include/vulkan/include" "${_dxvk_dir}/include/spirv/include")
set(_dxvk_def DXVK_NATIVE DXVK_WSI_SDL3 DXVK_PLATFORM_LINUX)

function(dxvk_lib name src)
    cmake_parse_arguments(A "SHARED" "" "INC;USE;EXCL;OPT" ${ARGN})
    file(GLOB_RECURSE _src CONFIGURE_DEPENDS "${src}/*.cpp" "${src}/*.c")
    foreach(e IN LISTS A_EXCL)
        list(FILTER _src EXCLUDE REGEX "${e}")
    endforeach()
    if(A_SHARED)
        add_library(${name} SHARED ${_src})
    else()
        add_library(${name} STATIC ${_src})
    endif()
    target_compile_options(${name} PRIVATE -std=c++17 -fno-reflection ${A_OPT})
    target_include_directories(${name} PRIVATE ${_dxvk_inc} ${A_INC})
    target_compile_definitions(${name} PRIVATE ${_dxvk_def})
    if(A_USE)
        target_link_libraries(${name} PRIVATE ${A_USE})
    endif()
endfunction()

file(GLOB_RECURSE _di_src CONFIGURE_DEPENDS "${_dxvk_dir}/subprojects/libdisplay-info/*.c")
list(FILTER _di_src EXCLUDE REGEX "di-edid-decode/")
add_library(display-info STATIC ${_di_src})
target_compile_options(display-info PRIVATE -std=c11)
target_compile_definitions(display-info PRIVATE _POSIX_C_SOURCE=200809L static_array=static)
target_include_directories(display-info PRIVATE "${_dxvk_dir}/subprojects/libdisplay-info/include")

file(GLOB_RECURSE _db_src CONFIGURE_DEPENDS
    "${_dxvk_dir}/subprojects/dxbc-spirv/*.cpp" "${_dxvk_dir}/subprojects/dxbc-spirv/*.c")
add_library(dxbc_spv STATIC ${_db_src})
target_compile_options(dxbc_spv PRIVATE -std=c++17 -fno-reflection -fno-exceptions)
target_compile_definitions(dxbc_spv PRIVATE ${_dxvk_def}
    DXBC_SPV_ENABLE_SM3 DXBC_SPV_ENABLE_SM5 DXBC_SPV_ENABLE_SPIRV)
target_include_directories(dxbc_spv PRIVATE "${_dxvk_dir}/subprojects/dxbc-spirv" ${_dxvk_inc})

dxvk_lib(spirv "${_dxvk_dir}/src/spirv")
dxvk_lib(dxvk_vulkan "${_dxvk_dir}/src/vulkan")
dxvk_lib(dxvk "${_dxvk_dir}/src/dxvk"
    INC "${_dxvk_dir}/src/dxvk" "${_dxvk_dir}/src" "${_dxvk_dir}/subprojects/dxbc-spirv"
    OPT -Wno-nontrivial-memcall USE dxvk_headers dxbc_spv)
dxvk_lib(wsi "${_dxvk_dir}/src/wsi"
    INC "${_dxvk_dir}/src/wsi" "${_dxvk_dir}/subprojects/libdisplay-info/include"
    USE display-info)
dxvk_lib(d3d9 "${_dxvk_dir}/src/d3d9" SHARED
    INC "${_dxvk_dir}/src/d3d9" "${_dxvk_dir}/src" "${_dxvk_dir}/subprojects/dxbc-spirv"
    USE dxvk_headers dxbc_spv display-info dxvk_vulkan dxvk dxvk_util wsi spirv PkgConfig::SDL3)
set_target_properties(d3d9 PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_dependencies(dxvk dxvk_generated)
add_dependencies(d3d9 dxvk_generated)
