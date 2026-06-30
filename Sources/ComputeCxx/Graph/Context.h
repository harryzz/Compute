#pragma once

#include "ComputeCxx/IAGBase.h"

#if TARGET_OS_MAC
#include "CoreFoundationPrivate/CFRuntime.h"
#else
#include <SwiftCorelibsCoreFoundation/CFRuntime.h>
#endif

#include "Graph.h"

IAG_ASSUME_NONNULL_BEGIN

struct IAGGraphStorage;

namespace IAG {

class Graph::Context {
  private:
    Graph *_graph;
    const void *_context = nullptr;
    uint64_t _id;

#if defined(__wasi__)
  public:
    // [wasm] plain-C persistent graph callbacks (no swiftcall). Swift emits @convention(c) thunks on
    // wasm that cannot be stored as swiftcall ClosureFunctions and invoked via the swift CC — firing
    // them traps "indirect call type mismatch". Store + call via the plain C ABI, exactly like
    // Subgraph::PlainObserverBody / PlainApplyBody. See IAGGraphSetUpdateCallbackC / *InvalidationC.
    struct PlainInvalidationCallback {
        void (*_Nullable fn)(IAGAttribute, const void *_Nullable) = nullptr;
        const void *_Nullable ctx = nullptr;
        explicit operator bool() const { return fn != nullptr; }
        void operator()(IAGAttribute a) const { fn(a, ctx); }
    };
    struct PlainUpdateCallback {
        void (*_Nullable fn)(const void *_Nullable) = nullptr;
        const void *_Nullable ctx = nullptr;
        explicit operator bool() const { return fn != nullptr; }
        void operator()() const { fn(ctx); }
    };
  private:
    PlainInvalidationCallback _invalidation_callback = {};
    PlainUpdateCallback _update_callback = {};
#else
    ClosureFunctionAV<void, IAGAttribute> _invalidation_callback = {nullptr, nullptr};
    ClosureFunctionVV<void> _update_callback = {nullptr, nullptr};
#endif

    uint64_t _deadline = UINT64_MAX;
    uint64_t _graph_version = 0; // [wandr] was uninitialized
    bool _needs_update = false; // [wandr] was uninitialized
    bool _invalidated = false; // [wandr] was uninitialized

    void call_invalidation(AttributeID attribute);

  public:
    Context(Graph *graph);
    ~Context();

    uint64_t id() const { return _id; }

    IAGGraphStorage *to_cf() const { return reinterpret_cast<IAGGraphStorage *>((char *)this - sizeof(CFRuntimeBase)); };
    static Context *from_cf(IAGGraphStorage *storage);

    Graph &graph() const { return *_graph; };

    const void *_Nullable context() const { return _context; };
    void set_context(const void *_Nullable context) { _context = context; };

#if defined(__wasi__)
    void set_invalidation_callback(PlainInvalidationCallback callback) { _invalidation_callback = callback; }
    void set_update_callback(PlainUpdateCallback callback) { _update_callback = callback; }
#else
    void set_invalidation_callback(IAG::ClosureFunctionAV<void, IAGAttribute> callback) {
        _invalidation_callback = callback;
    }
    void set_update_callback(IAG::ClosureFunctionVV<void> callback) { _update_callback = callback; }
#endif

    uint64_t deadline() const { return _deadline; };
    void set_deadline(uint64_t deadline);

    uint64_t graph_version() const { return _graph_version; }; // controls invalidation callback

    bool needs_update() const { return _needs_update; };
    void set_needs_update();

    bool invalidated() const { return _invalidated; };
    void set_invalidated(bool invalidated) { _invalidated = invalidated; };

    bool thread_is_updating();

    void call_invalidation_if_needed(AttributeID attribute) {
        if (graph_version() == graph().version()) {
            return;
        }
        call_invalidation(attribute);
    }
    void call_update();
};

} // namespace IAG

IAG_ASSUME_NONNULL_END
