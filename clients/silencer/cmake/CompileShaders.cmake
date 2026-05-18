# SDL3 GPU shader compilation for Silencer.
#
# Authors HLSL once and emits byte-array headers committed to the repo.
# Developers without the relevant compiler still build — they just can't
# regenerate after editing a .hlsl.
#
# DXIL  (Windows/D3D12)  — dxc from the Windows 10/11 SDK or brew install dxc
# SPIR-V (Linux/Vulkan)  — dxc from the Vulkan SDK (has -spirv support) or the
#                          standalone DirectXShaderCompiler release. The Windows
#                          SDK's dxc.exe is built without SPIRV codegen; detect
#                          capability by probing $VULKAN_SDK/bin/dxc first.
#
# Both sets of headers live in shaders/generated/ and are committed.

set(SILENCER_SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SILENCER_SHADER_GEN_DIR "${SILENCER_SHADER_SRC_DIR}/generated")

# ---------------------------------------------------------------------------
# DXIL: Windows SDK dxc or brew dxc (no -spirv flag needed)
# ---------------------------------------------------------------------------
file(GLOB _winsdk_dxc_candidates
    "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe"
    "C:/Program Files/Windows Kits/10/bin/*/x64/dxc.exe"
)
list(SORT _winsdk_dxc_candidates ORDER DESCENDING)

find_program(SILENCER_DXC
    NAMES dxc dxc.exe
    HINTS
        "$ENV{WindowsSdkDir}bin/$ENV{WindowsSDKVersion}/x64"
        "$ENV{HOMEBREW_PREFIX}/opt/dxc/bin"
        "/opt/homebrew/opt/dxc/bin"
        "/usr/local/opt/dxc/bin"
    PATHS ${_winsdk_dxc_candidates}
)

if(SILENCER_DXC)
    message(STATUS "SDL3 GPU shaders (DXIL): using dxc at ${SILENCER_DXC}")
else()
    message(STATUS
        "SDL3 GPU shaders (DXIL): dxc not found — using committed headers in "
        "${SILENCER_SHADER_GEN_DIR}. Install dxc (Windows 10/11 SDK or "
        "`brew install dxc`) to regenerate after editing HLSL sources.")
endif()

# ---------------------------------------------------------------------------
# SPIR-V: Vulkan SDK dxc or standalone DirectXShaderCompiler (has -spirv)
# Prefer $VULKAN_SDK/bin/dxc; fall back to PATH dxc if it supports -spirv.
# ---------------------------------------------------------------------------
find_program(SILENCER_DXC_SPIRV
    NAMES dxc dxc.exe
    HINTS
        "$ENV{VULKAN_SDK}/bin"
        "$ENV{VULKAN_SDK}/x86_64/bin"
        "/usr/bin"
        "/usr/local/bin"
    NO_DEFAULT_PATH
)

# Verify it actually supports -spirv (Vulkan SDK dxc does; Windows SDK's doesn't).
if(SILENCER_DXC_SPIRV)
    execute_process(
        COMMAND "${SILENCER_DXC_SPIRV}" -spirv -? 2>&1
        OUTPUT_VARIABLE _dxc_spirv_help ERROR_VARIABLE _dxc_spirv_help
        RESULT_VARIABLE _dxc_spirv_rc
    )
    if(_dxc_spirv_rc EQUAL 0 OR _dxc_spirv_help MATCHES "spirv|SPIR")
        message(STATUS "SDL3 GPU shaders (SPIR-V): using dxc at ${SILENCER_DXC_SPIRV}")
    else()
        message(STATUS "SDL3 GPU shaders (SPIR-V): dxc at ${SILENCER_DXC_SPIRV} lacks -spirv — using committed headers")
        unset(SILENCER_DXC_SPIRV CACHE)
    endif()
else()
    message(STATUS
        "SDL3 GPU shaders (SPIR-V): dxc not found — using committed headers in "
        "${SILENCER_SHADER_GEN_DIR}. Install Vulkan SDK or DirectXShaderCompiler "
        "to regenerate after editing HLSL sources.")
endif()

