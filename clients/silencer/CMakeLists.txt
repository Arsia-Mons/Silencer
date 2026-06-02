cmake_minimum_required(VERSION 3.16)

project(Silencer)

# nlohmann/json v3.12.0 — vendored header (third_party/nlohmann/json.hpp)
# Vendored to avoid CMake FetchContent redirect issues with GitHub CDN in CI.
set(NLOHMANN_JSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# Linux GNU lowercase convention; macOS/Windows pick up mixed-case via
# bundle/output-name overrides below.
set(SILENCER_TARGET silencer)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Shared monorepo dirs sit two levels up from clients/silencer/.
set(SILENCER_SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../shared")

if(APPLE)
	file(GLOB_RECURSE sources src/*.cpp src/*.h src/*.mm)
else()
	file(GLOB_RECURSE sources src/*.cpp src/*.h)
endif()

# Extra files that get bundled into the .app on macOS (icon + shared/assets/).
set(bundle_extras "")
if(APPLE)
	set(bundle_icon "${SILENCER_SHARED_DIR}/icons/icon.icns")
	set_source_files_properties("${bundle_icon}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	list(APPEND bundle_extras "${bundle_icon}")

	# Files land at Contents/assets/* in the .app — the C++ runtime's
	# fallback path (`main.cpp` `CDResDir`) looks for `../assets/PALETTE.BIN`
	# next to the executable in `Contents/MacOS/`. Engineer-facing docs
	# (CLAUDE.md, AGENTS.md) live in shared/assets/ but don't ship.
	file(GLOB_RECURSE asset_files "${SILENCER_SHARED_DIR}/assets/*")
	list(FILTER asset_files EXCLUDE REGEX "/(CLAUDE|AGENTS)[.]md$")
	foreach(f ${asset_files})
		file(RELATIVE_PATH rel "${SILENCER_SHARED_DIR}/assets" "${f}")
		get_filename_component(rel_dir "${rel}" DIRECTORY)
		if(rel_dir)
			set_source_files_properties("${f}" PROPERTIES MACOSX_PACKAGE_LOCATION "assets/${rel_dir}")
		else()
			set_source_files_properties("${f}" PROPERTIES MACOSX_PACKAGE_LOCATION "assets")
		endif()
		list(APPEND bundle_extras "${f}")
	endforeach()
endif()

if(APPLE)
	add_executable(${SILENCER_TARGET} MACOSX_BUNDLE ${sources} ${bundle_extras})
	set_target_properties(${SILENCER_TARGET} PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Silencer-Info.plist"
		MACOSX_BUNDLE_BUNDLE_NAME "Silencer"
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.silencer.app"
		MACOSX_BUNDLE_ICON_FILE "icon.icns"
		OUTPUT_NAME "Silencer"
	)
	add_executable(updater-stage-2
		tools/updater-stage-2/main.cpp
		src/updater/updaterstage2.cpp
		src/updater/updaterzip.cpp
	)
	target_compile_features(updater-stage-2 PRIVATE cxx_std_20)
	target_include_directories(updater-stage-2 PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src/updater")
	set_target_properties(updater-stage-2 PROPERTIES OUTPUT_NAME "updater-stage-2")
	set(updater_stage2_bundle_dir
		"${CMAKE_CURRENT_BINARY_DIR}/Silencer.app/Contents/Helpers")
	add_custom_command(TARGET updater-stage-2 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory
			"${updater_stage2_bundle_dir}"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<TARGET_FILE:updater-stage-2>"
			"${updater_stage2_bundle_dir}/updater-stage-2"
		COMMENT "Embedding macOS updater stage-2 helper")
	add_dependencies(${SILENCER_TARGET} updater-stage-2)
elseif(WIN32)
	add_executable(${SILENCER_TARGET} WIN32 ${sources})
	set_target_properties(${SILENCER_TARGET} PROPERTIES OUTPUT_NAME "Silencer")
	# Embed the application icon (resources.rc references shared/icons/icon.ico).
	target_sources(${SILENCER_TARGET} PRIVATE resources.rc)
else()
	add_executable(${SILENCER_TARGET} ${sources})
endif()

target_compile_features(${SILENCER_TARGET} PUBLIC cxx_std_20)

if(WIN32)
	# Strip <winsock.h> (and other legacy subsystems) out of <windows.h>.
	# Without this, any TU that pulls in <windows.h> via zlib's zconf.h,
	# behaviortree.cpp, actordef.cpp, etc. drags in winsock 1.x; another
	# TU including <winsock2.h> then redefines sockaddr/fd_set/etc. The
	# collision is invisible to non-unity builds (each TU is isolated)
	# but surfaces in unity batches. We use winsock2 only — no Shell/
	# RPC/Crypto/DDE call sites in the codebase to break.
	target_compile_definitions(${SILENCER_TARGET} PRIVATE WIN32_LEAN_AND_MEAN)
endif()

# Unity (jumbo) build: combines src/*.cpp into batched TUs to dramatically
# speed up clean builds. Off by default — incremental single-file edits are
# slower under unity since changing one .cpp recompiles its whole batch.
# CI release/deploy workflows pass -DSILENCER_UNITY_BUILD=ON.
option(SILENCER_UNITY_BUILD "Combine .cpp files into jumbo TUs for faster clean builds" OFF)
if(SILENCER_UNITY_BUILD)
	set_target_properties(${SILENCER_TARGET} PROPERTIES UNITY_BUILD ON)
	if(APPLE)
		# CMake batches Objective-C++ .mm files into the CXX jumbo TU on
		# macOS rather than separating by language, so #import <Cocoa/Cocoa.h>
		# from cocoawrapper.mm leaks Objective-C types (NSString, etc.) into
		# a C++ translation unit and breaks compilation. We have only one .mm
		# file — unity savings from including it are nil — so skip it.
		file(GLOB_RECURSE _mm_sources src/*.mm)
		set_source_files_properties(${_mm_sources} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
	endif()
	# The vendored retained cppx engine (ui/runtime, ui/style, render/cppx_ui)
	# and the app-shell (client/ui: app_theme, app_shell, providers) use
	# anonymous namespaces + file-local `using` decls that are not jumbo-safe —
	# concatenating two into one TU collides on shared symbols. Build them as
	# individual TUs (exclude from unity batches).
	file(GLOB _unity_excluded_sources
		src/ui/runtime/*.cpp
		src/ui/style/*.cpp
		src/render/cppx_ui/*.cpp
		src/client/ui/*.cpp
		src/client/ui/app_shell/*.cpp
		src/client/ui/app_shell/navigation/*.cpp
		src/client/ui/providers/*.cpp)
	set_source_files_properties(${_unity_excluded_sources} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
endif()

# Compile HLSL → DXIL bytecode headers for the SDL3 GPU backend. macOS keeps
# the existing MSL source strings; SPIRV/Linux is a separate change.
include(CompileShaders)
silencer_compile_shader(vert_screen   vertex  vert_screen)
silencer_compile_shader(frag_remap    pixel   frag_remap)
silencer_compile_shader(frag_upscale  pixel   frag_upscale)
silencer_compile_shader(frag_lobby_panel_blur pixel frag_lobby_panel_blur)
silencer_compile_shader(frag_light    pixel   frag_light)
silencer_compile_shader(vert_particle vertex  vert_particle)
silencer_compile_shader(frag_particle pixel   frag_particle)
silencer_compile_shader(comp_particle compute update_particles)
silencer_finalize_shaders(
    vert_screen frag_remap frag_upscale frag_lobby_panel_blur frag_light
    vert_particle frag_particle comp_particle
)
add_dependencies(${SILENCER_TARGET} silencer_shaders)
target_include_directories(${SILENCER_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated")
target_include_directories(${SILENCER_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
# Expose src/ and every first-level subdomain so that bare-name includes
# (#include "lobby.h") continue to resolve after the src/ reorganisation.
target_include_directories(${SILENCER_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/render"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/network"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/objects"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/gameplay"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/messaging"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/map"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/physics"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/actors"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/actors/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/actors/bt"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/actors/npc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/actors/player"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/projectiles"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/objects"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/stations"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/client/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/net"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/modes"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/session"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/actor"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/state"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/replay"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/updater"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gas"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/events"
)

# --- cppx authoring pipeline (SIL-7) -------------------------------------
# `.cppx`/`.hx` view sources transpile to ordinary C++ at build time through
# the vendored Python tooling. Generated `.cpp`/`.h` land under
# <build>/generated/cppx/ (gitignored, never committed) and regenerate every
# build; Python3 is a documented build dependency. The authored-source list
# below is the single source of truth — both the transpile step and the
# format gate consume it.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
include(cppx_transpile)

set(SILENCER_CPPX_SOURCES
	src/ui/cppx_smoke.cppx
	src/ui/cppx_smoke.hx
	# Generic UI primitive layer (SIL-16): game-agnostic, theme-based,
	# sprite-free components. common.h/components.h are plain headers
	# resolved via the src/ include dir, not authored cppx sources.
	src/ui/components/box.cppx
	src/ui/components/box.hx
	src/ui/components/button.cppx
	src/ui/components/button.hx
	src/ui/components/text.cppx
	src/ui/components/text.hx
	src/ui/components/checkbox.cppx
	src/ui/components/checkbox.hx
	src/ui/components/input.cppx
	src/ui/components/input.hx
	src/ui/components/dialog.cppx
	src/ui/components/dialog.hx
	# Product-intent semantic component layer (SIL-17): the shadcn-style app
	# components over the generic primitives, in namespace silencer::. Paint
	# resolves through silencer::tokens + the product theme. The plain headers
	# (tokens.h, app_button_variant.h, and the per-family umbrella headers
	# actions.h/layout.h/surfaces.h/text.h) are resolved via the src/ include
	# dir, not authored cppx sources.
	src/client/ui/components/actions/app_button.cppx
	src/client/ui/components/actions/app_button.hx
	src/client/ui/components/actions/app_checkbox.cppx
	src/client/ui/components/actions/app_checkbox.hx
	src/client/ui/components/actions/boolean_setting_row.cppx
	src/client/ui/components/actions/boolean_setting_row.hx
	src/client/ui/components/actions/app_input.cppx
	src/client/ui/components/actions/app_input.hx
	src/client/ui/components/actions/action_row.cppx
	src/client/ui/components/actions/action_row.hx
	src/client/ui/components/layout/screen_layout.cppx
	src/client/ui/components/layout/screen_layout.hx
	src/client/ui/components/surfaces/panel.cppx
	src/client/ui/components/surfaces/panel.hx
	src/client/ui/components/text/screen_title.cppx
	src/client/ui/components/text/screen_title.hx
	src/client/ui/components/text/screen_subtitle.cppx
	src/client/ui/components/text/screen_subtitle.hx
	src/client/ui/components/text/body_text.cppx
	src/client/ui/components/text/body_text.hx
	# Screens (SIL-18): the lowest-coupling screen views authored as cppx. Phase
	# screens (MainMenu/UpdateScreen) are keyed component factories returned by
	# AppRoot::make_phase_element; the modals are props-only OverlayScreens
	# pushed via use_navigation.
	src/client/ui/screens/main_menu.cppx
	src/client/ui/screens/main_menu.hx
	src/client/ui/screens/gallery.cppx
	src/client/ui/screens/gallery.hx
	src/client/ui/screens/update_screen.cppx
	src/client/ui/screens/update_screen.hx
	src/client/ui/screens/message_modal.cppx
	src/client/ui/screens/message_modal.hx
	src/client/ui/screens/password_modal.cppx
	src/client/ui/screens/password_modal.hx
	# Options cluster (SIL-19): the root + Audio/Display sub-screens are Overlays
	# pushed via use_navigation; they live-preview use_settings (Cancel reverts).
	src/client/ui/screens/options_screen.cppx
	src/client/ui/screens/options_screen.hx
	src/client/ui/screens/options_audio.cppx
	src/client/ui/screens/options_audio.hx
	src/client/ui/screens/options_display.cppx
	src/client/ui/screens/options_display.hx
	# OptionsControls (SIL-19 part B): rows-of-combos + multi-device keybind
	# capture (use_keybind_capture).
	src/client/ui/screens/options_controls.cppx
	src/client/ui/screens/options_controls.hx
	# Lobby phase screens (SIL-20): LobbyConnect (connect/auth via the game-tick
	# connect flow + use_lobby_session) and MissionSummary (use_progression).
	src/client/ui/screens/lobby_connect.cppx
	src/client/ui/screens/lobby_connect.hx
	src/client/ui/screens/mission_summary.cppx
	src/client/ui/screens/mission_summary.hx
	# CharacterCreate (SIL-21): the fresh-account gateway — alias + agency →
	# create → LOBBY (via the CREATECHARACTER tick reconcile).
	src/client/ui/screens/character_create.cppx
	src/client/ui/screens/character_create.hx
	# LobbyScreen (SIL-21 2/n + 3/n): the read cluster + the game-join cluster
	# (GameSelect/GameCreate/Staging right-column swap over use_games/use_staging).
	src/client/ui/screens/lobby_screen.cppx
	src/client/ui/screens/lobby_screen.hx
	# InGameScreen (SIL-21 4/n): the in-match HUD — transparent ScreenLayout(Hud)
	# over the live world, composed from use_match + use_player_status.
	src/client/ui/screens/in_game_screen.cppx
	src/client/ui/screens/in_game_screen.hx
)
cppx_transpile(SILENCER_CPPX_GENERATED ${SILENCER_CPPX_SOURCES})
target_sources(${SILENCER_TARGET} PRIVATE ${SILENCER_CPPX_GENERATED})

# Expose the generated root + the generated `src/` mirror + each generated
# output's directory so authored `.cppx`/`.hx` resolve their generated `.h`
# siblings both by bare name (generic primitives: `#include "button.h"`) and
# src-rooted (semantic layer: `#include "client/ui/components/.../foo.h"`). The
# generated tree mirrors `src/`, so its `src/` root must be on the include path
# exactly like the real `src/` root is for the authored sources. Generated TUs
# use named namespaces (no anon-namespace / global `using` vocabulary), so they
# are unity-safe — unlike the legacy Clay `_clay_unsafe_sources`. Re-audit when
# adding generated TUs.
set(SILENCER_CPPX_INCLUDE_DIRS
	"${CMAKE_CURRENT_BINARY_DIR}/generated/cppx"
	"${CMAKE_CURRENT_BINARY_DIR}/generated/cppx/src")
foreach(_gen ${SILENCER_CPPX_GENERATED})
	get_filename_component(_gen_dir "${_gen}" DIRECTORY)
	list(APPEND SILENCER_CPPX_INCLUDE_DIRS "${_gen_dir}")
endforeach()
list(REMOVE_DUPLICATES SILENCER_CPPX_INCLUDE_DIRS)
target_include_directories(${SILENCER_TARGET} PRIVATE ${SILENCER_CPPX_INCLUDE_DIRS})

# Format gate on the authored sources (CI: `ctest -R cppx_format_check`).
# Always registered so it does not depend on the full SILENCER_BUILD_TESTS
# tree. There is no drift gate: nothing generated lives in source.
enable_testing()
set(_cppx_format_files "")
foreach(_src ${SILENCER_CPPX_SOURCES})
	list(APPEND _cppx_format_files "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
endforeach()
add_test(NAME cppx_format_check
	COMMAND ${Python3_EXECUTABLE}
		"${CMAKE_CURRENT_SOURCE_DIR}/tools/cppx_format.py" --check ${_cppx_format_files})

# --- cppx retained runtime (SIL-9) ---------------------------------------
# The golden react+element+tree trio (+ SDL-free contract deps input.h,
# style/visual_style.h, style/text_measure.*) is vendored under src/ui/ and
# compiles into the client via the source glob. It is dead until the pipeline
# wires it (SIL-13+), but proves the vendor integrates build-clean here.
#
# react_runtime_tests: the golden hook-runtime unit test (a trivial component
# renders, hooks keep per-instance state across reorders). Built always so a
# normal build.sh produces it; run via `ctest -R react_runtime_tests`.
add_executable(react_runtime_tests
	tests/react_runtime_tests.cpp
	src/ui/runtime/react.cpp)
target_compile_features(react_runtime_tests PRIVATE cxx_std_20)
target_include_directories(react_runtime_tests PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/src")
if(MSVC)
	target_compile_definitions(react_runtime_tests PRIVATE NOMINMAX)
endif()
add_test(NAME react_runtime_tests COMMAND react_runtime_tests)

# SDL-free guard on the vendored runtime (scoped allowlist while the legacy
# Clay runtime still lives under src/ui/; SIL-23 broadens it post-Clay).
add_test(NAME cppx_runtime_sdl_free
	COMMAND ${Python3_EXECUTABLE}
		"${CMAKE_CURRENT_SOURCE_DIR}/tools/cppx_runtime_guard.py")

# SIL-23: cppx transpiler golden-output regression. Transpiles the authored
# fixtures and diffs against the committed *.expected.* (+ asserts the two error
# fixtures emit their precise diagnostics). The transpiler is byte-identical to
# the golden repo's, so the fixtures are reused verbatim.
add_test(NAME cppx_transpiler_tests
	COMMAND ${Python3_EXECUTABLE}
		"${CMAKE_CURRENT_SOURCE_DIR}/tests/cppx_transpiler_tests.py"
		--tool "${CMAKE_CURRENT_SOURCE_DIR}/tools/cppx_transpile.py"
		--fixtures "tests/fixtures/cppx")
# Run from the client root so the source paths the transpiler bakes into #line
# directives are repo-relative (matching the committed *.expected.*).
set_tests_properties(cppx_transpiler_tests PROPERTIES
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# SIL-23: component-descriptor architecture guard (ported from golden). Public
# API is descriptor factories — no Component<T> wrapper, no direct generic
# component bodies, no manual retained-provider macros; the transpiler lowers
# .cppx into ::ui::component(...) descriptors.
add_test(NAME component_descriptor_guard
	COMMAND ${Python3_EXECUTABLE}
		"${CMAKE_CURRENT_SOURCE_DIR}/tools/component_descriptor_guard.py"
		--root "${CMAKE_CURRENT_SOURCE_DIR}"
		--tool "${CMAKE_CURRENT_SOURCE_DIR}/tools/cppx_transpile.py")

# --- Yoga flex layout (SIL-10) -------------------------------------------
# Yoga v3.2.1, the layout backend behind the retained runtime. flex_layout.*
# is a Yoga-free adapter (FlexLayoutAdapter seam over UiTree); only
# yoga_flex_layout.cpp touches <yoga/Yoga.h>. Both compile into the client via
# the source glob (unity-excluded under src/ui/runtime) — dead until the
# pipeline drives compute_flex_layout (SIL-13), but matches the end state, so
# the client links yogacore now.
include(FetchContent)
FetchContent_Declare(
	yoga
	GIT_REPOSITORY https://github.com/facebook/yoga.git
	GIT_TAG v3.2.1
	GIT_SHALLOW TRUE
	SOURCE_SUBDIR yoga
)
FetchContent_MakeAvailable(yoga)
target_link_libraries(${SILENCER_TARGET} yogacore)

# Retained tree-reconciliation contract (injected layout backend, no Yoga).
add_executable(retained_ui_runtime_tests
	tests/retained_ui_runtime_tests.cpp
	src/ui/runtime/tree.cpp
	src/ui/style/text_measure.cpp
	src/ui/runtime/flex_layout.cpp)
target_compile_features(retained_ui_runtime_tests PRIVATE cxx_std_20)
target_include_directories(retained_ui_runtime_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(MSVC)
	target_compile_definitions(retained_ui_runtime_tests PRIVATE NOMINMAX)
endif()
add_test(NAME retained_ui_runtime_tests COMMAND retained_ui_runtime_tests)

# Real Yoga layout: resolves absolute bounds for a nested flex tree.
add_executable(retained_ui_yoga_tests
	tests/retained_ui_yoga_tests.cpp
	src/ui/runtime/tree.cpp
	src/ui/style/text_measure.cpp
	src/ui/runtime/flex_layout.cpp
	src/ui/runtime/yoga_flex_layout.cpp)
target_compile_features(retained_ui_yoga_tests PRIVATE cxx_std_20)
target_include_directories(retained_ui_yoga_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(retained_ui_yoga_tests PRIVATE yogacore)
if(MSVC)
	target_compile_definitions(retained_ui_yoga_tests PRIVATE NOMINMAX)
endif()
add_test(NAME retained_ui_yoga_tests COMMAND retained_ui_yoga_tests)

# --- Focus runtime + interaction hooks (SIL-12) --------------------------
# The vendored golden focus runtime, driven over a real Yoga-laid-out UiTree:
# keyboard/gamepad nav (skip-disabled + source tracking), pointer press/release
# confirm, and modal scope trap+restore. focus.cpp is a pure function over
# UiTree + a POD InputFrame (SDL-free); compiles into the client via the glob.
add_executable(retained_ui_focus_tests
	tests/retained_ui_focus_tests.cpp
	src/ui/runtime/focus.cpp
	src/ui/runtime/tree.cpp
	src/ui/style/text_measure.cpp
	src/ui/runtime/flex_layout.cpp
	src/ui/runtime/yoga_flex_layout.cpp)
target_compile_features(retained_ui_focus_tests PRIVATE cxx_std_20)
target_include_directories(retained_ui_focus_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(retained_ui_focus_tests PRIVATE yogacore)
if(MSVC)
	target_compile_definitions(retained_ui_focus_tests PRIVATE NOMINMAX)
endif()
add_test(NAME retained_ui_focus_tests COMMAND retained_ui_focus_tests)

# Fiber-keyed interaction hooks (use_focused/hovered/pressed/focus_visible).
# Hermetic — reads a published InteractionSnapshot via ReactContext, no UiTree.
add_executable(ui_interaction_hooks_tests
	tests/ui_interaction_hooks_tests.cpp
	src/ui/runtime/interaction_hooks.cpp
	src/ui/runtime/react.cpp)
target_compile_features(ui_interaction_hooks_tests PRIVATE cxx_std_20)
target_include_directories(ui_interaction_hooks_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(MSVC)
	target_compile_definitions(ui_interaction_hooks_tests PRIVATE NOMINMAX)
endif()
add_test(NAME ui_interaction_hooks_tests COMMAND ui_interaction_hooks_tests)

# --- App-shell 5-phase pipeline (SIL-13) ---------------------------------
# The golden frame contract: UiPipeline + ClientUi + ScreenStack/UiScreen +
# the NavigationProvider/use_navigation it compile-depends on (namespace
# client::ui, distinct from the live silencer::ui ClientUi/ScreenStack). All
# SDL-free; the render seam is an injected std::function. Vendored ALONGSIDE
# the live Clay pipeline — the production cutover is SIL-14+. The two CTests
# drive the full retained closure (react+element+tree+focus+yoga+draw IR).
set(_cppx_retained_sources
	src/ui/runtime/react.cpp
	src/ui/runtime/element.cpp
	src/ui/runtime/tree.cpp
	src/ui/runtime/focus.cpp
	src/ui/runtime/interaction_hooks.cpp
	src/ui/runtime/flex_layout.cpp
	src/ui/runtime/yoga_flex_layout.cpp
	src/ui/runtime/draw_command.cpp
	src/ui/runtime/draw_command_builder.cpp
	src/ui/style/text_measure.cpp
	src/ui/style/default_theme.cpp
	src/ui/style/resolve.cpp)
# NOTE: app_root.cpp is deliberately NOT here. Its make_phase_element names the
# real screens (which transitively pull the keybinds/config/os game deps), so it
# is added only to the target that exercises the phase reconciler with real
# screens (pipeline_host_tests). The lean shell tests below use their own test
# screens and never name AppRoot.
set(_cppx_app_shell_sources
	src/client/ui/app_shell/client_ui.cpp
	src/client/ui/app_shell/navigation/screen_stack.cpp
	src/client/ui/app_theme.cpp
	src/client/ui/providers/app_provider.cpp
	src/client/ui/providers/chrome_textures_provider.cpp
	src/client/ui/providers/key_map_provider.cpp
	src/client/ui/providers/keybind_capture_provider.cpp
	src/client/ui/providers/lobby_provider.cpp
	src/client/ui/providers/navigation_provider.cpp
	src/client/ui/providers/server_provider.cpp
	src/client/ui/providers/session_provider.cpp
	src/client/ui/providers/settings_provider.cpp
	src/client/ui/providers/updater_provider.cpp
	src/client/ui/providers/world_session_provider.cpp)

add_executable(client_ui_tests
	tests/client_ui_tests.cpp
	${_cppx_app_shell_sources}
	${_cppx_retained_sources})
target_compile_features(client_ui_tests PRIVATE cxx_std_20)
target_include_directories(client_ui_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(client_ui_tests PRIVATE yogacore)
if(MSVC)
	target_compile_definitions(client_ui_tests PRIVATE NOMINMAX)
endif()
add_test(NAME client_ui_tests COMMAND client_ui_tests)

add_executable(ui_pipeline_tests
	tests/ui_pipeline_tests.cpp
	src/client/ui/app_shell/ui_pipeline.cpp
	${_cppx_app_shell_sources}
	${_cppx_retained_sources})
target_compile_features(ui_pipeline_tests PRIVATE cxx_std_20)
target_include_directories(ui_pipeline_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ui_pipeline_tests PRIVATE yogacore)
if(MSVC)
	target_compile_definitions(ui_pipeline_tests PRIVATE NOMINMAX)
endif()
add_test(NAME ui_pipeline_tests COMMAND ui_pipeline_tests)

# --- Production render seam: PipelineHost (SIL-14) ------------------------
# Drives the real client::ui::UiPipeline and executes its IR to pixels via the
# SIL-11 UiSurface/execute_draw_commands path (the render-lambda -> UiSurface
# bridge SIL-13 deferred). Headless software renderer; a stub fill-box screen
# proves pipeline -> renderer end-to-end. pipeline_host.cpp compiles into the
# client via the src/render/cppx_ui glob.
add_executable(pipeline_host_tests
	tests/pipeline_host_tests.cpp
	src/render/cppx_ui/pipeline_host.cpp
	src/render/cppx_ui/ui_surface.cpp
	src/render/cppx_ui/draw_executor.cpp
	src/render/cppx_ui/sdf_raster.cpp
	src/render/cppx_ui/font_registry.cpp
	src/render/cppx_ui/texture_registry.cpp
	src/render/cppx_ui/sprite_bake.cpp
	src/render/cppx_ui/text_measure.cpp
	src/ui/runtime/geometry.cpp
	src/client/ui/app_shell/ui_pipeline.cpp
	# AppRoot's make_phase_element renders the real phase screens (SIL-18/19); this
	# is the one test that exercises the reconciler with them. The screen graph is
	# kept game-dep-free (labels are pre-resolved by the composition root), so no
	# keybinds/config/os link is needed here.
	src/client/ui/app_shell/app_root.cpp
	${_cppx_app_shell_sources}
	${_cppx_retained_sources}
	# SIL-16: the generated ui::components primitives, so the render test can
	# drive them headlessly.
	${SILENCER_CPPX_GENERATED})
target_compile_features(pipeline_host_tests PRIVATE cxx_std_20)
target_include_directories(pipeline_host_tests PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/src" ${SILENCER_CPPX_INCLUDE_DIRS})
target_link_libraries(pipeline_host_tests PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf yogacore)
target_compile_definitions(pipeline_host_tests PRIVATE
	SILENCER_TEST_FONT_DIR="${SILENCER_SHARED_DIR}/fonts")
if(MSVC)
	target_compile_definitions(pipeline_host_tests PRIVATE NOMINMAX)
endif()
add_test(NAME pipeline_host_tests COMMAND pipeline_host_tests)

# --- SDL-free draw model (SIL-11 foundation) -----------------------------
# RGBA DrawCommand IR + tessellation math. draw_command_builder.cpp compiles
# into the client via the source glob; the renderer-side SDL executor/
# registries are a separate bridge layer (later in SIL-11).
add_executable(ui_geometry_tests
	tests/ui_geometry_tests.cpp
	src/ui/runtime/geometry.cpp)
target_compile_features(ui_geometry_tests PRIVATE cxx_std_20)
target_include_directories(ui_geometry_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(MSVC)
	target_compile_definitions(ui_geometry_tests PRIVATE NOMINMAX)
endif()
add_test(NAME ui_geometry_tests COMMAND ui_geometry_tests)

add_executable(ui_draw_command_tests
	tests/ui_draw_command_tests.cpp
	src/ui/runtime/draw_command.cpp)
target_compile_features(ui_draw_command_tests PRIVATE cxx_std_20)
target_include_directories(ui_draw_command_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(MSVC)
	target_compile_definitions(ui_draw_command_tests PRIVATE NOMINMAX)
endif()
add_test(NAME ui_draw_command_tests COMMAND ui_draw_command_tests)

# SIL-11 font path: SDL_ttf + the four silencer .otf faces load + measure.
# Foundation for the golden FontRegistry + install_text_measurer (next).
add_executable(ttf_font_smoke_tests tests/ttf_font_smoke_tests.cpp)
target_link_libraries(ttf_font_smoke_tests PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)
target_compile_definitions(ttf_font_smoke_tests PRIVATE
	SILENCER_TEST_FONT_DIR="${SILENCER_SHARED_DIR}/fonts")
add_test(NAME ttf_font_smoke_tests COMMAND ttf_font_smoke_tests)

# Multi-face text-measure seam: FontRegistry + install_text_measurer wired into
# the SDL-free ui/ measurer, exercised through ui::text_measurer().
add_executable(ui_text_measure_tests
	tests/ui_text_measure_tests.cpp
	src/render/cppx_ui/font_registry.cpp
	src/render/cppx_ui/text_measure.cpp
	src/ui/style/text_measure.cpp)
target_compile_features(ui_text_measure_tests PRIVATE cxx_std_20)
target_include_directories(ui_text_measure_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ui_text_measure_tests PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)
target_compile_definitions(ui_text_measure_tests PRIVATE
	SILENCER_TEST_FONT_DIR="${SILENCER_SHARED_DIR}/fonts")
if(MSVC)
	target_compile_definitions(ui_text_measure_tests PRIVATE NOMINMAX)
endif()
add_test(NAME ui_text_measure_tests COMMAND ui_text_measure_tests)

# Sprite->RGBA bake: indexed Surface + palette -> premultiplied RGBA8.
add_executable(sprite_bake_tests
	tests/sprite_bake_tests.cpp
	src/render/cppx_ui/sprite_bake.cpp)
target_compile_features(sprite_bake_tests PRIVATE cxx_std_20)
target_include_directories(sprite_bake_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(sprite_bake_tests PRIVATE SDL3::SDL3)
if(MSVC)
	target_compile_definitions(sprite_bake_tests PRIVATE NOMINMAX)
endif()
add_test(NAME sprite_bake_tests COMMAND sprite_bake_tests)

# texture_id -> SDL_Texture registry (upload_rgba premultiplies). Headless via
# a software SDL_Renderer.
add_executable(texture_registry_tests
	tests/texture_registry_tests.cpp
	src/render/cppx_ui/texture_registry.cpp
	src/render/cppx_ui/sprite_bake.cpp)
target_compile_features(texture_registry_tests PRIVATE cxx_std_20)
target_include_directories(texture_registry_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(texture_registry_tests PRIVATE SDL3::SDL3)
if(MSVC)
	target_compile_definitions(texture_registry_tests PRIVATE NOMINMAX)
endif()
add_test(NAME texture_registry_tests COMMAND texture_registry_tests)

# DrawCommand IR -> pixels executor, verified by headless pixel readback.
add_executable(draw_executor_tests
	tests/draw_executor_tests.cpp
	src/render/cppx_ui/draw_executor.cpp
	src/render/cppx_ui/sdf_raster.cpp
	src/render/cppx_ui/font_registry.cpp
	src/render/cppx_ui/texture_registry.cpp
	src/ui/runtime/draw_command.cpp
	src/ui/runtime/geometry.cpp)
target_compile_features(draw_executor_tests PRIVATE cxx_std_20)
target_include_directories(draw_executor_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(draw_executor_tests PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)
target_compile_definitions(draw_executor_tests PRIVATE
	SILENCER_TEST_FONT_DIR="${SILENCER_SHARED_DIR}/fonts")
if(MSVC)
	target_compile_definitions(draw_executor_tests PRIVATE NOMINMAX)
endif()
add_test(NAME draw_executor_tests COMMAND draw_executor_tests)

# UiSurface: the per-frame clear -> execute IR -> resolve -> present wrapper
# (direct + supersample), driving the executor. Headless software renderer.
add_executable(ui_surface_tests
	tests/ui_surface_tests.cpp
	src/render/cppx_ui/ui_surface.cpp
	src/render/cppx_ui/draw_executor.cpp
	src/render/cppx_ui/sdf_raster.cpp
	src/render/cppx_ui/font_registry.cpp
	src/render/cppx_ui/texture_registry.cpp
	src/ui/runtime/draw_command.cpp
	src/ui/runtime/geometry.cpp)
target_compile_features(ui_surface_tests PRIVATE cxx_std_20)
target_include_directories(ui_surface_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ui_surface_tests PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)
target_compile_definitions(ui_surface_tests PRIVATE
	SILENCER_TEST_FONT_DIR="${SILENCER_SHARED_DIR}/fonts")
if(MSVC)
	target_compile_definitions(ui_surface_tests PRIVATE NOMINMAX)
endif()
add_test(NAME ui_surface_tests COMMAND ui_surface_tests)

# Executor Image path: plain textured rect + nine-slice (9-patch) button,
# corners 1:1. Headless software renderer + texture_registry upload.
add_executable(draw_image_tests
	tests/draw_image_tests.cpp
	src/render/cppx_ui/draw_executor.cpp
	src/render/cppx_ui/sdf_raster.cpp
	src/render/cppx_ui/font_registry.cpp
	src/render/cppx_ui/texture_registry.cpp
	src/ui/runtime/draw_command.cpp
	src/ui/runtime/geometry.cpp)
target_compile_features(draw_image_tests PRIVATE cxx_std_20)
target_include_directories(draw_image_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(draw_image_tests PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)
if(MSVC)
	target_compile_definitions(draw_image_tests PRIVATE NOMINMAX)
endif()
add_test(NAME draw_image_tests COMMAND draw_image_tests)

# /Z7 embeds debug info into .obj files instead of routing it through MSVC's
# mspdbsrv.exe — which has a habit of failing with C1090 PDB errors on some
# Windows installs. /Zi (the default) is faster but unreliable; /Z7 is rock
# solid and debuggers handle it identically. Linker still emits Silencer.pdb
# for runtime debugging.
if(MSVC)
	target_compile_options(${SILENCER_TARGET} PRIVATE $<$<CONFIG:Debug>:/Z7>)
	# MSVC 14.44+ removed _snprintf from std::; nlohmann/json uses it internally.
	# Force-include the shim before every TU to re-expose it.
	target_compile_options(${SILENCER_TARGET} PRIVATE
		"/FI${CMAKE_CURRENT_SOURCE_DIR}/cmake/msvc_snprintf_compat.h")
	# windows.h's min/max macros mangle std::min / std::max calls.
	target_compile_definitions(${SILENCER_TARGET} PRIVATE NOMINMAX)
endif()

set(SILENCER_LOBBY_HOST "lobby.arsiamons.com" CACHE STRING "Lobby host the client connects to at startup")
set(SILENCER_LOBBY_PORT "517" CACHE STRING "Lobby TCP port")
set(SILENCER_VERSION "00058" CACHE STRING "Client version string sent to the lobby during MSG_VERSION; overridden by release.yml from git tag")
set(SILENCER_MAP_API_URL "https://admin.arsiamons.com" CACHE STRING "Base URL for the community map HTTP API (used when fetching a missing map before falling back to P2P)")
set(SILENCER_ADMIN_API_URL "https://admin.arsiamons.com" CACHE STRING "Base URL for the admin API (actordefs sync, etc.)")
target_compile_definitions(${SILENCER_TARGET} PRIVATE
	SILENCER_LOBBY_HOST="${SILENCER_LOBBY_HOST}"
	SILENCER_LOBBY_PORT=${SILENCER_LOBBY_PORT}
	SILENCER_VERSION="${SILENCER_VERSION}"
	SILENCER_MAP_API_URL="${SILENCER_MAP_API_URL}"
	SILENCER_ADMIN_API_URL="${SILENCER_ADMIN_API_URL}"
	SILENCER_HAVE_LOBBY_UI=1
	# SIL-11 demo overlay (SILENCER_CPPX_UI_DEMO env) loads UI fonts from here.
	SILENCER_CPPX_FONT_DIR="${SILENCER_SHARED_DIR}/fonts"
)

find_package(ZLIB REQUIRED)
find_package(SDL3 REQUIRED)
find_package(SDL3_mixer REQUIRED)
find_package(SDL3_ttf REQUIRED CONFIG)  # SIL-11: golden TTF FontRegistry / UI text
find_package(CURL REQUIRED)

# Minizip: prefer vcpkg's CMake CONFIG packages (Windows CI), then fall back
# to pkg-config for Homebrew/apt. Wrap whichever we find in a single
# `minizip_dep` interface target so the link sections below don't have to
# care which source it came from. Fail loudly if none resolves — a silent
# miss used to slip through and surface as `unzip.h not found`.
find_package(unofficial-minizip CONFIG QUIET)
if(NOT unofficial-minizip_FOUND)
	find_package(minizip CONFIG QUIET)
endif()

add_library(minizip_dep INTERFACE)
if(TARGET unofficial::minizip::minizip)
	target_link_libraries(minizip_dep INTERFACE unofficial::minizip::minizip)
elseif(TARGET minizip::minizip)
	target_link_libraries(minizip_dep INTERFACE minizip::minizip)
else()
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(MINIZIP REQUIRED IMPORTED_TARGET minizip)
	# Homebrew/apt minizip ships headers at <prefix>/include/minizip/unzip.h,
	# so the pkg-config -I flag alone doesn't let `#include <unzip.h>` resolve.
	# vcpkg's minizip flattens them into include/ directly, which is why
	# Windows doesn't hit this.
	foreach(_dir ${MINIZIP_INCLUDE_DIRS})
		if(IS_DIRECTORY "${_dir}/minizip")
			target_include_directories(minizip_dep INTERFACE "${_dir}/minizip")
		endif()
	endforeach()
	target_link_libraries(minizip_dep INTERFACE PkgConfig::MINIZIP)
endif()

include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${NLOHMANN_JSON_INCLUDE_DIR})
include_directories(${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann)
find_package(SDL3_mixer REQUIRED CONFIG)
if(TARGET SDL3_mixer::SDL3_mixer)
  get_target_property(_sdl3mixer_inc SDL3_mixer::SDL3_mixer INTERFACE_INCLUDE_DIRECTORIES)
  if(_sdl3mixer_inc)
    include_directories(${_sdl3mixer_inc})
  endif()
endif()

if(APPLE)
	find_library(COCOA_FRAMEWORK Cocoa)
	target_link_libraries(${SILENCER_TARGET}
		SDL3::SDL3 SDL3_mixer::SDL3_mixer SDL3_ttf::SDL3_ttf ${ZLIB_LIBRARY}
		CURL::libcurl
		${COCOA_FRAMEWORK})
elseif(WIN32)
	target_link_libraries(${SILENCER_TARGET}
		SDL3::SDL3 SDL3_mixer::SDL3_mixer SDL3_ttf::SDL3_ttf ${ZLIB_LIBRARY}
		CURL::libcurl ws2_32)
else()
	target_link_libraries(${SILENCER_TARGET}
		SDL3::SDL3 SDL3_mixer::SDL3_mixer SDL3_ttf::SDL3_ttf ${ZLIB_LIBRARY}
		CURL::libcurl)
endif()

target_link_libraries(${SILENCER_TARGET} minizip_dep)

# Dev-build asset staging (Windows/Linux): copy shared/assets/ next to the
# binary so the runtime fallback (`GetResDir() + "<subdir>/..."` in
# `src/platform/os.cpp`) resolves without needing `cmake --install`. Affects
# gas/, keybinds/, behaviortrees/, and any future runtime asset subdir;
# without this every fresh build silently runs with empty GAS/keybind data.
# macOS bakes assets into the .app bundle directly via the bundle_extras
# list above, so this branch is no-op there.
if(NOT APPLE)
	add_custom_command(TARGET ${SILENCER_TARGET} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${SILENCER_SHARED_DIR}/assets"
				"$<TARGET_FILE_DIR:${SILENCER_TARGET}>/assets"
		COMMENT "Staging shared/assets/ next to ${SILENCER_TARGET}")
endif()

install(TARGETS "${SILENCER_TARGET}" RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" BUNDLE DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${SILENCER_SHARED_DIR}/assets/" DESTINATION "${CMAKE_INSTALL_DATADIR}/silencer")
install(FILES "${SILENCER_SHARED_DIR}/icons/icon_16.png"  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/16x16/apps"   RENAME "silencer.png")
install(FILES "${SILENCER_SHARED_DIR}/icons/icon_32.png"  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps"   RENAME "silencer.png")
install(FILES "${SILENCER_SHARED_DIR}/icons/icon_64.png"  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps"   RENAME "silencer.png")
install(FILES "${SILENCER_SHARED_DIR}/icons/icon_128.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps" RENAME "silencer.png")
install(FILES "silencer.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")

set(CPACK_PACKAGE_NAME "Silencer")
set(CPACK_PACKAGE_FILE_NAME "silencer")
set(CPACK_PACKAGE_VERSION "00023")

option(SILENCER_BUILD_TESTS "Build unit tests" OFF)
if(SILENCER_BUILD_TESTS)
	enable_testing()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../tests ${CMAKE_BINARY_DIR}/tests)
endif()

include(CPack)
