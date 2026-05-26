#include "doctest.h"

#include "runtime/ClayService.h"
#include "runtime/react.h"

#include "clay/clay.h"

#include <cstdlib>
#include <cstring>

namespace {

Clay_Context* g_clay = nullptr;
void* g_clayMemory = nullptr;

void OnClayError(Clay_ErrorData error) {
	(void)error;
}

Clay_Dimensions MeasureText(Clay_StringSlice text,
                            Clay_TextElementConfig*,
                            void*) {
	return Clay_Dimensions{static_cast<float>(text.length) * 8.0f, 16.0f};
}

void EnsureClay() {
	if(g_clay) {
		Clay_SetCurrentContext(g_clay);
		return;
	}

	uint32_t clayMemorySize = Clay_MinMemorySize();
	g_clayMemory = std::malloc(clayMemorySize);
	REQUIRE(g_clayMemory != nullptr);

	Clay_Arena arena =
		Clay_CreateArenaWithCapacityAndMemory(clayMemorySize, g_clayMemory);
	g_clay = Clay_Initialize(arena,
	                         Clay_Dimensions{640.0f, 480.0f},
	                         Clay_ErrorHandler{OnClayError, nullptr});
	REQUIRE(g_clay != nullptr);
	Clay_SetMeasureTextFunction(MeasureText, nullptr);
}

template <typename Build>
Clay_RenderCommandArray RunReactFrame(Build build) {
	EnsureClay();
	react_begin_frame();
	Clay_SetLayoutDimensions(Clay_Dimensions{640.0f, 480.0f});
	Clay_BeginLayout();
	CLAY({.id = CLAY_ID("TestRoot")}) {
		build();
	}
	Clay_RenderCommandArray commands = Clay_EndLayout();
	react_end_frame();
	return commands;
}

class RealClayBackend : public silencer::ui::ClayFrameBackend {
public:
	void SetCurrentContext() override { EnsureClay(); }
	void SetLayoutDimensions(int width, int height) override {
		Clay_SetLayoutDimensions(Clay_Dimensions{
			static_cast<float>(width), static_cast<float>(height)});
	}
	void SetUiScale(float) override {}
	void SetPointerState(float x, float y, bool down) override {
		Clay_SetPointerState(Clay_Vector2{x, y}, down);
	}
	void UpdateScrollContainers(float wheelX,
	                            float wheelY,
	                            float deltaTimeSeconds) override {
		Clay_UpdateScrollContainers(true,
		                            Clay_Vector2{wheelX, wheelY},
		                            deltaTimeSeconds);
	}
	void BeginLayout() override { Clay_BeginLayout(); }
	Clay_RenderCommandArray EndLayout() override { return Clay_EndLayout(); }
};

int gProbeValues[2] = {};

void PositionalStateProbe(int slot, int initial, int writeValue) {
	REACT_COMPONENT_BEGIN("StateProbe") {
		int* value = use_state_int(initial);
		if(writeValue >= 0) *value = writeValue;
		gProbeValues[slot] = *value;
	} REACT_COMPONENT_END();
}

void KeyedStateProbe(int key, int initial, int writeValue) {
	REACT_COMPONENT_BEGIN_KEY("StateProbe", key) {
		int* value = use_state_int(initial);
		if(writeValue >= 0) *value = writeValue;
		gProbeValues[key] = *value;
	} REACT_COMPONENT_END();
}

ReactContext gIntContext{};
int gProviderValues[2] = {};
int gContextValue = 0;

void ProviderProbe(int slot, int initial, int writeValue) {
	REACT_PROVIDER_ENTER("ProviderProbe");
	int* value = use_state_int(initial);
	if(writeValue >= 0) *value = writeValue;
	gProviderValues[slot] = *value;
	PROVIDE(&gIntContext, value) {
		int* provided = static_cast<int*>(use_context(&gIntContext));
		gContextValue = provided ? *provided : -1;
	}
	REACT_PROVIDER_EXIT();
}

int gEffectMounts = 0;
int gEffectCleanups = 0;

void OnMount(void*) { gEffectMounts++; }
void OnUnmount(void*) { gEffectCleanups++; }

void EffectProbe() {
	REACT_COMPONENT_BEGIN("EffectProbe") {
		use_effect(OnMount, OnUnmount, nullptr, 0);
	} REACT_COMPONENT_END();
}

bool gUseRefForKindProbe = false;
bool gExtraHookForCountProbe = false;

void KindDriftProbe() {
	REACT_COMPONENT_BEGIN("KindDriftProbe") {
		if(gUseRefForKindProbe){
			(void)use_ref(nullptr);
		}else{
			(void)use_state_int(1);
		}
	} REACT_COMPONENT_END();
}

void CountDriftProbe() {
	REACT_COMPONENT_BEGIN("CountDriftProbe") {
		(void)use_state_int(1);
		if(gExtraHookForCountProbe){
			(void)use_state_int(2);
		}
	} REACT_COMPONENT_END();
}

}  // namespace

