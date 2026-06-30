// IAGWasiClosureShim.cpp — the C side of Compute's wasm closure-ABI adaptation layer.
// See IAGWasiClosureShim.h for the why. Each entry is the plain-C counterpart of a Compute function
// that takes a swiftcall callback. Self-contained entries (the field visitors) reimplement the visit
// directly with a plain-C body — deliberately NOT via the swiftcall IAG::ClosureFunction, which is
// exactly what mislowers on wasm. Entries that need engine internals delegate to a small `…_c` method
// exposed on the core type instead (added in this layer, kept minimal).

#include "ComputeCxx/IAGWasiClosureShim.h"

#if defined(__wasi__)

#include "ComputeCxx/IAGType.h"
#include "ContextDescriptor.h"
#include "Metadata.h"
#include "MetadataVisitor.h"

// For the internals-coupled variants (e.g. MutateAttributeC -> Graph::attribute_modify_c).
#include "Attribute/AttributeID/AttributeID.h"
#include "Graph/Graph.h"
#include "Graph/Context.h"
#include "Subgraph/Subgraph.h"

// MARK: - Type field enumeration

void IAGTypeApplyFieldsC(IAGTypeID typeID,
                         void (*apply)(const char *field_name, size_t field_offset,
                                       IAGTypeID field_type, const void *context),
                         const void *apply_context) {
    class VisitorC : public IAG::swift::metadata_visitor {
        void (*_body)(const char *, size_t, IAGTypeID, const void *);
        const void *_context;

      public:
        VisitorC(void (*body)(const char *, size_t, IAGTypeID, const void *), const void *context)
            : _body(body), _context(context) {}

        bool unknown_result() override { return true; }
        bool visit_field(const IAG::swift::metadata &type, const IAG::swift::field_record &field,
                         size_t field_offset, size_t field_size) override {
            auto field_type = type.mangled_type_name_ref(field.MangledTypeName.get(), true, nullptr);
            if (field_type) {
                _body(field.FieldName.get(), field_offset, IAGTypeID(field_type), _context);
            }
            return true;
        }
    };

    VisitorC visitor(apply, apply_context);
    reinterpret_cast<const IAG::swift::metadata *>(typeID)->visit(visitor);
}

bool IAGTypeApplyFields2C(IAGTypeID typeID, IAGTypeApplyOptions options,
                          bool (*apply)(const char *field_name, size_t field_offset,
                                        IAGTypeID field_type, const void *context),
                          const void *apply_context) {
    class VisitorC : public IAG::swift::metadata_visitor {
        IAGTypeApplyOptions _options;
        bool (*_body)(const char *, size_t, IAGTypeID, const void *);
        const void *_context;

      public:
        VisitorC(IAGTypeApplyOptions options, bool (*body)(const char *, size_t, IAGTypeID, const void *),
                 const void *context)
            : _options(options), _body(body), _context(context) {}

        bool unknown_result() override {
            return _options & IAGTypeApplyOptionsContinueAfterUnknownField;
        }
        bool visit_field(const IAG::swift::metadata &type, const IAG::swift::field_record &field,
                         size_t field_offset, size_t field_size) override {
            auto field_type = type.mangled_type_name_ref(field.MangledTypeName.get(), true, nullptr);
            if (!field_type) {
                return unknown_result();
            }
            return _body(field.FieldName.get(), field_offset, IAGTypeID(field_type), _context);
        }
        bool visit_case(const IAG::swift::metadata &type, const IAG::swift::field_record &field,
                        uint32_t index) override {
            auto field_type = type.mangled_type_name_ref(field.MangledTypeName.get(), true, nullptr);
            if (!field_type) {
                return unknown_result();
            }
            return _body(field.FieldName.get(), index, IAGTypeID(field_type), _context);
        }
    };

    VisitorC visitor(options, apply, apply_context);
    auto type = reinterpret_cast<const IAG::swift::metadata *>(typeID);
    switch (type->getKind()) {
    case ::swift::MetadataKind::Class:
        if (options & IAGTypeApplyOptionsEnumerateClassFields) {
            return type->visit_heap(visitor, IAG::LayoutDescriptor::HeapMode::Class);
        }
        return false;
    case ::swift::MetadataKind::Struct:
        if (!(options & IAGTypeApplyOptionsEnumerateClassFields) &&
            !(options & IAGTypeApplyOptionsEnumerateEnumCases)) {
            return type->visit(visitor);
        }
        return false;
    case ::swift::MetadataKind::Enum:
    case ::swift::MetadataKind::Optional:
        if (options & IAGTypeApplyOptionsEnumerateEnumCases) {
            return type->visit(visitor);
        }
        return false;
    case ::swift::MetadataKind::Tuple:
        if (!(options & IAGTypeApplyOptionsEnumerateClassFields) &&
            !(options & IAGTypeApplyOptionsEnumerateEnumCases)) {
            return type->visit(visitor);
        }
        return false;
    default:
        return false;
    }
}

