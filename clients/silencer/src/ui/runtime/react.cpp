#include "runtime/react.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kMaxFibers = 128;
constexpr int kHooksPerFiber = 8;
constexpr int kMaxEffectQueue = 64;
constexpr int kMaxRenderDepth = 128;

enum HookKind : uint8_t {
	HOOK_NONE = 0,
	HOOK_STATE = 1,
	HOOK_EFFECT = 2,
	HOOK_REF = 3,
};

struct StateData {
	int value;
};

struct EffectData {
	ReactEffectFn pending_fn;
	ReactCleanupFn pending_cleanup;
	void* pending_user;
	bool has_pending;

	ReactCleanupFn active_cleanup;
	void* active_user;
	bool has_active;

	uint64_t deps_hash;
};

struct RefData {
	void* current;
};

struct HookSlot {
	HookKind kind;
	union {
		StateData state;
		EffectData effect;
		RefData ref;
	} u;
};

struct Fiber {
	uint32_t id;
	int32_t next_index;
	uint32_t generation;
	HookSlot slots[kHooksPerFiber];
	int32_t slot_count;
	int32_t render_slot_count;
	uint32_t next_child_index;
};

struct EffectQueueEntry {
	uint32_t fiber_id;
	int32_t slot_index;
};

struct RenderFrame {
	Fiber* current;
	int32_t hook_index;
};

struct ReactRuntimeState {
	Fiber fibers[kMaxFibers];
	int32_t fiber_count;
	int32_t buckets[kMaxFibers];
	EffectQueueEntry effect_queue[kMaxEffectQueue];
	int32_t effect_queue_count;
	RenderFrame render_stack[kMaxRenderDepth];
	int32_t render_stack_count;

	Fiber* current;
	int32_t hook_index;
	uint32_t frame;
	uint32_t root_child_index;
	int32_t error_count;
	bool initialized;
};

ReactRuntimeState G;

void run_active_cleanup(HookSlot* slot);

void report_error(const char* fmt, ...) {
	G.error_count++;
	va_list args;
	va_start(args, fmt);
	std::vfprintf(stderr, fmt, args);
	va_end(args);
}

Fiber* fiber_lookup(uint32_t id) {
	if(id == 0) return nullptr;
	uint32_t bucket = id % kMaxFibers;
	int32_t idx = G.buckets[bucket];
	while(idx >= 0){
		Fiber* fiber = &G.fibers[idx];
		if(fiber->id == id) return fiber;
		idx = fiber->next_index;
	}
	return nullptr;
}

void fiber_link_to_bucket(int32_t idx) {
	Fiber* fiber = &G.fibers[idx];
	uint32_t bucket = fiber->id % kMaxFibers;
	fiber->next_index = G.buckets[bucket];
	G.buckets[bucket] = idx;
}

void fiber_unlink_from_bucket(int32_t idx) {
	Fiber* fiber = &G.fibers[idx];
	if(fiber->id == 0) return;

	uint32_t bucket = fiber->id % kMaxFibers;
	int32_t prev = -1;
	int32_t cur = G.buckets[bucket];
	while(cur >= 0){
		Fiber* entry = &G.fibers[cur];
		if(cur == idx){
			if(prev >= 0){
				G.fibers[prev].next_index = entry->next_index;
			}else{
				G.buckets[bucket] = entry->next_index;
			}
			entry->next_index = -1;
			return;
		}
		prev = cur;
		cur = entry->next_index;
	}
}

Fiber* fiber_create(uint32_t id) {
	int32_t idx = -1;
	for(int32_t i = 0; i < G.fiber_count; i++){
		if(G.fibers[i].id == 0){
			idx = i;
			break;
		}
	}
	if(idx < 0){
		if(G.fiber_count >= kMaxFibers){
			report_error("react: out of fibers (max=%d)\n", kMaxFibers);
			return nullptr;
		}
		idx = G.fiber_count++;
	}

	Fiber* fiber = &G.fibers[idx];
	*fiber = {};
	fiber->id = id;
	fiber->slot_count = -1;
	fiber_link_to_bucket(idx);
	return fiber;
}

