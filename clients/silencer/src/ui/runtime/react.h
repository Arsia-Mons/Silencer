// Minimal React-style hook runtime.
//
// Fiber identity comes from the runtime's parent fiber plus a sibling position
// or explicit key. Use REACT_COMPONENT_BEGIN_KEY for reorderable same-type
// siblings. Hook state lives in a side table keyed by that ID. Effects run
// after react_end_frame(). Unmount detection uses the runtime's frame
// generation.
//
// Public API:
//   react_init_runtime()           - initialize the hook runtime.
//   react_begin_frame()            - call once per frame, before component
//   tree. react_end_frame()        - call once per frame, after layout.
//   REACT_COMPONENT_BEGIN/END      - bracket a component body without emitting
//   a layout node. REACT_PROVIDER_ENTER/EXIT      - bracket a transparent
//   provider body. use_state_int(initial)         -
//   returns int* that persists across frames. use_effect(fn, cleanup, user,
//   deps_hash) - runs after commit when deps change. PROVIDE(ctx_ptr, value) {
//   ... } - pushes a context value for the body. use_context(ctx_ptr) - reads
//   current value of a context.
//
// Component shapes:
//   ui::UiElement Component(const ComponentProps &props);
//
// Components receive children as ordinary props data. Parent components create
// element descriptors for children; the retained reconciler later invokes
// component functions, owns fiber entry/exit, and walks child descriptors.

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
#include <functional>
#include <new>
#include <type_traits>
#include <utility>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t ReactFiberId;

