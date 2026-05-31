#include "ui/runtime/react.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                    #expr);                                                    \
            return false;                                                      \
        }                                                                      \
    } while (0)

template <typename Build>
static void run_frame(Build build) {
    react_begin_frame();
    build();
    react_end_frame();
}

static int g_probe_values[2] = {};

static void PositionalStateProbe(int slot, int initial, int write_value) {
    REACT_COMPONENT_BEGIN("StateProbe") {
        int *value = use_state_int(initial);
        if (write_value >= 0) {
            *value = write_value;
        }
        g_probe_values[slot] = *value;
    }
    REACT_COMPONENT_END();
}

static void KeyedStateProbe(int key, int initial, int write_value) {
    REACT_COMPONENT_BEGIN_KEY("StateProbe", key) {
        int *value = use_state_int(initial);
        if (write_value >= 0) {
            *value = write_value;
        }
        g_probe_values[key] = *value;
    }
    REACT_COMPONENT_END();
}

static bool positional_siblings_keep_distinct_state(void) {
    react_init_runtime();
    g_probe_values[0] = 0;
    g_probe_values[1] = 0;

    run_frame([] {
        PositionalStateProbe(0, 1, 10);
        PositionalStateProbe(1, 2, 20);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_probe_values[0] == 10);
    CHECK(g_probe_values[1] == 20);

    run_frame([] {
        PositionalStateProbe(0, 99, -1);
        PositionalStateProbe(1, 99, -1);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_probe_values[0] == 10);
    CHECK(g_probe_values[1] == 20);
    return true;
}

static bool keyed_siblings_keep_state_across_reorder(void) {
    react_init_runtime();
    g_probe_values[0] = 0;
    g_probe_values[1] = 0;

    run_frame([] {
        KeyedStateProbe(0, 1, 10);
        KeyedStateProbe(1, 2, 20);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_probe_values[0] == 10);
    CHECK(g_probe_values[1] == 20);

    run_frame([] {
        KeyedStateProbe(1, 99, -1);
        KeyedStateProbe(0, 99, -1);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_probe_values[0] == 10);
    CHECK(g_probe_values[1] == 20);
    return true;
}

static void ComponentStateProbe(int key, int initial, int write_value) {
    REACT_COMPONENT_BEGIN_KEY("ComponentStateProbe", key) {
        int *value = use_state_int(initial);
        if (write_value >= 0) {
            *value = write_value;
        }
        g_probe_values[key] = *value;
    }
    REACT_COMPONENT_END();
}

static bool components_use_hooks_without_layout_backend(void) {
    react_init_runtime();
    g_probe_values[0] = 0;
    g_probe_values[1] = 0;

    react_begin_frame();
    ComponentStateProbe(0, 1, 10);
    ComponentStateProbe(1, 2, 20);
    react_end_frame();
    CHECK(react_error_count() == 0);
    CHECK(g_probe_values[0] == 10);
    CHECK(g_probe_values[1] == 20);

    react_begin_frame();
    ComponentStateProbe(1, 99, -1);
    ComponentStateProbe(0, 99, -1);
    react_end_frame();
    CHECK(react_error_count() == 0);
    CHECK(g_probe_values[0] == 10);
    CHECK(g_probe_values[1] == 20);
    return true;
}

static int g_provider_values[2] = {};
static int g_provider_child_values[2] = {};

static void PositionalProviderProbe(int slot, int initial, int write_value) {
    REACT_PROVIDER_ENTER("ProviderProbe");
    int *value = use_state_int(initial);
    if (write_value >= 0) {
        *value = write_value;
    }
    g_provider_values[slot] = *value;
    REACT_PROVIDER_EXIT();
}

static void KeyedProviderProbe(int key, int initial, int write_value) {
    REACT_PROVIDER_ENTER_KEY("ProviderProbe", key);
    int *value = use_state_int(initial);
    if (write_value >= 0) {
        *value = write_value;
    }
    g_provider_values[key] = *value;
    REACT_PROVIDER_EXIT();
}

static void ProviderChildProbe(int provider_slot, int initial,
                               int write_value) {
    REACT_COMPONENT_BEGIN("ProviderChildProbe") {
        int *value = use_state_int(initial);
        if (write_value >= 0) {
            *value = write_value;
        }
        g_provider_child_values[provider_slot] = *value;
    }
    REACT_COMPONENT_END();
}

static void ProviderWithChildProbe(int slot, int initial, int write_value) {
    REACT_PROVIDER_ENTER("ProviderWithChildProbe");
    ProviderChildProbe(slot, initial, write_value);
    REACT_PROVIDER_EXIT();
}

static bool transparent_providers_use_instance_identity(void) {
    react_init_runtime();
    g_provider_values[0] = 0;
    g_provider_values[1] = 0;

    run_frame([] {
        PositionalProviderProbe(0, 1, 10);
        PositionalProviderProbe(1, 2, 20);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_provider_values[0] == 10);
    CHECK(g_provider_values[1] == 20);

    run_frame([] {
        PositionalProviderProbe(0, 99, -1);
        PositionalProviderProbe(1, 99, -1);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_provider_values[0] == 10);
    CHECK(g_provider_values[1] == 20);

    react_init_runtime();
    g_provider_values[0] = 0;
    g_provider_values[1] = 0;

    run_frame([] {
        KeyedProviderProbe(0, 1, 10);
        KeyedProviderProbe(1, 2, 20);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_provider_values[0] == 10);
    CHECK(g_provider_values[1] == 20);

    run_frame([] {
        KeyedProviderProbe(1, 99, -1);
        KeyedProviderProbe(0, 99, -1);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_provider_values[0] == 10);
    CHECK(g_provider_values[1] == 20);

    react_init_runtime();
    g_provider_child_values[0] = 0;
    g_provider_child_values[1] = 0;

    run_frame([] {
        ProviderWithChildProbe(0, 1, 10);
        ProviderWithChildProbe(1, 2, 20);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_provider_child_values[0] == 10);
    CHECK(g_provider_child_values[1] == 20);

    run_frame([] {
        ProviderWithChildProbe(0, 99, -1);
        ProviderWithChildProbe(1, 99, -1);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_provider_child_values[0] == 10);
    CHECK(g_provider_child_values[1] == 20);
    return true;
}

static int g_scratch_cleanups = 0;
static const char *g_scratch_text[2] = {};

static void cleanup_text_scratch(void *user) {
    g_scratch_cleanups++;
    delete[] static_cast<char *>(user);
}

static void TextScratchProbe(int slot, int value) {
    REACT_COMPONENT_BEGIN("TextScratchProbe") {
        void **scratch_ref = use_ref(nullptr);
        if (!*scratch_ref) {
            *scratch_ref = new char[64];
        }
        char *scratch = static_cast<char *>(*scratch_ref);
        snprintf(scratch, 64, "Value:%d", value);
        use_effect(nullptr, cleanup_text_scratch, scratch, 0);
        g_scratch_text[slot] = scratch;
    }
    REACT_COMPONENT_END();
}

static bool text_scratch_is_instance_owned(void) {
    react_init_runtime();
    g_scratch_cleanups = 0;
    g_scratch_text[0] = nullptr;
    g_scratch_text[1] = nullptr;

    run_frame([] {
        TextScratchProbe(0, 10);
        TextScratchProbe(1, 20);
    });

    CHECK(react_error_count() == 0);
    CHECK(g_scratch_text[0] != nullptr);
    CHECK(g_scratch_text[1] != nullptr);
    CHECK(strcmp(g_scratch_text[0], "Value:10") == 0);
    CHECK(strcmp(g_scratch_text[1], "Value:20") == 0);
    CHECK(g_scratch_text[0] != g_scratch_text[1]);

    react_shutdown();
    CHECK(g_scratch_cleanups == 2);
    return true;
}

static int g_effect_mounts = 0;
static int g_effect_cleanups = 0;

static void on_mount(void *) { g_effect_mounts++; }

static void on_unmount(void *) { g_effect_cleanups++; }

static void EffectProbe(void) {
    REACT_COMPONENT_BEGIN("EffectProbe") {
        use_effect(on_mount, on_unmount, nullptr, 0);
    }
    REACT_COMPONENT_END();
}

static bool remounts_reuse_unmounted_fibers(void) {
    react_init_runtime();
    g_effect_mounts = 0;
    g_effect_cleanups = 0;

    for (int i = 0; i < 200; i++) {
        run_frame([] { EffectProbe(); });
        run_frame([] {});
    }

    CHECK(react_error_count() == 0);
    CHECK(g_effect_mounts == 200);
    CHECK(g_effect_cleanups == 200);
    return true;
}

static bool shutdown_cleans_live_effects(void) {
    react_init_runtime();
    g_effect_mounts = 0;
    g_effect_cleanups = 0;

    run_frame([] { EffectProbe(); });

    CHECK(react_error_count() == 0);
    CHECK(g_effect_mounts == 1);
    CHECK(g_effect_cleanups == 0);

    react_shutdown();
    CHECK(g_effect_cleanups == 1);

    react_shutdown();
    CHECK(g_effect_cleanups == 1);
    return true;
}

static bool g_use_ref_for_kind_probe = false;
static bool g_extra_hook_for_count_probe = false;

static void KindDriftProbe(void) {
    REACT_COMPONENT_BEGIN("KindDriftProbe") {
        if (g_use_ref_for_kind_probe) {
            (void)use_ref(nullptr);
        } else {
            (void)use_state_int(1);
        }
    }
    REACT_COMPONENT_END();
}

static void CountDriftProbe(void) {
    REACT_COMPONENT_BEGIN("CountDriftProbe") {
        (void)use_state_int(1);
        if (g_extra_hook_for_count_probe) {
            (void)use_state_int(2);
        }
    }
    REACT_COMPONENT_END();
}

static bool hook_drift_is_diagnosed(void) {
    react_init_runtime();
    g_use_ref_for_kind_probe = false;
    run_frame([] { KindDriftProbe(); });
    CHECK(react_error_count() == 0);

    g_use_ref_for_kind_probe = true;
    run_frame([] { KindDriftProbe(); });
    CHECK(react_error_count() == 1);

    react_init_runtime();
    g_extra_hook_for_count_probe = false;
    run_frame([] { CountDriftProbe(); });
    CHECK(react_error_count() == 0);

    g_extra_hook_for_count_probe = true;
    run_frame([] { CountDriftProbe(); });
    CHECK(react_error_count() == 1);
    return true;
}

// ---- use_state<T> ----

struct PodState {
    int a;
    float b;
    char tag;
};

static PodState *g_pod_ptr[2] = {};
static int g_pod_a[2] = {};
static float g_pod_b[2] = {};
static bool *g_bool_ptr[2] = {};
static bool g_bool_val[2] = {};

static void PodStateProbe(int slot, PodState initial, bool write,
                          PodState write_value) {
    REACT_COMPONENT_BEGIN_KEY("PodStateProbe", slot) {
        PodState *p = use_state<PodState>(initial);
        if (write)
            *p = write_value;
        g_pod_ptr[slot] = p;
        g_pod_a[slot] = p->a;
        g_pod_b[slot] = p->b;
    }
    REACT_COMPONENT_END();
}

static void BoolStateProbe(int slot, bool initial, bool write,
                           bool write_value) {
    REACT_COMPONENT_BEGIN_KEY("BoolStateProbe", slot) {
        bool *p = use_state<bool>(initial);
        if (write)
            *p = write_value;
        g_bool_ptr[slot] = p;
        g_bool_val[slot] = *p;
    }
    REACT_COMPONENT_END();
}

static bool use_state_template_persists_typed_values(void) {
    react_init_runtime();

    PodState init0 = {1, 1.5f, 'a'};
    PodState init1 = {2, 2.5f, 'b'};
    PodState updated = {99, 9.5f, 'z'};

    run_frame([&] {
        PodStateProbe(0, init0, true, updated);
        PodStateProbe(1, init1, false, init1);
        BoolStateProbe(0, false, true, true);
        BoolStateProbe(1, true, false, true);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_pod_a[0] == 99);
    CHECK(g_pod_b[0] == 9.5f);
    CHECK(g_pod_a[1] == 2);
    CHECK(g_pod_b[1] == 2.5f);
    CHECK(g_bool_val[0] == true);
    CHECK(g_bool_val[1] == true);

    PodState *first_pod_ptr_0 = g_pod_ptr[0];
    PodState *first_pod_ptr_1 = g_pod_ptr[1];
    bool *first_bool_ptr_0 = g_bool_ptr[0];

    // Second frame: initial values are ignored; pointer identity preserved.
    PodState noop = {0, 0.0f, '?'};
    run_frame([&] {
        PodStateProbe(0, noop, false, noop);
        PodStateProbe(1, noop, false, noop);
        BoolStateProbe(0, false, false, false);
        BoolStateProbe(1, false, false, false);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_pod_a[0] == 99);
    CHECK(g_pod_b[0] == 9.5f);
    CHECK(g_pod_a[1] == 2);
    CHECK(g_pod_b[1] == 2.5f);
    CHECK(g_bool_val[0] == true);
    CHECK(g_bool_val[1] == true);
    CHECK(g_pod_ptr[0] == first_pod_ptr_0);
    CHECK(g_pod_ptr[1] == first_pod_ptr_1);
    CHECK(g_bool_ptr[0] == first_bool_ptr_0);

    // Different fibers must yield different pointers.
    CHECK(g_pod_ptr[0] != g_pod_ptr[1]);
    CHECK(g_bool_ptr[0] != g_bool_ptr[1]);
    return true;
}

struct alignas(256) OverAlignedState {
    int value;
};

static bool g_overaligned_state_was_null = false;

static void OverAlignedStateProbe(void) {
    REACT_COMPONENT_BEGIN("OverAlignedStateProbe") {
        OverAlignedState *value = use_state<OverAlignedState>({7});
        g_overaligned_state_was_null = value == nullptr;
    }
    REACT_COMPONENT_END();
}

static bool use_state_template_reports_unavailable_storage(void) {
    if constexpr (alignof(OverAlignedState) <= alignof(max_align_t)) {
        return true;
    }

    react_init_runtime();
    g_overaligned_state_was_null = false;

    run_frame([] { OverAlignedStateProbe(); });

    CHECK(g_overaligned_state_was_null);
    CHECK(react_error_count() > 0);
    return true;
}

// ---- use_state_int regression ----

static int *g_legacy_int_ptr[2] = {};
static int g_legacy_int_val[2] = {};

static void LegacyIntProbe(int slot, int initial, int write_value) {
    REACT_COMPONENT_BEGIN_KEY("LegacyIntProbe", slot) {
        int *p = use_state_int(initial);
        if (write_value >= 0)
            *p = write_value;
        g_legacy_int_ptr[slot] = p;
        g_legacy_int_val[slot] = *p;
    }
    REACT_COMPONENT_END();
}

static bool use_state_int_still_works(void) {
    react_init_runtime();
    run_frame([] {
        LegacyIntProbe(0, 5, 42);
        LegacyIntProbe(1, 7, -1);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_legacy_int_val[0] == 42);
    CHECK(g_legacy_int_val[1] == 7);

    int *prev_0 = g_legacy_int_ptr[0];
    int *prev_1 = g_legacy_int_ptr[1];

    run_frame([] {
        LegacyIntProbe(0, 999, -1);
        LegacyIntProbe(1, 999, -1);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_legacy_int_val[0] == 42);
    CHECK(g_legacy_int_val[1] == 7);
    CHECK(g_legacy_int_ptr[0] == prev_0);
    CHECK(g_legacy_int_ptr[1] == prev_1);
    return true;
}

// ---- use_callback ----
//
// We can't count "how many times the std::function was rebuilt" by counting
// functor copy constructions, because the functor passed to use_callback is
// materialized at the call site every frame regardless of whether use_callback
// chooses to placement-new it into the slot. Instead we prove stability two
// ways: (1) the storage slot pointer is identical across frames, and (2) the
// captured value from frame 1 survives a frame 2 call that "would have"
// captured a different value, proving the slot wasn't rebuilt.

static int g_callback_invocations = 0;
static std::function<void()> *g_callback_slot[2] = {};

static void CallbackProbe(int slot, uint64_t deps, int capture) {
    REACT_COMPONENT_BEGIN_KEY("CallbackProbe", slot) {
        std::function<void()> &fn = use_callback(
            [capture] { g_callback_invocations += capture; }, deps);
        g_callback_slot[slot] = &fn;
        fn();
    }
    REACT_COMPONENT_END();
}

static bool use_callback_is_stable_across_frames(void) {
    react_init_runtime();
    g_callback_invocations = 0;

    // Frame 1: cold — function is constructed once per slot.
    run_frame([] {
        CallbackProbe(0, 0xAAA, 1);
        CallbackProbe(1, 0xBBB, 10);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_callback_invocations == 11);

    std::function<void()> *slot0 = g_callback_slot[0];
    std::function<void()> *slot1 = g_callback_slot[1];
    CHECK(slot0 != nullptr);
    CHECK(slot1 != nullptr);
    CHECK(slot0 != slot1);

    // Frame 2: deps unchanged — the closure passed in this frame would capture
    // 999 if the slot were rebuilt; but use_callback should keep the frame-1
    // closure and ignore the new lambda. Slot pointers must be stable.
    run_frame([] {
        CallbackProbe(0, 0xAAA, 999);
        CallbackProbe(1, 0xBBB, 999);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_callback_slot[0] == slot0);
    CHECK(g_callback_slot[1] == slot1);
    // Frame-1 captures preserved: +1 and +10, not +999.
    CHECK(g_callback_invocations == 11 + 11);

    // Frame 3: change deps_hash on slot 0 only; slot 0 rebuilds, slot 1 stays.
    run_frame([] {
        CallbackProbe(0, 0xCCC, 100); // deps changed — rebuild with capture=100
        CallbackProbe(1, 0xBBB, 999); // unchanged — still capture=10
    });
    CHECK(react_error_count() == 0);
    CHECK(g_callback_slot[0] == slot0); // storage slot still stable
    CHECK(g_callback_slot[1] == slot1);
    CHECK(g_callback_invocations == 22 + 100 + 10);
    return true;
}

// ---- use_text_storage ----

static const char *g_text_ptr[2] = {};

static void TextStorageProbe(int key, int value) {
    REACT_COMPONENT_BEGIN_KEY("TextStorageProbe", key) {
        const char *s = use_text_storage("v=%d", value);
        g_text_ptr[key] = s;
    }
    REACT_COMPONENT_END();
}

static bool use_text_storage_is_per_instance_and_stable(void) {
    react_init_runtime();

    run_frame([] {
        TextStorageProbe(0, 10);
        TextStorageProbe(1, 20);
    });
    CHECK(react_error_count() == 0);

    const char *p0_frame_1 = g_text_ptr[0];
    const char *p1_frame_1 = g_text_ptr[1];
    CHECK(p0_frame_1 != nullptr);
    CHECK(p1_frame_1 != nullptr);
    // Two instances in the same frame must have distinct buffers — proves the
    // file-static `char buf[N]` clobber bug is impossible with this hook.
    CHECK(p0_frame_1 != p1_frame_1);
    CHECK(strcmp(p0_frame_1, "v=10") == 0);
    CHECK(strcmp(p1_frame_1, "v=20") == 0);

    // Frame 2 — same call sites, different values. Buffer pointer must be the
    // same per call site (proves the hook is fiber-scoped, not
    // allocate-each-frame).
    run_frame([] {
        TextStorageProbe(0, 30);
        TextStorageProbe(1, 40);
    });
    CHECK(react_error_count() == 0);
    CHECK(g_text_ptr[0] == p0_frame_1);
    CHECK(g_text_ptr[1] == p1_frame_1);
    CHECK(strcmp(g_text_ptr[0], "v=30") == 0);
    CHECK(strcmp(g_text_ptr[1], "v=40") == 0);
    return true;
}

int main(void) {
    if (!positional_siblings_keep_distinct_state())
        return 1;
    if (!keyed_siblings_keep_state_across_reorder())
        return 1;
    if (!components_use_hooks_without_layout_backend())
        return 1;
    if (!transparent_providers_use_instance_identity())
        return 1;
    if (!text_scratch_is_instance_owned())
        return 1;
    if (!remounts_reuse_unmounted_fibers())
        return 1;
    if (!shutdown_cleans_live_effects())
        return 1;
    if (!hook_drift_is_diagnosed())
        return 1;
    if (!use_state_template_persists_typed_values())
        return 1;
    if (!use_state_template_reports_unavailable_storage())
        return 1;
    if (!use_state_int_still_works())
        return 1;
    if (!use_callback_is_stable_across_frames())
        return 1;
    if (!use_text_storage_is_per_instance_and_stable())
        return 1;

    react_shutdown();
    return 0;
}
