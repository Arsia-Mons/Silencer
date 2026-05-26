#include "ui/focus/UiFocus.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace silencer {
namespace ui {

namespace {

UiFocusRuntime * g_current = nullptr;

bool SameId(Clay_ElementId a, Clay_ElementId b) {
	return a.id != 0 && a.id == b.id;
}

void ReportError(UiFocusRuntime * runtime, const char * message) {
	if(!runtime) return;
	runtime->errorCount++;
	std::fprintf(stderr, "ui_focus: %s\n", message);
}

int ClampLimit(int value, int fallback, int maxValue) {
	if(value <= 0) return fallback;
	if(value > maxValue) return maxValue;
	return value;
}

UiFocusScope * FindScope(UiFocusRuntime * runtime, Clay_ElementId id) {
	if(!runtime || id.id == 0) return nullptr;
	for(int i = 0; i < runtime->scopeCount; ++i){
		if(SameId(runtime->scopes[i].id, id)) return &runtime->scopes[i];
	}
	return nullptr;
}

int FindScopeIndex(UiFocusRuntime * runtime, Clay_ElementId id) {
	if(!runtime || id.id == 0) return -1;
	for(int i = 0; i < runtime->scopeCount; ++i){
		if(SameId(runtime->scopes[i].id, id)) return i;
	}
	return -1;
}

UiFocusScope * CreateScope(UiFocusRuntime * runtime, const UiFocusScopeDesc& desc) {
	if(!runtime || desc.id.id == 0) return nullptr;
	if(runtime->scopeCount >= runtime->limits.maxFocusScopes){
		ReportError(runtime, "scope overflow");
		return nullptr;
	}
	UiFocusScope * scope = &runtime->scopes[runtime->scopeCount++];
	*scope = {};
	scope->id = desc.id;
	scope->modal = desc.modal;
	scope->wrap = desc.wrap;
	return scope;
}

void ClearPendingRegistrations(UiFocusScope * scope) {
	if(!scope) return;
	for(int i = 0; i < scope->pendingCount; ++i){
		scope->pending[i] = {};
	}
	scope->pendingCount = 0;
}

UiFocusScope * ScopeForDeclaration(UiFocusRuntime * runtime,
                                   const UiFocusScopeDesc& desc) {
	UiFocusScope * scope = FindScope(runtime, desc.id);
	if(!scope) scope = CreateScope(runtime, desc);
	if(!scope) return nullptr;

	scope->modal = desc.modal;
	scope->wrap = desc.wrap;
	if(scope->declaredFrame != runtime->frame){
		scope->declaredFrame = runtime->frame;
		scope->declarationOrder = runtime->nextDeclarationOrder++;
		ClearPendingRegistrations(scope);
	}
	return scope;
}

UiFocusScope * ActiveScopeForDeclaration(UiFocusRuntime * runtime) {
	if(!runtime || runtime->scopeStackCount <= 0) return nullptr;
	if(runtime->scopeNoopPushDepth > 0) return nullptr;
	int index = runtime->scopeStack[runtime->scopeStackCount - 1];
	if(index < 0 || index >= runtime->scopeCount) return nullptr;
	return &runtime->scopes[index];
}

UiFocusScope * ActiveDeclaredScope(UiFocusRuntime * runtime, uint32_t frame) {
	if(!runtime) return nullptr;
	UiFocusScope * bestModal = nullptr;
	UiFocusScope * bestAny = nullptr;
	for(int i = 0; i < runtime->scopeCount; ++i){
		UiFocusScope * scope = &runtime->scopes[i];
		if(scope->declaredFrame != frame) continue;
		if(!bestAny || scope->declarationOrder > bestAny->declarationOrder){
			bestAny = scope;
		}
		if(scope->modal &&
		   (!bestModal || scope->declarationOrder > bestModal->declarationOrder)){
			bestModal = scope;
		}
	}
	return bestModal ? bestModal : bestAny;
}

UiNavRule RuleForDir(const UiNavRules& rules, UiNavDir dir) {
	switch(dir){
		case UiNavDir::Up: return rules.up;
		case UiNavDir::Down: return rules.down;
		case UiNavDir::Left: return rules.left;
		case UiNavDir::Right: return rules.right;
	}
	return {};
}

const UiFocusableLayout * FindLayout(const UiFocusScope * scope, Clay_ElementId id) {
	if(!scope || id.id == 0) return nullptr;
	for(int i = 0; i < scope->layoutCount; ++i){
		if(SameId(scope->layout[i].id, id)) return &scope->layout[i];
	}
	return nullptr;
}

bool PointerOver(Clay_ElementId id) {
	return id.id != 0 && Clay_PointerOver(id);
}

bool ContainsEnabled(const UiFocusScope * scope, Clay_ElementId id) {
	const UiFocusableLayout * layout = FindLayout(scope, id);
	return layout && !layout->disabled;
}

Clay_ElementId FirstEnabled(const UiFocusScope * scope) {
	if(!scope) return {};
	for(int i = 0; i < scope->layoutCount; ++i){
		if(!scope->layout[i].disabled) return scope->layout[i].id;
	}
	return {};
}

bool PointIn(Clay_BoundingBox rect, float x, float y) {
	return x >= rect.x && y >= rect.y
	    && x < rect.x + rect.width && y < rect.y + rect.height;
}

Clay_ElementId HoveredEnabled(const UiFocusScope * scope, float x, float y) {
	if(!scope) return {};
	for(int i = scope->layoutCount - 1; i >= 0; --i){
		const UiFocusableLayout& layout = scope->layout[i];
		if(!layout.disabled && PointIn(layout.rect, x, y)){
			return layout.id;
		}
	}
	return {};
}

const UiFocusableRegistration * FindRegistration(const UiFocusScope * scope,
                                                 Clay_ElementId id) {
	if(!scope || id.id == 0) return nullptr;
	for(int i = 0; i < scope->pendingCount; ++i){
		if(SameId(scope->pending[i].id, id)) return &scope->pending[i];
	}
	return nullptr;
}

float CenterX(Clay_BoundingBox r) {
	return r.x + r.width * 0.5f;
}

float CenterY(Clay_BoundingBox r) {
	return r.y + r.height * 0.5f;
}

bool IntervalOverlaps(float a0, float a1, float b0, float b1) {
	return a0 < b1 && b0 < a1;
}

bool IsCandidateInDirection(Clay_BoundingBox from,
                            Clay_BoundingBox to,
                            UiNavDir dir) {
	switch(dir){
		case UiNavDir::Left: return CenterX(to) < CenterX(from);
		case UiNavDir::Right: return CenterX(to) > CenterX(from);
		case UiNavDir::Up: return CenterY(to) < CenterY(from);
		case UiNavDir::Down: return CenterY(to) > CenterY(from);
	}
	return false;
}

float PrimaryDistance(Clay_BoundingBox from,
                      Clay_BoundingBox to,
                      UiNavDir dir) {
	switch(dir){
		case UiNavDir::Left: return from.x - (to.x + to.width);
		case UiNavDir::Right: return to.x - (from.x + from.width);
		case UiNavDir::Up: return from.y - (to.y + to.height);
		case UiNavDir::Down: return to.y - (from.y + from.height);
	}
	return 0.0f;
}

float PerpendicularMiss(Clay_BoundingBox from,
                        Clay_BoundingBox to,
                        UiNavDir dir) {
	bool horizontal = dir == UiNavDir::Left || dir == UiNavDir::Right;
	if(horizontal){
		if(IntervalOverlaps(from.y, from.y + from.height, to.y, to.y + to.height)){
			return 0.0f;
		}
		return std::fabs(CenterY(to) - CenterY(from));
	}
	if(IntervalOverlaps(from.x, from.x + from.width, to.x, to.x + to.width)){
		return 0.0f;
	}
	return std::fabs(CenterX(to) - CenterX(from));
}

Clay_ElementId ResolveWrap(const UiFocusScope * scope, UiNavDir dir) {
	if(!scope) return {};
	const UiFocusableLayout * best = nullptr;
	for(int i = 0; i < scope->layoutCount; ++i){
		const UiFocusableLayout * candidate = &scope->layout[i];
		if(candidate->disabled) continue;
		if(!best){
			best = candidate;
			continue;
		}
		switch(dir){
			case UiNavDir::Left:
				if(CenterX(candidate->rect) > CenterX(best->rect)) best = candidate;
				break;
			case UiNavDir::Right:
				if(CenterX(candidate->rect) < CenterX(best->rect)) best = candidate;
				break;
			case UiNavDir::Up:
				if(CenterY(candidate->rect) > CenterY(best->rect)) best = candidate;
				break;
			case UiNavDir::Down:
				if(CenterY(candidate->rect) < CenterY(best->rect)) best = candidate;
				break;
		}
	}
	return best ? best->id : Clay_ElementId{};
}

Clay_ElementId ResolveSpatial(const UiFocusScope * scope,
                              Clay_ElementId fromId,
                              UiNavDir dir,
                              bool forceWrap) {
	if(!scope) return {};
	const UiFocusableLayout * from = FindLayout(scope, fromId);
	if(!from || from->disabled) return FirstEnabled(scope);

	const UiFocusableLayout * best = nullptr;
	float bestPerp = 0.0f;
	float bestPrimary = 0.0f;
	float bestCenter = 0.0f;

	for(int i = 0; i < scope->layoutCount; ++i){
		const UiFocusableLayout * candidate = &scope->layout[i];
		if(SameId(candidate->id, fromId) || candidate->disabled) continue;
		if(!IsCandidateInDirection(from->rect, candidate->rect, dir)) continue;

		float primary = PrimaryDistance(from->rect, candidate->rect, dir);
		if(primary < 0.0f) primary = 0.0f;

		float perp = PerpendicularMiss(from->rect, candidate->rect, dir);
		float dx = CenterX(candidate->rect) - CenterX(from->rect);
		float dy = CenterY(candidate->rect) - CenterY(from->rect);
		float center = dx * dx + dy * dy;

		bool better =
			!best ||
			perp < bestPerp ||
			(perp == bestPerp && primary < bestPrimary) ||
			(perp == bestPerp && primary == bestPrimary && center < bestCenter) ||
			(perp == bestPerp && primary == bestPrimary &&
			 center == bestCenter && candidate->order < best->order);

		if(better){
			best = candidate;
			bestPerp = perp;
			bestPrimary = primary;
			bestCenter = center;
		}
	}

	if(best) return best->id;
	return (scope->wrap || forceWrap) ? ResolveWrap(scope, dir) : fromId;
}

Clay_ElementId ResolveSequential(const UiFocusScope * scope,
                                  Clay_ElementId fromId,
                                  int direction) {
	if(!scope || direction == 0) return {};
	int current = -1;
	for(int i = 0; i < scope->layoutCount; ++i){
		if(SameId(scope->layout[i].id, fromId)){
			current = i;
			break;
		}
	}

	int index = direction > 0
		? (current < 0 ? 0 : current + 1)
		: (current < 0 ? scope->layoutCount - 1 : current - 1);
	for(int visited = 0; visited < scope->layoutCount; ++visited){
		if(index < 0 || index >= scope->layoutCount){
			if(!scope->wrap) break;
			index = direction > 0 ? 0 : scope->layoutCount - 1;
		}
		const UiFocusableLayout& candidate = scope->layout[index];
		if(!candidate.disabled) return candidate.id;
		index += direction > 0 ? 1 : -1;
	}

	return ContainsEnabled(scope, fromId) ? fromId : Clay_ElementId{};
}

Clay_ElementId ResolveNavigation(const UiFocusScope * scope,
                                 Clay_ElementId fromId,
                                 UiNavDir dir) {
	if(!scope) return {};
	const UiFocusableLayout * from = FindLayout(scope, fromId);
	UiNavRule rule = from ? RuleForDir(from->nav, dir) : UiNavRule{};
	switch(rule.kind){
		case UiNavRuleKind::Stop:
			return fromId;
		case UiNavRuleKind::Explicit:
			return ContainsEnabled(scope, rule.explicitTarget)
				? rule.explicitTarget
				: fromId;
		case UiNavRuleKind::Wrap:
			return ResolveSpatial(scope, fromId, dir, true);
		case UiNavRuleKind::Auto:
			return ResolveSpatial(scope, fromId, dir, false);
	}
	return fromId;
}

bool ReadNavDir(const UiFocusInputFrame& input, UiNavDir * dir) {
	if(input.navUp){
		*dir = UiNavDir::Up;
		return true;
	}
	if(input.navDown){
		*dir = UiNavDir::Down;
		return true;
	}
	if(input.navLeft){
		*dir = UiNavDir::Left;
		return true;
	}
	if(input.navRight){
		*dir = UiNavDir::Right;
		return true;
	}
	return false;
}

UiFocusSource NavigationSource(const UiFocusInputFrame& input) {
	if(input.source == UiFocusSource::Gamepad) return UiFocusSource::Gamepad;
	if(input.source == UiFocusSource::Touch) return UiFocusSource::Touch;
	if(input.source == UiFocusSource::Mouse) return UiFocusSource::Mouse;
	return UiFocusSource::Keyboard;
}

UiFocusSource PointerSource(const UiFocusInputFrame& input) {
	return input.source == UiFocusSource::Touch ? UiFocusSource::Touch : UiFocusSource::Mouse;
}

void HarvestScopeLayout(UiFocusRuntime * runtime, UiFocusScope * scope) {
	scope->layoutCount = 0;
	uint32_t order = 0;
	for(int i = 0; i < scope->pendingCount; ++i){
		const UiFocusableRegistration& entry = scope->pending[i];
		Clay_ElementData data = Clay_GetElementData(entry.id);
		if(!data.found) continue;
		if(scope->layoutCount >= runtime->limits.maxFocusablesPerScope){
			ReportError(runtime, "focus layout overflow");
			break;
		}
		scope->layout[scope->layoutCount++] = {
			entry.id,
			data.boundingBox,
			entry.disabled,
			order++,
			entry.nav,
		};
	}
}

void InvokeFocusCallbackIfRegistered(UiFocusScope * scope, Clay_ElementId id) {
	const UiFocusableRegistration * registration = FindRegistration(scope, id);
	if(registration && registration->onFocus){
		registration->onFocus();
	}
}

}  // namespace

void ui_focus_init(UiFocusRuntime * runtime, UiRuntimeLimits limits) {
	if(!runtime) return;
	*runtime = UiFocusRuntime{};
	runtime->scopesStorage.reset(new UiFocusScopeStorage{});
	runtime->scopes = runtime->scopesStorage->data();
	runtime->limits.maxFocusScopes = ClampLimit(
		limits.maxFocusScopes, UI_FOCUS_MAX_SCOPES, UI_FOCUS_MAX_SCOPES);
	runtime->limits.maxFocusablesPerScope = ClampLimit(
		limits.maxFocusablesPerScope,
		UI_FOCUS_MAX_FOCUSABLES_PER_SCOPE,
		UI_FOCUS_MAX_FOCUSABLES_PER_SCOPE);
	runtime->limits.maxUiIntents = limits.maxUiIntents > 0 ? limits.maxUiIntents : 128;
	runtime->limits.maxScreens = limits.maxScreens > 0 ? limits.maxScreens : 32;
	ui_focus_set_current(runtime);
}

void ui_focus_set_current(UiFocusRuntime * runtime) {
	g_current = runtime;
}

UiFocusRuntime * ui_focus_current() {
	return g_current;
}

void ui_focus_begin_frame(const UiFocusInputFrame& input) {
	UiFocusRuntime * runtime = g_current;
	if(!runtime) return;

	runtime->frame++;
	runtime->scopeStackCount = 0;
	runtime->scopeNoopPushDepth = 0;
	runtime->nextDeclarationOrder = 0;
	runtime->pendingFocusCallbackId = {};
	runtime->pendingPointerConfirmId = {};
	runtime->pointerDown = input.pointerDown;

	UiFocusScope * scope = ActiveDeclaredScope(runtime, runtime->frame - 1);
	UiNavDir dir;
	if(!scope) return;

	if(input.focusNext || input.focusPrevious){
		Clay_ElementId next = ResolveSequential(
			scope, scope->focusedId, input.focusNext ? 1 : -1);
		if(next.id != 0 && !SameId(next, scope->focusedId)){
			scope->focusedId = next;
			scope->source = NavigationSource(input);
			scope->autoFocusSuppressed = false;
			runtime->pendingFocusCallbackId = next;
		}
	}else if(ReadNavDir(input, &dir)){
		Clay_ElementId next = ResolveNavigation(scope, scope->focusedId, dir);
		if(next.id != 0 && !SameId(next, scope->focusedId)){
			scope->focusedId = next;
			scope->source = NavigationSource(input);
			scope->autoFocusSuppressed = false;
			runtime->pendingFocusCallbackId = next;
		}
	}

	if(input.pointerPressed){
		Clay_ElementId hovered = HoveredEnabled(scope, input.pointerX, input.pointerY);
		scope->pointerPressOrigin = hovered;
		if(hovered.id != 0){
			if(!SameId(scope->focusedId, hovered)){
				runtime->pendingFocusCallbackId = hovered;
			}
			scope->focusedId = hovered;
			scope->source = PointerSource(input);
			scope->autoFocusSuppressed = false;
		}
	}

	if(input.pointerMoved && !input.pointerPressed && !input.pointerReleased){
		Clay_ElementId hovered = HoveredEnabled(scope, input.pointerX, input.pointerY);
		if(hovered.id != 0){
			if(!SameId(scope->focusedId, hovered)){
				runtime->pendingFocusCallbackId = hovered;
			}
			scope->focusedId = hovered;
			scope->source = PointerSource(input);
			scope->autoFocusSuppressed = false;
		}else if(scope->source == UiFocusSource::Mouse ||
		         scope->source == UiFocusSource::Touch){
			scope->focusedId = {};
			scope->source = UiFocusSource::None;
			scope->pointerClearedFocus = true;
			scope->autoFocusSuppressed = true;
		}
	}

	if(input.pointerReleased){
		Clay_ElementId hovered = HoveredEnabled(scope, input.pointerX, input.pointerY);
		if(SameId(scope->pointerPressOrigin, hovered)){
			runtime->pendingPointerConfirmId = hovered;
		}
		scope->pointerPressOrigin = {};
	}else if(!input.pointerDown){
		scope->pointerPressOrigin = {};
	}
}

void ui_focus_push_scope(const UiFocusScopeDesc& desc) {
	UiFocusRuntime * runtime = g_current;
	if(!runtime) return;
	if(runtime->scopeNoopPushDepth > 0){
		runtime->scopeNoopPushDepth++;
		return;
	}
	if(runtime->scopeStackCount >= runtime->limits.maxFocusScopes){
		ReportError(runtime, "scope stack overflow");
		runtime->scopeNoopPushDepth++;
		return;
	}

	UiFocusScope * scope = ScopeForDeclaration(runtime, desc);
	if(!scope){
		runtime->scopeNoopPushDepth++;
		return;
	}
	int index = FindScopeIndex(runtime, scope->id);
	if(index < 0){
		runtime->scopeNoopPushDepth++;
		return;
	}
	runtime->scopeStack[runtime->scopeStackCount++] = index;
}

void ui_focus_pop_scope() {
	UiFocusRuntime * runtime = g_current;
	if(!runtime) return;
	if(runtime->scopeNoopPushDepth > 0){
		runtime->scopeNoopPushDepth--;
		return;
	}
	if(runtime->scopeStackCount <= 0){
		ReportError(runtime, "scope pop without matching push");
		return;
	}
	runtime->scopeStackCount--;
}

void ui_focus_request_initial_focus(Clay_ElementId id) {
	UiFocusScope * scope = ActiveScopeForDeclaration(g_current);
	if(!scope) return;
	scope->requestedInitialFocus = id;
}

void ui_focus_request_focus(Clay_ElementId id) {
	UiFocusScope * scope = ActiveScopeForDeclaration(g_current);
	if(!scope) return;
	scope->requestedFocus = id;
}

void ui_focus_retire_scope(Clay_ElementId id) {
	UiFocusRuntime * runtime = g_current;
	const int index = FindScopeIndex(runtime, id);
	if(index < 0) return;

	for(int stackIndex = 0; stackIndex < runtime->scopeStackCount;){
		int& scopeIndex = runtime->scopeStack[stackIndex];
		if(scopeIndex == index){
			for(int j = stackIndex; j + 1 < runtime->scopeStackCount; ++j){
				runtime->scopeStack[j] = runtime->scopeStack[j + 1];
			}
			--runtime->scopeStackCount;
			continue;
		}
		if(scopeIndex > index) --scopeIndex;
		++stackIndex;
	}

	for(int i = index; i + 1 < runtime->scopeCount; ++i){
		runtime->scopes[i] = runtime->scopes[i + 1];
	}
	runtime->scopes[runtime->scopeCount - 1] = {};
	--runtime->scopeCount;
}

UiFocusableState ui_focusable(const UiFocusableDesc& desc) {
	UiFocusRuntime * runtime = g_current;
	UiFocusScope * scope = ActiveScopeForDeclaration(runtime);
	UiFocusableState state = {};
	state.id = desc.id;
	state.disabled = desc.disabled;
	state.hovered = PointerOver(desc.id);

	if(!runtime || !scope || desc.id.id == 0) return state;
	if(scope->pendingCount >= runtime->limits.maxFocusablesPerScope){
		ReportError(runtime, "focusable overflow");
		return state;
	}

	scope->pending[scope->pendingCount++] = {
		desc.id,
		desc.disabled,
		desc.nav,
		desc.onConfirm,
		desc.onFocus,
	};

	state.focused = SameId(scope->focusedId, desc.id);
	state.focusVisible = state.focused &&
		(scope->source == UiFocusSource::Keyboard ||
		 scope->source == UiFocusSource::Gamepad ||
		 scope->source == UiFocusSource::Programmatic);
	state.pressed = runtime->pointerDown &&
		!desc.disabled &&
		state.hovered &&
		SameId(scope->pointerPressOrigin, desc.id);
	return state;
}

void ui_focus_end_layout(const UiFocusInputFrame& input) {
	UiFocusRuntime * runtime = g_current;
	if(!runtime) return;

	UiFocusScope * activeBeforeHarvest = ActiveDeclaredScope(runtime, runtime->frame);
	Clay_ElementId confirmTarget = activeBeforeHarvest
		? activeBeforeHarvest->focusedId
		: Clay_ElementId{};

	for(int i = 0; i < runtime->scopeCount; ++i){
		UiFocusScope * scope = &runtime->scopes[i];
		if(scope->declaredFrame != runtime->frame) continue;

		Clay_ElementId previousFocus = scope->focusedId;
		HarvestScopeLayout(runtime, scope);

		if(ContainsEnabled(scope, scope->requestedFocus)){
			scope->focusedId = scope->requestedFocus;
			scope->source = UiFocusSource::Programmatic;
			scope->autoFocusSuppressed = false;
		}else if(!ContainsEnabled(scope, scope->focusedId)){
			Clay_ElementId next = {};
			if(ContainsEnabled(scope, scope->requestedInitialFocus)){
				next = scope->requestedInitialFocus;
				scope->autoFocusSuppressed = false;
			}else if(!scope->pointerClearedFocus && !scope->autoFocusSuppressed){
				next = FirstEnabled(scope);
			}
			scope->focusedId = next;
			scope->source = next.id != 0
				? UiFocusSource::Programmatic
				: UiFocusSource::None;
		}
		scope->pointerClearedFocus = false;

		if(!SameId(previousFocus, scope->focusedId) && scope->focusedId.id != 0){
			InvokeFocusCallbackIfRegistered(scope, scope->focusedId);
		}else if(SameId(runtime->pendingFocusCallbackId, scope->focusedId)){
			InvokeFocusCallbackIfRegistered(scope, scope->focusedId);
		}

		scope->requestedInitialFocus = {};
		scope->requestedFocus = {};
	}

	UiFocusScope * active = ActiveDeclaredScope(runtime, runtime->frame);
	bool confirmDispatched = false;
	if(active && input.confirmPressed && ContainsEnabled(active, confirmTarget)){
		const UiFocusableRegistration * registration =
			FindRegistration(active, confirmTarget);
		if(registration && !registration->disabled && registration->onConfirm){
			registration->onConfirm();
			confirmDispatched = true;
		}
	}
	if(active &&
	   runtime->pendingPointerConfirmId.id != 0 &&
	   (!confirmDispatched || !SameId(runtime->pendingPointerConfirmId, confirmTarget)) &&
	   ContainsEnabled(active, runtime->pendingPointerConfirmId)){
		const UiFocusableRegistration * registration =
			FindRegistration(active, runtime->pendingPointerConfirmId);
		if(registration && !registration->disabled && registration->onConfirm){
			registration->onConfirm();
		}
	}

	for(int i = 0; i < runtime->scopeCount; ++i){
		UiFocusScope * scope = &runtime->scopes[i];
		if(scope->declaredFrame != runtime->frame) continue;
		ClearPendingRegistrations(scope);
	}
}

Clay_ElementId ui_focus_focused_id_for_scope(Clay_ElementId scopeId) {
	UiFocusScope * scope = FindScope(g_current, scopeId);
	return scope ? scope->focusedId : Clay_ElementId{};
}

Clay_ElementId ui_focus_focused_id() {
	UiFocusRuntime * runtime = g_current;
	UiFocusScope * scope = ActiveDeclaredScope(runtime, runtime ? runtime->frame : 0);
	return scope ? scope->focusedId : Clay_ElementId{};
}

UiFocusSource ui_focus_source() {
	UiFocusRuntime * runtime = g_current;
	UiFocusScope * scope = ActiveDeclaredScope(runtime, runtime ? runtime->frame : 0);
	return scope ? scope->source : UiFocusSource::None;
}

UiFocusSource ui_focus_source_for_scope(Clay_ElementId scopeId) {
	UiFocusScope * scope = FindScope(g_current, scopeId);
	return scope ? scope->source : UiFocusSource::None;
}

int ui_focus_error_count() {
	return g_current ? g_current->errorCount : 0;
}

}  // namespace ui
}  // namespace silencer
