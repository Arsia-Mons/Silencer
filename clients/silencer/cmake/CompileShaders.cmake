# SDL3 GPU shader compilation for Silencer.
#
# Authors HLSL once and emits byte-array headers for the SDL3 GPU backend:
#
#   <name>.dxil.h   — D3D12 (Windows)
#   <name>.spv.h    — Vulkan / WebGPU-via-emscripten (Linux, browser)
#
# Both formats come from the DirectX Shader Compiler (`dxc`). Generated
# headers live in `shaders/generated/` and are committed to the repo, so a
# developer without `dxc` can still build — they just can't change a shader
# source. Metal/macOS keeps the hand-written MSL strings in
# `src/render/sdl3gpubackend.cpp`.
#
# Two dxc capabilities are probed independently:
#
#   - DXIL: any `dxc` (Windows SDK's dxc, Homebrew's, Vulkan SDK's) works.
#   - SPIRV: only `dxc` built with -DENABLE_SPIRV_CODEGEN works. The
#     Windows SDK build is NOT, so SPIRV regeneration requires the Vulkan
#     SDK's `dxc` (LunarG installer) or a standalone DirectXShaderCompiler
#     release built with SPIRV support. Detected by probing `dxc -spirv`.
#
# When a dxc lacks SPIRV support, only the DXIL header gets regenerated and
# the committed `.spv.h` is used as-is.

set(SILENCER_SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SILENCER_SHADER_GEN_DIR "${SILENCER_SHADER_SRC_DIR}/generated")

# Probe Windows SDK install dirs in addition to PATH. Newer SDKs first.
file(GLOB _winsdk_dxc_candidates
    "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe"
    "C:/Program Files/Windows Kits/10/bin/*/x64/dxc.exe"
)
list(SORT _winsdk_dxc_candidates ORDER DESCENDING)

# Probe Vulkan SDK install dirs (preferred — gives SPIRV codegen). Look at
# $VULKAN_SDK first (set by setup-env.sh / vcvars), then default install
# paths on each platform.
set(_vulkan_dxc_hints "")
if(DEFINED ENV{VULKAN_SDK})
    list(APPEND _vulkan_dxc_hints "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/macOS/bin")
endif()
file(GLOB _vulkan_sdk_dxc_candidates
    "/Applications/VulkanSDK*/macOS/bin/dxc"
    "$ENV{HOME}/VulkanSDK/*/macOS/bin/dxc"
    "/usr/local/VulkanSDK/*/x86_64/bin/dxc"
    "C:/VulkanSDK/*/Bin/dxc.exe"
)
list(SORT _vulkan_sdk_dxc_candidates ORDER DESCENDING)

find_program(SILENCER_DXC
    NAMES dxc dxc.exe
    HINTS
        ${_vulkan_dxc_hints}
        "$ENV{WindowsSdkDir}bin/$ENV{WindowsSDKVersion}/x64"
        "$ENV{HOMEBREW_PREFIX}/opt/dxc/bin"
        "/opt/homebrew/opt/dxc/bin"
        "/usr/local/opt/dxc/bin"
    PATHS
        ${_vulkan_sdk_dxc_candidates}
        ${_winsdk_dxc_candidates}
)

# Detect SPIRV codegen support. dxc without -DENABLE_SPIRV_CODEGEN prints
# "error: SPIR-V CodeGen not available" when -spirv is passed. The Windows
# SDK build of dxc lists `-fspv-` flags in `-help` (documented) but rejects
# them at compile time, so we can't rely on the help text — we have to
# actually invoke a SPIRV codegen and see if it errors.
set(SILENCER_DXC_SPIRV FALSE)
if(SILENCER_DXC)
    # Trivial null-body PS we can throw at dxc to check the SPIRV path.
    set(_probe_src "${CMAKE_BINARY_DIR}/CMakeFiles/silencer_dxc_spirv_probe.hlsl")
    file(WRITE "${_probe_src}" "float4 main() : SV_Target { return 0; }\n")
    execute_process(
        COMMAND "${SILENCER_DXC}" -nologo -spirv -T ps_6_0 -E main "${_probe_src}"
        OUTPUT_QUIET
        ERROR_VARIABLE _dxc_probe_err
        RESULT_VARIABLE _dxc_probe_rc
    )
    if(_dxc_probe_rc EQUAL 0)
        set(SILENCER_DXC_SPIRV TRUE)
    endif()
endif()

