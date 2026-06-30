#include "IAGSubgraph-Private.h"

#include <platform/once.h>

#include "Graph/Context.h"
#include "Subgraph.h"

#if IAG_DBG_STORAGE_COUNT
// [#14] Subgraph-storage liveness counter — proves faithful frees actually happen (created vs finalized).
// Opt-in (silent by default): set IAG_STORAGE_LOG=1 to print created/finalized/live at process exit, the
// same env-gated style as IAG_TREE / IAG_LOG_GROW. SAFE under real frees (globals only, no per-storage
// walk). Under the faithful lifecycle a teardown-heavy run ends at live~=0 (vs live==created when immortal).
#include <cstdio>
#include <cstdlib>
namespace {
struct StorageCount {
    long created = 0, finalized = 0;
    bool armed = false, enabled = false;
} g_sc;
inline void iag_sc_dump() {
    if (!g_sc.enabled) return;
    fprintf(stderr, "[STORAGE #14] created=%ld finalized=%ld live=%ld\n", g_sc.created, g_sc.finalized,
            g_sc.created - g_sc.finalized);
    fflush(stderr);
}
inline void iag_sc_arm() {
    if (!g_sc.armed) {
        g_sc.armed = true;
        const char *e = getenv("IAG_STORAGE_LOG");
        g_sc.enabled = e && atoi(e) != 0;
        atexit(iag_sc_dump);
    }
}
} // namespace
#endif

namespace {

CFRuntimeClass &subgraph_type_id() {
    static auto finalize = [](CFTypeRef subgraph_ref) {
        IAGSubgraphStorage *storage = (IAGSubgraphStorage *)subgraph_ref;
#if IAG_DBG_STORAGE_COUNT
        g_sc.finalized++;
#endif
        IAG::Subgraph *subgraph = storage->subgraph;
        if (subgraph) {
            subgraph->clear_object();
            subgraph->invalidate_and_delete_(false);
        }
    };
    static CFRuntimeClass klass = {
        0,            // version
        "IAGSubgraph", // className
        NULL,         // init
        NULL,         // copy,
        finalize,
        NULL, // equal
        NULL, // hash
        NULL, // copyFormattingDesc
        NULL, // copyDebugDesc,
        NULL, // reclaim
        NULL, // refcount
        0     // requiredAlignment
    };
    return klass;
}

} // namespace

#if defined(__wasi__)
// [#14 faithful] Off-Apple `objc_bridge(id)` is empty, so Swift ARC does not refcount the CF storage on
// its own. On wasm we import Subgraph as a foreign-reference type (IAGSubgraph.h IAG_SWIFT_SHARED_REFERENCE)
// whose retain/release map HERE — make them REAL CFRetain/CFRelease. Combined with the graph-alive
// self-ref (extra CFRetain at create, released at Subgraph::clear_object) the refcount tracks true
// liveness: storage is freed only when the subgraph is dead AND no Swift handle references it. The
// Phase-1 balance instrument proved this never releases-to-zero while the subgraph is alive.
IAGSubgraphRef IAGSubgraphRetainRef(IAGSubgraphRef subgraph) {
    if (subgraph) {
        CFRetain(subgraph);
    }
    return subgraph;
}
void IAGSubgraphReleaseRef(IAGSubgraphRef subgraph) {
    if (subgraph) {
        CFRelease(subgraph);
    }
}
#endif

CFTypeID IAGSubgraphGetTypeID() {
    static CFTypeID type = _CFRuntimeRegisterClass(&subgraph_type_id());
    return type;
}

#pragma mark - Current subgraph

IAGSubgraphRef IAGSubgraphGetCurrent() {
    auto current = IAG::Subgraph::current_subgraph();
    if (current == nullptr) {
        return nullptr;
    }
    return current->to_cf();
}

