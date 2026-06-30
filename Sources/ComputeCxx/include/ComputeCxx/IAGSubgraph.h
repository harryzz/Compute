#pragma once

#include <ComputeCxx/IAGBase.h>
#include <ComputeCxx/IAGGraph.h>
#include <ComputeCxx/IAGTreeElement.h>
#include <ComputeCxx/IAGUniqueID.h>

IAG_ASSUME_NONNULL_BEGIN
IAG_IMPLICIT_BRIDGING_ENABLED

IAG_EXTERN_C_BEGIN

// MARK: CFType

#if defined(__wasi__)
// [wasm32] IAG_BRIDGED_TYPE(id) (objc_bridge) is empty off-Apple, so the CF type imports UNMANAGED
// -> struct-held Subgraph refs don't keep the storage alive -> use-after-free. Import as a Swift
// foreign-reference type (upstream SwiftBridging IAG_SWIFT_SHARED_REFERENCE) so memberwise ARC gives
// every reference (incl. struct-held) the correct value-witness retain/release SHAPE, exactly as
// objc_bridge ARC does on Apple. retain/release are NO-OP (immortal storage — see IAGSubgraph.cpp).
typedef struct IAG_SWIFT_SHARED_REFERENCE(IAGSubgraphRetainRef, IAGSubgraphReleaseRef)
    IAGSubgraphStorage *IAGSubgraphRef IAG_SWIFT_NAME(Subgraph);
IAG_EXPORT IAGSubgraphRef IAGSubgraphRetainRef(IAGSubgraphRef subgraph);
IAG_EXPORT void IAGSubgraphReleaseRef(IAGSubgraphRef subgraph);

// [wasm32] A foreign-reference class's importer-synthesized members (self: members, statics, nested
// Flags) are visible ONLY in modules that DIRECTLY import ComputeCxx; they do NOT cross
// `@_exported import` (unlike the value-type import). OpenSwiftUICore sees the graph only via
// OpenAttributeGraphShims, so drop the swift_name here -> each imports as a refined free func
// __IAGSubgraph...; Compute/Graph/Subgraph.swift re-declares them as Swift EXTENSION members (which
// DO cross-module). On Apple (value type) keep the importer members.
#define IAG_SUBGRAPH_SELF_NAME(_name)
#else
typedef struct IAG_BRIDGED_TYPE(id) IAGSubgraphStorage *IAGSubgraphRef IAG_SWIFT_NAME(Subgraph);
#define IAG_SUBGRAPH_SELF_NAME(_name) IAG_SWIFT_NAME(_name)
#endif

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
CFTypeID IAGSubgraphGetTypeID(void) IAG_SUBGRAPH_SELF_NAME(getter:IAGSubgraphRef.typeID());

// MARK: Current subgraph

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
IAG_SWIFT_RETURNS_UNRETAINED
IAGSubgraphRef _Nullable IAGSubgraphGetCurrent(void) IAG_SUBGRAPH_SELF_NAME(getter:IAGSubgraphRef.current());

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphSetCurrent(IAGSubgraphRef _Nullable subgraph) IAG_SUBGRAPH_SELF_NAME(setter:IAGSubgraphRef.current(_:));