void fiber_destroy(int32_t idx) {
	Fiber* fiber = &G.fibers[idx];
	if(fiber->id == 0) return;

	for(int slot = 0; slot < kHooksPerFiber; slot++){
		run_active_cleanup(&fiber->slots[slot]);
	}
	fiber_unlink_from_bucket(idx);
	*fiber = {};
}

const char* hook_kind_name(HookKind kind) {
	switch(kind){
	case HOOK_NONE: return "none";
	case HOOK_STATE: return "state";
	case HOOK_EFFECT: return "effect";
	case HOOK_REF: return "ref";
	}
	return "unknown";
}

HookSlot* take_slot(HookKind expected, int32_t* index_out) {
	if(!G.current) return nullptr;
	int index = G.hook_index++;
	if(index + 1 > G.current->render_slot_count){
		G.current->render_slot_count = index + 1;
	}
	if(index >= kHooksPerFiber){
		report_error("react: hook overflow on fiber %u (max=%d)\n",
		             G.current->id,
		             kHooksPerFiber);
		return nullptr;
	}
	if(index_out) *index_out = index;
	HookSlot* slot = &G.current->slots[index];
	if(slot->kind != HOOK_NONE && slot->kind != expected){
		report_error("react: hook kind changed on fiber %u slot %d (was=%s now=%s)\n",
		             G.current->id,
		             index,
		             hook_kind_name(slot->kind),
		             hook_kind_name(expected));
		return nullptr;
	}
	return slot;
}

void run_active_cleanup(HookSlot* slot) {
	if(slot->kind == HOOK_EFFECT &&
	   slot->u.effect.has_active &&
	   slot->u.effect.active_cleanup){
		slot->u.effect.active_cleanup(slot->u.effect.active_user);
		slot->u.effect.has_active = false;
	}
}

}  // namespace

void react_init(Clay_Context* clay_ctx) {
	(void)clay_ctx;
	if(G.initialized){
		react_shutdown();
	}
	std::memset(&G, 0, sizeof(G));
	for(int i = 0; i < kMaxFibers; i++){
		G.buckets[i] = -1;
	}
	G.initialized = true;
}

int react_error_count(void) {
	return G.error_count;
}

void react_shutdown(void) {
	for(int32_t i = 0; i < G.fiber_count; i++){
		fiber_destroy(i);
	}
	G.effect_queue_count = 0;
	G.render_stack_count = 0;
	G.current = nullptr;
	G.hook_index = 0;
	G.root_child_index = 0;
}

void react_enter(uint32_t fiber_id) {
	if(G.render_stack_count >= kMaxRenderDepth){
		report_error("react: render stack overflow (max=%d)\n", kMaxRenderDepth);
		G.current = nullptr;
		G.hook_index = 0;
		return;
	}

	G.render_stack[G.render_stack_count++] = {G.current, G.hook_index};

	if(fiber_id == 0){
		report_error("react: component entered with id=0; add a Clay .id\n");
		G.current = nullptr;
		G.hook_index = 0;
		return;
	}

	Fiber* fiber = fiber_lookup(fiber_id);
	if(!fiber) fiber = fiber_create(fiber_id);
	if(!fiber){
		G.current = nullptr;
		G.hook_index = 0;
		return;
	}
	fiber->generation = G.frame;
	fiber->render_slot_count = 0;
	fiber->next_child_index = 0;
	G.current = fiber;
	G.hook_index = 0;
}

uint32_t react_next_child_index(void) {
	if(!G.current) return G.root_child_index++;
	return G.current->next_child_index++;
}

Clay_ElementId react_make_instance_id(Clay_String name,
                                      uint32_t index,
                                      bool keyed) {
	uint32_t parent_id = G.current ? G.current->id : 0x811C9DC5u;
	uint32_t seed = parent_id ^ (keyed ? 0x9E3779B9u : 0x85EBCA6Bu);
	return Clay__HashString(name, index, seed);
}

void react_leave(void) {
	if(G.render_stack_count <= 0){
		report_error("react: leave without matching enter\n");
		G.current = nullptr;
		G.hook_index = 0;
		return;
	}

	Fiber* leaving = G.current;
	if(leaving){
		if(leaving->slot_count >= 0 &&
		   leaving->slot_count != leaving->render_slot_count){
			report_error("react: hook count changed on fiber %u (was=%d now=%d)\n",
			             leaving->id,
			             leaving->slot_count,
			             leaving->render_slot_count);
		}
		leaving->slot_count = leaving->render_slot_count;
	}

	RenderFrame previous = G.render_stack[--G.render_stack_count];
	G.current = previous.current;
	G.hook_index = previous.hook_index;
}