void IAGSubgraphSetCurrent(IAGSubgraphRef subgraph) {
    IAG::Subgraph *old_subgraph = IAG::Subgraph::current_subgraph();
    if (subgraph != nullptr) {
        IAG::Subgraph::set_current_subgraph(IAG::Subgraph::from_cf(subgraph));
        if (IAG::Subgraph::from_cf(subgraph) != nullptr) {
            CFRetain(subgraph); // current-subgraph ref (released when replaced here or at clear_object)
        }
    } else {
        IAG::Subgraph::set_current_subgraph(nullptr);
    }
#if IAG_CF_STORAGE_SWIFT_MANAGED
    // Release the previous current-subgraph ref. (Apple: ARC-bridged storage; wasm: foreign-ref import
    // — both refcount the storage; gate in IAGBase.h.) The graph-alive self-ref taken at create keeps
    // the storage alive until the subgraph truly dies, so this never frees a live subgraph's storage.
    if (old_subgraph && old_subgraph->to_cf()) {
        CFRelease(old_subgraph->to_cf());
    }
#endif
}

#pragma mark - Graph context

IAGSubgraphRef IAGSubgraphCreate(IAGGraphRef graph) { return IAGSubgraphCreate2(graph, IAGAttributeNil); };

IAGSubgraphRef IAGSubgraphCreate2(IAGGraphRef graph, IAGAttribute attribute) {
    CFIndex extra_bytes = sizeof(struct IAGSubgraphStorage) - sizeof(CFRuntimeBase);
    IAGSubgraphRef instance =
        (IAGSubgraphRef)_CFRuntimeCreateInstance(kCFAllocatorDefault, IAGSubgraphGetTypeID(), extra_bytes, NULL);
    if (!instance) {
        IAG::precondition_failure("memory allocation failure.");
    }

    IAG::Graph::Context *context = IAG::Graph::Context::from_cf(graph);

    instance->subgraph = new IAG::Subgraph((IAG::SubgraphObject *)instance, *context, IAG::AttributeID(attribute));
#if defined(__wasi__) && IAG_CF_STORAGE_SWIFT_MANAGED
    // [#14 faithful] graph-alive self-ref: _CFRuntimeCreateInstance returns +1, but create is
    // RETURNS_RETAINED so Swift ARC takes that ref as the returned handle and releases it when the
    // handle dies. Take ONE extra ref to represent "alive in the graph", released at clear_object (the
    // subgraph's true death). This keeps the refcount >=1 for the whole alive lifetime -> no premature
    // free (Phase-1-proven), and the storage is freed only once dead AND unreferenced by any handle.
    CFRetain(instance);
#endif
#if IAG_DBG_STORAGE_COUNT
    iag_sc_arm();
    g_sc.created++;
#endif
    return instance;
};

IAGUnownedGraphContextRef IAGSubgraphGetCurrentGraphContext() {
    IAG::Subgraph *current_subgraph = IAG::Subgraph::current_subgraph();
    if (current_subgraph == nullptr) {
        return nullptr;
    }

    IAG::Graph *graph = current_subgraph->graph();
    return reinterpret_cast<IAGUnownedGraphContextRef>(graph);
}

IAGGraphRef IAGSubgraphGetGraph(IAGSubgraphRef subgraph) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }

    auto context_id = IAG::Subgraph::from_cf(subgraph)->context_id();
    if (context_id != 0) {
        if (IAG::Graph::Context *context = IAG::Subgraph::from_cf(subgraph)->graph()->context_with_id(context_id)) {
            return IAGGraphContextGetGraph(context);
        }
    }

    IAG::precondition_failure("accessing invalidated context");
}

bool IAGSubgraphIsValid(IAGSubgraphRef subgraph) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        return false;
    }

    return IAG::Subgraph::from_cf(subgraph)->is_valid();
}

void IAGSubgraphInvalidate(IAGSubgraphRef subgraph) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        return;
    }

    IAG::Subgraph::from_cf(subgraph)->invalidate_and_delete_(false);
}

#pragma mark - Index