TEST_CASE("React runtime keeps positional and keyed component state") {
	EnsureClay();
	react_init(g_clay);
	gProbeValues[0] = 0;
	gProbeValues[1] = 0;

	RunReactFrame([] {
		PositionalStateProbe(0, 1, 10);
		PositionalStateProbe(1, 2, 20);
	});
	CHECK(react_error_count() == 0);
	CHECK(gProbeValues[0] == 10);
	CHECK(gProbeValues[1] == 20);

	RunReactFrame([] {
		PositionalStateProbe(0, 99, -1);
		PositionalStateProbe(1, 99, -1);
	});
	CHECK(react_error_count() == 0);
	CHECK(gProbeValues[0] == 10);
	CHECK(gProbeValues[1] == 20);

	react_init(g_clay);
	RunReactFrame([] {
		KeyedStateProbe(0, 1, 10);
		KeyedStateProbe(1, 2, 20);
	});
	RunReactFrame([] {
		KeyedStateProbe(1, 99, -1);
		KeyedStateProbe(0, 99, -1);
	});
	CHECK(react_error_count() == 0);
	CHECK(gProbeValues[0] == 10);
	CHECK(gProbeValues[1] == 20);
}

TEST_CASE("React providers own hook identity and expose context values") {
	EnsureClay();
	react_init(g_clay);
	gProviderValues[0] = 0;
	gProviderValues[1] = 0;
	gContextValue = 0;

	RunReactFrame([] {
		ProviderProbe(0, 1, 10);
		ProviderProbe(1, 2, 20);
	});
	CHECK(react_error_count() == 0);
	CHECK(gProviderValues[0] == 10);
	CHECK(gProviderValues[1] == 20);
	CHECK(gContextValue == 20);

	RunReactFrame([] {
		ProviderProbe(0, 99, -1);
		ProviderProbe(1, 99, -1);
	});
	CHECK(react_error_count() == 0);
	CHECK(gProviderValues[0] == 10);
	CHECK(gProviderValues[1] == 20);
	CHECK(gContextValue == 20);
}

TEST_CASE("React effects clean up on unmount and shutdown") {
	EnsureClay();
	react_init(g_clay);
	gEffectMounts = 0;
	gEffectCleanups = 0;

	RunReactFrame([] { EffectProbe(); });
	CHECK(react_error_count() == 0);
	CHECK(gEffectMounts == 1);
	CHECK(gEffectCleanups == 0);

	RunReactFrame([] {});
	CHECK(react_error_count() == 0);
	CHECK(gEffectCleanups == 1);

	RunReactFrame([] { EffectProbe(); });
	CHECK(gEffectMounts == 2);
	react_shutdown();
	CHECK(gEffectCleanups == 2);
}

TEST_CASE("React runtime diagnoses hook kind and count drift") {
	EnsureClay();
	react_init(g_clay);
	gUseRefForKindProbe = false;
	RunReactFrame([] { KindDriftProbe(); });
	CHECK(react_error_count() == 0);

	gUseRefForKindProbe = true;
	RunReactFrame([] { KindDriftProbe(); });
	CHECK(react_error_count() == 1);

	react_init(g_clay);
	gExtraHookForCountProbe = false;
	RunReactFrame([] { CountDriftProbe(); });
	CHECK(react_error_count() == 0);

	gExtraHookForCountProbe = true;
	RunReactFrame([] { CountDriftProbe(); });
	CHECK(react_error_count() == 1);
}

TEST_CASE("ClayService brackets UI declarations with React frame lifecycle") {
	RealClayBackend backend;
	silencer::ui::ClayService service(backend);
	silencer::ui::UiInteractionRegistry registry;
	silencer::ui::UiInputState input;
	input.width = 640;
	input.height = 480;

	react_shutdown();
	gProbeValues[0] = 0;
	service.BeginFrame(input, registry);
	CLAY({.id = CLAY_ID("ServiceRoot")}) {
		PositionalStateProbe(0, 1, 42);
	}
	service.EndFrame();
	CHECK(react_error_count() == 0);
	CHECK(gProbeValues[0] == 42);

	gProbeValues[0] = 0;
	service.BeginFrame(input, registry);
	CLAY({.id = CLAY_ID("ServiceRoot")}) {
		PositionalStateProbe(0, 1, -1);
	}
	service.EndFrame();
	CHECK(react_error_count() == 0);
	CHECK(gProbeValues[0] == 42);
}