// MARK: - Attribute body mutation

// Resolves the attribute to its node/subgraph and forwards to Graph::attribute_modify_c (core). The
// modify callback uses the C convention so a Swift @convention(c) thunk + pointer-to-closure matches.
void IAGGraphMutateAttributeC(IAGAttribute attribute, IAGTypeID type, bool invalidating,
                              void (*modify)(void *body, const void *context), const void *modify_context) {
    auto attribute_id = IAG::AttributeID(attribute);
    auto node = attribute_id.get_node();
    if (!node) {
        IAG::precondition_failure("non-direct attribute id: %u", attribute);
    }
    attribute_id.validate_data_offset();

    auto subgraph = attribute_id.subgraph();
    if (!subgraph) {
        IAG::precondition_failure("no graph: %u", attribute);
    }

    subgraph->graph()->attribute_modify_c(node, *reinterpret_cast<const IAG::swift::metadata *>(type), modify,
                                          modify_context, invalidating);
}

// MARK: - Subgraph apply

// Forwards to Subgraph::apply with a plain-C PlainApplyBody (the body is invoked via the C ABI so a
// Swift @convention(c) thunk + pointer-to-closure matches it).
void IAGSubgraphApplyC(IAGSubgraphRef subgraph, uint32_t options,
                       void (*body)(IAGAttribute attribute, const void *context), const void *body_context) {
    auto sg = IAG::Subgraph::from_cf(subgraph);
    if (sg == nullptr) {
        return;
    }
    sg->apply(options, IAG::Subgraph::PlainApplyBody{body, body_context});
}

// MARK: - Subgraph observer

// Registers a persistent observer via a plain-C PlainObserverBody (invoked through the C ABI by
// notify_observers, so the Swift @convention(c) thunk + context pointer matches it).
IAGUniqueID IAGSubgraphAddObserverC(IAGSubgraphRef subgraph,
                                    void (*observer)(const void *context), const void *observer_context) {
    auto sg = IAG::Subgraph::from_cf(subgraph);
    if (sg == nullptr) {
        IAG::precondition_failure("accessing invalidated subgraph");
    }
    return sg->add_observer(IAG::Subgraph::PlainObserverBody{observer, observer_context});
}

// MARK: - Graph callbacks (persistent)

// Store plain-C bodies so call_update / call_invalidation fire the Swift @convention(c) thunk via the
// C ABI (the swiftcall ClosureFunction path traps on wasm).
void IAGGraphSetUpdateCallbackC(IAGGraphRef graph,
                                void (*callback)(const void *context), const void *callback_context) {
    auto graph_context = IAG::Graph::Context::from_cf(graph);
    graph_context->set_update_callback(IAG::Graph::Context::PlainUpdateCallback{callback, callback_context});
}

void IAGGraphSetInvalidationCallbackC(IAGGraphRef graph,
                                      void (*callback)(IAGAttribute attribute, const void *context),
                                      const void *callback_context) {
    auto graph_context = IAG::Graph::Context::from_cf(graph);
    graph_context->set_invalidation_callback(
        IAG::Graph::Context::PlainInvalidationCallback{callback, callback_context});
}

#endif // __wasi__