void react_init_runtime(void);
void react_begin_frame(void);
void react_end_frame(void);
void react_shutdown(void);
int react_error_count(void);
#ifdef __GNUC__
void react_report_error(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
#else
void react_report_error(const char *fmt, ...);
#endif

// Internal: push/pop the "currently rendering fiber" + reset/restore hook
// index.
void react_enter(ReactFiberId fiber_id);
void react_leave(void);
// Id of the fiber currently rendering (0 if none). Lets a hook identify "which
// component am I" so per-component interaction state can be keyed by fiber.
ReactFiberId react_current_fiber_id(void);
uint32_t react_next_child_index(void);
ReactFiberId react_make_instance_fiber_id(const char *name, uint32_t index,
                                          bool keyed);
ReactFiberId react_make_instance_fiber_key_id(const char *name,
                                              const char *key);

#define REACT_INSTANCE_ID(name_literal)                                        \
    react_make_instance_fiber_id((name_literal), react_next_child_index(),     \
                                 false)

#define REACT_INSTANCE_ID_KEY(name_literal, key_index)                         \
    react_make_instance_fiber_id(                                              \
        (name_literal),                                                        \
        ((void)react_next_child_index(), (uint32_t)(key_index)), true)

#define REACT_COMPONENT_BEGIN(name_literal)                                    \
    {                                                                          \
        react_enter(REACT_INSTANCE_ID(name_literal));

#define REACT_COMPONENT_BEGIN_KEY(name_literal, key_index)                     \
    {                                                                          \
        react_enter(REACT_INSTANCE_ID_KEY(name_literal, key_index));

#define REACT_COMPONENT_END()                                                  \
    react_leave();                                                             \
    }

#define REACT_FRAGMENT_COMPONENT_BEGIN(name_literal)                           \
    {                                                                          \
        react_enter(REACT_INSTANCE_ID(name_literal));

#define REACT_FRAGMENT_COMPONENT_BEGIN_KEY(name_literal, key_index)            \
    {                                                                          \
        react_enter(REACT_INSTANCE_ID_KEY(name_literal, key_index));

#define REACT_FRAGMENT_COMPONENT_END()                                         \
    react_leave();                                                             \
    }

#define REACT_PROVIDER_ENTER(name_literal)                                     \
    react_enter(REACT_INSTANCE_ID(name_literal))

#define REACT_PROVIDER_ENTER_KEY(name_literal, key_index)                      \
    react_enter(REACT_INSTANCE_ID_KEY(name_literal, key_index))

#define REACT_PROVIDER_EXIT() react_leave()

typedef struct ReactNoProps {
    uint8_t unused;
} ReactNoProps;

#ifdef __cplusplus
#define REACT_NO_PROPS                                                         \
    ReactNoProps {}
#else
#define REACT_NO_PROPS ((ReactNoProps){0})
#endif

// --- Hooks ---

int *use_state_int(int initial);

typedef void (*ReactEffectFn)(void *user);
typedef void (*ReactCleanupFn)(void *user);

void use_effect(ReactEffectFn fn, ReactCleanupFn cleanup, void *user,
                uint64_t deps_hash);

// A slot holding a single pointer that persists across renders.
// Returns the address of the slot; the caller reads `*ref` and writes `*ref =
// ...`.
void **use_ref(void *initial);

// --- Generic state (raw byte slot + destructor) ---
//
// Backs the `use_state<T>` template below. Returns a stable pointer to the
// slot's storage (heap-allocated to honor `align`). `destructor` is invoked
// once when the owning fiber unmounts. `is_new_slot` is set to true on the
// frame the slot is first created so the caller can placement-new the value.

typedef void (*ReactSlotDestructor)(void *storage);

void *react_use_generic_state_slot(uint32_t size, uint32_t align,
                                   ReactSlotDestructor destructor,
                                   bool *is_new_slot);

// --- Callback memo slot ---
//
// Backs the `use_callback` template below. Returns a stable storage pointer
// large enough for a std::function<void()>. When `deps_hash` differs from the
// previous frame (or on first frame), the existing function is destroyed and
// `is_stale` is set so the caller can reconstruct it; otherwise `is_stale` is
// false and the caller reuses the existing function in place.

void *react_use_callback_slot(uint32_t size, uint32_t align,
                              ReactSlotDestructor destructor,
                              uint64_t deps_hash, bool *is_stale);

// --- Per-fiber printf scratch ---
//
// Returns a stable per-call-site buffer (192 bytes cap). Two instances of the
// same component at different fiber identities receive distinct buffers, so
// they don't clobber each other within a single frame. If the formatted text
// exceeds the cap, the result is truncated (snprintf semantics).

#define REACT_TEXT_STORAGE_CAP 192

#ifdef __GNUC__
const char *use_text_storage(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
#else
const char *use_text_storage(const char *fmt, ...);
#endif
const char *use_text_storage_v(const char *fmt, va_list args);

// --- Context / providers ---

#define REACT_CONTEXT_MAX_DEPTH 16

typedef struct ReactContext {
    void *current;
    void *stack[REACT_CONTEXT_MAX_DEPTH];
    int depth;
} ReactContext;

void react_provider_push(ReactContext *ctx, void *value);
void react_provider_pop(ReactContext *ctx);
void *use_context(ReactContext *ctx);

// Scoped provider via a single-iteration for-loop.
#define PROVIDE(ctx_ptr, value)                                                \
    for (int _react_once = (react_provider_push((ctx_ptr), (value)), 0);       \
         !_react_once; _react_once = 1, react_provider_pop((ctx_ptr)))

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

// --- use_state<T> ---
//
// Typed analogue of use_state_int. Returns a stable T* keyed by fiber identity
// + call-site index, or nullptr after reporting a runtime error when storage
// cannot be provided. `initial` is consumed only when the slot is first
// constructed; subsequent frames reuse the existing value. The destructor runs
// once when the owning fiber unmounts.

namespace react_detail {

template <typename T> void destroy_state_slot(void *storage) {
    static_cast<T *>(storage)->~T();
}

} // namespace react_detail

namespace react_detail {

template <typename T, typename Initial> T *use_state_impl(Initial &&initial) {
    bool is_new_slot = false;
    void *storage = react_use_generic_state_slot(
        (uint32_t)sizeof(T), (uint32_t)alignof(T),
        &react_detail::destroy_state_slot<T>, &is_new_slot);
    if (!storage) {
        react_report_error("react: use_state storage unavailable\n");
        return nullptr;
    }
    if (is_new_slot) {
        new (storage) T(std::forward<Initial>(initial));
    }
    return static_cast<T *>(storage);
}

} // namespace react_detail

template <typename T> T *use_state(const T &initial) {
    return react_detail::use_state_impl<T>(initial);
}

template <typename T> T *use_state(T &&initial) {
    return react_detail::use_state_impl<T>(std::move(initial));
}

// --- use_callback ---
//
// Returns a std::function that is stable across frames as long as `deps_hash`
// matches the previous call at this site. When the hash changes, the stored
// function is destroyed and rebuilt from `fn`.
//
// The returned reference is to per-fiber slot storage; callers typically copy
// it into retained handler fields.

namespace react_detail {

template <typename Fn> void destroy_callback_slot(void *storage) {
    static_cast<Fn *>(storage)->~Fn();
}

} // namespace react_detail

template <typename Signature, typename F>
std::function<Signature> &use_callback(F &&fn, uint64_t deps_hash) {
    using Fn = std::function<Signature>;
    bool is_stale = false;
    void *storage = react_use_callback_slot(
        (uint32_t)sizeof(Fn), (uint32_t)alignof(Fn),
        &react_detail::destroy_callback_slot<Fn>, deps_hash, &is_stale);
    if (!storage) {
        react_report_error("react: use_callback storage unavailable\n");
        static thread_local Fn sink;
        sink = Fn{};
        return sink;
    }
    if (is_stale) {
        new (storage) Fn(std::forward<F>(fn));
    }
    return *static_cast<Fn *>(storage);
}

template <typename F>
std::function<void()> &use_callback(F &&fn, uint64_t deps_hash) {
    return use_callback<void()>(std::forward<F>(fn), deps_hash);
}

#endif // __cplusplus
