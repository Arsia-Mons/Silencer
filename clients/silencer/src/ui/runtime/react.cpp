#include "react.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Fixed capacities. Bump if needed; "incredibly simple" hello-world doesn't
// need much. 256: the dense screens (Options·Controls' 30-action keybind table)
// plus the stacked overlays below them (main menu + options root) exceed 128
// component instances. Each Fiber is ~420B, so 256 costs ~54KB extra — cheap.
#define REACT_MAX_FIBERS 256
#define REACT_HOOKS_PER_FIBER 8
#define REACT_MAX_EFFECT_QUEUE 64
#define REACT_MAX_RENDER_DEPTH 128

enum HookKind : uint8_t {
    HOOK_NONE = 0,
    HOOK_STATE = 1,
    HOOK_EFFECT = 2,
    HOOK_REF = 3,
    HOOK_GENERIC_STATE = 4,
    HOOK_CALLBACK = 5,
    HOOK_TEXT_STORAGE = 6,
};

struct StateData {
    int value;
};

struct EffectData {
    // What use_effect just scheduled, awaiting next flush.
    ReactEffectFn pending_fn;
    ReactCleanupFn pending_cleanup;
    void *pending_user;
    bool has_pending;

    // The cleanup paired with the most recently executed effect, waiting
    // to be invoked on the next deps-change or on unmount.
    ReactCleanupFn active_cleanup;
    void *active_user;
    bool has_active;

    uint64_t deps_hash;
};

struct RefData {
    void *current;
};

struct GenericStateData {
    void *storage;
    ReactSlotDestructor destructor;
    uint32_t size;
};

struct CallbackData {
    void *storage;
    ReactSlotDestructor destructor;
    uint32_t size;
    uint64_t deps_hash;
    bool has_value;
};

struct TextStorageData {
    char *buffer;
};

struct HookSlot {
    HookKind kind;
    union {
        StateData state;
        EffectData effect;
        RefData ref;
        GenericStateData generic;
        CallbackData callback;
        TextStorageData text;
    } u;
};

struct Fiber {
    ReactFiberId id;     // 0 = dead/free slot
    int32_t next_index;  // hash collision chain
    uint32_t generation; // last frame seen
    HookSlot slots[REACT_HOOKS_PER_FIBER];
    int32_t slot_count; // hook count from the previous render; -1 on mount
    int32_t render_slot_count;
    uint32_t next_child_index;
};

struct EffectQueueEntry {
    ReactFiberId fiber_id;
    int32_t slot_index;
};

struct RenderFrame {
    Fiber *current;
    int32_t hook_index;
};

static struct {
    Fiber fibers[REACT_MAX_FIBERS];
    int32_t fiber_count;
    int32_t buckets[REACT_MAX_FIBERS]; // hash%cap -> fiber index, or -1
    EffectQueueEntry effect_queue[REACT_MAX_EFFECT_QUEUE];
    int32_t effect_queue_count;
    RenderFrame render_stack[REACT_MAX_RENDER_DEPTH];
    int32_t render_stack_count;

    Fiber *current;
    int32_t hook_index;
    uint32_t frame; // our own per-frame generation counter
    uint32_t root_child_index;
    int32_t error_count;
} G;

static void run_active_cleanup(HookSlot *s);
static void destroy_slot_storage(HookSlot *s);

