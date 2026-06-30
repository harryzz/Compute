import ComputeCxx

#if !arch(wasm32)
// Non-wasm only. On wasm this 2-arg shape disagrees with the real 3-arg C symbol
// `(IAGSubgraphRef, fn-with-context, context) -> IAGUniqueID`; declaring it there would emit a second,
// conflicting wasm import of the SAME symbol as the refined `__IAGSubgraphAddObserver` used by
// WasiClosureShim.subgraphAddObserver -> "indirect call type mismatch". The wasm path uses that shim.
@_silgen_name("IAGSubgraphAddObserver")
func IAGSubgraphAddObserver(
    _ subgraph: UnsafeRawPointer,
    observer: () -> Void
) -> Int
#endif

extension Subgraph {
    public func addObserver(_ observer: @escaping () -> Void) -> Int {
        #if arch(wasm32)
        return WasiClosureShim.subgraphAddObserver(self, observer)
        #else
        return IAGSubgraphAddObserver(unsafeBitCast(self, to: UnsafeRawPointer.self), observer: observer)
        #endif
    }
}

@_silgen_name("IAGSubgraphApply")
func IAGSubgraphApply(
    _ subgraph: UnsafeRawPointer,
    _ flags: Subgraph.Flags,
    _ body: (AnyAttribute) -> Void
)

extension Subgraph {

    public func apply<T>(_ body: () -> T) -> T {
        let update = Graph.clearUpdate()
        let current = Subgraph.current
        defer {
            Subgraph.current = current
            Graph.setUpdate(update)
        }
        Subgraph.current = self
        return body()
    }

    public func forEach(
        _ flags: Subgraph.Flags,
        _ body: (AnyAttribute) -> Void
    ) {
        #if arch(wasm32)
        WasiClosureShim.subgraphForEach(self, flags, body)
        #else
        // `body` is non-escaping, so its closure context is not a retainable heap object. IAGSubgraphApply
        // wraps it in a swiftcall ClosureFunctionAV whose ctor swift_retain()s that context (and dtor
        // releases) — retaining a non-heap context crashes flakily in incrementSlow (oagrender Sig11 on
        // linux). withoutActuallyEscaping promotes it to a valid escaping context for the synchronous
        // call, so the retain/release pair is well-defined. (wasm uses the plain-C, no-retain path above.)
        withoutActuallyEscaping(body) { escaping in
            IAGSubgraphApply(unsafeBitCast(self, to: UnsafeRawPointer.self), flags, escaping)
        }
        #endif
    }

}

extension Subgraph {

    public static func beginTreeElement<Value>(
        value: Attribute<Value>,
        flags: UInt32
    ) {
        if shouldRecordTree {
            __IAGSubgraphBeginTreeElement(
                value.identifier,
                Metadata(Value.self),
                flags
            )
        }
    }

    public static func endTreeElement<Value>(value: Attribute<Value>) {
        if shouldRecordTree {
            __IAGSubgraphEndTreeElement(value.identifier)
        }
    }

    public static func addTreeValue<Value>(
        _ value: Attribute<Value>,
        forKey key: UnsafePointer<Int8>,
        flags: UInt32
    ) {
        if shouldRecordTree {
            __IAGSubgraphAddTreeValue(value.identifier, Metadata(Value.self), key, flags)
        }
    }

}

#if arch(wasm32)
// [wasm32] Subgraph is imported as a Swift FOREIGN-REFERENCE class (IAGSubgraph.h
// IAG_SWIFT_SHARED_REFERENCE). A foreign-reference class's Clang-importer-synthesized members
// (statics, nested Flags, and the self: instance members) are visible ONLY in modules that DIRECTLY
// import ComputeCxx — they do NOT cross `@_exported import` the way the value-type import did.
// OpenSwiftUICore sees the graph only via OpenAttributeGraphShims, so on wasm IAGSubgraph.h drops
// the swift_name (IAG_SUBGRAPH_SELF_NAME) and these are re-declared here as Swift EXTENSION members
// (which DO propagate cross-module), wrapping the refined free functions `__IAGSubgraph...`. Never
// @_silgen_name a foreign-ref return — that bypasses the importer's retain/release ownership.
extension Subgraph {
    // [wasm32] On wasm IAGAttribute.h keeps the flags enum's bare name `IAGAttributeFlags` (the nested
    // `IAGSubgraphRef.Flags` swift_name doesn't synthesize a real member on the foreign-ref class), so
    // restore the `Subgraph.Flags` spelling here for all consumers (Attribute.swift, AnyAttribute.swift,
    // WasiClosureShim.swift, forEach above).
    public typealias Flags = IAGAttributeFlags

