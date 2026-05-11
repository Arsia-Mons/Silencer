# SDL3 GPU shader compilation for Silencer.
#
# Authors HLSL once and emits DXIL byte-array headers via the DirectX Shader
# Compiler (`dxc`). Generated headers live in `shaders/generated/` and are
# committed to the repo, so a developer without `dxc` can still build — they
# just can't change a shader source.
#
# When `dxc` is found on PATH (or in the Windows SDK), modifying a `.hlsl`
# triggers regeneration via add_custom_command. When it isn't found we emit
# a STATUS message and skip the regen — the committed `.h` files are used.
#
# SPIRV/Vulkan codegen is intentionally NOT wired here. The Windows SDK's
# dxc.exe is built without -DENABLE_SPIRV_CODEGEN, so adding SPIRV needs
# either the Vulkan SDK's dxc or a standalone DirectXShaderCompiler release.
# The HLSL sources already carry [[vk::binding]] annotations so a future
# SPIRV path is purely build-glue work.

set(SILENCER_SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SILENCER_SHADER_GEN_DIR "${SILENCER_SHADER_SRC_DIR}/generated")

# Probe Windows SDK install dirs in addition to PATH. Newer SDKs first.
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
    message(STATUS "SDL3 GPU shaders: using dxc at ${SILENCER_DXC}")
else()
    message(STATUS
        "SDL3 GPU shaders: dxc not found — using committed bytecode in "
        "${SILENCER_SHADER_GEN_DIR}. Install dxc (Windows 10/11 SDK or "
        "`brew install dxc`) to regenerate after editing HLSL sources.")
endif()

# silencer_compile_shader(<name> <stage> <entry>)
#
#   <name>    base filename without extension, e.g. `frag_remap`
#   <stage>   one of `vertex`, `pixel`, `compute`
#   <entry>   HLSL entry-point name, e.g. `frag_remap`
#
# Emits a DXIL header at ${SILENCER_SHADER_GEN_DIR}/<name>.dxil.h whose
# byte-array symbol is `k<CamelName>DXIL`. When dxc is missing the function
# is a no-op — the committed header is consumed as-is.
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

    # CamelCase the symbol: frag_remap → kFragRemapDXIL.
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

# Emits a custom target that depends on every committed/regenerated header,
# so adding it to the silencer target's PRIVATE deps forces shader codegen
# to run before sdl3gpubackend.cpp compiles.
function(silencer_finalize_shaders)
    set(_outputs "")
    foreach(_n ${ARGN})
        list(APPEND _outputs "${SILENCER_SHADER_GEN_DIR}/${_n}.dxil.h")
    endforeach()
    if(SILENCER_DXC)
        add_custom_target(silencer_shaders DEPENDS ${_outputs})
    else()
        add_custom_target(silencer_shaders) # no-op
    endif()
endfunction()