uint32_t IAGSubgraphGetIndex(IAGSubgraphRef subgraph) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }

    return IAG::Subgraph::from_cf(subgraph)->index();
}

void IAGSubgraphSetIndex(IAGSubgraphRef subgraph, uint32_t index) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }

    IAG::Subgraph::from_cf(subgraph)->set_index(index);
}

#pragma mark - Observers

#if !defined(__wasi__)
// Swiftcall observer registration (Darwin). On wasm this swiftcc path can't be fed from the
// @convention(c) thunk Swift emits — observers are registered via IAGSubgraphAddObserverC instead
// (plain C ABI), see IAGWasiClosureShim.cpp. The Subgraph stores a PlainObserverBody on wasm.
IAGUniqueID IAGSubgraphAddObserver(IAGSubgraphRef subgraph,
                                 void (*observer)(const void *context IAG_SWIFT_CONTEXT) IAG_SWIFT_CC(swift),
                                 const void *observer_context) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }

    auto callback = IAG::ClosureFunctionVV<void>(observer, observer_context);
    return IAG::Subgraph::from_cf(subgraph)->add_observer(callback);
}
#endif

void IAGSubgraphRemoveObserver(IAGSubgraphRef subgraph, IAGUniqueID observer_id) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }

    IAG::Subgraph::from_cf(subgraph)->remove_observer(observer_id);
}

#pragma mark - Children

void IAGSubgraphAddChild(IAGSubgraphRef subgraph, IAGSubgraphRef child) { IAGSubgraphAddChild2(subgraph, child, 0); }

void IAGSubgraphAddChild2(IAGSubgraphRef subgraph, IAGSubgraphRef child, uint8_t tag) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }
    if (IAG::Subgraph::from_cf(child) == nullptr) {
        return;
    }

    IAG::Subgraph::from_cf(subgraph)->add_child(*IAG::Subgraph::from_cf(child), tag);
}

void IAGSubgraphRemoveChild(IAGSubgraphRef subgraph, IAGSubgraphRef child) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }

    if (child->subgraph) {
        IAG::Subgraph::from_cf(subgraph)->remove_child(*IAG::Subgraph::from_cf(child), false);
    }
}

IAGSubgraphRef IAGSubgraphGetChild(IAGSubgraphRef subgraph, uint32_t index, uint8_t *tag_out) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }
    if (index >= IAG::Subgraph::from_cf(subgraph)->children().size()) {
        IAG::precondition_failure("invalid child index: %u", index);
    }

    IAG::Subgraph::SubgraphChild &child = IAG::Subgraph::from_cf(subgraph)->children()[index];
    if (tag_out) {
        *tag_out = child.tag();
    }
    return child.subgraph()->to_cf();
}

uint32_t IAGSubgraphGetChildCount(IAGSubgraphRef subgraph) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }

    return IAG::Subgraph::from_cf(subgraph)->children().size();
}

IAGSubgraphRef IAGSubgraphGetParent(IAGSubgraphRef subgraph, int64_t index) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }
    if (index >= IAG::Subgraph::from_cf(subgraph)->parents().size()) {
        IAG::precondition_failure("invalid parent index: %u", index);
    }

    return IAG::Subgraph::from_cf(subgraph)->parents()[index]->to_cf();
}

uint64_t IAGSubgraphGetParentCount(IAGSubgraphRef subgraph) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        return 0;
    }

    return IAG::Subgraph::from_cf(subgraph)->parents().size();
}

bool IAGSubgraphIsAncestor(IAGSubgraphRef subgraph, IAGSubgraphRef other) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        return false;
    }
    if (IAG::Subgraph::from_cf(other) == nullptr) {
        return false;
    }

    return IAG::Subgraph::from_cf(subgraph)->ancestor_of(*IAG::Subgraph::from_cf(other));
}

#pragma mark - Flags

bool IAGSubgraphIntersects(IAGSubgraphRef subgraph, IAGAttributeFlags flags) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        return false;
    }

    return IAG::Subgraph::from_cf(subgraph)->intersects(flags);
}