// MARK: Graph Context

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
IAG_SWIFT_RETURNS_RETAINED
IAGSubgraphRef IAGSubgraphCreate(IAGGraphRef graph) IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.init(graph:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
IAG_SWIFT_RETURNS_RETAINED
IAGSubgraphRef IAGSubgraphCreate2(IAGGraphRef graph, IAGAttribute attribute)
    IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.init(graph:attribute:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
IAGUnownedGraphContextRef _Nullable IAGSubgraphGetCurrentGraphContext(void)
    IAG_SUBGRAPH_SELF_NAME(getter:IAGSubgraphRef.currentGraphContext());

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
IAGGraphRef IAGSubgraphGetGraph(IAGSubgraphRef subgraph) IAG_SUBGRAPH_SELF_NAME(getter:IAGSubgraphRef.graph(self:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
bool IAGSubgraphIsValid(IAGSubgraphRef subgraph) IAG_SUBGRAPH_SELF_NAME(getter:IAGSubgraphRef.isValid(self:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphInvalidate(IAGSubgraphRef subgraph) IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.invalidate(self:));

// MARK: Index

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
uint32_t IAGSubgraphGetIndex(IAGSubgraphRef subgraph) IAG_SUBGRAPH_SELF_NAME(getter:IAGSubgraphRef.index(self:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphSetIndex(IAGSubgraphRef subgraph, uint32_t index) IAG_SUBGRAPH_SELF_NAME(setter:IAGSubgraphRef.index(self:_:));

// MARK: Observers

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
IAGUniqueID IAGSubgraphAddObserver(IAGSubgraphRef subgraph,
                                 void (*observer)(const void *_Nullable context IAG_SWIFT_CONTEXT) IAG_SWIFT_CC(swift),
                                 const void *_Nullable observer_context);

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphRemoveObserver(IAGSubgraphRef subgraph, IAGUniqueID observer_id)
    IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.removeObserver(self:_:));

// MARK: Children

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphAddChild(IAGSubgraphRef subgraph, IAGSubgraphRef child) IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.addChild(self:_:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphAddChild2(IAGSubgraphRef subgraph, IAGSubgraphRef child, uint8_t tag)
    IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.addChild(self:_:tag:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphRemoveChild(IAGSubgraphRef subgraph, IAGSubgraphRef child)
    IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.removeChild(self:_:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
IAG_SWIFT_RETURNS_UNRETAINED
IAGSubgraphRef IAGSubgraphGetChild(IAGSubgraphRef subgraph, uint32_t index, uint8_t *_Nullable tag_out)
    IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.child(self:at:tag:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
uint32_t IAGSubgraphGetChildCount(IAGSubgraphRef subgraph) IAG_SUBGRAPH_SELF_NAME(getter:IAGSubgraphRef.childCount(self:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
IAG_SWIFT_RETURNS_UNRETAINED
IAGSubgraphRef IAGSubgraphGetParent(IAGSubgraphRef subgraph, int64_t index) IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.parent(self:at:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
uint64_t IAGSubgraphGetParentCount(IAGSubgraphRef subgraph) IAG_SUBGRAPH_SELF_NAME(getter:IAGSubgraphRef.parentCount(self:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
bool IAGSubgraphIsAncestor(IAGSubgraphRef subgraph, IAGSubgraphRef other)
    IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.isAncestor(self:of:));

// MARK: Flags

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
bool IAGSubgraphIntersects(IAGSubgraphRef subgraph, IAGAttributeFlags flags)
    IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.intersects(self:flags:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
bool IAGSubgraphIsDirty(IAGSubgraphRef subgraph, IAGAttributeFlags flags) IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.isDirty(self:flags:));

// MARK: Attributes

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
IAG_SWIFT_RETURNS_UNRETAINED
IAGSubgraphRef IAGGraphGetAttributeSubgraph(IAGAttribute attribute) IAG_SWIFT_NAME(getter:IAGAttribute.subgraph(self:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
IAG_SWIFT_RETURNS_UNRETAINED
IAGSubgraphRef _Nullable IAGGraphGetAttributeSubgraph2(IAGAttribute attribute)
    IAG_SWIFT_NAME(getter:IAGAttribute.subgraphOrNil(self:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphApply(IAGSubgraphRef subgraph, uint32_t options,
                     void (*body)(IAGAttribute, const void *context IAG_SWIFT_CONTEXT) IAG_SWIFT_CC(swift),
                     const void *body_context);

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphUpdate(IAGSubgraphRef subgraph, IAGAttributeFlags flags) IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.update(self:flags:));

// MARK: Tree

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
_Nullable IAGTreeElement IAGSubgraphGetTreeRoot(IAGSubgraphRef subgraph) IAG_SUBGRAPH_SELF_NAME(getter:IAGSubgraphRef.treeRoot(self:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphBeginTreeElement(IAGAttribute value, IAGTypeID type, uint32_t flags);

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphEndTreeElement(IAGAttribute value);

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphSetTreeOwner(IAGSubgraphRef subgraph, IAGAttribute owner)
    IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.setTreeOwner(self:_:));

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphAddTreeValue(IAGAttribute value, IAGTypeID type, const char *key, uint32_t flags);

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
bool IAGSubgraphShouldRecordTree(void) IAG_SUBGRAPH_SELF_NAME(getter:IAGSubgraphRef.shouldRecordTree());

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGSubgraphSetShouldRecordTree(void) IAG_SUBGRAPH_SELF_NAME(IAGSubgraphRef.setShouldRecordTree());

IAG_EXTERN_C_END

IAG_IMPLICIT_BRIDGING_DISABLED
IAG_ASSUME_NONNULL_END