    public static var current: Subgraph? {
        get { __IAGSubgraphGetCurrent() }
        set { __IAGSubgraphSetCurrent(newValue) }
    }

    public static var currentGraphContext: UnownedGraphContext? {
        __IAGSubgraphGetCurrentGraphContext()
    }

    public static var shouldRecordTree: Bool {
        __IAGSubgraphShouldRecordTree()
    }

    public static func setShouldRecordTree() {
        __IAGSubgraphSetShouldRecordTree()
    }

    public static var typeID: CFTypeID {
        __IAGSubgraphGetTypeID()
    }

    // [wasm32] OpenSwiftUICore needs Subgraph identity (ObjectIdentifier / === / !==). Foreign-
    // reference types don't support those operators, and the class-ness doesn't cross `@_exported
    // import` to a non-cxx-interop consumer anyway. Provide identity via the underlying storage
    // pointer (stable for the lifetime of the storage) so it crosses as plain Swift API.
    public var rawIdentity: UInt {
        UInt(bitPattern: unsafeBitCast(self, to: UnsafeRawPointer.self))
    }
    public func isIdentical(to other: Subgraph) -> Bool {
        unsafeBitCast(self, to: UnsafeRawPointer.self) == unsafeBitCast(other, to: UnsafeRawPointer.self)
    }

    public var graph: Graph { __IAGSubgraphGetGraph(self) }

    public var isValid: Bool { __IAGSubgraphIsValid(self) }

    public func invalidate() { __IAGSubgraphInvalidate(self) }

    public var index: UInt32 {
        get { __IAGSubgraphGetIndex(self) }
        set { __IAGSubgraphSetIndex(self, newValue) }
    }

    public func removeObserver(_ observerID: IAGUniqueID) {
        __IAGSubgraphRemoveObserver(self, observerID)
    }

    public func addChild(_ child: Subgraph) { __IAGSubgraphAddChild(self, child) }

    public func addChild(_ child: Subgraph, tag: UInt8) { __IAGSubgraphAddChild2(self, child, tag) }

    public func removeChild(_ child: Subgraph) { __IAGSubgraphRemoveChild(self, child) }

    public func child(at index: UInt32, tag: UnsafeMutablePointer<UInt8>?) -> Subgraph {
        __IAGSubgraphGetChild(self, index, tag)
    }

    public var childCount: UInt32 { __IAGSubgraphGetChildCount(self) }

    public func parent(at index: Int64) -> Subgraph { __IAGSubgraphGetParent(self, index) }

    public var parentCount: UInt64 { __IAGSubgraphGetParentCount(self) }

    public func isAncestor(of other: Subgraph) -> Bool { __IAGSubgraphIsAncestor(self, other) }

    public func intersects(flags: IAGAttributeFlags) -> Bool { __IAGSubgraphIntersects(self, flags) }

    public func isDirty(flags: IAGAttributeFlags) -> Bool { __IAGSubgraphIsDirty(self, flags) }

    public func update(flags: IAGAttributeFlags) { __IAGSubgraphUpdate(self, flags) }

    public var treeRoot: TreeElement? { __IAGSubgraphGetTreeRoot(self) }

    public func setTreeOwner(_ owner: AnyAttribute) { __IAGSubgraphSetTreeOwner(self, owner) }
}

// Free-function "initializers" — a foreign-reference class can't have its importer init re-surfaced
// cross-module as an extension init, so mirror OpenSwiftUI's AnyAttributeFix pattern
// (`func Subgraph(graph:attribute:) -> Subgraph`). __IAGSubgraphCreate* are RETURNS_RETAINED so ARC
// balances the +1 on return.
public func Subgraph(graph: Graph) -> Subgraph {
    __IAGSubgraphCreate(graph)
}

public func Subgraph(graph: Graph, attribute: AnyAttribute) -> Subgraph {
    __IAGSubgraphCreate2(graph, attribute)
}
#endif