int* use_state_int(int initial) {
	HookSlot* slot = take_slot(HOOK_STATE, nullptr);
	if(!slot){
		static int sink = 0;
		sink = initial;
		return &sink;
	}
	if(slot->kind == HOOK_NONE){
		slot->kind = HOOK_STATE;
		slot->u.state.value = initial;
	}
	return &slot->u.state.value;
}

void use_effect(ReactEffectFn fn,
                ReactCleanupFn cleanup,
                void* user,
                uint64_t deps_hash) {
	int32_t index = 0;
	HookSlot* slot = take_slot(HOOK_EFFECT, &index);
	if(!slot) return;

	bool first = (slot->kind == HOOK_NONE);
	if(first){
		slot->kind = HOOK_EFFECT;
		slot->u.effect = {};
		slot->u.effect.deps_hash = ~deps_hash;
	}
	EffectData* effect = &slot->u.effect;

	if(first || effect->deps_hash != deps_hash){
		if(G.effect_queue_count >= kMaxEffectQueue){
			report_error("react: effect queue full\n");
			return;
		}
		effect->pending_fn = fn;
		effect->pending_cleanup = cleanup;
		effect->pending_user = user;
		effect->has_pending = true;
		effect->deps_hash = deps_hash;
		G.effect_queue[G.effect_queue_count++] = {G.current->id, index};
	}
}

void** use_ref(void* initial) {
	HookSlot* slot = take_slot(HOOK_REF, nullptr);
	if(!slot){
		static void* sink = nullptr;
		sink = initial;
		return &sink;
	}
	if(slot->kind == HOOK_NONE){
		slot->kind = HOOK_REF;
		slot->u.ref.current = initial;
	}
	return &slot->u.ref.current;
}

void react_provider_push(ReactContext* ctx, void* value) {
	if(ctx->depth < REACT_CONTEXT_MAX_DEPTH){
		ctx->stack[ctx->depth++] = ctx->current;
		ctx->current = value;
		return;
	}
	ctx->overflowDepth++;
	report_error("react: provider stack overflow (max=%d)\n", REACT_CONTEXT_MAX_DEPTH);
}

void react_provider_pop(ReactContext* ctx) {
	if(ctx->overflowDepth > 0){
		ctx->overflowDepth--;
		return;
	}
	if(ctx->depth > 0){
		ctx->current = ctx->stack[--ctx->depth];
	}else{
		ctx->current = nullptr;
	}
}

void* use_context(ReactContext* ctx) {
	return ctx->current;
}

void react_begin_frame(void) {
	if(!G.initialized){
		react_init(Clay_GetCurrentContext());
	}
	G.frame++;
	G.effect_queue_count = 0;
	G.render_stack_count = 0;
	G.current = nullptr;
	G.hook_index = 0;
	G.root_child_index = 0;
}

void react_end_frame(void) {
	uint32_t current_gen = G.frame;
	for(int32_t i = 0; i < G.fiber_count; i++){
		Fiber* fiber = &G.fibers[i];
		if(fiber->id == 0) continue;
		if(fiber->generation != current_gen){
			fiber_destroy(i);
		}
	}

	for(int i = 0; i < G.effect_queue_count; i++){
		EffectQueueEntry& queued = G.effect_queue[i];
		Fiber* fiber = fiber_lookup(queued.fiber_id);
		if(!fiber) continue;
		HookSlot* slot = &fiber->slots[queued.slot_index];
		if(slot->kind != HOOK_EFFECT) continue;
		EffectData* effect = &slot->u.effect;
		if(!effect->has_pending) continue;

		if(effect->has_active && effect->active_cleanup){
			effect->active_cleanup(effect->active_user);
		}
		if(effect->pending_fn){
			effect->pending_fn(effect->pending_user);
		}
		effect->active_cleanup = effect->pending_cleanup;
		effect->active_user = effect->pending_user;
		effect->has_active = true;
		effect->has_pending = false;
	}
	G.effect_queue_count = 0;
}