if(SILENCER_DXC)
    if(SILENCER_DXC_SPIRV)
        message(STATUS "SDL3 GPU shaders: using dxc at ${SILENCER_DXC} (DXIL + SPIRV)")
    else()
        message(STATUS "SDL3 GPU shaders: using dxc at ${SILENCER_DXC} (DXIL only — install Vulkan SDK for SPIRV regen)")
    endif()
else()
    message(STATUS
        "SDL3 GPU shaders: dxc not found — using committed bytecode in "
        "${SILENCER_SHADER_GEN_DIR}. Install dxc (Vulkan SDK for SPIRV, "
        "Windows 10/11 SDK or `brew install dxc` for DXIL only) to "
        "regenerate after editing HLSL sources.")
endif()

# silencer_compile_shader(<name> <stage> <entry>)
#
#   <name>    base filename without extension, e.g. `frag_remap`
#   <stage>   one of `vertex`, `pixel`, `compute`
#   <entry>   HLSL entry-point name, e.g. `frag_remap`
#
# Emits up to two headers in ${SILENCER_SHADER_GEN_DIR}/:
#   <name>.dxil.h — byte-array symbol `k<CamelName>DXIL`
#   <name>.spv.h  — byte-array symbol `k<CamelName>SPIRV`  (when dxc supports SPIRV)
#
# When dxc is missing the function is a no-op — the committed headers are
# consumed as-is.
function(silencer_compile_shader name stage entry)
    set(_src "${SILENCER_SHADER_SRC_DIR}/${name}.hlsl")
    set(_dxil_h "${SILENCER_SHADER_GEN_DIR}/${name}.dxil.h")
    set(_spv_h  "${SILENCER_SHADER_GEN_DIR}/${name}.spv.h")

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

    # CamelCase the base symbol: frag_remap → kFragRemap.
    string(REPLACE "_" ";" _parts "${name}")
    set(_base_sym "k")
    foreach(_p ${_parts})
        string(SUBSTRING "${_p}" 0 1 _first)
        string(SUBSTRING "${_p}" 1 -1 _rest)
        string(TOUPPER "${_first}" _first)
        string(APPEND _base_sym "${_first}${_rest}")
    endforeach()

    add_custom_command(
        OUTPUT  "${_dxil_h}"
        COMMAND "${SILENCER_DXC}"
                -nologo
                -T "${_profile}"
                -E "${entry}"
                -Fh "${_dxil_h}"
                -Vn "${_base_sym}DXIL"
                "${_src}"
        DEPENDS "${_src}"
        COMMENT "DXC ${name} → ${_base_sym}DXIL (${_profile})"
        VERBATIM
    )

    if(SILENCER_DXC_SPIRV)
        # SPIRV codegen flags match SDL_shadercross's HLSL→SPIRV pipeline:
        #   -spirv                       — emit SPIRV
        #   -fspv-preserve-bindings      — keep all declared resources, even
        #                                  if unused, so descriptor-set
        #                                  layouts stay stable
        #   -fspv-preserve-interface     — keep declared shader-stage inputs
        #                                  and outputs even if unused
        add_custom_command(
            OUTPUT  "${_spv_h}"
            COMMAND "${SILENCER_DXC}"
                    -nologo
                    -spirv
                    -fspv-preserve-bindings
                    -fspv-preserve-interface
                    -T "${_profile}"
                    -E "${entry}"
                    -Fh "${_spv_h}"
                    -Vn "${_base_sym}SPIRV"
                    "${_src}"
            DEPENDS "${_src}"
            COMMENT "DXC ${name} → ${_base_sym}SPIRV (${_profile})"
            VERBATIM
        )
    endif()
endfunction()

# Emits a custom target that depends on every committed/regenerated header,
# so adding it to the silencer target's PRIVATE deps forces shader codegen
# to run before sdl3gpubackend.cpp compiles.
function(silencer_finalize_shaders)
    set(_outputs "")
    foreach(_n ${ARGN})
        list(APPEND _outputs "${SILENCER_SHADER_GEN_DIR}/${_n}.dxil.h")
        if(SILENCER_DXC_SPIRV)
            list(APPEND _outputs "${SILENCER_SHADER_GEN_DIR}/${_n}.spv.h")
        endif()
    endforeach()
    if(SILENCER_DXC)
        add_custom_target(silencer_shaders DEPENDS ${_outputs})
    else()
        add_custom_target(silencer_shaders) # no-op
    endif()
endfunction()
