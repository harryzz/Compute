#pragma once

#include "ComputeCxx/AGBase.h"

#if TARGET_OS_MAC
#include "CoreFoundationPrivate/CFRuntime.h"
#else
#include <SwiftCorelibsCoreFoundation/CFRuntime.h>
#endif

#include "Graph.h"

AG_ASSUME_NONNULL_BEGIN

struct AGGraphStorage;

namespace AG {

class Graph::Context {
  private:
    Graph *_graph;
    const void *_context = nullptr;
    uint64_t _id;

    ClosureFunctionAV<void, AGAttribute> _invalidation_callback = {nullptr, nullptr};
    ClosureFunctionVV<void> _update_callback = {nullptr, nullptr};

#if defined(__wasi__)
    // WASI: the swiftcall closure ABI mislowers on wasm, so store STORED callbacks as
    // plain-C fn + context (set via the *C entry points) and invoke them plain-C.
    void (*_update_callback_c)(const void *context) = nullptr;
    const void *_update_callback_c_context = nullptr;
    void (*_invalidation_callback_c)(AGAttribute attribute, const void *context) = nullptr;
    const void *_invalidation_callback_c_context = nullptr;
#endif

    uint64_t _deadline = UINT64_MAX;
    uint64_t _graph_version;
    bool _needs_update;
    bool _invalidated;

    void call_invalidation(AttributeID attribute);

  public:
    Context(Graph *graph);
    ~Context();

    uint64_t id() const { return _id; }

    AGGraphStorage *to_cf() const { return reinterpret_cast<AGGraphStorage *>((char *)this - sizeof(CFRuntimeBase)); };
    static Context *from_cf(AGGraphStorage *storage);

    Graph &graph() const { return *_graph; };

    const void *_Nullable context() const { return _context; };
    void set_context(const void *_Nullable context) { _context = context; };

    void set_invalidation_callback(AG::ClosureFunctionAV<void, AGAttribute> callback) {
        _invalidation_callback = callback;
    }
    void set_update_callback(AG::ClosureFunctionVV<void> callback) { _update_callback = callback; }

#if defined(__wasi__)
    void set_update_callback_c(void (*cb)(const void *), const void *ctx) {
        _update_callback_c = cb;
        _update_callback_c_context = ctx;
    }
    void set_invalidation_callback_c(void (*cb)(AGAttribute, const void *), const void *ctx) {
        _invalidation_callback_c = cb;
        _invalidation_callback_c_context = ctx;
    }
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

} // namespace AG

AG_ASSUME_NONNULL_END