# silencer_compile_shader(<name> <stage> <entry>)  — DXIL
#
#   Emits a DXIL header at ${SILENCER_SHADER_GEN_DIR}/<name>.dxil.h
#   whose byte-array symbol is `k<CamelName>DXIL`.
function(silencer_compile_shader name stage entry)
    set(_src "${SILENCER_SHADER_SRC_DIR}/${name}.hlsl")
    set(_dxil_h "${SILENCER_SHADER_GEN_DIR}/${name}.dxil.h")

    if(NOT SILENCER_DXC)
        return()
    endif()

    if(stage STREQUAL "vertex")
        set(_profile "vs_6_0")
    elseif(stage STREQUAL "pixel")
        set(_profile "ps_6_0")
    elseif(stage STREQUAL "compute")
        set(_profile "cs_6_0")
    else()
        message(FATAL_ERROR "silencer_compile_shader: unknown stage '${stage}'")
    endif()

    string(REPLACE "_" ";" _parts "${name}")
    set(_sym "k")
    foreach(_p ${_parts})
        string(SUBSTRING "${_p}" 0 1 _first)
        string(SUBSTRING "${_p}" 1 -1 _rest)
        string(TOUPPER "${_first}" _first)
        string(APPEND _sym "${_first}${_rest}")
    endforeach()
    string(APPEND _sym "DXIL")

    add_custom_command(
        OUTPUT  "${_dxil_h}"
        COMMAND "${SILENCER_DXC}"
                -nologo
                -T "${_profile}"
                -E "${entry}"
                -Fh "${_dxil_h}"
                -Vn "${_sym}"
                "${_src}"
        DEPENDS "${_src}"
        COMMENT "DXC ${name} → ${_sym} (${_profile})"
        VERBATIM
    )
endfunction()

# silencer_compile_shader_spirv(<name> <stage> <entry>)  — SPIR-V
#
#   Emits a SPIR-V header at ${SILENCER_SHADER_GEN_DIR}/<name>.spv.h
#   whose byte-array symbol is `k<CamelName>SPV`.
#   Requires SILENCER_DXC_SPIRV (Vulkan SDK dxc with -spirv support).
function(silencer_compile_shader_spirv name stage entry)
    set(_src "${SILENCER_SHADER_SRC_DIR}/${name}.hlsl")
    set(_spv_h "${SILENCER_SHADER_GEN_DIR}/${name}.spv.h")

    if(NOT SILENCER_DXC_SPIRV)
        return()
    endif()

    if(stage STREQUAL "vertex")
        set(_profile "vs_6_0")
    elseif(stage STREQUAL "pixel")
        set(_profile "ps_6_0")
    elseif(stage STREQUAL "compute")
        set(_profile "cs_6_0")
    else()
        message(FATAL_ERROR "silencer_compile_shader_spirv: unknown stage '${stage}'")
    endif()

    string(REPLACE "_" ";" _parts "${name}")
    set(_sym "k")
    foreach(_p ${_parts})
        string(SUBSTRING "${_p}" 0 1 _first)
        string(SUBSTRING "${_p}" 1 -1 _rest)
        string(TOUPPER "${_first}" _first)
        string(APPEND _sym "${_first}${_rest}")
    endforeach()
    string(APPEND _sym "SPV")

    add_custom_command(
        OUTPUT  "${_spv_h}"
        COMMAND "${SILENCER_DXC_SPIRV}"
                -nologo
                -spirv
                -T "${_profile}"
                -E "${entry}"
                -Fh "${_spv_h}"
                -Vn "${_sym}"
                "${_src}"
        DEPENDS "${_src}"
        COMMENT "DXC -spirv ${name} → ${_sym} (${_profile})"
        VERBATIM
    )
endfunction()

# silencer_finalize_shaders(<name>...)
#
# Collects all committed/regenerated DXIL and SPIR-V headers into one
# custom target so sdl3gpubackend.cpp always compiles after codegen.
function(silencer_finalize_shaders)
    set(_outputs "")
    foreach(_n ${ARGN})
        list(APPEND _outputs "${SILENCER_SHADER_GEN_DIR}/${_n}.dxil.h")
        list(APPEND _outputs "${SILENCER_SHADER_GEN_DIR}/${_n}.spv.h")
    endforeach()
    if(SILENCER_DXC OR SILENCER_DXC_SPIRV)
        # Filter to only outputs that have a custom_command driving them.
        set(_cmd_outputs "")
        foreach(_o ${_outputs})
            if(SILENCER_DXC AND "${_o}" MATCHES "\\.dxil\\.h$")
                list(APPEND _cmd_outputs "${_o}")
            elseif(SILENCER_DXC_SPIRV AND "${_o}" MATCHES "\\.spv\\.h$")
                list(APPEND _cmd_outputs "${_o}")
            endif()
        endforeach()
        if(_cmd_outputs)
            add_custom_target(silencer_shaders DEPENDS ${_cmd_outputs})
        else()
            add_custom_target(silencer_shaders)
        endif()
    else()
        add_custom_target(silencer_shaders)
    endif()
endfunction()
