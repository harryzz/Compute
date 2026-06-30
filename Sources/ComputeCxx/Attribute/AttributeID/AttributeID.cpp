#include "AttributeID.h"

#include "Data/Table.h"
#include "Errors/Errors.h"

#include "Attribute/AttributeData/Node/IndirectNode.h"
#include "Attribute/AttributeData/Node/Node.h"
#include "Attribute/AttributeType/AttributeType.h"
#include "Graph/Graph.h"
#include "OffsetAttributeID.h"
#include "Subgraph/Subgraph.h"

namespace IAG {

bool AttributeID::has_subgraph_flags() const {
    if (auto node = get_node()) {
        return node->subgraph_flags() != IAGAttributeFlagsNone;
    }
    return false;
}

std::optional<size_t> AttributeID::size() const {
    if (auto node = get_node()) {
        const AttributeType &attribute_type = subgraph()->graph()->attribute_type(node->type_id());
        size_t size = attribute_type.value_metadata().vw_size();
        return std::optional<size_t>(size);
    }
    if (auto indirect_node = get_indirect_node()) {
        return indirect_node->size();
    }
    return std::optional<size_t>();
}

bool AttributeID::traverses(AttributeID other, TraversalOptions options) const {
    if (auto indirect_node = get_indirect_node()) {
        if (AttributeID(indirect_node) == other) {
            return true;
        }
        if (!(options & TraversalOptions::SkipMutableReference) || !indirect_node->is_mutable()) {
            return indirect_node->source().identifier().traverses(other, options);
        }
    }
    return *this == other;
}

OffsetAttributeID AttributeID::resolve(TraversalOptions options) const {
    if (is_node()) {
        return OffsetAttributeID(*this);
    }
    return resolve_slow(options);
}

OffsetAttributeID AttributeID::resolve_slow(TraversalOptions options) const {
    AttributeID result = *this;
    uint32_t offset = 0;
    while (auto indirect_node = result.get_indirect_node()) {
        if (offset == 0 && options & TraversalOptions::ReportIndirectionInOffset) {
            offset = 1;
        }

        if (indirect_node->is_mutable()) {
            if (options & TraversalOptions::SkipMutableReference) {
                return OffsetAttributeID(result, offset);
            }

            if (options & TraversalOptions::UpdateDependencies) {
                auto dependency = indirect_node->to_mutable().dependency();
                if (dependency) {
                    // [#12] `_dependency` is a plain (non-weak) AttributeID. After the dependency's source
                    // subgraph is torn down (and on wasm its page recycled) this stale dependency can be
                    // non-null yet NOT a live node, so `get_node()` is null/dangling and
                    // `update_attribute(...)` aborts in `data::ptr::operator->` (offset 0). Same dangling-
                    // cross-subgraph-ref class as the offset-projection weak-expiry (see input_value_ref_slow):
                    // teardown does not reach this dependency to clear it. The dependency pre-update is
                    // best-effort, so skip it unless the dependency is still a LIVE node (page allocated, its
                    // subgraph not invalidated) — matches AG (a dead dependency is simply not updated; the
                    // read then proceeds). Latent on 64-bit Apple where the page isn't freed/recycled.
                    auto dependency_node = dependency.get_node();
                    if (dependency_node != nullptr &&
                        data::table::shared().raw_page_seed(dependency.page_ptr()) != 0) {
                        auto subgraph = dependency.subgraph();
                        // Also skip if the dependency node is already updating: a stale dependency whose
                        // page was RECYCLED to a node already on the update stack would otherwise form a
                        // false cycle (UpdateStack::push_slow -> print_cycle, which traps on wasm). The
                        // pre-update is best-effort, so skipping a self/ancestor is safe.
                        if (subgraph && !subgraph->is_invalidated() && !dependency_node->is_updating()) {
                            subgraph->graph()->update_attribute(dependency_node, IAGGraphUpdateOptionsNone);
                        }
                    }
                }
            }
        }

        if (options & TraversalOptions::EvaluateWeakReferences) {
            if (indirect_node->source().expired()) {
                if (options & TraversalOptions::AssertNotNil) {
                    precondition_failure("invalid indirect ref: %u", _value);
                }
                return OffsetAttributeID(AttributeID(IAGAttributeNil));
            }
        }

        offset += indirect_node->offset();
        result = indirect_node->source().identifier();
    }

    if (options & TraversalOptions::AssertNotNil && !result.is_node()) {
        precondition_failure("invalid attribute id: %u", _value);
    }

    return OffsetAttributeID(result, offset);
}

} // namespace IAG
