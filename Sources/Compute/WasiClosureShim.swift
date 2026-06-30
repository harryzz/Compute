// WasiClosureShim — the Swift side of Compute's wasm closure-ABI adaptation layer.
//
// Pairs with IAGWasiClosureShim.{h,cpp}. The public Compute API delegates here under
// `#if arch(wasm32)`, so the non-wasm path stays byte-identical to upstream.
//
// Discipline (kept uniform across this layer):
//   • SYNCHRONOUS closures (invoked during the call, never stored) → pass a pointer to the escaping
//     closure and reconstruct it in a NON-capturing @convention(c) thunk. No heap box, no ARC.
//   • PERSISTENT closures (the engine stores the callback) → a heap box whose retain ownership
//     matches that C API's contract (added per-function as those land).
#if arch(wasm32)
import ComputeCxx

enum WasiClosureShim {

    // MARK: - Type field enumeration (synchronous)

    static func forEachField(
        of type: Metadata,
        _ body: (UnsafePointer<CChar>, Int, Any.Type) -> Void
    ) {
        withoutActuallyEscaping(body) { escaping in
            withUnsafePointer(to: escaping) { ctxPtr in
                IAGTypeApplyFieldsC(
                    type,
                    { name, offset, ty, ctx in
                        ctx.assumingMemoryBound(to: ((UnsafePointer<CChar>, Int, Any.Type) -> Void).self)
                            .pointee(name, offset, ty.type)
                    },
                    UnsafeRawPointer(ctxPtr))
            }
        }
    }

    static func forEachField2(
        of type: Metadata,
        options: Metadata.ApplyOptions,
        _ body: (UnsafePointer<CChar>, Int, Any.Type) -> Bool
    ) -> Bool {
        withoutActuallyEscaping(body) { escaping in
            withUnsafePointer(to: escaping) { ctxPtr in
                IAGTypeApplyFields2C(
                    type, options,
                    { name, offset, ty, ctx in
                        ctx.assumingMemoryBound(to: ((UnsafePointer<CChar>, Int, Any.Type) -> Bool).self)
                            .pointee(name, offset, ty.type)
                    },
                    UnsafeRawPointer(ctxPtr))
            }
        }
    }

    // MARK: - Attribute body mutation (synchronous)

    static func mutateBody<Body>(
        _ attribute: AnyAttribute, as type: Body.Type, invalidating: Bool,
        _ mutator: (inout Body) -> Void
    ) {
        withoutActuallyEscaping(mutator) { escapingMutator in
            let modify: (UnsafeMutableRawPointer) -> Void = { pointer in
                escapingMutator(&pointer.assumingMemoryBound(to: Body.self).pointee)
            }
            withUnsafePointer(to: modify) { ctxPtr in
                IAGGraphMutateAttributeC(
                    attribute, Metadata(type), invalidating,
                    { body, ctx in
                        ctx.assumingMemoryBound(to: ((UnsafeMutableRawPointer) -> Void).self).pointee(body)
                    },
                    UnsafeRawPointer(ctxPtr))
            }
        }
    }

    // MARK: - Cached value read (synchronous type-id getter)

    static func readCachedAttribute(
        hash: Int, type: Metadata, body: UnsafeRawPointer, valueType: Metadata,
        options: CachedValueOptions, owner: AnyAttribute,
        makeTypeID: (UnownedGraphContext) -> UInt32
    ) -> UnsafeRawPointer {
        withoutActuallyEscaping(makeTypeID) { escaping in
            withUnsafePointer(to: escaping) { ctxPtr in
                UnsafeRawPointer(
                    IAGGraphReadCachedAttributeC(
                        hash, type, body, valueType, options, owner, nil,
                        { graph, ctx in
                            ctx.assumingMemoryBound(to: ((UnownedGraphContext) -> UInt32).self).pointee(graph)
                        },
                        UnsafeRawPointer(ctxPtr))!)
            }
        }
    }

    // MARK: - Subgraph apply (synchronous traversal)

    static func subgraphForEach(_ subgraph: Subgraph, _ flags: Subgraph.Flags, _ body: (AnyAttribute) -> Void) {
        withoutActuallyEscaping(body) { escaping in
            withUnsafePointer(to: escaping) { ctxPtr in
                IAGSubgraphApplyC(
                    subgraph, UInt32(flags.rawValue),
                    { attr, ctx in
                        ctx.assumingMemoryBound(to: ((AnyAttribute) -> Void).self).pointee(attr)
                    },
                    UnsafeRawPointer(ctxPtr))
            }
        }
    }

    // MARK: - Graph callbacks (persistent — the engine stores the callback)
    //
    // These closures outlive the call (the engine invokes them on later updates/invalidations), so a
    // stack pointer won't do — box on the heap. The engine does NOT retain the context, so we own the
    // +1 (passRetained) and intentionally never release it: the callback is registered once for the
    // graph's lifetime. The no/one-arg swiftcc callback matches a @convention(c) thunk, so these route
    // through the refined imports directly (no C-variant needed).

    static func onUpdate(_ graph: Graph, _ handler: @escaping () -> Void) {
        let ctx = Unmanaged.passRetained(_UpdateBox(handler)).toOpaque()
        IAGGraphSetUpdateCallbackC(
            graph,
            { c in Unmanaged<_UpdateBox>.fromOpaque(c).takeUnretainedValue().f() },
            UnsafeRawPointer(ctx))
    }

    static func onInvalidation(_ graph: Graph, _ handler: @escaping (AnyAttribute) -> Void) {
        let ctx = Unmanaged.passRetained(_InvalidationBox(handler)).toOpaque()
        IAGGraphSetInvalidationCallbackC(
            graph,
            { attr, c in Unmanaged<_InvalidationBox>.fromOpaque(c).takeUnretainedValue().f(attr) },
            UnsafeRawPointer(ctx))
    }

    // Subgraph observer — PERSISTENT (the subgraph stores it and fires it on invalidation), so heap-box
    // like the graph callbacks above. The stale `@_silgen_name("IAGSubgraphAddObserver")` 2-arg binding
    // (Subgraph.swift) lowered to a wasm import signature that disagreed with the real 3-arg C symbol
    // `(IAGSubgraphRef, fn-with-context swiftcc, context) -> IAGUniqueID`, trapping `signature_mismatch`
    // on wasm. Route through the refined import `__IAGSubgraphAddObserver` (IAG_REFINED_FOR_SWIFT) whose
    // signature matches the C definition exactly.
    static func subgraphAddObserver(_ subgraph: Subgraph, _ observer: @escaping () -> Void) -> Int {
        let ctx = Unmanaged.passRetained(_ObserverBox(observer)).toOpaque()
        return Int(IAGSubgraphAddObserverC(
            subgraph,
            { c in Unmanaged<_ObserverBox>.fromOpaque(c).takeUnretainedValue().f() },
            UnsafeRawPointer(ctx)))
    }
}

// Heap boxes for the persistent Graph callbacks (see WasiClosureShim.onUpdate/onInvalidation).
private final class _UpdateBox {
    let f: () -> Void
    init(_ f: @escaping () -> Void) { self.f = f }
}
private final class _InvalidationBox {
    let f: (AnyAttribute) -> Void
    init(_ f: @escaping (AnyAttribute) -> Void) { self.f = f }
}
private final class _ObserverBox {
    let f: () -> Void
    init(_ f: @escaping () -> Void) { self.f = f }
}
#endif