bool IAGSubgraphIsDirty(IAGSubgraphRef subgraph, IAGAttributeFlags flags) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        return false;
    }

    return IAG::Subgraph::from_cf(subgraph)->is_dirty(flags);
}

#pragma mark - Graph

IAGSubgraphRef IAGGraphGetAttributeSubgraph(IAGAttribute attribute) {
    auto subgraph = IAGGraphGetAttributeSubgraph2(attribute);
    if (subgraph == nullptr) {
        IAG::precondition_failure("no subgraph");
    }

    return subgraph;
}

IAGSubgraphRef IAGGraphGetAttributeSubgraph2(IAGAttribute attribute) {
    auto attribute_id = IAG::AttributeID(attribute);
    attribute_id.validate_data_offset();

    auto subgraph = attribute_id.subgraph();
    if (subgraph == nullptr) {
        IAG::precondition_failure("internal error");
    }

    return subgraph->to_cf();
}

void IAGSubgraphApply(IAGSubgraphRef subgraph, uint32_t options,
                     void (*body)(IAGAttribute, const void *context IAG_SWIFT_CONTEXT) IAG_SWIFT_CC(swift), const void *body_context) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        return;
    }

    IAG::Subgraph::from_cf(subgraph)->apply(options, IAG::ClosureFunctionAV<void, unsigned int>(body, body_context));
}

void IAGSubgraphUpdate(IAGSubgraphRef subgraph, IAGAttributeFlags flags) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        return;
    }

    IAG::Subgraph::from_cf(subgraph)->update(flags);
}

#pragma mark - Tree

IAGTreeElement IAGSubgraphGetTreeRoot(IAGSubgraphRef subgraph) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        return IAGTreeElement();
    }

    auto tree_root = IAG::Subgraph::from_cf(subgraph)->tree_root();
    return IAGTreeElement((uintptr_t)tree_root);
}

void IAGSubgraphSetTreeOwner(IAGSubgraphRef subgraph, IAGAttribute owner) {
    if (IAG::Subgraph::from_cf(subgraph) == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }
    IAG::Subgraph::from_cf(subgraph)->set_tree_owner(IAG::AttributeID(owner));
}

void IAGSubgraphAddTreeValue(IAGAttribute value, IAGTypeID type, const char *key, uint32_t flags) {
    IAG::Subgraph *current_subgraph = IAG::Subgraph::current_subgraph();
    if (current_subgraph == nullptr) {
        return;
    }

    auto metadata = reinterpret_cast<const IAG::swift::metadata *>(type);
    current_subgraph->add_tree_value(IAG::AttributeID(value), metadata, key, flags);
}

void IAGSubgraphBeginTreeElement(IAGAttribute value, IAGTypeID type, uint32_t flags) {
    IAG::Subgraph *current_subgraph = IAG::Subgraph::current_subgraph();
    if (current_subgraph == nullptr) {
        return;
    }

    auto metadata = reinterpret_cast<const IAG::swift::metadata *>(type);
    current_subgraph->begin_tree(IAG::AttributeID(value), metadata, flags);
}

void IAGSubgraphEndTreeElement(IAGAttribute value) {
    IAG::Subgraph *current_subgraph = IAG::Subgraph::current_subgraph();
    if (current_subgraph == nullptr) {
        return;
    }

    current_subgraph->end_tree();
}

static platform_once_t should_record_tree_once = 0;
static bool should_record_tree = true;

void init_should_record_tree() {
    char *result = getenv("IAG_TREE");
    if (result) {
        should_record_tree = atoi(result) != 0;
    } else {
        should_record_tree = false;
    }
}

bool IAGSubgraphShouldRecordTree() {
    platform_once(&should_record_tree_once, init_should_record_tree);
    return should_record_tree;
}

void IAGSubgraphSetShouldRecordTree() {
    platform_once(&should_record_tree_once, init_should_record_tree);
    should_record_tree = true;
}
