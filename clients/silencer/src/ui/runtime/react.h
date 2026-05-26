// React-style hook runtime on top of Clay.
//
// Fiber identity comes from Clay's parent-hashed element IDs plus a sibling
// position. Use REACT_COMPONENT_BEGIN_KEY for reorderable same-type siblings.
// Hook state lives in a side table keyed by that ID. Effects run after
// Clay_EndLayout. Unmount detection uses the runtime's frame generation.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "clay/clay.h"

#ifdef __cplusplus
extern "C" {
#endif

void react_init(Clay_Context* clay_ctx);
void react_begin_frame(void);
void react_end_frame(void);
void react_shutdown(void);
int react_error_count(void);

void react_enter(uint32_t fiber_id);
void react_leave(void);
uint32_t react_next_child_index(void);
Clay_ElementId react_make_instance_id(Clay_String name,
                                      uint32_t index,
                                      bool keyed);

#define REACT_INSTANCE_ID(name_literal) \
	react_make_instance_id(CLAY_STRING(name_literal), react_next_child_index(), false)

#define REACT_INSTANCE_ID_KEY(name_literal, key_index) \
	react_make_instance_id(CLAY_STRING(name_literal), \
		((void)react_next_child_index(), (uint32_t)(key_index)), true)

#define REACT_COMPONENT_BEGIN(name_literal) \
	{ \
		Clay_ElementId _react_cid = REACT_INSTANCE_ID(name_literal); \
		react_enter(_react_cid.id); \
		CLAY({ .id = _react_cid })

#define REACT_COMPONENT_BEGIN_KEY(name_literal, key_index) \
	{ \
		Clay_ElementId _react_cid = REACT_INSTANCE_ID_KEY(name_literal, key_index); \
		react_enter(_react_cid.id); \
		CLAY({ .id = _react_cid })

#define REACT_COMPONENT_END() \
		react_leave(); \
	}

#define REACT_PROVIDER_ENTER(name_literal) \
	react_enter(REACT_INSTANCE_ID(name_literal).id)

#define REACT_PROVIDER_ENTER_KEY(name_literal, key_index) \
	react_enter(REACT_INSTANCE_ID_KEY(name_literal, key_index).id)

#define REACT_PROVIDER_EXIT() react_leave()

typedef struct ReactNoProps {
	uint8_t unused;
} ReactNoProps;

#ifdef __cplusplus
#define REACT_NO_PROPS ReactNoProps{}
#else
#define REACT_NO_PROPS ((ReactNoProps){0})
#endif

int* use_state_int(int initial);

typedef void (*ReactEffectFn)(void* user);
typedef void (*ReactCleanupFn)(void* user);

void use_effect(ReactEffectFn fn,
                ReactCleanupFn cleanup,
                void* user,
                uint64_t deps_hash);

void** use_ref(void* initial);

#define REACT_CONTEXT_MAX_DEPTH 16

typedef struct ReactContext {
	void* current;
	void* stack[REACT_CONTEXT_MAX_DEPTH];
	int depth;
} ReactContext;

void react_provider_push(ReactContext* ctx, void* value);
void react_provider_pop(ReactContext* ctx);
void* use_context(ReactContext* ctx);

#define PROVIDE(ctx_ptr, value) \
	for(int _react_once = (react_provider_push((ctx_ptr), (value)), 0); \
	    !_react_once; \
	    _react_once = 1, react_provider_pop((ctx_ptr)))

#ifdef __cplusplus
}
#endif