void react_report_error(const char *fmt, ...) {
    G.error_count++;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void react_init_runtime(void) {
    if (G.fiber_count > 0) {
        react_shutdown();
    }
    memset(&G, 0, sizeof(G));
    for (int i = 0; i < REACT_MAX_FIBERS; i++)
        G.buckets[i] = -1;
}

int react_error_count(void) { return G.error_count; }

static Fiber *fiber_lookup(ReactFiberId id) {
    if (id == 0)
        return nullptr;
    uint32_t bucket = (uint32_t)(id % REACT_MAX_FIBERS);
    int32_t idx = G.buckets[bucket];
    while (idx >= 0) {
        Fiber *f = &G.fibers[idx];
        if (f->id == id)
            return f;
        idx = f->next_index;
    }
    return nullptr;
}

static void fiber_link_to_bucket(int32_t idx) {
    Fiber *f = &G.fibers[idx];
    uint32_t bucket = (uint32_t)(f->id % REACT_MAX_FIBERS);
    f->next_index = G.buckets[bucket];
    G.buckets[bucket] = idx;
}

static void fiber_unlink_from_bucket(int32_t idx) {
    Fiber *f = &G.fibers[idx];
    if (f->id == 0)
        return;

    uint32_t bucket = (uint32_t)(f->id % REACT_MAX_FIBERS);
    int32_t prev = -1;
    int32_t cur = G.buckets[bucket];
    while (cur >= 0) {
        Fiber *entry = &G.fibers[cur];
        if (cur == idx) {
            if (prev >= 0) {
                G.fibers[prev].next_index = entry->next_index;
            } else {
                G.buckets[bucket] = entry->next_index;
            }
            entry->next_index = -1;
            return;
        }
        prev = cur;
        cur = entry->next_index;
    }
}

static Fiber *fiber_create(ReactFiberId id) {
    int32_t idx = -1;
    for (int32_t i = 0; i < G.fiber_count; i++) {
        if (G.fibers[i].id == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (G.fiber_count >= REACT_MAX_FIBERS) {
            react_report_error("react: out of fibers (max=%d)\n",
                               REACT_MAX_FIBERS);
            return nullptr;
        }
        idx = G.fiber_count++;
    }

    Fiber *f = &G.fibers[idx];
    *f = {};
    f->id = id;
    f->slot_count = -1;
    fiber_link_to_bucket(idx);
    return f;
}

static void fiber_destroy(int32_t idx) {
    Fiber *f = &G.fibers[idx];
    if (f->id == 0)
        return;

    for (int j = 0; j < REACT_HOOKS_PER_FIBER; j++) {
        run_active_cleanup(&f->slots[j]);
        destroy_slot_storage(&f->slots[j]);
    }
    fiber_unlink_from_bucket(idx);
    *f = {};
}

void react_shutdown(void) {
    for (int32_t i = 0; i < G.fiber_count; i++) {
        fiber_destroy(i);
    }
    G.effect_queue_count = 0;
    G.render_stack_count = 0;
    G.current = nullptr;
    G.hook_index = 0;
    G.root_child_index = 0;
}

void react_enter(ReactFiberId fiber_id) {
    if (G.render_stack_count >= REACT_MAX_RENDER_DEPTH) {
        react_report_error("react: render stack overflow (max=%d)\n",
                           REACT_MAX_RENDER_DEPTH);
        G.current = nullptr;
        G.hook_index = 0;
        return;
    }

    G.render_stack[G.render_stack_count++] = {G.current, G.hook_index};

    if (fiber_id == 0) {
        react_report_error("react: component entered with id=0\n");
        G.current = nullptr;
        G.hook_index = 0;
        return;
    }

    Fiber *f = fiber_lookup(fiber_id);
    if (!f)
        f = fiber_create(fiber_id);
    if (!f) {
        G.current = nullptr;
        G.hook_index = 0;
        return;
    }
    f->generation = G.frame;
    f->render_slot_count = 0;
    f->next_child_index = 0;
    G.current = f;
    G.hook_index = 0;
}

uint32_t react_next_child_index(void) {
    if (!G.current)
        return G.root_child_index++;
    return G.current->next_child_index++;
}

ReactFiberId react_current_fiber_id(void) {
    return G.current ? G.current->id : 0;
}

static ReactFiberId hash_bytes(ReactFiberId hash, const char *text) {
    const unsigned char *cursor =
        reinterpret_cast<const unsigned char *>(text ? text : "");
    while (*cursor) {
        hash ^= *cursor++;
        hash *= 1099511628211ull;
    }
    return hash;
}

static ReactFiberId mix_fiber_id(ReactFiberId hash, ReactFiberId value) {
    hash ^= value;
    hash *= 1099511628211ull;
    hash ^= value >> 32u;
    hash *= 1099511628211ull;
    return hash;
}

ReactFiberId react_make_instance_fiber_id(const char *name, uint32_t index,
                                          bool keyed) {
    ReactFiberId parent_id = G.current ? G.current->id : 0xCBF29CE484222325ull;
    ReactFiberId hash = 1469598103934665603ull;
    hash = mix_fiber_id(hash, parent_id);
    hash = hash_bytes(hash, name);
    hash = mix_fiber_id(hash,
                        keyed ? 0x9E3779B97F4A7C15ull : 0x85EBCA6B27D4EB2Full);
    hash = mix_fiber_id(hash, index);
    return hash == 0 ? 1ull : hash;
}

ReactFiberId react_make_instance_fiber_key_id(const char *name,
                                              const char *key) {
    ReactFiberId parent_id = G.current ? G.current->id : 0xCBF29CE484222325ull;
    ReactFiberId hash = 1469598103934665603ull;
    hash = mix_fiber_id(hash, parent_id);
    hash = hash_bytes(hash, name);
    hash = mix_fiber_id(hash, 0x9E3779B97F4A7C15ull);
    hash = hash_bytes(hash, key);
    return hash == 0 ? 1ull : hash;
}

void react_leave(void) {
    if (G.render_stack_count <= 0) {
        react_report_error("react: leave without matching enter\n");
        G.current = nullptr;
        G.hook_index = 0;
        return;
    }

    Fiber *leaving = G.current;
    if (leaving) {
        if (leaving->slot_count >= 0 &&
            leaving->slot_count != leaving->render_slot_count) {
            react_report_error(
                "react: hook count changed on fiber %llu (was=%d now=%d)\n",
                (unsigned long long)leaving->id, leaving->slot_count,
                leaving->render_slot_count);
        }
        leaving->slot_count = leaving->render_slot_count;
    }

    RenderFrame previous = G.render_stack[--G.render_stack_count];
    G.current = previous.current;
    G.hook_index = previous.hook_index;
}

static const char *hook_kind_name(HookKind kind) {
    switch (kind) {
    case HOOK_NONE:
        return "none";
    case HOOK_STATE:
        return "state";
    case HOOK_EFFECT:
        return "effect";
    case HOOK_REF:
        return "ref";
    case HOOK_GENERIC_STATE:
        return "generic_state";
    case HOOK_CALLBACK:
        return "callback";
    case HOOK_TEXT_STORAGE:
        return "text_storage";
    }
    return "unknown";
}

static HookSlot *take_slot(HookKind expected, int32_t *index_out) {
    if (!G.current)
        return nullptr;
    int i = G.hook_index++;
    if (i + 1 > G.current->render_slot_count)
        G.current->render_slot_count = i + 1;
    if (i >= REACT_HOOKS_PER_FIBER) {
        react_report_error("react: hook overflow on fiber %llu (max=%d)\n",
                           (unsigned long long)G.current->id,
                           REACT_HOOKS_PER_FIBER);
        return nullptr;
    }
    if (index_out)
        *index_out = i;
    HookSlot *slot = &G.current->slots[i];
    if (slot->kind != HOOK_NONE && slot->kind != expected) {
        react_report_error(
            "react: hook kind changed on fiber %llu slot %d (was=%s now=%s)\n",
            (unsigned long long)G.current->id, i, hook_kind_name(slot->kind),
            hook_kind_name(expected));
        return nullptr;
    }
    return slot;
}

// --- Hooks ---

int *use_state_int(int initial) {
    HookSlot *s = take_slot(HOOK_STATE, nullptr);
    if (!s) {
        static int sink = 0;
        sink = initial;
        return &sink;
    }
    if (s->kind == HOOK_NONE) {
        s->kind = HOOK_STATE;
        s->u.state.value = initial;
    }
    return &s->u.state.value;
}

void use_effect(ReactEffectFn fn, ReactCleanupFn cleanup, void *user,
                uint64_t deps_hash) {
    int32_t idx;
    HookSlot *s = take_slot(HOOK_EFFECT, &idx);
    if (!s)
        return;

    bool first = (s->kind == HOOK_NONE);
    if (first) {
        s->kind = HOOK_EFFECT;
        s->u.effect = {};
        s->u.effect.deps_hash = ~deps_hash; // force first-run mismatch
    }
    EffectData *e = &s->u.effect;

    if (first || e->deps_hash != deps_hash) {
        e->pending_fn = fn;
        e->pending_cleanup = cleanup;
        e->pending_user = user;
        e->has_pending = true;
        e->deps_hash = deps_hash;
        if (G.effect_queue_count < REACT_MAX_EFFECT_QUEUE) {
            G.effect_queue[G.effect_queue_count++] = {G.current->id, idx};
        } else {
            react_report_error("react: effect queue full\n");
        }
    }
}

void **use_ref(void *initial) {
    HookSlot *s = take_slot(HOOK_REF, nullptr);
    if (!s) {
        static void *sink = nullptr;
        sink = initial;
        return &sink;
    }
    if (s->kind == HOOK_NONE) {
        s->kind = HOOK_REF;
        s->u.ref.current = initial;
    }
    return &s->u.ref.current;
}

// --- Generic state, callback, and text-storage backing ---
//
// All three use a heap-allocated payload owned by the slot. malloc returns
// memory aligned for std::max_align_t (typically 16 bytes), which is enough
// for every type we currently expect to store. Anything larger is rejected.

static void *react_aligned_alloc(uint32_t size, uint32_t align) {
    if (align > alignof(max_align_t)) {
        react_report_error(
            "react: slot alignment %u exceeds max_align_t (%zu)\n",
            (unsigned)align, (size_t)alignof(max_align_t));
        return nullptr;
    }
    if (size == 0)
        size = 1;
    return malloc(size);
}

static void destroy_slot_storage(HookSlot *s) {
    switch (s->kind) {
    case HOOK_GENERIC_STATE: {
        GenericStateData &g = s->u.generic;
        if (g.storage) {
            if (g.destructor)
                g.destructor(g.storage);
            free(g.storage);
            g.storage = nullptr;
        }
        break;
    }
    case HOOK_CALLBACK: {
        CallbackData &c = s->u.callback;
        if (c.storage) {
            if (c.has_value && c.destructor)
                c.destructor(c.storage);
            free(c.storage);
            c.storage = nullptr;
            c.has_value = false;
        }
        break;
    }
    case HOOK_TEXT_STORAGE: {
        TextStorageData &t = s->u.text;
        if (t.buffer) {
            free(t.buffer);
            t.buffer = nullptr;
        }
        break;
    }
    default:
        break;
    }
}

void *react_use_generic_state_slot(uint32_t size, uint32_t align,
                                   ReactSlotDestructor destructor,
                                   bool *is_new_slot) {
    if (is_new_slot)
        *is_new_slot = false;
    HookSlot *s = take_slot(HOOK_GENERIC_STATE, nullptr);
    if (!s)
        return nullptr;
    if (s->kind == HOOK_NONE) {
        s->kind = HOOK_GENERIC_STATE;
        s->u.generic = {};
        void *storage = react_aligned_alloc(size, align);
        if (!storage) {
            // Leave the slot empty so subsequent frames may retry.
            s->kind = HOOK_NONE;
            return nullptr;
        }
        s->u.generic.storage = storage;
        s->u.generic.destructor = destructor;
        s->u.generic.size = size;
        if (is_new_slot)
            *is_new_slot = true;
    } else if (s->u.generic.size != size) {
        // Same call-site swapped T behind use_state — diagnose like hook drift.
        react_report_error(
            "react: use_state size changed on fiber %llu (was=%u now=%u)\n",
            (unsigned long long)(G.current ? G.current->id : 0u),
            (unsigned)s->u.generic.size, (unsigned)size);
        return nullptr;
    }
    return s->u.generic.storage;
}

void *react_use_callback_slot(uint32_t size, uint32_t align,
                              ReactSlotDestructor destructor,
                              uint64_t deps_hash, bool *is_stale) {
    if (is_stale)
        *is_stale = false;
    HookSlot *s = take_slot(HOOK_CALLBACK, nullptr);
    if (!s)
        return nullptr;
    if (s->kind == HOOK_NONE) {
        s->kind = HOOK_CALLBACK;
        s->u.callback = {};
        void *storage = react_aligned_alloc(size, align);
        if (!storage) {
            s->kind = HOOK_NONE;
            return nullptr;
        }
        s->u.callback.storage = storage;
        s->u.callback.destructor = destructor;
        s->u.callback.size = size;
        s->u.callback.deps_hash = deps_hash;
        s->u.callback.has_value = false;
        if (is_stale)
            *is_stale = true;
    } else if (s->u.callback.size != size) {
        react_report_error(
            "react: use_callback size changed on fiber %llu (was=%u now=%u)\n",
            (unsigned long long)(G.current ? G.current->id : 0u),
            (unsigned)s->u.callback.size, (unsigned)size);
        return nullptr;
    } else if (!s->u.callback.has_value ||
               s->u.callback.deps_hash != deps_hash) {
        // Destroy the previous function in place before the caller
        // reconstructs.
        if (s->u.callback.has_value && s->u.callback.destructor) {
            s->u.callback.destructor(s->u.callback.storage);
        }
        s->u.callback.deps_hash = deps_hash;
        s->u.callback.has_value = false;
        if (is_stale)
            *is_stale = true;
    }
    // Caller will placement-new into storage when is_stale is true; mark as
    // populated so we destroy it on next stale-rebuild or fiber teardown.
    if (is_stale && *is_stale) {
        s->u.callback.has_value = true;
    }
    return s->u.callback.storage;
}

const char *use_text_storage_v(const char *fmt, va_list args) {
    HookSlot *s = take_slot(HOOK_TEXT_STORAGE, nullptr);
    if (!s) {
        // Sink path: format into a thread-local buffer. Mirrors use_state_int's
        // graceful-degrade behavior so callers can always feed a valid char*.
        static thread_local char sink[REACT_TEXT_STORAGE_CAP];
        vsnprintf(sink, REACT_TEXT_STORAGE_CAP, fmt, args);
        return sink;
    }
    if (s->kind == HOOK_NONE) {
        s->kind = HOOK_TEXT_STORAGE;
        s->u.text.buffer = (char *)malloc(REACT_TEXT_STORAGE_CAP);
        if (!s->u.text.buffer) {
            s->kind = HOOK_NONE;
            static thread_local char sink[REACT_TEXT_STORAGE_CAP];
            vsnprintf(sink, REACT_TEXT_STORAGE_CAP, fmt, args);
            return sink;
        }
    }
    vsnprintf(s->u.text.buffer, REACT_TEXT_STORAGE_CAP, fmt, args);
    return s->u.text.buffer;
}

const char *use_text_storage(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const char *result = use_text_storage_v(fmt, args);
    va_end(args);
    return result;
}

// --- Context ---

void react_provider_push(ReactContext *ctx, void *value) {
    if (ctx->depth < REACT_CONTEXT_MAX_DEPTH) {
        ctx->stack[ctx->depth++] = ctx->current;
    }
    ctx->current = value;
}

void react_provider_pop(ReactContext *ctx) {
    if (ctx->depth > 0) {
        ctx->current = ctx->stack[--ctx->depth];
    } else {
        ctx->current = nullptr;
    }
}

void *use_context(ReactContext *ctx) {
    // useContext doesn't consume a hook slot in this simple impl; it's a pure
    // read of the descent-stack. (No selective rerender, so no subscription.)
    return ctx->current;
}

// --- Frame boundaries ---

void react_begin_frame(void) {
    G.frame++;
    G.effect_queue_count = 0;
    G.render_stack_count = 0;
    G.current = nullptr;
    G.hook_index = 0;
    G.root_child_index = 0;
}

static void run_active_cleanup(HookSlot *s) {
    if (s->kind == HOOK_EFFECT && s->u.effect.has_active &&
        s->u.effect.active_cleanup) {
        s->u.effect.active_cleanup(s->u.effect.active_user);
        s->u.effect.has_active = false;
    }
}

void react_end_frame(void) {
    uint32_t current_gen = G.frame;

    // 1. Unmount sweep: any fiber not seen this frame is dead.
    for (int i = 0; i < G.fiber_count; i++) {
        Fiber *f = &G.fibers[i];
        if (f->id == 0)
            continue;
        if (f->generation != current_gen) {
            fiber_destroy(i);
        }
    }

    // 2. Flush queued effects: prior active cleanup, then new effect.
    //    Uses pending_* for what just got scheduled, active_* for what was
    //    previously running. Crucially: active cleanup uses the *old* user,
    //    not whatever use_effect was just called with.
    for (int i = 0; i < G.effect_queue_count; i++) {
        EffectQueueEntry &q = G.effect_queue[i];
        Fiber *f = fiber_lookup(q.fiber_id);
        if (!f)
            continue; // fiber unmounted in same frame
        HookSlot *s = &f->slots[q.slot_index];
        if (s->kind != HOOK_EFFECT)
            continue;
        EffectData *e = &s->u.effect;
        if (!e->has_pending)
            continue;

        if (e->has_active && e->active_cleanup) {
            e->active_cleanup(e->active_user);
        }
        if (e->pending_fn) {
            e->pending_fn(e->pending_user);
        }
        e->active_cleanup = e->pending_cleanup;
        e->active_user = e->pending_user;
        e->has_active = true;
        e->has_pending = false;
    }
    G.effect_queue_count = 0;
}
